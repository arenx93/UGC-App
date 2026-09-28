// Local Framecraft server for the desktop app.
// Runs the built Cloudflare worker (dist/) through Wrangler/workerd, with all
// state (D1 database, R2 media, encryption key) inside the user's data folder.
const { spawn, spawnSync } = require("node:child_process");
const crypto = require("node:crypto");
const fs = require("node:fs");
const net = require("node:net");
const path = require("node:path");

const PREFERRED_PORT = 47831;

function readSecret(dataDir) {
  const envFile = path.join(dataDir, "secrets.env");
  const stateDir = path.join(dataDir, "state");
  if (!fs.existsSync(envFile)) {
    if (fs.existsSync(stateDir)) {
      throw new Error(
        `Falta ${envFile} pero existen datos guardados. Restaura ese archivo desde tu copia de seguridad.`,
      );
    }
    fs.mkdirSync(dataDir, { recursive: true });
    const secret = crypto.randomBytes(32).toString("base64");
    fs.writeFileSync(envFile, `KEY_ENCRYPTION_SECRET=${secret}\n`, { flag: "wx", mode: 0o600 });
  }
  const secret = fs
    .readFileSync(envFile, "utf8")
    .match(/^KEY_ENCRYPTION_SECRET=(.+)$/m)?.[1]
    ?.trim();
  if (!secret || Buffer.from(secret, "base64").length !== 32) {
    throw new Error("La clave de cifrado local no es válida. No la reemplaces si ya tienes datos guardados.");
  }
  return envFile;
}

// Wrangler writes temp files next to its config, and the app bundle is
// read-only (and signed) on macOS, so run from a per-version writable copy.
function prepareRuntime(appDir, dataDir, version, reuse) {
  const runtimeDir = path.join(dataDir, "runtime", version);
  const marker = path.join(runtimeDir, ".ready");
  if (!reuse || !fs.existsSync(marker)) {
    fs.rmSync(runtimeDir, { recursive: true, force: true });
    fs.mkdirSync(runtimeDir, { recursive: true });
    fs.cpSync(path.join(appDir, "dist"), path.join(runtimeDir, "dist"), {
      recursive: true,
      filter: (source) => !source.includes(`${path.sep}.wrangler`),
    });
    fs.cpSync(path.join(appDir, "drizzle"), path.join(runtimeDir, "drizzle"), { recursive: true });
    fs.copyFileSync(path.join(appDir, "wrangler.local.json"), path.join(runtimeDir, "wrangler.local.json"));
    fs.writeFileSync(marker, new Date().toISOString());
  }
  // Remove runtimes left behind by older versions.
  for (const entry of fs.readdirSync(path.join(dataDir, "runtime"))) {
    if (entry !== version) fs.rmSync(path.join(dataDir, "runtime", entry), { recursive: true, force: true });
  }
  return runtimeDir;
}

function wranglerEnv(dataDir) {
  const tools = path.join(dataDir, "wrangler");
  return {
    ...process.env,
    ELECTRON_RUN_AS_NODE: "1",
    CI: "true",
    NO_COLOR: "1",
    FORCE_COLOR: "0",
    CLOUDFLARE_CF_FETCH_ENABLED: "false",
    WRANGLER_SEND_METRICS: "false",
    WRANGLER_WRITE_LOGS: "false",
    WRANGLER_LOG_PATH: path.join(tools, "logs"),
    WRANGLER_REGISTRY_PATH: path.join(tools, "dev-registry"),
    MINIFLARE_REGISTRY_PATH: path.join(tools, "registry"),
    XDG_CONFIG_HOME: path.join(tools, "config"),
  };
}

function portFree(port) {
  return new Promise((resolve) => {
    const server = net.createServer();
    server.once("error", () => resolve(false));
    server.listen(port, "127.0.0.1", () => server.close(() => resolve(true)));
  });
}

async function pickPort() {
  if (await portFree(PREFERRED_PORT)) return PREFERRED_PORT;
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const { port } = server.address();
      server.close(() => resolve(port));
    });
  });
}

async function waitForServer(url, child, timeoutMs = 90000) {
  const started = Date.now();
  while (Date.now() - started < timeoutMs) {
    if (child.exitCode !== null) throw new Error("El servidor local se cerró durante el arranque.");
    try {
      const response = await fetch(url, { signal: AbortSignal.timeout(2000) });
      if (response.status < 500) return;
    } catch {
      // Not ready yet.
    }
    await new Promise((resolve) => setTimeout(resolve, 300));
  }
  throw new Error("El servidor local tardó demasiado en arrancar.");
}

/**
 * Starts the local server. Returns { url, stop, child, token }.
 * `log` receives every line printed by Wrangler (used for the log file).
 */
async function startServer({ appDir, dataDir, version, nodeBinary, log, reuseRuntime = true }) {
  const envFile = readSecret(dataDir);
  const runtimeDir = prepareRuntime(appDir, dataDir, version, reuseRuntime);
  // Same flags as wrangler/bin/wrangler.js, without its extra wrapper process.
  const wrangler = [
    "--no-warnings",
    "--experimental-vm-modules",
    "--require", path.join(__dirname, "node-shim.cjs"),
    path.join(appDir, "node_modules", "wrangler", "wrangler-dist", "cli.js"),
  ];
  const persist = path.join(dataDir, "state");
  const env = wranglerEnv(dataDir);

  const migrate = spawnSync(
    nodeBinary,
    [...wrangler, "d1", "migrations", "apply", "DB", "--local", "--config", path.join(runtimeDir, "wrangler.local.json"), "--persist-to", persist],
    { cwd: runtimeDir, env, encoding: "utf8", windowsHide: true },
  );
  log(`[migrations] ${migrate.stdout || ""}${migrate.stderr || ""}`);
  if (migrate.error) throw migrate.error;
  if (migrate.status !== 0) throw new Error("No se pudo preparar la base de datos local. Revisa el registro.");

  // Per-launch secret: only requests from this app's window may act as the
  // local desktop user (see app/chatgpt-auth.ts).
  const token = crypto.randomBytes(24).toString("hex");
  const sessionEnv = path.join(runtimeDir, "session.env");
  fs.rmSync(sessionEnv, { force: true });
  fs.writeFileSync(sessionEnv, `DESKTOP_TOKEN=${token}\n`, { mode: 0o600 });

  const port = await pickPort();
  const child = spawn(
    nodeBinary,
    [
      ...wrangler, "dev",
      "--config", path.join(runtimeDir, "dist", "server", "wrangler.json"),
      "--local",
      "--persist-to", persist,
      "--ip", "127.0.0.1",
      "--port", String(port),
      "--inspector-port", "0",
      "--env-file", envFile,
      "--env-file", sessionEnv,
      "--show-interactive-dev-session=false",
      "--log-level", "warn",
    ],
    { cwd: runtimeDir, env, windowsHide: true, detached: process.platform !== "win32" },
  );
  child.stdout.on("data", (chunk) => log(String(chunk)));
  child.stderr.on("data", (chunk) => log(String(chunk)));

  let stopped = false;
  const stop = () => {
    if (stopped || child.exitCode !== null) return;
    stopped = true;
    try {
      if (process.platform === "win32") {
        // Also terminate workerd, which Wrangler starts as a child process.
        spawnSync("taskkill", ["/pid", String(child.pid), "/T", "/F"], { windowsHide: true });
      } else {
        process.kill(-child.pid, "SIGTERM");
      }
    } catch {
      child.kill();
    }
  };

  const url = `http://127.0.0.1:${port}`;
  try {
    await waitForServer(url + "/", child);
  } catch (error) {
    stop();
    throw error;
  }
  return { url, stop, child, token };
}

module.exports = { startServer };
