// Preloaded (--require) into Wrangler when it runs on Electron's bundled Node
// (ELECTRON_RUN_AS_NODE). yargs treats packaged Electron apps as having no
// script path in process.argv; marking this as a "default app" restores the
// normal Node argument layout.
process.defaultApp = true;

// Shut down (through Wrangler's own signal handlers, which also stop workerd)
// if the desktop app disappears without stopping us, e.g. after a crash.
const parent = process.ppid;
setInterval(() => {
  try {
    process.kill(parent, 0);
  } catch {
    if (process.listenerCount("SIGTERM")) process.emit("SIGTERM", "SIGTERM");
    else if (process.listenerCount("SIGINT")) process.emit("SIGINT", "SIGINT");
    setTimeout(() => process.exit(0), 3000).unref();
  }
}, 2000).unref();
