/*
 * Checks images against the parts of the guide a machine can check.
 *
 *   node tools/check.mjs file.svg other.png ...   # these files
 *   node tools/check.mjs                          # everything in assets/ and examples/
 *
 * SVG: every color is in the palette (core.mjs's PALETTE and RAMPS); no
 * live <text> (outline it, as the kit's tools do); no typeface but Jost;
 * pixel art (crispEdges) is scaled only by whole numbers.
 * PNG: how much of the image is palette colors, and which colors aren't.
 *
 * It can't judge composition, the motifs, or whether something looks
 * right: that's the review skill's checklist (GUIDE.md, "Checklist").
 * Exits 1 if anything failed.
 */
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { ALL_COLORS, colorName } from './lib/core.mjs';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');

/* ---------------------------------------------------------------- colors */

const NAMED = { black: '#000000', white: '#FFFFFF', red: '#FF0000', blue: '#0000FF', green: '#008000', gray: '#808080', grey: '#808080', yellow: '#FFFF00' };

function normalize(c) {
  c = c.trim().toLowerCase();
  if (NAMED[c]) return NAMED[c];
  let m = /^#([0-9a-f]{3})$/.exec(c);
  if (m) return '#' + [...m[1]].map((x) => x + x).join('').toUpperCase();
  m = /^#([0-9a-f]{6})$/.exec(c);
  if (m) return '#' + m[1].toUpperCase();
  m = /^rgba?\(\s*(\d+)[\s,]+(\d+)[\s,]+(\d+)/.exec(c);
  if (m) return '#' + m.slice(1, 4).map((v) => (+v).toString(16).padStart(2, '0')).join('').toUpperCase();
  return null;
}

const rgb = (hex) => [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
const dist = (a, b) => Math.hypot(...a.map((v, i) => v - b[i]));
const PAL_RGB = ALL_COLORS.map((h) => [h, rgb(h)]);
function nearest(hex) {
  const c = rgb(hex);
  let best = null;
  for (const [h, v] of PAL_RGB) {
    const d = dist(c, v);
    if (!best || d < best.d) best = { hex: h, d };
  }
  return `${best.hex} (${colorName(best.hex)}), ${Math.round(best.d)} away`;
}

/* ------------------------------------------------------------------- SVG */

function checkSvg(text) {
  const out = [];
  const colors = new Map();
  // Attributes (either quote) and CSS declarations.
  const re = /\b(?:fill|stroke|stop-color|flood-color|color)\s*(?:=\s*(["'])(.*?)\1|:\s*([^;"'>}]+))/g;
  for (const m of text.matchAll(re)) {
    const raw = (m[2] ?? m[3]).trim();
    if (/^(none|transparent|currentcolor|inherit)$/i.test(raw) || raw.startsWith('url(')) continue;
    const hex = normalize(raw);
    if (!hex) {
      out.push(['WARN', `can't read the color "${raw}"`]);
      continue;
    }
    colors.set(hex, (colors.get(hex) || 0) + 1);
  }
  const off = [...colors.keys()].filter((h) => !ALL_COLORS.includes(h));
  if (off.length) off.forEach((h) => out.push(['FAIL', `off-palette color ${h}, used ${colors.get(h)}x; nearest is ${nearest(h)}`]));
  else out.push(['PASS', `${colors.size} colors, all in the palette`]);

  const texts = [...text.matchAll(/<text\b[^>]*>/g)];
  if (texts.length) out.push(['WARN', `${texts.length} live <text> element(s): they render in whatever font the viewer has. Set text with pixelText() or jostText(), which draw outlines`]);
  for (const m of text.matchAll(/font-family\s*(?:=\s*(["'])(.*?)\1|:\s*([^;"'>}]+))/g)) {
    const fam = (m[2] ?? m[3]).replace(/&quot;|'/g, '').trim();
    if (!/^jost\b/i.test(fam)) out.push(['FAIL', `typeface "${fam}": the kit uses its pixel faces, and Jost for patches only`]);
  }
  if (/<image\b/.test(text)) out.push(['WARN', 'embeds a raster <image>: check it with this tool too']);

  if (/<svg[^>]*shape-rendering="crispEdges"/.test(text)) {
    // A tag with data-brand-check="ignore" is a deliberate exception, like the don'ts figure's.
    for (const tag of text.matchAll(/<[^>]*\btransform=(["'])[^"']*scale\([^)]*\)[^"']*\1[^>]*>/g)) {
      if (/data-brand-check=["']ignore["']/.test(tag[0])) continue;
      for (const m of tag[0].matchAll(/scale\(\s*([\d.]+)(?:[\s,]+([\d.]+))?\s*\)/g)) {
        const ks = [m[1], m[2]].filter(Boolean).map(Number);
        if (ks.some((k) => k > 1 && Math.abs(k - Math.round(k)) > 1e-6)) out.push(['WARN', `pixel art scaled by ${m[0]}: scale pixel art by whole numbers`]);
      }
    }
  }
  return out;
}

/* ------------------------------------------------------------------- PNG */

// Decodes 8-bit, non-interlaced PNGs into RGBA. Enough for checking colors.
function decodePng(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let pos = 8, ihdr, palette, trns;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos), type = buf.toString('latin1', pos + 4, pos + 8), data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') ihdr = { w: data.readUInt32BE(0), h: data.readUInt32BE(4), depth: data[8], type: data[9], interlace: data[12] };
    else if (type === 'PLTE') palette = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IDAT') idat.push(data);
    pos += 12 + len;
  }
  if (ihdr.depth !== 8 || ihdr.interlace) throw new Error(`only 8-bit, non-interlaced PNGs (this is ${ihdr.depth}-bit${ihdr.interlace ? ', interlaced' : ''})`);
  const ch = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ihdr.type];
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = ihdr.w * ch, px = Buffer.alloc(ihdr.h * stride);
  for (let y = 0; y < ihdr.h; y++) {
    const f = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let x = 0; x < stride; x++) {
      const a = x >= ch ? px[y * stride + x - ch] : 0, b = y ? px[(y - 1) * stride + x] : 0, c = x >= ch && y ? px[(y - 1) * stride + x - ch] : 0;
      let v = line[x];
      if (f === 1) v += a;
      else if (f === 2) v += b;
      else if (f === 3) v += (a + b) >> 1;
      else if (f === 4) {
        const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      px[y * stride + x] = v & 255;
    }
  }
  const rgba = Buffer.alloc(ihdr.w * ihdr.h * 4);
  for (let i = 0; i < ihdr.w * ihdr.h; i++) {
    const s = px.subarray(i * ch, i * ch + ch);
    let r, g, b, a = 255;
    if (ihdr.type === 0) [r, g, b] = [s[0], s[0], s[0]];
    else if (ihdr.type === 4) [r, g, b, a] = [s[0], s[0], s[0], s[1]];
    else if (ihdr.type === 2) [r, g, b] = s;
    else if (ihdr.type === 6) [r, g, b, a] = s;
    else {
      [r, g, b] = palette.subarray(s[0] * 3, s[0] * 3 + 3);
      a = trns && s[0] < trns.length ? trns[s[0]] : 255;
    }
    rgba.set([r, g, b, a], i * 4);
  }
  return { w: ihdr.w, h: ihdr.h, rgba };
}

function checkPng(buf) {
  const out = [];
  const { w, h, rgba } = decodePng(buf);
  const counts = new Map();
  let opaque = 0;
  for (let i = 0; i < w * h; i++) {
    if (rgba[i * 4 + 3] < 255) continue; // edges and transparency aren't colors to check
    opaque++;
    const hex = '#' + [0, 1, 2].map((k) => rgba[i * 4 + k].toString(16).padStart(2, '0')).join('').toUpperCase();
    counts.set(hex, (counts.get(hex) || 0) + 1);
  }
  if (!opaque) return [['WARN', 'no opaque pixels']];
  let onPal = 0;
  for (const [hex, n] of counts) if (ALL_COLORS.includes(hex)) onPal += n;
  const share = onPal / opaque;
  out.push([share > 0.97 ? 'PASS' : share > 0.85 ? 'WARN' : 'FAIL', `${(share * 100).toFixed(1)}% of opaque pixels are palette colors; ${counts.size} colors in all`]);
  const off = [...counts].filter(([hx]) => !ALL_COLORS.includes(hx)).sort((a, b) => b[1] - a[1]).slice(0, 5);
  off.filter(([, n]) => n / opaque > 0.005).forEach(([hx, n]) => out.push(['WARN', `${hx} covers ${((n / opaque) * 100).toFixed(1)}%; nearest is ${nearest(hx)}`]));
  if (counts.size > 64 && share > 0.5) out.push(['INFO', 'many in-between colors: fine for vector art (patches); pixel art should have almost none, so check it was rendered at a whole-number scale']);
  return out;
}

/* ------------------------------------------------------------------ main */

function walk(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
    const p = path.join(dir, e.name);
    return e.isDirectory() ? walk(p) : /\.(svg|png)$/.test(e.name) ? [p] : [];
  });
}

const files = process.argv.slice(2).length
  ? process.argv.slice(2)
  : [...walk(path.join(ROOT, 'assets')), ...walk(path.join(ROOT, 'examples'))].filter((f) => f.endsWith('.svg'));
let failed = 0;
for (const f of files) {
  let res;
  try {
    res = f.endsWith('.svg') ? checkSvg(fs.readFileSync(f, 'utf8')) : checkPng(fs.readFileSync(f));
  } catch (e) {
    res = [['FAIL', e.message]];
  }
  const worst = res.some((r) => r[0] === 'FAIL') ? 'FAIL' : res.some((r) => r[0] === 'WARN') ? 'WARN' : 'PASS';
  if (worst === 'FAIL') failed++;
  if (process.argv.slice(2).length || worst !== 'PASS') {
    console.log(`${worst}  ${path.relative(process.cwd(), f)}`);
    res.filter((r) => r[0] !== 'PASS' || process.argv.slice(2).length).forEach(([lvl, msg]) => console.log(`      ${lvl.padEnd(4)} ${msg}`));
  }
}
console.log(`${files.length} checked, ${failed} failed`);
process.exit(failed ? 1 : 0);
