/*
 * Renders SVG to PNG with headless Chrome, over the DevTools protocol, so
 * the kit needs no npm packages. One browser serves a whole build.
 *
 *   const r = await openRasterizer();
 *   const png = await r.png(svg, 512, 512);   // a Buffer
 *   await r.close();
 *
 * Set CHROME to the browser's path if it isn't Google Chrome in
 * /Applications (macOS) or google-chrome / chromium on the PATH.
 */
import { spawn, execSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

function findChrome() {
  if (process.env.CHROME) return process.env.CHROME;
  const mac = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  if (fs.existsSync(mac)) return mac;
  for (const name of ['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser']) {
    try {
      return execSync(`command -v ${name}`, { encoding: 'utf8' }).trim();
    } catch { /* not this one */ }
  }
  throw new Error('No Chrome found. Set CHROME to the path of Chrome or Chromium.');
}

export async function openRasterizer() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ogos-raster-'));
  const proc = spawn(findChrome(), [
    '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run', '--no-default-browser-check',
    '--force-color-profile=srgb', '--remote-debugging-port=0', `--user-data-dir=${dir}`, 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'] });

  const wsUrl = await new Promise((resolve, reject) => {
    let buf = '';
    const t = setTimeout(() => reject(new Error('Chrome did not start in 20 s')), 20000);
    proc.stderr.on('data', (d) => {
      buf += d;
      const m = /DevTools listening on (ws:\/\/\S+)/.exec(buf);
      if (m) { clearTimeout(t); resolve(m[1]); }
    });
    proc.on('exit', (code) => reject(new Error(`Chrome exited (${code}): ${buf.slice(-400)}`)));
  });

  const ws = new WebSocket(wsUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let nextId = 0;
  const pending = new Map();
  const waiters = [];
  ws.onmessage = (ev) => {
    const msg = JSON.parse(ev.data);
    if (msg.id != null && pending.has(msg.id)) {
      const { resolve, reject } = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) reject(new Error(msg.error.message));
      else resolve(msg.result);
    } else if (msg.method) {
      for (let i = waiters.length - 1; i >= 0; i--) {
        if (waiters[i].method === msg.method && waiters[i].sessionId === msg.sessionId) {
          waiters[i].resolve(msg.params);
          waiters.splice(i, 1);
        }
      }
    }
  };
  const send = (method, params = {}, sessionId) => new Promise((resolve, reject) => {
    const id = ++nextId;
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }));
  });
  const once = (method, sessionId) => new Promise((resolve) => waiters.push({ method, sessionId, resolve }));

  const { targetId } = await send('Target.createTarget', { url: 'about:blank' });
  const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true });
  const s = (method, params) => send(method, params, sessionId);
  await s('Page.enable');
  await s('Emulation.setDefaultBackgroundColorOverride', { color: { r: 0, g: 0, b: 0, a: 0 } });

  return {
    /*
     * Renders an SVG document at width x height pixels and returns the PNG.
     * The SVG scales to fit; pixel art should be rendered at a whole multiple
     * of its size.
     */
    async png(svg, width, height) {
      const sized = svg.replace(/<svg([^>]*)>/, (m, attrs) =>
        `<svg${attrs.replace(/\s(width|height)="[^"]*"/g, '')} width="${width}" height="${height}" preserveAspectRatio="xMidYMid meet">`);
      const html = `<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;padding:0;background:transparent}svg{display:block}</style></head><body>${sized}</body></html>`;
      await s('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: false });
      const loaded = once('Page.loadEventFired', sessionId);
      await s('Page.navigate', { url: 'data:text/html;base64,' + Buffer.from(html).toString('base64') });
      await loaded;
      const { data } = await s('Page.captureScreenshot', { format: 'png', clip: { x: 0, y: 0, width, height, scale: 1 } });
      return Buffer.from(data, 'base64');
    },
    async close() {
      const exited = proc.exitCode != null ? Promise.resolve() : new Promise((res) => proc.once('exit', res));
      try { await send('Browser.close'); } catch { /* already gone */ }
      ws.close();
      await Promise.race([exited, new Promise((res) => setTimeout(res, 5000))]);
      proc.kill();
      fs.rmSync(dir, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 });
    },
  };
}

/*
 * Packs PNGs into a .ico file (PNG-compressed entries, which every current
 * browser and OS reads). images: [{ size, png }].
 */
export function ico(images) {
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0);
  header.writeUInt16LE(1, 2);
  header.writeUInt16LE(images.length, 4);
  const dir = Buffer.alloc(16 * images.length);
  let offset = 6 + dir.length;
  images.forEach(({ size, png }, n) => {
    const o = n * 16;
    dir.writeUInt8(size >= 256 ? 0 : size, o);
    dir.writeUInt8(size >= 256 ? 0 : size, o + 1);
    dir.writeUInt8(0, o + 2);
    dir.writeUInt8(0, o + 3);
    dir.writeUInt16LE(1, o + 4);
    dir.writeUInt16LE(32, o + 6);
    dir.writeUInt32LE(png.length, o + 8);
    dir.writeUInt32LE(offset, o + 12);
    offset += png.length;
  });
  return Buffer.concat([header, dir, ...images.map((i) => i.png)]);
}
