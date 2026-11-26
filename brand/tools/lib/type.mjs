/*
 * Type: setting text in the kit's pixel faces, and in Jost.
 *
 * The pixel faces (faces/*.mjs) are the brand's default voice. Jost is for
 * the Mission Patch theme only, in capitals; see GUIDE.md, "Type".
 */
import { bitmap, bbox, f2 } from './core.mjs';
import display from './faces/display.mjs';
import label from './faces/label.mjs';
import micro from './faces/micro.mjs';
import system from './faces/system.mjs';
import JOST from './jost-glyphs.mjs';

export const FACES = { display, label, micro, system };

const blankGlyph = (face, w) => Array(face.height).fill('.'.repeat(w));

// A glyph's rows: the character, else its other case, else '?', else a box.
export function glyph(face, ch) {
  const g = face.glyphs;
  if (ch === ' ') return blankGlyph(face, face.space);
  const c = face.capsOnly ? ch.toUpperCase() : ch;
  const rows = g[c] || g[c.toUpperCase()] || g[c.toLowerCase()] || g['?'];
  if (rows) return rows;
  const w = Math.max(3, Math.round(face.cap * 0.6));
  return [...Array(face.cap).fill('#'.repeat(w)), ...blankGlyph(face, w).slice(face.cap)];
}

/*
 * Lays text out as rows of '#' and '.', so a design can recolor, outline or
 * lean the pixels itself. One line; the rows are face.height tall.
 *   opts.tracking: blank columns between glyphs (default face.tracking)
 */
export function textRows(text, faceId = 'display', opts = {}) {
  const face = typeof faceId === 'string' ? FACES[faceId] : faceId;
  const tracking = opts.tracking == null ? face.tracking : opts.tracking;
  const rows = Array(face.height).fill('');
  [...text].forEach((ch, n) => {
    const g = glyph(face, ch);
    for (let j = 0; j < face.height; j++) rows[j] += (n ? '.'.repeat(tracking) : '') + (g[j] || '.'.repeat(g[0].length));
  });
  return rows;
}

/*
 * Draws a line of text in a pixel face. Returns { svg, w, h, rows } in
 * output units.
 *   opts: face ('display' | 'label' | 'micro' | 'system'), x, y (the top of
 *   the glyph cells, which is the cap line), px (pixel size), fill,
 *   tracking, anchor ('start' | 'middle' | 'end'),
 *   shadow: { dx, dy, fill }, in font pixels, drawn behind
 */
export function pixelText(text, opts = {}) {
  const face = FACES[opts.face || 'display'];
  const px = opts.px || 4;
  const rows = textRows(text, face, opts);
  const w = rows[0].length * px, h = face.height * px;
  let x = opts.x || 0;
  if (opts.anchor === 'middle') x -= w / 2;
  else if (opts.anchor === 'end') x -= w;
  const y = opts.y || 0;
  let svg = '';
  if (opts.shadow) {
    const s = opts.shadow;
    svg += bitmap(rows, { '#': s.fill }, { x: x + (s.dx ?? 1) * px, y: y + (s.dy ?? 1) * px, px });
  }
  svg += bitmap(rows, { '#': opts.fill || '#000' }, { x, y, px });
  return { svg, w, h, x, y, rows, face };
}

// The width of pixelText's text in output units.
export const pixelWidth = (text, opts = {}) => textRows(text, opts.face || 'display', opts)[0].length * (opts.px || 4);

/*
 * The largest whole pixel size at which the text fits in maxW (and maxH, if
 * given), at least 1.
 */
export function fitPx(text, opts, maxW, maxH = Infinity) {
  const face = FACES[opts.face || 'display'];
  const cols = textRows(text, face, opts)[0].length;
  return Math.max(1, Math.min(Math.floor(maxW / cols), Math.floor(maxH / face.height)));
}

// Where a word's ink sits in its rows, for centering it optically.
export const inkBox = (text, opts = {}) => bbox(textRows(text, opts.face || 'display', opts));

/* ------------------------------------------------------------------ Jost */

/*
 * Jost is set as outlines, so the SVGs need no font. Sizes are in output
 * units; tracking is in ems, added after every glyph but the last. Use it in
 * capitals, tracked about 0.2em; patch() and friends uppercase for you.
 */

const PATH_RE = /([MLQCZ])([^MLQCZ]*)/g;

// Rewrites a glyph's path (font units, y down) through f(x, y) -> [x, y].
function mapPath(d, f) {
  let out = '';
  for (const [, cmd, args] of d.matchAll(PATH_RE)) {
    const n = args.trim() ? args.trim().split(/[\s,]+/).map(Number) : [];
    let s = cmd;
    for (let i = 0; i < n.length; i += 2) {
      const [x, y] = f(n[i], n[i + 1]);
      s += (i ? ' ' : '') + f2(x) + ' ' + f2(y);
    }
    out += s;
  }
  return out;
}

// Each glyph's left edge and advance, in output units.
export function jostLayout(text, opts = {}) {
  const font = JOST[opts.weight || 700];
  const size = opts.size || 24, k = size / font.upm, track = (opts.tracking || 0) * size;
  const chars = [...text];
  let x = 0;
  const glyphs = chars.map((ch, n) => {
    const g = font.glyphs[ch] || font.glyphs['?'];
    const kern = n + 1 < chars.length ? (font.kern[ch + chars[n + 1]] || 0) * k : 0;
    const item = { ch, g, x, adv: g.a * k };
    x += g.a * k + kern + (n + 1 < chars.length ? track : 0);
    return item;
  });
  return { glyphs, width: x, size, k, font };
}

export const jostWidth = (text, opts = {}) => jostLayout(text, opts).width;

// The size at which text is maxW wide, no bigger than opts.size.
export const jostFit = (text, opts, maxW) => Math.min(opts.size || 24, (maxW / jostWidth(text, { ...opts, size: 100 })) * 100);

// The height of Jost's capitals at a size.
export const jostCap = (size, weight = 700) => (JOST[weight].capHeight / JOST[weight].upm) * size;

/*
 * Jost on a straight baseline. opts: x, y (the baseline), size, weight
 * (400, 500 or 700), tracking (em), fill, anchor.
 */
export function jostText(text, opts = {}) {
  const L = jostLayout(text, opts);
  let x0 = opts.x || 0;
  if (opts.anchor === 'middle') x0 -= L.width / 2;
  else if (opts.anchor === 'end') x0 -= L.width;
  const y0 = opts.y || 0;
  const d = L.glyphs.map(({ g, x }) => mapPath(g.d, (gx, gy) => [x0 + x + gx * L.k, y0 + gy * L.k])).join('');
  return `<path d="${d}" fill="${opts.fill || '#000'}"/>`;
}

/*
 * Jost around a circle, centered on its top or bottom, reading left to
 * right either way. The circle is the baseline: on the top the capitals
 * point outward, on the bottom toward the center, so the bottom's radius
 * should be the larger. opts: cx, cy, r (the baseline's radius), side
 * ('top' | 'bottom'), and jostText's options.
 */
export function jostArc(text, opts = {}) {
  const L = jostLayout(text, opts);
  const { cx = 0, cy = 0, r = 100 } = opts;
  const top = (opts.side || 'top') === 'top';
  const d = L.glyphs.map(({ g, x, adv }) => {
    const s = x + adv / 2 - L.width / 2; // arc length from the middle to the glyph's center
    const th = top ? -Math.PI / 2 + s / r : Math.PI / 2 - s / r;
    const px = cx + r * Math.cos(th), py = cy + r * Math.sin(th);
    // The baseline's direction, and the glyph's downward direction.
    const T = top ? [-Math.sin(th), Math.cos(th)] : [Math.sin(th), -Math.cos(th)];
    const D = top ? [Math.cos(th), Math.sin(th)].map((v) => -v) : [Math.cos(th), Math.sin(th)];
    return mapPath(g.d, (gx, gy) => {
      const u = gx * L.k - adv / 2, v = gy * L.k;
      return [px + u * T[0] + v * D[0], py + u * T[1] + v * D[1]];
    });
  }).join('');
  return `<path d="${d}" fill="${opts.fill || '#000'}"/>`;
}
