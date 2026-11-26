/*
 * The kit's foundations: the palette, SVG documents, and pixel art.
 *
 * Everything in the kit draws by returning SVG markup as a string. A
 * composition is markup wrapped in svgDoc(); pixel art is a grid of color
 * keys turned into markup by bitmap().
 */

/* ---------------------------------------------------------------- colors */

/*
 * The palette. GUIDE.md's "Color" section is the reference for what each is
 * for; tools/check.mjs flags any color not listed here.
 */
export const PALETTE = {
  // The core.
  blue: '#1F5F9E', // Desktop Blue: the ground everything starts on
  ink: '#0F1B2D', // outlines, and text on light grounds
  cream: '#EFEADB', // the machine's case, and text on blue
  paper: '#F4F1E8', // the light ground
  orange: '#FF6B2C', // Sunrise Orange: the LED, the sun's upper bands
  red: '#E0452B', // Signal Red: the sun's lower bands, selections
  screen: '#12243A', // a screen that's off
  // Support.
  blueDeep: '#163F6B', // shadows and bevels on blue, the planet's limb
  blueLight: '#4F8FD0', // highlights and bevels on blue
  sky: '#8EC3EE', // a cool accent: a stripe, a patch's small print
  steel: '#9DB0C6', // the monitor's foot, quiet labels
  steelDark: '#7A8FA8',
  white: '#FFFDF6', // highlights on cream
  peach: '#FFB08A', // the LED's glint
  space: '#0C1C31', // the sky on a mission patch
  charcoal: '#2A303C', // a neutral to show objects on, like patches
};

/*
 * Shading ramps for pixel art, light to dark. They extend the palette for
 * bevels and isometric faces; use them only for shading.
 */
export const RAMPS = {
  cream: ['#FFFDF6', '#FFF8E6', '#EFEADB', '#E2D7BC', '#C9BB99', '#B3A482'],
  blue: ['#8EC3EE', '#4F8FD0', '#2A6FB2', '#1F5F9E', '#163F6B'],
  steel: ['#9DB0C6', '#7A8FA8', '#5E6F86'],
  night: ['#12243A', '#0C1C31', '#0A1626', '#060E1A'],
};

// Every color the kit may use, for tools/check.mjs.
export const ALL_COLORS = [...new Set([...Object.values(PALETTE), ...Object.values(RAMPS).flat()].map((c) => c.toUpperCase()))];

// The name of a palette or ramp color, or null.
export function colorName(hex) {
  const h = hex.toUpperCase();
  for (const [k, v] of Object.entries(PALETTE)) if (v.toUpperCase() === h) return k;
  for (const [k, ramp] of Object.entries(RAMPS)) {
    const i = ramp.findIndex((v) => v.toUpperCase() === h);
    if (i >= 0) return `${k}[${i}]`;
  }
  return null;
}

/* ------------------------------------------------------------ SVG markup */

// Rounds a coordinate to 2 decimals, so output is short and stable.
export const f2 = (n) => +(+n).toFixed(2);

export const esc = (s) =>
  String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export const rect = (x, y, w, h, fill, extra = '') =>
  `<rect x="${f2(x)}" y="${f2(y)}" width="${f2(w)}" height="${f2(h)}" fill="${fill}"${extra ? ' ' + extra : ''}/>`;

export const circle = (cx, cy, r, fill, extra = '') =>
  `<circle cx="${f2(cx)}" cy="${f2(cy)}" r="${f2(r)}" fill="${fill}"${extra ? ' ' + extra : ''}/>`;

export const group = (inner, attrs = '') => `<g${attrs ? ' ' + attrs : ''}>${inner}</g>`;

// A unique id for clip paths and filters. svgDoc() renumbers them per
// document, so output doesn't depend on what else was drawn first.
let idCount = 0;
export const uid = (prefix = 'u') => `${prefix}${++idCount}`;

// Renames every id in a document to i0, i1, ... in order of appearance.
function renumberIds(svg) {
  const ids = [];
  svg.replace(/\sid="([^"]+)"/g, (m, id) => ids.push(id));
  if (!ids.length) return svg;
  const map = new Map(ids.map((id, n) => [id, `i${n}`]));
  return svg
    .replace(/(\sid=")([^"]+)(")/g, (m, a, id, b) => a + (map.get(id) || id) + b)
    .replace(/url\(#([^)]+)\)/g, (m, id) => `url(#${map.get(id) || id})`)
    .replace(/(href="#)([^"]+)(")/g, (m, a, id, b) => a + (map.get(id) || id) + b);
}

/*
 * A standalone SVG document, w by h user units, drawn at that size by
 * default. opts:
 *   crisp: turn off antialiasing, for pixel art
 *   bg: fill the canvas with this color first
 *   title: an accessible name
 */
export function svgDoc(w, h, inner, opts = {}) {
  const attrs = [
    'xmlns="http://www.w3.org/2000/svg"',
    `width="${f2(w)}"`,
    `height="${f2(h)}"`,
    `viewBox="0 0 ${f2(w)} ${f2(h)}"`,
  ];
  if (opts.crisp) attrs.push('shape-rendering="crispEdges"');
  const title = opts.title ? `<title>${esc(opts.title)}</title>` : '';
  const bg = opts.bg ? rect(0, 0, w, h, opts.bg) : '';
  return renumberIds(`<svg ${attrs.join(' ')}>${title}${bg}${inner}</svg>\n`);
}

// The size of a document from its width and height attributes.
export function docSize(svg) {
  const w = +(/<svg[^>]*\swidth="([\d.]+)"/.exec(svg) || [])[1];
  const h = +(/<svg[^>]*\sheight="([\d.]+)"/.exec(svg) || [])[1];
  return { w, h };
}

/*
 * Places one document inside another: its markup, scaled by `scale` and
 * moved to (x, y), with its ids kept apart from the host's.
 */
export function embed(svg, x, y, scale = 1) {
  const inner = svg.replace(/^[\s\S]*?<svg[^>]*>/, '').replace(/<\/svg>\s*$/, '').replace(/<title>[\s\S]*?<\/title>/, '');
  const crisp = /<svg[^>]*shape-rendering="crispEdges"/.test(svg) ? ' shape-rendering="crispEdges"' : '';
  const sfx = uid('e');
  const scoped = inner
    .replace(/(\sid=")([^"]+)(")/g, `$1$2${sfx}$3`)
    .replace(/url\(#([^)]+)\)/g, `url(#$1${sfx})`)
    .replace(/(href="#)([^"]+)(")/g, `$1$2${sfx}$3`);
  return `<g transform="translate(${f2(x)} ${f2(y)})${scale !== 1 ? ` scale(${f2(scale)})` : ''}"${crisp}>${scoped}</g>`;
}

/* ------------------------------------------------------------- pixel art */

/*
 * Pixel art is a list of equal-length strings, one per row, one character
 * per pixel. A map gives each character's color; characters it doesn't
 * list ('.' by convention) are transparent.
 */

/*
 * Draws pixel art as one <path> per color. opts: x, y (top left), px (the
 * size of a pixel), py (its height, default px). Keep px a whole number of
 * output pixels: see GUIDE.md, "The pixel grid".
 */
export function bitmap(rows, map, opts = {}) {
  const { x = 0, y = 0, px = 1 } = opts;
  const py = opts.py == null ? px : opts.py;
  const d = {};
  rows.forEach((row, j) => {
    let i = 0;
    while (i < row.length) {
      const ch = row[i];
      let k = i + 1;
      while (k < row.length && row[k] === ch) k++;
      if (map[ch]) {
        const w = (k - i) * px;
        (d[ch] = d[ch] || []).push(`M${f2(x + i * px)} ${f2(y + j * py)}h${f2(w)}v${f2(py)}h${f2(-w)}z`);
      }
      i = k;
    }
  });
  return Object.keys(d).map((ch) => `<path d="${d[ch].join('')}" fill="${map[ch]}"/>`).join('');
}

// A grid of color keys to draw pixel art into, then read back as rows.
export class Grid {
  constructor(w, h, key = '.') {
    this.w = w;
    this.h = h;
    this.g = Array.from({ length: h }, () => Array(w).fill(key));
  }
  set(x, y, k) {
    if (x >= 0 && y >= 0 && x < this.w && y < this.h) this.g[y][x] = k;
    return this;
  }
  get(x, y) {
    return x >= 0 && y >= 0 && x < this.w && y < this.h ? this.g[y][x] : '.';
  }
  // Fills the rectangle with corners (x0, y0) and (x1, y1), inclusive.
  fill(x0, y0, x1, y1, k) {
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) this.set(x, y, k);
    return this;
  }
  // Fills a w by h rectangle at (x, y).
  rect(x, y, w, h, k) {
    return this.fill(x, y, x + w - 1, y + h - 1, k);
  }
  // Copies rows in at (x, y), skipping '.'.
  stamp(rows, x, y, recolor = null) {
    rows.forEach((r, j) => [...r].forEach((c, i) => {
      if (c !== '.') this.set(x + i, y + j, recolor ? recolor[c] || c : c);
    }));
    return this;
  }
  rows() {
    return this.g.map((r) => r.join(''));
  }
}

// The bounding box of the set cells, with exclusive ends.
export function bbox(rows) {
  let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
  rows.forEach((r, j) => [...r].forEach((c, i) => {
    if (c !== '.' && c !== ' ') {
      x0 = Math.min(x0, i); x1 = Math.max(x1, i + 1); y0 = Math.min(y0, j); y1 = Math.max(y1, j + 1);
    }
  }));
  return x1 < 0 ? { x0: 0, y0: 0, x1: 0, y1: 0, w: 0, h: 0 } : { x0, y0, x1, y1, w: x1 - x0, h: y1 - y0 };
}

// Every empty cell beside a filled one (not diagonally) becomes `k`.
export function outline(rows, k) {
  const H = rows.length, W = rows[0].length;
  const solid = (i, j) => j >= 0 && j < H && i >= 0 && i < W && rows[j][i] !== '.' && rows[j][i] !== k;
  return rows.map((row, j) => [...row].map((c, i) => {
    if (c !== '.') return c;
    return [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([di, dj]) => solid(i + di, j + dj)) ? k : '.';
  }).join(''));
}

// Pads rows with transparent cells on each side.
export function pad(rows, l, t = l, r = l, b = t) {
  const w = rows[0].length + l + r;
  const blank = '.'.repeat(w);
  return [
    ...Array(t).fill(blank),
    ...rows.map((s) => '.'.repeat(l) + s + '.'.repeat(r)),
    ...Array(b).fill(blank),
  ];
}

// Recolors every set cell to one key.
export const solid = (rows, k) => rows.map((r) => r.replace(/[^.]/g, k));

/*
 * Leans pixel art forward the pixel-art way: each band of `step` rows,
 * counted up from the bottom, moves one pixel further right. Rows get
 * wider by the lean.
 */
export function lean(rows, step = 4) {
  const H = rows.length, extra = Math.floor((H - 1) / step);
  return rows.map((r, j) => {
    const s = Math.floor((H - 1 - j) / step);
    return '.'.repeat(s) + r + '.'.repeat(extra - s);
  });
}

// A seeded random number generator (mulberry32): the same art every run.
export function rng(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// Crops rows to their set cells, leaving `margin` empty cells around them.
export function crop(rows, margin = 0) {
  const b = bbox(rows);
  return pad(rows.slice(b.y0, b.y1).map((r) => r.slice(b.x0, b.x1)), margin);
}
