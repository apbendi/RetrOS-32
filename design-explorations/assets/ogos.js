/*
 * OgOs design explorations: the shared registry and drawing helpers.
 *
 * Every direction file calls OG.add({...}) with a logo and an icon, each a
 * function that returns an SVG string. The helpers here draw pixel art and
 * text in the system's own 8x8 font (lib/font8.c), so a direction can stay
 * small.
 */
(function () {
  'use strict';

  const OG = (window.OG = {});
  OG.directions = [];
  OG.add = (d) => {
    OG.directions.push(d);
    return d;
  };

  /* ---------------------------------------------------------------- colors */

  // The 16 colors of CGA/EGA/VGA text mode, in their usual order.
  OG.VGA = [
    '#000000', '#0000AA', '#00AA00', '#00AAAA', '#AA0000', '#AA00AA', '#AA5500', '#AAAAAA',
    '#555555', '#5555FF', '#55FF55', '#55FFFF', '#FF5555', '#FF55FF', '#FFFF55', '#FFFFFF',
  ];
  OG.C = {
    black: '#000000', blue: '#0000AA', green: '#00AA00', cyan: '#00AAAA',
    red: '#AA0000', magenta: '#AA00AA', brown: '#AA5500', lightgray: '#AAAAAA',
    darkgray: '#555555', lightblue: '#5555FF', lightgreen: '#55FF55', lightcyan: '#55FFFF',
    lightred: '#FF5555', lightmagenta: '#FF55FF', yellow: '#FFFF55', white: '#FFFFFF',
    teal: '#008080', silver: '#C0C0C0', navy: '#000080',
  };
  OG.CGA1 = ['#000000', '#55FFFF', '#FF55FF', '#FFFFFF'];
  OG.CGA0 = ['#000000', '#55FF55', '#FF5555', '#FFFF55'];
  OG.GAMEBOY = ['#0F380F', '#306230', '#8BAC0F', '#9BBC0F'];

  // RetrOS draws in RGB332: 3 bits red, 3 bits green, 2 bits blue.
  OG.rgb332 = (byte) => {
    const r = Math.round((((byte >> 5) & 7) * 255) / 7);
    const g = Math.round((((byte >> 2) & 7) * 255) / 7);
    const b = Math.round(((byte & 3) * 255) / 3);
    return '#' + [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('').toUpperCase();
  };

  /* ------------------------------------------------------------ the 8x8 font */

  // font8x8_basic from lib/font8.c, ASCII 0x20 to 0x7E. Eight bytes a glyph,
  // one per row, and bit 0 is the leftmost pixel.
  const FONT8_HEX = ["0000000000000000","183c3c1818001800","3636000000000000","36367f367f363600","0c3e031e301f0c00","006333180c666300","1c361c6e3b336e00","0606030000000000","180c0606060c1800","060c1818180c0600","00663cff3c660000","000c0c3f0c0c0000","00000000000c0c06","0000003f00000000","00000000000c0c00","6030180c06030100","3e63737b6f673e00","0c0e0c0c0c0c3f00","1e33301c06333f00","1e33301c30331e00","383c36337f307800","3f031f3030331e00","1c06031f33331e00","3f3330180c0c0c00","1e33331e33331e00","1e33333e30180e00","000c0c00000c0c00","000c0c00000c0c06","180c0603060c1800","00003f00003f0000","060c1830180c0600","1e3330180c000c00","3e637b7b7b031e00","0c1e33333f333300","3f66663e66663f00","3c66030303663c00","1f36666666361f00","7f46161e16467f00","7f46161e16060f00","3c66030373667c00","3333333f33333300","1e0c0c0c0c0c1e00","7830303033331e00","6766361e36666700","0f06060646667f00","63777f7f6b636300","63676f7b73636300","1c36636363361c00","3f66663e06060f00","1e3333333b1e3800","3f66663e36666700","1e33070e38331e00","3f2d0c0c0c0c1e00","3333333333333f00","33333333331e0c00","6363636b7f776300","6363361c1c366300","3333331e0c0c1e00","7f6331184c667f00","1e06060606061e00","03060c1830604000","1e18181818181e00","081c366300000000","00000000000000ff","0c0c180000000000","00001e303e336e00","0706063e66663b00","00001e3303331e00","3830303e33336e00","00001e333f031e00","1c36060f06060f00","00006e33333e301f","0706366e66666700","0c000e0c0c0c1e00","300030303033331e","070666361e366700","0e0c0c0c0c0c1e00","0000337f7f6b6300","00001f3333333300","00001e3333331e00","00003b66663e060f","00006e33333e3078","00003b6e66060f00","00003e031e301f00","080c3e0c0c2c1800","0000333333336e00","00003333331e0c00","0000636b7f7f3600","000063361c366300","00003333333e301f","00003f190c263f00","380c0c070c0c3800","1818180018181800","070c0c380c0c0700","6e3b000000000000"];

  const glyphCache = {};
  // Returns a glyph as 8 strings of 8 characters, '#' for set, '.' for clear.
  OG.glyph = (ch) => {
    if (glyphCache[ch]) return glyphCache[ch];
    let code = ch.charCodeAt(0);
    if (code < 0x20 || code > 0x7e) code = 0x3f;
    const hex = FONT8_HEX[code - 0x20];
    const rows = [];
    for (let i = 0; i < 8; i++) {
      const v = parseInt(hex.substr(i * 2, 2), 16);
      let s = '';
      for (let b = 0; b < 8; b++) s += (v >> b) & 1 ? '#' : '.';
      rows.push(s);
    }
    return (glyphCache[ch] = rows);
  };

  // The columns a glyph actually uses, for proportional spacing.
  const glyphSpan = (rows) => {
    let lo = 8, hi = -1;
    rows.forEach((r) => {
      for (let i = 0; i < 8; i++) if (r[i] === '#') { lo = Math.min(lo, i); hi = Math.max(hi, i); }
    });
    return hi < 0 ? [0, 3] : [lo, hi + 1]; // a space is 3 columns wide
  };

  /* ------------------------------------------------------------- pixel art */

  /*
   * Draws a bitmap. `rows` is an array of equal-length strings; `map` gives
   * each character's fill, and characters not in it (like '.' or ' ') are
   * left empty. Runs of the same character on a row become one <rect>.
   *   opts: x, y (top-left), px (pixel width), py (pixel height, default px)
   */
  OG.bitmap = (rows, map, opts = {}) => {
    const { x = 0, y = 0, px = 1 } = opts;
    const py = opts.py == null ? px : opts.py;
    let out = '';
    rows.forEach((row, j) => {
      let i = 0;
      while (i < row.length) {
        const ch = row[i];
        let k = i + 1;
        while (k < row.length && row[k] === ch) k++;
        const fill = map[ch];
        if (fill) {
          out += `<rect x="${+(x + i * px).toFixed(3)}" y="${+(y + j * py).toFixed(3)}" width="${+((k - i) * px).toFixed(3)}" height="${+py.toFixed(3)}" fill="${fill}"/>`;
        }
        i = k;
      }
    });
    return out;
  };

  /*
   * Lays text out in the 8x8 font and returns its rows of '#'/'.', so a
   * design can recolor, outline or distort the pixels itself.
   *   opts: prop (proportional spacing, default false), gap (blank columns
   *   between glyphs: default 0 fixed, 1 proportional)
   */
  OG.textRows = (text, opts = {}) => {
    const prop = !!opts.prop;
    const gap = opts.gap == null ? (prop ? 1 : 0) : opts.gap;
    const rows = ['', '', '', '', '', '', '', ''];
    [...text].forEach((ch, n) => {
      const g = OG.glyph(ch);
      const [lo, hi] = prop ? glyphSpan(g) : [0, 8];
      for (let r = 0; r < 8; r++) {
        rows[r] += (n ? '.'.repeat(gap) : '') + (ch === ' ' && prop ? '...' : g[r].slice(lo, hi));
      }
    });
    return rows;
  };

  /*
   * Draws text in the 8x8 font.
   *   opts: x, y (top-left), px, py, fill, prop, gap,
   *         shadow: {dx, dy, fill} in pixels of the font, drawn behind
   */
  OG.pixText = (text, opts = {}) => {
    const { x = 0, y = 0, px = 4, fill = '#000' } = opts;
    const py = opts.py == null ? px : opts.py;
    const rows = OG.textRows(text, opts);
    let out = '';
    if (opts.shadow) {
      const s = opts.shadow;
      out += OG.bitmap(rows, { '#': s.fill }, { x: x + (s.dx || 1) * px, y: y + (s.dy || 1) * py, px, py });
    }
    return out + OG.bitmap(rows, { '#': fill }, { x, y, px, py });
  };

  // Width of OG.pixText's text in user units, for centering.
  OG.pixTextWidth = (text, opts = {}) => OG.textRows(text, opts)[0].length * (opts.px || 4);

  // A seeded random number generator (mulberry32), so generated art draws
  // the same every time. OG.rng(seed) returns a function giving [0, 1).
  OG.rng = (seed) => {
    let a = seed >>> 0;
    return () => {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  };

  /* ------------------------------------------------------------ SVG helpers */

  OG.esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

  // Wraps markup in an <svg>. opts.crisp turns off antialiasing for pixel art.
  OG.svg = (w, h, inner, opts = {}) =>
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}"${opts.crisp ? ' shape-rendering="crispEdges"' : ''}>${inner}</svg>`;

  // A logo lockup is drawn on a 480x300 canvas (VGA's 320x200 aspect, 16:10).
  OG.LOGO_W = 480;
  OG.LOGO_H = 300;
  OG.logoSvg = (inner, opts) => OG.svg(OG.LOGO_W, OG.LOGO_H, inner, opts);
  // An icon is drawn on a 64x64 canvas.
  OG.iconSvg = (inner, opts) => OG.svg(64, 64, inner, opts);

  /*
   * An SVG <text>. opts: x, y, size, family, weight, fill, anchor
   * ('start' | 'middle' | 'end'), ls (letter-spacing), italic, len
   * (textLength, stretching the text to exactly that width), attrs (extra
   * attribute text).
   */
  OG.text = (str, opts = {}) => {
    const a = [];
    a.push(`x="${opts.x || 0}" y="${opts.y || 0}"`);
    if (opts.size) a.push(`font-size="${opts.size}"`);
    if (opts.family) a.push(`font-family="${OG.esc(opts.family)}"`);
    if (opts.weight) a.push(`font-weight="${opts.weight}"`);
    if (opts.italic) a.push('font-style="italic"');
    a.push(`fill="${opts.fill || '#000'}"`);
    if (opts.anchor) a.push(`text-anchor="${opts.anchor}"`);
    if (opts.ls != null) a.push(`letter-spacing="${opts.ls}"`);
    if (opts.len) a.push(`textLength="${opts.len}" lengthAdjust="spacingAndGlyphs"`);
    if (opts.attrs) a.push(opts.attrs);
    return `<text ${a.join(' ')}>${OG.esc(str)}</text>`;
  };

  /*
   * Inline SVGs share one id namespace, and the dashboard draws each logo
   * several times. This gives every id in a rendered SVG a unique suffix, and
   * rewrites the references to it.
   */
  let scopeCount = 0;
  OG.scope = (svg) => {
    const ids = [];
    svg.replace(/\sid="([^"]+)"/g, (m, id) => ids.push(id));
    if (!ids.length) return svg;
    const sfx = '_s' + ++scopeCount;
    ids.sort((a, b) => b.length - a.length);
    let out = svg;
    ids.forEach((id) => {
      const e = id.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      out = out
        .replace(new RegExp(`(\\sid=")${e}(")`, 'g'), `$1${id}${sfx}$2`)
        .replace(new RegExp(`url\\(#${e}\\)`, 'g'), `url(#${id}${sfx})`)
        .replace(new RegExp(`(href=")#${e}(")`, 'g'), `$1#${id}${sfx}$2`)
        .replace(new RegExp(`(begin="[^"]*?)\\b${e}\\.`, 'g'), `$1${id}${sfx}.`);
    });
    return out;
  };

  // The capitalizations the dashboard can switch every flexible logo to.
  OG.CASES = ['OgOs', 'OgOS', 'OGOS', 'ogos'];

  // The values a direction's tags may take, in the order the filters list them.
  OG.FAMILIES = ['Pixel', 'Geometric', 'Wordmark', 'Emblem', 'Mascot', 'Interface', 'Object', 'Experimental'];
  OG.SOURCES = [
    'Windows 9x', 'Mac OS Classic', 'BeOS', 'IBM & OS/2', 'Unix workstations', 'Home computers',
    'DOS & BIOS', 'Games & arcade', 'Demoscene & early web', 'OgOs internals', 'Print & packaging',
    'Beyond computing',
  ];
  OG.PALETTES = ['VGA 16', 'Rainbow', 'Mono', 'Phosphor', 'Neon', 'Pastel', 'Earthy', 'Primary', 'Corporate', 'Metallic'];
  OG.RISKS = ['Safe', 'Bold', 'Wild'];

  // Problems with a direction's fields, as strings. The preview sheet and the
  // dashboard both show them.
  OG.check = (d) => {
    const p = [];
    ['num', 'id', 'name', 'caps', 'family', 'sources', 'paletteTag', 'risk', 'bg', 'ink', 'palette', 'fonts', 'type', 'concept', 'logo', 'icon']
      .forEach((k) => { if (d[k] == null) p.push(`missing ${k}`); });
    if (d.family && !OG.FAMILIES.includes(d.family)) p.push(`family "${d.family}"`);
    (d.sources || []).forEach((s) => { if (!OG.SOURCES.includes(s)) p.push(`source "${s}"`); });
    if (d.paletteTag && !OG.PALETTES.includes(d.paletteTag)) p.push(`paletteTag "${d.paletteTag}"`);
    if (d.risk && !OG.RISKS.includes(d.risk)) p.push(`risk "${d.risk}"`);
    return p;
  };
})();
