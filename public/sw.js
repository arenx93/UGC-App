self.addEventListener('install',()=>self.skipWaiting());
self.addEventListener('activate',event=>event.waitUntil(self.clients.claim()));
// Network only: never cache private account data, images, credentials or auth responses.
self.addEventListener('fetch',event=>{if(event.request.mode==='navigate')event.respondWith(fetch(event.request).catch(()=>new Response('<!doctype html><html><meta name="viewport" content="width=device-width"><title>Framecraft — Offline</title><body style="background:#101113;color:#eee;font:16px system-ui;padding:40px"><h1>You’re offline</h1><p>Reconnect to open your synced workspace and generate images.</p><button onclick="location.reload()">Try again</button></body></html>',{headers:{'Content-Type':'text/html'}})));});
