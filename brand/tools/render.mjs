/*
 * Renders one composition: a module that draws with the brand library.
 *
 *   node tools/render.mjs my-poster.mjs                 # my-poster.svg beside it
 *   node tools/render.mjs my-poster.mjs --png           # and my-poster.png
 *   node tools/render.mjs my-poster.mjs --png 1,2       # .png and @2x.png
 *   node tools/render.mjs my-poster.mjs --out /some/dir
 *   node tools/render.mjs some.svg --png 2             # any SVG to PNG, e.g. to look at it
 *
 * A composition module exports a default function that returns an SVG
 * document (svgDoc), and may export `meta`:
 *
 *   import * as B from '<path to brand>/tools/lib/index.mjs';
 *   export const meta = { title: 'Poster', use: 'One line on what it is for', theme: 'base' };
 *   export default () => B.svgDoc(800, 600, B.rect(0, 0, 800, 600, B.PALETTE.blue) + ...);
 *
 * examples/*.mjs are all compositions; start from the closest one.
 */
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { docSize } from './lib/core.mjs';

export async function loadComposition(file) {
  const mod = await import(pathToFileURL(path.resolve(file)).href);
  if (typeof mod.default !== 'function') throw new Error(`${file}: the default export should be a function returning an SVG document`);
  return { meta: { title: path.basename(file, '.mjs'), ...mod.meta }, svg: mod.default };
}

async function cli() {
  const args = process.argv.slice(2);
  const file = args.find((a) => !a.startsWith('--') && /\.(mjs|svg)$/.test(a));
  if (!file) {
    console.error('usage: node tools/render.mjs <composition.mjs | file.svg> [--png [scales]] [--out <dir>]');
    process.exit(2);
  }
  const opt = (name) => {
    const i = args.indexOf(name);
    if (i < 0) return undefined;
    const v = args[i + 1];
    return v && !v.startsWith('--') && !/\.(mjs|svg)$/.test(v) ? v : true;
  };
  const isSvg = file.endsWith('.svg');
  const svg = isSvg ? fs.readFileSync(file, 'utf8') : (await loadComposition(file)).svg();
  const outDir = typeof opt('--out') === 'string' ? opt('--out') : path.dirname(file);
  fs.mkdirSync(outDir, { recursive: true });
  const base = path.join(outDir, path.basename(file).replace(/\.(mjs|svg)$/, ''));
  if (!isSvg) {
    fs.writeFileSync(`${base}.svg`, svg);
    console.log(`${base}.svg`);
  }
  const png = opt('--png') || (isSvg ? true : undefined);
  if (png) {
    const { openRasterizer } = await import('./lib/raster.mjs');
    const scales = png === true ? [1] : String(png).split(',').map(Number);
    const size = docSize(svg);
    if (!size.w || !size.h) throw new Error(`${file}: the <svg> needs width and height attributes`);
    const r = await openRasterizer();
    try {
      for (const s of scales) {
        const f = `${base}${s === 1 ? '' : `@${s}x`}.png`;
        fs.writeFileSync(f, await r.png(svg, Math.round(size.w * s), Math.round(size.h * s)));
        console.log(f);
      }
    } finally {
      await r.close();
    }
  }
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  cli().catch((e) => {
    console.error(e.message);
    process.exit(1);
  });
}
