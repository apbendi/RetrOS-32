/*
 * Makes one-off pieces from the command line, without writing a
 * composition. Writes an SVG, and a PNG too if --out ends in .png.
 *
 *   node tools/make.mjs badge --name OgOs --line "POWERED BY" --above [--sun]
 *   node tools/make.mjs patch --shape round|monitor --name OGOS --mission "RELEASE 1.0" [--shadow]
 *   node tools/make.mjs text "Hello, world" --face display|label|micro|system --px 4 --color cream [--bg blue] [--shadow]
 *   node tools/make.mjs icon --size 512           # the app icon at a size (a multiple of 32, or 16)
 *
 * Common options: --out <file.svg|file.png> (default: ./<kind>.svg),
 * --scale N (PNG scale, whole numbers for pixel art; default 1).
 * Colors are palette names (tools/lib/core.mjs): blue, cream, ink, ...
 */
import fs from 'node:fs';
import path from 'node:path';
import * as B from './lib/index.mjs';

const P = B.PALETTE;
const [kind, ...rest] = process.argv.slice(2);
const opts = {};
const positional = [];
for (let i = 0; i < rest.length; i++) {
  if (rest[i].startsWith('--')) {
    const k = rest[i].slice(2);
    if (i + 1 < rest.length && !rest[i + 1].startsWith('--')) opts[k] = rest[++i];
    else opts[k] = true;
  } else positional.push(rest[i]);
}

const color = (name, fallback) => {
  if (!name) return fallback;
  if (!P[name]) throw new Error(`unknown color "${name}"; use one of: ${Object.keys(P).join(', ')}`);
  return P[name];
};

function make() {
  if (kind === 'badge') {
    const o = { name: opts.name, line: opts.line === true ? '' : opts.line, above: !!opts.above, screen: opts.sun ? 'sun' : 'led' };
    const map = { ...B.MARK_MAP, e: B.RAMPS.cream[4], n: P.ink, m: P.red };
    return { svg: B.svgDoc(88, 31, B.bitmap(B.badgeRows(o), map), { crisp: true, title: `${o.name || 'OgOs'} badge` }), pixel: true };
  }
  if (kind === 'patch') {
    const f = (opts.shape || 'round') === 'monitor' ? B.monitorPatch : B.roundPatch;
    const p = f({ name: opts.name, mission: opts.mission, shadow: !!opts.shadow });
    const m = opts.shadow ? 12 : 0;
    return { svg: B.svgDoc(p.w + 2 * m, p.h + 2 * m, B.group(p.svg, `transform="translate(${m} ${m})"`), { title: 'Mission patch' }) };
  }
  if (kind === 'text') {
    const text = positional.join(' ');
    if (!text) throw new Error('text: give the text to set');
    const face = opts.face || 'display', px = +(opts.px || 4);
    if (!B.FACES[face]) throw new Error(`unknown face "${face}"; use display, label, micro or system`);
    const shadow = opts.shadow ? { dx: 1, dy: 1, fill: P.ink } : undefined;
    const t = B.pixelText(text, { face, px, x: px, y: px, fill: color(opts.color, P.cream), shadow });
    return { svg: B.svgDoc(t.w + 2 * px + (shadow ? px : 0), t.h + 2 * px, t.svg, { crisp: true, bg: opts.bg ? color(opts.bg) : undefined, title: text }), pixel: true };
  }
  if (kind === 'icon') {
    const size = +(opts.size || 32);
    const rows = B.tile(size === 16 ? 16 : 32);
    if (size !== 16 && size % 32) throw new Error('icon: --size must be 16 or a multiple of 32, so the pixels stay whole');
    return { svg: B.svgDoc(rows.length, rows.length, B.bitmap(rows, B.MARK_MAP), { crisp: true, title: 'OgOs app icon' }), pixel: true, scale: size / rows.length };
  }
  throw new Error('usage: node tools/make.mjs badge|patch|text|icon [options]; see the top of tools/make.mjs');
}

const main = async () => {
  const piece = make();
  const out = opts.out || `${kind}.svg`;
  const scale = +(opts.scale || piece.scale || 1);
  if (piece.pixel && scale !== Math.round(scale)) throw new Error('--scale must be a whole number for pixel art');
  const svgPath = out.endsWith('.png') ? out.replace(/\.png$/, '.svg') : out;
  fs.mkdirSync(path.dirname(path.resolve(svgPath)), { recursive: true });
  fs.writeFileSync(svgPath, piece.svg);
  console.log(svgPath);
  if (out.endsWith('.png')) {
    const { openRasterizer } = await import('./lib/raster.mjs');
    const { w, h } = B.docSize(piece.svg);
    const r = await openRasterizer();
    try {
      fs.writeFileSync(out, await r.png(piece.svg, Math.round(w * scale), Math.round(h * scale)));
    } finally {
      await r.close();
    }
    console.log(out);
  }
};

main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
