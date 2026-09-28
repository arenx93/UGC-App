// Only the local splash page (file://) gets these helpers.
const { contextBridge, ipcRenderer } = require("electron");

if (location.protocol === "file:") {
  contextBridge.exposeInMainWorld("framecraft", {
    retry: () => ipcRenderer.invoke("framecraft:retry"),
    openLogs: () => ipcRenderer.invoke("framecraft:open-logs"),
    openData: () => ipcRenderer.invoke("framecraft:open-data"),
  });
}
