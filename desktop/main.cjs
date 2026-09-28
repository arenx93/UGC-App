// Framecraft desktop shell (macOS + Windows).
const { app, BrowserWindow, Menu, dialog, ipcMain, nativeTheme, session, shell } = require("electron");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { startServer } = require("./server.cjs");

const APP_DIR = path.join(__dirname, "..");
const IS_MAC = process.platform === "darwin";
const IS_WIN = process.platform === "win32";
const SMOKE = process.argv.includes("--smoke-test");
const BG = "#0b0a12";

app.setName("Framecraft");
if (!app.requestSingleInstanceLock()) app.quit();

const dataDir = path.join(app.getPath("userData"), "Framecraft-Data");
const logFile = path.join(dataDir, "logs", "server.log");
let server = null;
let mainWindow = null;
let quitting = false;

function log(text) {
  try {
    fs.mkdirSync(path.dirname(logFile), { recursive: true });
    fs.appendFileSync(logFile, text.endsWith("\n") ? text : text + "\n");
  } catch {
    // Logging must never break the app.
  }
}

// Mica needs Windows 11 (build 22000+); Windows 10 gets a solid window.
function windowsSupportsMica() {
  return IS_WIN && Number(os.release().split(".")[2] || 0) >= 22000;
}
const translucent = IS_MAC || windowsSupportsMica();
const platformTag = IS_MAC ? "mac" : IS_WIN ? "win" : "linux";

// ---------- window state ----------
const stateFile = path.join(app.getPath("userData"), "window-state.json");
function loadBounds() {
  try {
    return JSON.parse(fs.readFileSync(stateFile, "utf8"));
  } catch {
    return { width: 1440, height: 900 };
  }
}
function saveBounds(win) {
  try {
    fs.writeFileSync(stateFile, JSON.stringify({ ...win.getNormalBounds(), maximized: win.isMaximized() }));
  } catch {
    // Ignore.
  }
}

function createWindow() {
  const bounds = loadBounds();
  const platformOptions = IS_MAC
    ? {
        titleBarStyle: "hiddenInset",
        trafficLightPosition: { x: 20, y: 24 },
        vibrancy: "under-window",
        visualEffectState: "active",
        backgroundColor: "#00000000",
      }
    : IS_WIN
      ? {
          titleBarStyle: "hidden",
          titleBarOverlay: { color: "#00000000", symbolColor: "#f3f0ff", height: 64 },
          ...(translucent ? { backgroundMaterial: "mica", backgroundColor: "#00000000" } : { backgroundColor: BG }),
        }
      : { backgroundColor: BG, autoHideMenuBar: true };

  const win = new BrowserWindow({
    x: bounds.x,
    y: bounds.y,
    width: bounds.width || 1440,
    height: bounds.height || 900,
    minWidth: 980,
    minHeight: 640,
    show: false,
    title: "Framecraft",
    icon: path.join(__dirname, "assets", "icon.png"),
    ...platformOptions,
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      sandbox: true,
      nodeIntegration: false,
      spellcheck: true,
    },
  });
  if (bounds.maximized) win.maximize();
  win.once("ready-to-show", () => win.show());
  win.on("close", () => saveBounds(win));

  // Open external links in the default browser; keep the app on its own server.
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (/^https?:\/\//.test(url)) shell.openExternal(url);
    return { action: "deny" };
  });
  win.webContents.on("will-navigate", (event, url) => {
    if (server && url.startsWith(server.url)) return;
    if (url.startsWith("file:")) return;
    event.preventDefault();
    if (/^https?:\/\//.test(url)) shell.openExternal(url);
  });
  return win;
}

function showSplash(win, error) {
  const query = error ? { error: String(error.message || error) } : {};
  return win.loadFile(path.join(__dirname, "splash.html"), { query: { platform: platformTag, ...query } });
}

async function boot() {
  try {
    fs.rmSync(logFile, { force: true });
  } catch {
    // Ignore.
  }
  log(`Framecraft ${app.getVersion()} · ${process.platform} ${process.arch} · ${new Date().toISOString()}`);
  showSplash(mainWindow);
  try {
    server = await startServer({
      appDir: APP_DIR,
      dataDir,
      version: app.getVersion(),
      nodeBinary: process.execPath,
      log,
      reuseRuntime: app.isPackaged,
    });
    log(`Server ready at ${server.url}`);
    server.child.on("exit", (code) => {
      log(`Server exited with code ${code}`);
      if (!quitting && mainWindow && !mainWindow.isDestroyed()) {
        server = null;
        showSplash(mainWindow, new Error("El servidor local se detuvo inesperadamente."));
      }
    });
    // The first visit creates the local profile session, then lands on the studio.
    await mainWindow.loadURL(`${server.url}/signin-with-chatgpt?return_to=/`);
    if (SMOKE) {
      const ok = await mainWindow.webContents.executeJavaScript(
        "!!document.querySelector('.studio') && document.documentElement.dataset.platform === " + JSON.stringify(platformTag),
      );
      console.log(ok ? "SMOKE OK" : "SMOKE FAIL");
      server.stop();
      app.exit(ok ? 0 : 1);
    }
  } catch (error) {
    log(`Startup failed: ${error.stack || error}`);
    if (SMOKE) {
      console.error(error);
      app.exit(1);
    }
    showSplash(mainWindow, error);
  }
}

function buildMenu() {
  const extras = [
    { label: "Abrir carpeta de datos", click: () => shell.openPath(dataDir) },
    { label: "Ver registro del servidor", click: () => shell.openPath(logFile) },
  ];
  const template = [
    ...(IS_MAC
      ? [
          {
            label: "Framecraft",
            submenu: [
              { role: "about", label: "Acerca de Framecraft" },
              { type: "separator" },
              ...extras,
              { type: "separator" },
              { role: "hide", label: "Ocultar Framecraft" },
              { role: "hideOthers", label: "Ocultar otros" },
              { role: "unhide", label: "Mostrar todo" },
              { type: "separator" },
              { role: "quit", label: "Salir de Framecraft" },
            ],
          },
        ]
      : [{ label: "Archivo", submenu: [...extras, { type: "separator" }, { role: "quit", label: "Salir" }] }]),
    {
      label: "Edición",
      submenu: [
        { role: "undo", label: "Deshacer" },
        { role: "redo", label: "Rehacer" },
        { type: "separator" },
        { role: "cut", label: "Cortar" },
        { role: "copy", label: "Copiar" },
        { role: "paste", label: "Pegar" },
        { role: "selectAll", label: "Seleccionar todo" },
      ],
    },
    {
      label: "Ver",
      submenu: [
        { role: "reload", label: "Recargar" },
        { type: "separator" },
        { role: "resetZoom", label: "Tamaño real" },
        { role: "zoomIn", label: "Acercar" },
        { role: "zoomOut", label: "Alejar" },
        { type: "separator" },
        { role: "togglefullscreen", label: "Pantalla completa" },
        ...(app.isPackaged ? [] : [{ role: "toggleDevTools" }]),
      ],
    },
    { role: "windowMenu", label: "Ventana" },
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

app.on("second-instance", () => {
  if (!mainWindow) return;
  if (mainWindow.isMinimized()) mainWindow.restore();
  mainWindow.focus();
});

app.whenReady().then(() => {
  nativeTheme.themeSource = "dark";
  buildMenu();

  // Identify the desktop shell to the local server (only for its own origin).
  session.defaultSession.webRequest.onBeforeSendHeaders((details, callback) => {
    if (server && details.url.startsWith(server.url)) {
      details.requestHeaders["x-framecraft-desktop"] = "1";
      details.requestHeaders["x-framecraft-desktop-token"] = server.token;
      details.requestHeaders["x-framecraft-platform"] = platformTag;
      if (translucent) details.requestHeaders["x-framecraft-material"] = "1";
    }
    callback({ requestHeaders: details.requestHeaders });
  });
  session.defaultSession.setPermissionRequestHandler((_webContents, permission, callback) => {
    callback(permission === "clipboard-sanitized-write" || permission === "fullscreen");
  });

  ipcMain.handle("framecraft:retry", () => {
    if (server) return;
    boot();
  });
  ipcMain.handle("framecraft:open-logs", () => shell.openPath(logFile));
  ipcMain.handle("framecraft:open-data", () => shell.openPath(dataDir));

  mainWindow = createWindow();
  boot();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      mainWindow = createWindow();
      if (server) mainWindow.loadURL(server.url + "/");
      else boot();
    }
  });
});

app.on("window-all-closed", () => {
  if (!IS_MAC) app.quit();
});

app.on("before-quit", () => {
  quitting = true;
  server?.stop();
});

process.on("uncaughtException", (error) => {
  log(`Uncaught: ${error.stack || error}`);
  if (!app.isReady()) return;
  dialog.showErrorBox("Framecraft", String(error.message || error));
});
