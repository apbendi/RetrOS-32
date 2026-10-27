/* Batch D: Inside OgOs, type & geometry. Directions 6, 11, 16 ... 96. */
(function () {
  'use strict';
  const { C } = OG;

  /* --------------------------------------------------------------- helpers */

  const f2 = (n) => +(+n).toFixed(2);
  const JB = "'JetBrains Mono', monospace";
  const MM = "'Martian Mono', monospace";

  // Draws a bitmap as one <path> per color, instead of a <rect> per run.
  // opts: x, y, px, py, inset (a gap left around every cell, which also
  // stops runs merging), attrs (extra attribute text for every path).
  function pixPath(rows, map, opts = {}) {
    const { x = 0, y = 0, px = 1, inset = 0 } = opts;
    const py = opts.py == null ? px : opts.py;
    const d = {};
    rows.forEach((row, j) => {
      let i = 0;
      while (i < row.length) {
        const ch = row[i];
        let k = i + 1;
        if (!inset) while (k < row.length && row[k] === ch) k++;
        if (map[ch]) {
          const w = (k - i) * px - inset, h = py - inset;
          (d[ch] = d[ch] || []).push(`M${f2(x + i * px + inset / 2)} ${f2(y + j * py + inset / 2)}h${f2(w)}v${f2(h)}h${f2(-w)}z`);
        }
        i = k;
      }
    });
    return Object.keys(d)
      .map((ch) => `<path d="${d[ch].join('')}" fill="${map[ch]}"${opts.attrs ? ' ' + opts.attrs : ''}/>`)
      .join('');
  }

  // Sets a word in a bitmap alphabet. Every glyph in `font` has the same
  // number of rows; a missing letter falls back to its other case.
  function layout(word, font, gap = 1) {
    const gl = [...word].map((c) => font[c] || font[c.toUpperCase()] || font[c.toLowerCase()]);
    const rows = gl[0].map(() => '');
    gl.forEach((g, n) => g.forEach((r, j) => (rows[j] += (n ? '.'.repeat(gap) : '') + r)));
    return rows;
  }

  // The bounding box of a bitmap's set cells: {x0, y0, x1, y1}, exclusive ends.
  function bbox(rows) {
    let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
    rows.forEach((r, j) => [...r].forEach((c, i) => {
      if (c !== '.' && c !== ' ') { x0 = Math.min(x0, i); x1 = Math.max(x1, i + 1); y0 = Math.min(y0, j); y1 = Math.max(y1, j + 1); }
    }));
    return { x0, y0, x1, y1, w: x1 - x0, h: y1 - y0 };
  }

  // Points along an ellipse arc. Angles in degrees: 0 is right, 90 is down.
  function arc(cx, cy, rx, ry, a0, a1, step = 2) {
    const n = Math.max(2, Math.ceil(Math.abs(a1 - a0) / step));
    const pts = [];
    for (let k = 0; k <= n; k++) {
      const a = ((a0 + ((a1 - a0) * k) / n) * Math.PI) / 180;
      pts.push([cx + rx * Math.cos(a), cy + ry * Math.sin(a)]);
    }
    return pts;
  }

  /*
   * Rasterizes strokes into a W x H bitmap of '#' and '.'. A part is
   *   {pts, rx, ry}: an elliptical nib swept along a polyline,
   *   {disc: [x, y, r]}, or {rect: [x, y, w, h]},
   * and a pixel is set when its center falls inside one. A wide, flat nib
   * gives thick verticals and hairline horizontals, as a Didone has.
   */
  function raster(W, H, parts) {
    const g = Array.from({ length: H }, () => Array(W).fill(false));
    const nib = (cx, cy, rx, ry) => {
      for (let j = Math.max(0, Math.floor(cy - ry - 1)); j <= Math.min(H - 1, Math.ceil(cy + ry + 1)); j++) {
        for (let i = Math.max(0, Math.floor(cx - rx - 1)); i <= Math.min(W - 1, Math.ceil(cx + rx + 1)); i++) {
          const dx = (i + 0.5 - cx) / rx, dy = (j + 0.5 - cy) / ry;
          if (dx * dx + dy * dy <= 1.0001) g[j][i] = true;
        }
      }
    };
    parts.forEach((p) => {
      if (p.disc) return nib(p.disc[0], p.disc[1], p.disc[2], p.disc[2]);
      if (p.rect) {
        const [x, y, w, h] = p.rect;
        for (let j = 0; j < H; j++) for (let i = 0; i < W; i++) {
          if (i + 0.5 >= x && i + 0.5 <= x + w && j + 0.5 >= y && j + 0.5 <= y + h) g[j][i] = true;
        }
        return;
      }
      const pts = p.pts;
      for (let k = 0; k < pts.length - 1; k++) {
        const [ax, ay] = pts[k], [bx, by] = pts[k + 1];
        const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 0.1));
        for (let s = 0; s <= n; s++) nib(ax + ((bx - ax) * s) / n, ay + ((by - ay) * s) / n, p.rx, p.ry);
      }
    });
    return g.map((r) => r.map((b) => (b ? '#' : '.')).join(''));
  }

  /* 6. Protection Rings --------------------------------------------------- */

  // Ring 0 to ring 3, inside out.
  const RING = ['#FF5555', '#FFFF55', '#55FF55', '#5555FF'];
  const RING_BG = '#10121B';

  // The four privilege rings around (cx, cy) at scale s: ring 0 a solid core,
  // rings 1 to 3 as arcs around it, broken by the int $48 gate at angle a.
  // The arcs' round ends line up into a channel of width gw.
  function rings(cx, cy, s, a, o = {}) {
    const core = (o.core || 24) * s, t = (o.t || 13) * s, gap = (o.gap == null ? 7 : o.gap) * s;
    const gw = (o.gw || 12) * s;
    let out = '';
    const pt = (r, deg) => [f2(cx + r * Math.cos((deg * Math.PI) / 180)), f2(cy + r * Math.sin((deg * Math.PI) / 180))];
    for (let k = 3; k >= 1; k--) {
      const r = core + gap + (k - 1) * (t + gap) + t / 2;
      const d = (Math.asin(Math.min(1, (gw / 2 + t / 2) / r)) * 180) / Math.PI;
      const [x0, y0] = pt(r, a + d), [x1, y1] = pt(r, a + 360 - d);
      out += `<path d="M${x0} ${y0}A${f2(r)} ${f2(r)} 0 1 1 ${x1} ${y1}" fill="none" stroke="${RING[k]}" stroke-width="${f2(t)}" stroke-linecap="round"/>`;
    }
    out += `<circle cx="${cx}" cy="${cy}" r="${f2(core)}" fill="${RING[0]}"/>`;
    if (o.labels) {
      // Each ring's number on its end, just past the gate.
      for (let k = 1; k <= 3; k++) {
        const r = core + gap + (k - 1) * (t + gap) + t / 2;
        const d = (Math.asin((gw / 2 + t / 2) / r) * 180) / Math.PI;
        const [x, y] = pt(r, a - d);
        out += OG.text(String(k), { x, y: f2(y + 4.2 * s), size: f2(12 * s), family: JB, weight: 800, fill: RING_BG, anchor: 'middle' });
      }
      out += OG.text('0', { x: cx, y: f2(cy + 7.5 * s), size: f2(21 * s), family: JB, weight: 800, fill: RING_BG, anchor: 'middle' });
    }
    return out;
  }

  OG.add({
    num: 6,
    id: 'protection-rings',
    name: 'Protection Rings',
    caps: 'OgOs',
    flexCase: true,
    family: 'Geometric',
    sources: ['OgOs internals'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: RING_BG,
    ink: '#F3F3F7',
    palette: [RING_BG, RING[0], RING[1], RING[2], RING[3]],
    fonts: ['Outfit:wght@600', 'JetBrains Mono:wght@500;800'],
    type: 'Outfit SemiBold, a geometric sans whose round O echoes the rings; ring numbers and the caption in JetBrains Mono.',
    concept:
      "The O is the x86 privilege rings seen from above: ring 0, the kernel, a solid red core, and rings 1 to 3 around it in VGA yellow, green and blue. " +
      'A narrow gate cuts from the outside straight to the core, the one way a program gets into the kernel: the int $48 system call.',
    logo: (w) =>
      OG.logoSvg(
        rings(126, 150, 1, -40, { labels: true }) +
          OG.text(w, { x: 352, y: 170, size: 68, family: "'Outfit', sans-serif", weight: 600, fill: '#F3F3F7', anchor: 'middle' }) +
          OG.text('RING 0 · INT $48', { x: 352, y: 200, size: 12, family: JB, weight: 500, fill: '#7F86A6', anchor: 'middle', ls: 2.5 })
      ),
    icon: () =>
      OG.iconSvg(`<rect width="64" height="64" rx="14" fill="${RING_BG}"/>` + rings(32, 32, 0.3, -40, { core: 24, gw: 22, gap: 6, t: 17 })),
  });

  /* 11. RGB332 ------------------------------------------------------------ */

  // 6 wide and 8 tall: capitals on rows 1-6, small letters on rows 2-6, the
  // g's tail on row 7.
  const F332 = {
    O: ['......', '.####.', '##..##', '##..##', '##..##', '##..##', '.####.', '......'],
    G: ['......', '.#####', '##....', '##.###', '##..##', '##..##', '.####.', '......'],
    S: ['......', '.#####', '##....', '.####.', '....##', '....##', '#####.', '......'],
    o: ['......', '......', '.####.', '##..##', '##..##', '##..##', '.####.', '......'],
    g: ['......', '......', '.#####', '##..##', '##..##', '.#####', '....##', '#####.'],
    s: ['......', '......', '.#####', '##....', '.####.', '....##', '#####.', '......'],
  };
  const P332_BG = '#2A2A31';

  // The 256 colors in byte order, 16 to a row. The cells spelling the word,
  // two letters over two, are whole; the rest are small and dim.
  function tile332(w, x, y, cell) {
    const lit = new Set();
    const L = [...w];
    [[1, 0], [9, 0], [1, 8], [9, 8]].forEach(([c0, r0], n) => {
      const g = F332[L[n]] || F332[L[n].toUpperCase()];
      g.forEach((row, j) => [...row].forEach((v, i) => { if (v === '#') lit.add((r0 + j) * 16 + c0 + i); }));
    });
    let out = '';
    for (let b = 0; b < 256; b++) {
      const i = b % 16, j = b >> 4, col = OG.rgb332(b);
      if (lit.has(b)) out += `<rect x="${f2(x + i * cell + 0.25)}" y="${f2(y + j * cell + 0.25)}" width="${f2(cell - 0.5)}" height="${f2(cell - 0.5)}" fill="${col}"/>`;
      else {
        const s = cell * 0.24;
        out += `<rect x="${f2(x + i * cell + (cell - s) / 2)}" y="${f2(y + j * cell + (cell - s) / 2)}" width="${f2(s)}" height="${f2(s)}" fill="${col}" opacity="0.42"/>`;
      }
    }
    return out;
  }

  OG.add({
    num: 11,
    id: 'rgb332',
    name: 'RGB332',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['OgOs internals'],
    paletteTag: 'Rainbow',
    risk: 'Bold',
    scene: false,
    bg: P332_BG,
    ink: '#F2F2F2',
    palette: [P332_BG, OG.rgb332(0xe0), OG.rgb332(0x1c), OG.rgb332(0x03), OG.rgb332(0xfc)],
    fonts: ['Martian Mono:wght@400;700'],
    type: 'The tile letters are 6x6 cells, two strokes thick, drawn for the grid; the name beside it in Martian Mono Bold.',
    concept:
      'Every color OgOs can draw, the whole RGB332 palette, laid out as a 16x16 grid in byte order: red rising down the rows, green and blue cycling across. ' +
      'The cells that spell the name are shown whole and the rest shrink to dim dots, so the logo is literally made of the system palette.',
    logo: (w) => {
      const bits = ['R', 'R', 'R', 'G', 'G', 'G', 'B', 'B'];
      const bc = { R: '#FF4A4A', G: '#3BD45B', B: '#4A6BFF' };
      let b = '';
      bits.forEach((k, i) => {
        const x = 293 + i * 18;
        b += `<rect x="${x}" y="150" width="16" height="16" fill="${bc[k]}"/>` + OG.text(k, { x: x + 8, y: 162, size: 10, family: MM, weight: 700, fill: P332_BG, anchor: 'middle' });
      });
      return OG.logoSvg(
        tile332(w, 28, 30, 15) +
          OG.text(w, { x: 364, y: 130, size: 44, family: MM, weight: 700, fill: '#F2F2F2', anchor: 'middle' }) +
          b +
          OG.text('256 COLORS', { x: 364, y: 192, size: 10, family: MM, weight: 400, fill: '#8D8D99', anchor: 'middle', ls: 3 }),
        { crisp: true }
      );
    },
    icon: () => {
      // A 4x4 excerpt of the palette: an O of twelve whole cells.
      const ring = ['####', '#..#', '#..#', '####'];
      let out = `<rect width="64" height="64" rx="12" fill="${P332_BG}"/>`;
      ring.forEach((row, j) => [...row].forEach((v, i) => {
        const col = OG.rgb332((10 + j) * 16 + 4 + i * 3);
        if (v === '#') out += `<rect x="${6 + i * 13}" y="${6 + j * 13}" width="12" height="12" fill="${col}"/>`;
        else out += `<rect x="${10 + i * 13}" y="${10 + j * 13}" width="4" height="4" fill="${col}" opacity="0.6"/>`;
      }));
      return OG.iconSvg(out, { crisp: true });
    },
  });

  /* 16. Hex Dump ---------------------------------------------------------- */

  const HEX_BG = '#F2EEE4', HEX_INK = '#17171C', HEX_ACC = '#E0461A', HEX_DIM = '#C3BBAA';

  OG.add({
    num: 16,
    id: 'hex-dump',
    name: 'Hex Dump',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['OgOs internals'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: HEX_BG,
    ink: HEX_INK,
    palette: [HEX_BG, HEX_INK, HEX_ACC, HEX_DIM],
    fonts: ['JetBrains Mono:wght@500;700;800'],
    type: 'JetBrains Mono ExtraBold for the ASCII column, Bold for the bytes; each byte is computed from the letter under it.',
    concept:
      'One row of a hex dump at 00010000, the address the kernel loads at, blown up so the ASCII column is the wordmark. ' +
      'Each letter carries its own byte above it, so the logo shows what the name is to the machine as well as to people.',
    logo: (w) => {
      const chars = ['|', ...w, '|'];
      const size = 104, cw = size * 0.6;
      const x0 = 240 - (chars.length * cw) / 2;
      // The offset sits over the left bar, the bytes over their letters, and
      // a hex editor's cursor marks the first byte in both columns.
      let out = OG.text('00010000', { x: f2(x0 + cw - 6), y: 104, size: 12.5, family: JB, weight: 500, fill: '#8F8778', anchor: 'end' });
      const c1 = x0 + cw * 1.5;
      out += `<rect x="${f2(c1 - 23)}" y="80" width="46" height="32" fill="${HEX_ACC}"/>`;
      out += `<rect x="${f2(c1 - cw / 2 + 4)}" y="230" width="${f2(cw - 8)}" height="7" fill="${HEX_ACC}"/>`;
      chars.forEach((c, i) => {
        const cx = f2(x0 + cw * (i + 0.5));
        const pipe = i === 0 || i === chars.length - 1;
        if (!pipe) {
          const hex = c.charCodeAt(0).toString(16).toUpperCase().padStart(2, '0');
          out += OG.text(hex, { x: cx, y: 105, size: 26, family: JB, weight: 700, fill: i === 1 ? HEX_BG : HEX_ACC, anchor: 'middle' });
        }
        out += OG.text(c, { x: cx, y: 214, size, family: JB, weight: 800, fill: pipe ? HEX_DIM : HEX_INK, anchor: 'middle' });
      });
      return OG.logoSvg(out);
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="12" fill="${HEX_INK}"/>` +
          `<rect x="9" y="12" width="5" height="40" fill="${HEX_ACC}"/><rect x="50" y="12" width="5" height="40" fill="${HEX_ACC}"/>` +
          `<rect x="20.5" y="15.5" width="23" height="33" rx="9" fill="none" stroke="${HEX_BG}" stroke-width="7"/>`
      ),
  });

  /* 21. Boot Sector ------------------------------------------------------- */

  // The first 512 bytes of RetrOS-32-debug.img: boot/bootloader.s around
  // the FAT16 fields, with the volume label and banner renamed.
  const BOOT_HEX =
    'eb3e006d6b66732e66617400020444030200020000f84000200004000000000000000100800029787f59b9524554524f53333220202046415431362020200000fa8cc88ed88ec08ee08ee88ed0bc00308816267de82300bebb7ce84400b800008ec0bb007eb402b003b500b102b6008a16267dcd137218e99201b8000631c9ba' +
    '4f18b71fcd1030ff31d2b402cd10c3bee57ce80c00be027de80600e80000f4ebfd30ffb40eac3c0074103c0a7508b00dcd10b40eb00acd10ebe7c3526574724f532d3332202d20537461676520310a3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d3d0a00203e204572726f722072656164696e672066726f6d206469736b2e' +
    '0a000a556e61626c6520746f206c6f6164207374616765203220626f6f746c6f616465720a00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000' +
    '0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000800000000e000000000000000000010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000055aa';
  const BOOT = (() => {
    const b = [];
    for (let i = 0; i < 1024; i += 2) b.push(parseInt(BOOT_HEX.substr(i, 2), 16) || 0);
    const put = (o, s) => [...s].forEach((c, i) => (b[o + i] = c.charCodeAt(0)));
    put(0x2b, 'OGOS32     ');
    put(0xbb, '  OgOs-32');
    return b;
  })();

  // 6 wide, 13 tall: capitals on rows 0-9, small letters on 3-9, tails to 12.
  const E3 = ['......', '......', '......'];
  const FBOOT = {
    O: ['.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E3],
    G: ['.####.', '##..##', '##....', '##....', '##.###', '##..##', '##..##', '##..##', '##..##', '.####.', ...E3],
    S: ['.####.', '##..##', '##....', '##....', '.####.', '....##', '....##', '....##', '##..##', '.####.', ...E3],
    o: [...E3, '.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E3],
    g: [...E3, '.#####', '##..##', '##..##', '##..##', '##..##', '##..##', '.#####', '....##', '....##', '.####.'],
    s: [...E3, '.####.', '##..##', '##....', '.####.', '....##', '##..##', '.####.', ...E3],
  };
  const BS_BG = '#0D0E12', BS_CELL = '#1B1D24', BS_ON = '#EEEBE1', BS_ACC = '#FF6A13';

  // The sector as a 32x16 grid: each cell one byte, with its value in it.
  function bootGrid(lit, x0, y0, cs, o = {}) {
    const groups = { dark: [], on: [], acc: [] };
    for (let b = 0; b < 512; b++) groups[b >= 510 ? 'acc' : lit.has(b) ? 'on' : 'dark'].push(b);
    const g = o.gap == null ? 1.2 : o.gap;
    const cells = (list, fill, extra = '') =>
      `<path d="${list.map((b) => `M${f2(x0 + (b % 32) * cs + g / 2)} ${f2(y0 + (b >> 5) * cs + g / 2)}h${f2(cs - g)}v${f2(cs - g)}h${f2(g - cs)}z`).join('')}" fill="${fill}"${extra}/>`;
    // The bytes, one <text> per row and layer, each digit placed on its own.
    const bytes = (list, fill, size, weight) => {
      const rows = {};
      list.forEach((b) => (rows[b >> 5] = rows[b >> 5] || []).push(b));
      return Object.keys(rows).map((j) => {
        const xs = [], s = [];
        rows[j].forEach((b) => {
          const cx = x0 + (b % 32) * cs + cs / 2, h = BOOT[b].toString(16).toUpperCase().padStart(2, '0');
          xs.push(f2(cx - size * 0.6), f2(cx));
          s.push(h);
        });
        return `<text x="${xs.join(' ')}" y="${f2(y0 + j * cs + cs / 2 + size * 0.37)}" font-size="${size}" font-family="${JB}" font-weight="${weight}" fill="${fill}">${s.join('')}</text>`;
      }).join('');
    };
    let out = cells(groups.dark, BS_CELL);
    if (o.glow) out += `<filter id="bglow" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="4"/></filter>` + cells(groups.on, '#FFD9A8', ' opacity="0.35" filter="url(#bglow)"');
    out += cells(groups.on, BS_ON) + cells(groups.acc, BS_ACC);
    if (o.text) {
      out += bytes(groups.dark, '#454A57', o.text, 500) + bytes(groups.on, '#CDC7B8', o.text, 600) + bytes(groups.acc, BS_BG, o.text + 1.5, 800);
    }
    return out;
  }

  OG.add({
    num: 21,
    id: 'boot-sector',
    name: 'Boot Sector',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['OgOs internals'],
    paletteTag: 'Mono',
    risk: 'Wild',
    scene: false,
    bg: BS_BG,
    ink: BS_ON,
    palette: [BS_BG, BS_ON, BS_ACC, BS_CELL],
    fonts: ['JetBrains Mono:wght@500;600;800'],
    type: 'Letters 6 cells wide and 10 tall, two cells per stroke, lit in a 32x16 byte grid; every cell carries its byte in JetBrains Mono.',
    concept:
      'The 512 bytes of the boot sector, one cell each, holding the real bytes of the OgOs bootloader. ' +
      'The name is lit across them, and the last two cells are the 55 AA signature that tells the BIOS this disk will boot.',
    logo: (w) => {
      const rows = layout(w, FBOOT, 2);
      const lit = new Set();
      rows.forEach((r, j) => [...r].forEach((v, i) => { if (v === '#') lit.add((j + 3) * 32 + i + 1); }));
      return OG.logoSvg(
        bootGrid(lit, 24, 26, 13.5, { text: 6.6, glow: true }) +
          OG.text('SECTOR 0 · 512 BYTES · SIGNATURE 55 AA', { x: 240, y: 270, size: 11, family: JB, weight: 500, fill: '#6C7182', anchor: 'middle', ls: 2.2 })
      );
    },
    icon: () => {
      const O = ['........', '..####..', '.##..##.', '.##..##.', '.##..##.', '.##..##.', '..####..', '........'];
      let out = `<rect width="64" height="64" rx="10" fill="${BS_BG}"/>`;
      const at = { dark: '', on: '', acc: '' };
      O.forEach((row, j) => [...row].forEach((v, i) => {
        const k = j === 7 && i >= 6 ? 'acc' : v === '#' ? 'on' : 'dark';
        at[k] += `M${4 + i * 7} ${4 + j * 7}h6v6h-6z`;
      }));
      out += `<path d="${at.dark}" fill="${BS_CELL}"/><path d="${at.on}" fill="${BS_ON}"/><path d="${at.acc}" fill="${BS_ACC}"/>`;
      return OG.iconSvg(out, { crisp: true });
    },
  });

  /* 26. Sine Table -------------------------------------------------------- */

  const SIN_BG = '#EFEAE0', SIN_INK = '#1D1C1A', SIN_RED = '#E5352B';

  // 60 dots at 6 degree steps, the quarter points bigger, and a red dot that
  // ticks round once a minute when `tick` is set.
  function dial(cx, cy, R, o) {
    let out = '';
    for (let i = 0; i < 60; i++) {
      const a = ((-90 + 6 * i) * Math.PI) / 180;
      out += `<circle cx="${f2(cx + R * Math.cos(a))}" cy="${f2(cy + R * Math.sin(a))}" r="${i % 15 ? o.small : o.big}" fill="${SIN_INK}"/>`;
    }
    const a = ((-90 + 6 * o.at) * Math.PI) / 180;
    let hand = `<circle cx="${f2(cx + R * Math.cos(a))}" cy="${f2(cy + R * Math.sin(a))}" r="${o.hand}" fill="${SIN_RED}"/>`;
    if (o.tick) {
      const vals = Array.from({ length: 60 }, (_, k) => `${6 * k} ${cx} ${cy}`).join(';');
      hand = `<g>${hand}<animateTransform attributeName="transform" type="rotate" calcMode="discrete" dur="60s" repeatCount="indefinite" values="${vals}"/></g>`;
    }
    return out + hand;
  }

  OG.add({
    num: 26,
    id: 'sine-table',
    name: 'Sine Table',
    caps: 'OgOs',
    flexCase: true,
    family: 'Geometric',
    sources: ['OgOs internals'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: SIN_BG,
    ink: SIN_INK,
    palette: [SIN_BG, SIN_INK, SIN_RED],
    fonts: ['Outfit:wght@200;400'],
    type: 'Outfit ExtraLight, as thin as the dots are small, with a tracked caption in Outfit Regular.',
    concept:
      "OgOs keeps its sines in a table of 60 entries, 6 degrees apart, and this O is that table: 60 dots, the four quarter points bigger. " +
      'One red dot ticks round like a second hand, once a minute, with the name set in the middle of the dial.',
    logo: (w) =>
      OG.logoSvg(
        dial(240, 150, 120, { small: 3.3, big: 7, hand: 6.2, at: 8, tick: true }) +
          OG.text(w, { x: 240, y: 168, size: 60, family: "'Outfit', sans-serif", weight: 200, fill: SIN_INK, anchor: 'middle', ls: 1 }) +
          OG.text('SIN 6° × 60', { x: 240, y: 200, size: 10, family: "'Outfit', sans-serif", weight: 400, fill: '#8C867B', anchor: 'middle', ls: 3 })
      ),
    icon: () => OG.iconSvg(`<rect width="64" height="64" rx="32" fill="${SIN_BG}"/>` + dial(32, 32, 24, { small: 1.9, big: 3.6, hand: 4.2, at: 8 })),
  });

  /* 31. Defrag ------------------------------------------------------------ */

  // 7 wide, 14 tall: capitals on rows 0-10, small letters on 3-10, tails to 13.
  const E3W = ['.......', '.......', '.......'];
  const FDEF = {
    O: ['.#####.', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '.#####.', ...E3W],
    G: ['.#####.', '##...##', '##.....', '##.....', '##.....', '##..###', '##...##', '##...##', '##...##', '##...##', '.#####.', ...E3W],
    S: ['.#####.', '##...##', '##.....', '##.....', '.####..', '..####.', '.....##', '.....##', '.....##', '##...##', '.#####.', ...E3W],
    o: [...E3W, '.#####.', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '.#####.', ...E3W],
    g: [...E3W, '.######', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '.######', '.....##', '.....##', '.#####.'],
    s: [...E3W, '.#####.', '##...##', '##.....', '.#####.', '.....##', '.....##', '##...##', '.#####.', ...E3W],
  };
  const DF = { used: '#0000A8', free: '#FFFFFF', unmov: '#B00000', moving: '#00B4C8', line: '#B9B9B9', silver: '#C0C0C0' };

  // The cluster map: letter cells are used, a halo round them stays free,
  // and the rest is scattered fragments. The last letter's tail is still moving.
  function defragMap(w, cols, rows, seed) {
    const glyphs = layout(w, FDEF, 2);
    const bb = { w: glyphs[0].length };
    const ox = Math.floor((cols - bb.w) / 2), oy = 3;
    const grid = Array.from({ length: rows }, () => Array(cols).fill('f'));
    glyphs.forEach((r, j) => [...r].forEach((v, i) => { if (v === '#' && grid[j + oy]) grid[j + oy][i + ox] = 'u'; }));
    // The last few cells of the last letter, in reading order, are moving.
    const lastX = ox + glyphs[0].length - 7;
    const tail = [];
    for (let j = rows - 1; j >= 0 && tail.length < 7; j--) for (let i = cols - 1; i >= lastX && tail.length < 7; i--) if (grid[j][i] === 'u') tail.push([j, i]);
    tail.forEach(([j, i]) => (grid[j][i] = 'm'));
    const near = (j, i) => {
      for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
        const r = grid[j + dj];
        if (r && (r[i + di] === 'u' || r[i + di] === 'm')) return true;
      }
      return false;
    };
    const rnd = OG.rng(seed);
    const out = grid.map((r) => r.slice());
    for (let j = 0; j < rows; j++) for (let i = 0; i < cols; i++) {
      if (grid[j][i] !== 'f' || near(j, i)) continue;
      const v = rnd();
      // Fragments cluster toward the edges of the disk, as they do.
      const edge = Math.min(i, cols - 1 - i) < 4 || j < 2 || j > rows - 2 ? 2.2 : 1;
      if (v < 0.05 * edge) out[j][i] = 'u';
      else if (v < 0.05 * edge + 0.012) out[j][i] = 'r';
    }
    // Two cells on their way to the tail.
    out[rows - 2][ox + bb.w + 1] = 'm';
    out[rows - 3][ox + bb.w + 2 < cols ? ox + bb.w + 2 : ox + bb.w] = 'm';
    return out.map((r) => r.join(''));
  }

  OG.add({
    num: 31,
    id: 'defrag',
    name: 'Defrag',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['OgOs internals', 'Windows 9x'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: DF.silver,
    ink: '#000000',
    palette: [DF.used, DF.free, DF.unmov, DF.moving, DF.silver],
    fonts: [],
    type: 'Letters 7 clusters wide and 11 tall, two clusters per stroke, set in the map itself; the legend in the kernel font.',
    concept:
      "A disk defragmenter's cluster map, the slowest show of the 90s, caught at the moment the used clusters have gathered into the name. " +
      'Stray fragments and a few unmovable red clusters lie around it, and the last letter is still being moved into place. It is also, quietly, about a FAT16 disk.',
    logo: (w) => {
      const cols = 40, rows = 17, cw = 10.5, ch = 11.5;
      const x = (480 - cols * cw) / 2, y = 28;
      const map = defragMap(w, cols, rows, 31);
      const H = rows * ch, W = cols * cw;
      let out =
        // A sunken field, as Windows drew one.
        `<rect x="${x - 3}" y="${y - 3}" width="${W + 6}" height="${H + 6}" fill="#FFFFFF"/>` +
        `<path d="M${x - 3} ${y + H + 3}V${y - 3}H${x + W + 3}" fill="none" stroke="#808080" stroke-width="2"/>` +
        `<path d="M${x - 1} ${y + H + 1}V${y - 1}H${x + W + 1}" fill="none" stroke="#000" stroke-width="1.5"/>` +
        `<rect x="${x}" y="${y}" width="${W}" height="${H}" fill="${DF.line}"/>` +
        pixPath(map, { f: DF.free, u: DF.used, r: DF.unmov, m: DF.moving }, { x, y, px: cw, py: ch, inset: 1.5 });
      // Legend.
      const items = [['u', 'Used'], ['f', 'Free'], ['r', 'Unmovable'], ['m', 'Moving']];
      const fills = { u: DF.used, f: DF.free, r: DF.unmov, m: DF.moving };
      let lx = 52;
      items.forEach(([k, label]) => {
        out += `<rect x="${lx}" y="${y + H + 22}" width="12" height="12" fill="${fills[k]}" stroke="#000" stroke-width="1"/>`;
        out += OG.pixText(label, { x: lx + 19, y: y + H + 24, px: 1.25, fill: '#000', prop: true });
        lx += 19 + OG.pixTextWidth(label, { px: 1.25, prop: true }) + 26;
      });
      return OG.logoSvg(out, { crisp: true });
    },
    icon: () => {
      // An O gathered from used clusters, one fragment still moving.
      const map = ['fuuuuf', 'uuffuu', 'uuffuu', 'uuffuu', 'fuuuuf', 'rffffm'];
      return OG.iconSvg(
        `<rect width="64" height="64" fill="${DF.line}"/>` + pixPath(map, { f: DF.free, u: DF.used, r: DF.unmov, m: DF.moving }, { x: 2, y: 2, px: 10, inset: 1.5 }),
        { crisp: true }
      );
    },
  });

  /* 36. Triple Fault ------------------------------------------------------ */

  const TF_FONT = "'Sora', sans-serif";

  OG.add({
    num: 36,
    id: 'triple-fault',
    name: 'Triple Fault',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['OgOs internals'],
    paletteTag: 'Neon',
    risk: 'Wild',
    scene: false,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#FF0000', '#00FF00', '#0000FF', '#FFFFFF'],
    fonts: ['Sora:wght@800', 'JetBrains Mono:wght@500'],
    type: 'Sora ExtraBold, whose open counters survive the damage, set three times in pure red, green and blue and added together so the overlap comes back white.',
    concept:
      'A triple fault is the crash that skips the error screen and resets the machine; RetrOS had one when an interrupt came too early at boot (#41). ' +
      'Here the name splits into its three color channels and tears into slices, the frame the CPU never got to draw. One slice jumps now and then.',
    logo: (w) => {
      const size = 120, base = 192;
      const t = (fill, dx, dy) =>
        OG.text(w, { x: 240 + dx, y: base + dy, size, family: TF_FONT, weight: 800, fill, anchor: 'middle', attrs: 'style="mix-blend-mode:screen"' });
      const word = `<g id="tfw">${t('#FF0000', -4, -1.5)}${t('#00FF00', 0, 0)}${t('#0000FF', 4, 1.5)}</g>`;
      // Slices: [top, bottom, shift].
      const bands = [[0, 128, 0], [128, 133, -24], [133, 154, 0], [154, 170, 12], [170, 300, 0]];
      let defs = `<defs>${word}`;
      let body = '';
      bands.forEach(([y0, y1, dx], k) => {
        defs += `<clipPath id="tfc${k}"><rect x="0" y="${y0}" width="480" height="${y1 - y0}"/></clipPath>`;
        const anim = k === 3 ? `<animate attributeName="x" values="12;12;-20;12;12;32;12" keyTimes="0;0.55;0.58;0.62;0.86;0.88;1" calcMode="discrete" dur="4s" repeatCount="indefinite"/>` : '';
        body += `<g clip-path="url(#tfc${k})"><use href="#tfw" x="${dx}">${anim}</use></g>`;
      });
      defs += '</defs>';
      const noise =
        '<rect x="420" y="160" width="34" height="3" fill="#00FFFF"/>' +
        '<rect x="24" y="129" width="22" height="4" fill="#FF00FF"/><rect x="46" y="129" width="8" height="4" fill="#FFFFFF"/>' +
        '<rect x="300" y="208" width="16" height="5" fill="#FF0000"/><rect x="316" y="208" width="8" height="5" fill="#0000FF"/>';
      // Scanlines over everything.
      const scan = '<pattern id="tfs" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="4" height="1.4" fill="#000"/></pattern>';
      return OG.logoSvg(
        defs + body + noise + `<defs>${scan}</defs><rect x="0" y="80" width="480" height="140" fill="url(#tfs)" opacity="0.3"/>` +
          OG.text('#41', { x: 456, y: 278, size: 13, family: JB, weight: 500, fill: '#FF3B3B', anchor: 'end', ls: 1 }) +
          OG.text('TRIPLE FAULT · CPU RESET', { x: 24, y: 278, size: 10, family: JB, weight: 500, fill: '#5A5A66', ls: 2 })
      );
    },
    icon: () => {
      // The O as a rounded slab ring, a red fringe left, a blue one right.
      const ring = (dx, fill) => `<path d="M${16 + dx} 8h${32}a10 10 0 0 1 10 10v28a10 10 0 0 1 -10 10h-32a10 10 0 0 1 -10 -10v-28a10 10 0 0 1 10 -10zM${22 + dx} 20v24h20v-24z" fill="${fill}" fill-rule="evenodd"/>`;
      const o = ring(-3, '#FF1E1E') + ring(3, '#2E5BFF') + ring(0, '#FFFFFF');
      return OG.iconSvg(
        `<rect width="64" height="64" fill="#000"/>` +
          `<defs><clipPath id="ia"><rect width="64" height="28"/></clipPath><clipPath id="ib"><rect y="28" width="64" height="8"/></clipPath><clipPath id="ic"><rect y="36" width="64" height="28"/></clipPath></defs>` +
          `<g clip-path="url(#ia)">${o}</g><g clip-path="url(#ib)" transform="translate(7 0)">${o}</g><g clip-path="url(#ic)">${o}</g>`
      );
    },
  });

  /* 41. Curly ------------------------------------------------------------- */

  const CU_BG = '#F6F3EA', CU_INK = '#1D2230', CU_ACC = '#FF4D2E', CU_CARET = '#2C6BFF';
  // An opening brace, 6 wide and 15 tall; the closing one is its mirror.
  const BRACE = ['...###', '..####', '..##..', '..##..', '..##..', '.###..', '###...', '.###..', '..##..', '..##..', '..##..', '..####', '...###'];
  const mirror = (rows) => rows.map((r) => [...r].reverse().join(''));
  const blink = '<animate attributeName="opacity" values="1;1;0.12;0.12" keyTimes="0;0.5;0.5;1" dur="1.06s" repeatCount="indefinite"/>';

  OG.add({
    num: 41,
    id: 'curly',
    name: 'Curly',
    caps: 'ogos',
    flexCase: true,
    family: 'Wordmark',
    sources: ['OgOs internals'],
    paletteTag: 'Primary',
    risk: 'Bold',
    scene: false,
    bg: CU_BG,
    ink: CU_INK,
    palette: [CU_BG, CU_INK, CU_ACC, CU_CARET],
    fonts: ['Martian Mono:wght@500'],
    type: 'Martian Mono Medium for the word; the braces are drawn on a pixel grid, 6 by 15, two pixels a stroke.',
    concept:
      'The name typed into an empty block of C, with the text cursor still blinking after it. ' +
      'OgOs is for writing code as much as running it, down to a C compiler on the system itself, and every program starts as a pair of braces.',
    logo: (w) => {
      const px = 10, size = 78, adv = 0.7 * size, ww = 4 * adv;
      const total = 6 * px + 16 + ww + 5 + 8 + 16 + 6 * px;
      const x0 = f2((480 - total) / 2), y0 = 85;
      const wx = x0 + 6 * px + 16;
      const cx = wx + ww + 5;
      return OG.logoSvg(
        pixPath(BRACE, { '#': CU_ACC }, { x: x0, y: y0, px }) +
          OG.text(w, { x: f2(wx), y: 176, size, family: MM, weight: 500, fill: CU_INK, len: f2(ww) }) +
          `<rect x="${f2(cx)}" y="112" width="8" height="84" fill="${CU_CARET}">${blink}</rect>` +
          pixPath(mirror(BRACE), { '#': CU_ACC }, { x: f2(cx + 8 + 16), y: y0, px })
      );
    },
    icon: () => {
      const b = ['...##', '..##.', '..##.', '..##.', '.##..', '##...', '.##..', '..##.', '..##.', '..##.', '...##'];
      return OG.iconSvg(
        `<rect width="64" height="64" rx="12" fill="${CU_INK}"/>` +
          pixPath(b, { '#': CU_ACC }, { x: 4, y: 10, px: 4 }) +
          pixPath(mirror(b), { '#': CU_ACC }, { x: 40, y: 10, px: 4 }) +
          `<rect x="29" y="14" width="6" height="36" fill="${CU_BG}"/>`,
        { crisp: true }
      );
    },
  });

  /* 46. Legacy 32 --------------------------------------------------------- */

  const LG = { bg: '#33404C', line: '#1B1E22', cream: '#ECE8D8', screen: '#262A30', led: '#F2614B', glyph: '#E4E4E0', base: '#9DA69B', baseDark: '#7B8479' };
  // The screen's letters, 8 wide and 14 tall, as chunky as the old "32":
  // capitals on rows 0-10, small letters on 3-10, the g's tail to 13.
  const E3L = ['........', '........', '........'];
  const FLEG_BIG = {
    O: ['.######.', '########', '##....##', '##....##', '##....##', '##....##', '##....##', '##....##', '##....##', '########', '.######.', ...E3L],
    G: ['.######.', '########', '##....##', '##......', '##......', '##..####', '##..####', '##....##', '##....##', '########', '.######.', ...E3L],
    o: [...E3L, '.######.', '########', '##....##', '##....##', '##....##', '##....##', '########', '.######.', ...E3L],
    g: [...E3L, '.#######', '########', '##....##', '##....##', '##....##', '##....##', '########', '.#######', '......##', '########', '#######.'],
  };
  // The name's letters, 6 wide and 11 tall.
  const E2 = ['......', '......'];
  const FLEG = {
    O: ['.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E2],
    G: ['.####.', '##..##', '##....', '##....', '##.###', '##..##', '##..##', '##..##', '.####.', ...E2],
    S: ['.####.', '##..##', '##....', '##....', '.####.', '....##', '....##', '##..##', '.####.', ...E2],
    o: [...E2, '.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E2],
    g: [...E2, '.#####', '##..##', '##..##', '##..##', '##..##', '##..##', '.#####', '....##', '.####.'],
    s: [...E2, '.#####', '##....', '##....', '.####.', '....##', '....##', '#####.', ...E2],
  };

  // The framed screen. (x, y) is the top left, s the scale.
  function legacy(w, x, y, s) {
    const W = 236 * s, H = 264 * s, o = 6 * s;
    const sx = x + 18 * s, sy = y + 18 * s, sw = W - 36 * s, sh = 146 * s;
    let out =
      `<rect x="${f2(x)}" y="${f2(y)}" width="${f2(W)}" height="${f2(H)}" fill="${LG.line}"/>` +
      `<rect x="${f2(x + o)}" y="${f2(y + o)}" width="${f2(W - 2 * o)}" height="${f2(H - 2 * o)}" fill="${LG.cream}"/>` +
      `<rect x="${f2(x + o)}" y="${f2(y + H - o - 22 * s)}" width="${f2(W - 2 * o)}" height="${f2(22 * s)}" fill="${LG.base}"/>` +
      `<rect x="${f2(x + o)}" y="${f2(y + H - o - 22 * s)}" width="${f2(W - 2 * o)}" height="${f2(4 * s)}" fill="${LG.baseDark}"/>` +
      `<rect x="${f2(sx)}" y="${f2(sy)}" width="${f2(sw)}" height="${f2(sh)}" fill="${LG.screen}"/>` +
      `<rect x="${f2(sx)}" y="${f2(sy)}" width="${f2(30 * s)}" height="${f2(15 * s)}" fill="${LG.led}"/>`;
    // The screen shows the first two letters, big.
    const big = layout(w.slice(0, 2), FLEG_BIG, 1);
    const bb = bbox(big), bp = 8 * s;
    out += pixPath(big, { '#': LG.glyph }, { x: f2(sx + (sw - bb.w * bp) / 2 - bb.x0 * bp), y: f2(sy + (sh - bb.h * bp) / 2 - bb.y0 * bp + 4 * s), px: bp });
    if (w.length > 2) {
      const name = layout(w, FLEG, 1);
      const nb = bbox(name), np = 5 * s;
      const top = sy + sh, bottom = y + H - o - 22 * s;
      // Centered on the capitals' height, so a tail hangs below.
      out += pixPath(name, { '#': LG.line }, { x: f2(x + (W - nb.w * np) / 2 - nb.x0 * np), y: f2((top + bottom) / 2 - 4.5 * np), px: np });
    }
    return out;
  }

  OG.add({
    num: 46,
    id: 'legacy-32',
    name: 'Legacy 32',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['OgOs internals'],
    paletteTag: 'Earthy',
    risk: 'Safe',
    scene: false,
    bg: LG.bg,
    ink: LG.cream,
    palette: [LG.cream, LG.screen, LG.led, LG.base, LG.bg],
    fonts: [],
    type: 'Two bitmap faces in the manner of the current logo: chunky 8x11 letters on the screen, and a 6x9 face, two pixels a stroke, for the name.',
    concept:
      "The RetrOS logo, renamed rather than redrawn: the same cream frame, dark screen, red LED and gray-green foot, with the screen's big \"32\" now reading \"Og\". " +
      'Anyone who knew the old mark recognizes the new one at a glance.',
    logo: (w) => OG.logoSvg(legacy(w, 122, 18, 1), { crisp: true }),
    icon: () => {
      // The frame and screen alone, with a big O.
      let out =
        `<rect x="4" y="2" width="56" height="60" fill="${LG.line}"/><rect x="7" y="5" width="50" height="54" fill="${LG.cream}"/>` +
        `<rect x="7" y="51" width="50" height="8" fill="${LG.base}"/><rect x="7" y="51" width="50" height="2" fill="${LG.baseDark}"/>` +
        `<rect x="11" y="9" width="42" height="38" fill="${LG.screen}"/><rect x="11" y="9" width="10" height="6" fill="${LG.led}"/>`;
      out += pixPath(FLEG_BIG.O.slice(0, 11), { '#': LG.glyph }, { x: 22, y: 14.5, px: 2.5 });
      return OG.iconSvg(out, { crisp: true });
    },
  });

  /* 51. Monogram ---------------------------------------------------------- */

  const MO_BG = '#123A2F', MO_INK = '#F1EADA';

  // A G inside an O: two concentric rings, the inner one open above its
  // crossbar, and the crossbar running out to join the outer ring.
  function monogram(cx, cy, R, fill) {
    const t = R * 0.22, r2 = R * 0.52;
    const ro = R - t / 2;
    const p = (r, deg) => [f2(cx + r * Math.cos((deg * Math.PI) / 180)), f2(cy + r * Math.sin((deg * Math.PI) / 180))];
    const [ax, ay] = p(r2, -48), [bx, by] = p(r2, 0);
    return (
      `<circle cx="${cx}" cy="${cy}" r="${f2(ro)}" fill="none" stroke="${fill}" stroke-width="${f2(t)}"/>` +
      `<path d="M${ax} ${ay}A${f2(r2)} ${f2(r2)} 0 1 0 ${bx} ${by}" fill="none" stroke="${fill}" stroke-width="${f2(t)}"/>` +
      `<rect x="${f2(cx + r2 * 0.05)}" y="${f2(cy - t / 2)}" width="${f2(ro - r2 * 0.05)}" height="${f2(t)}" fill="${fill}"/>`
    );
  }

  OG.add({
    num: 51,
    id: 'monogram',
    name: 'Monogram',
    caps: 'OGOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Beyond computing'],
    paletteTag: 'Earthy',
    risk: 'Safe',
    scene: false,
    bg: MO_BG,
    ink: MO_INK,
    palette: [MO_BG, MO_INK],
    fonts: ['Lexend Mega:wght@300'],
    type: 'Lexend Mega Light, wide and evenly tracked; the monogram is drawn from two circles and a bar, all one stroke weight.',
    concept:
      'O and G as one mark: a G set inside an O, its crossbar running out to join them. ' +
      'One color, no detail finer than its stroke, so it holds together as a favicon, a stamp or a boot badge.',
    logo: (w) =>
      OG.logoSvg(
        monogram(140, 150, 88, MO_INK) +
          OG.text(w, { x: 258, y: 164, size: 40, family: "'Lexend Mega', sans-serif", weight: 300, fill: MO_INK, ls: 2 }) +
          `<rect x="260" y="184" width="40" height="2" fill="${MO_INK}" opacity="0.5"/>` +
          OG.text('OPERATING SYSTEM', { x: 260, y: 206, size: 9, family: "'Lexend Mega', sans-serif", weight: 300, fill: MO_INK, ls: 1.5, attrs: 'opacity="0.7"' })
      ),
    icon: () => OG.iconSvg(`<rect width="64" height="64" rx="14" fill="${MO_BG}"/>` + monogram(32, 32, 27, MO_INK)),
  });

  /* 56. Two Dots ---------------------------------------------------------- */

  const TD = { bg: '#FAFAF7', ink: '#151515', red: '#E5322B', blue: '#1F4ED8' };
  // Red Hat Display Medium, per 100 units of size: ink left, ink right.
  const TD_INK = { g: [3.8, 53.3], s: [1.2, 43.6], G: [4.3, 74.8], S: [0.9, 57.9] };
  const TD_FONT = "'Red Hat Display', sans-serif";

  OG.add({
    num: 56,
    id: 'two-dots',
    name: 'Two Dots',
    caps: 'ogos',
    flexCase: true,
    family: 'Geometric',
    sources: ['Beyond computing'],
    paletteTag: 'Primary',
    risk: 'Safe',
    scene: false,
    bg: TD.bg,
    ink: TD.ink,
    palette: [TD.bg, TD.ink, TD.red, TD.blue],
    fonts: ['Red Hat Display:wght@500'],
    type: 'Red Hat Display Medium for the g and the s; each o is a solid disc, as tall as the letters beside it.',
    concept:
      'Swiss restraint: the two o\'s become a red disc and a blue disc, and only the g and s are letters. ' +
      'It is the outlier here on purpose, the version of OgOs that could sit on a modern laptop lid, and the dots read even at 16 pixels.',
    logo: (w) => {
      const L = [...w];
      const caps = /[A-Z]/.test(w);
      const unit = (S) => {
        const gap = 0.095 * S;
        const items = L.map((c, i) => {
          if (c === 'O' || c === 'o') return { disc: true, d: (c === 'O' ? 0.725 : 0.53) * S, fill: i === 0 ? TD.red : TD.blue };
          const [l, r] = TD_INK[c] || TD_INK[c.toLowerCase()];
          return { c, l: (l * S) / 100, d: ((r - l) * S) / 100 };
        });
        return { items, gap, total: items.reduce((a, it) => a + it.d, 0) + gap * (items.length - 1) };
      };
      let S = 150;
      let u = unit(S);
      if (u.total > 392) { S = (S * 392) / u.total; u = unit(S); }
      const band = (caps ? 0.705 : 0.505) * S;
      const base = f2(150 + band / 2 - (/g/.test(w) ? 0.07 * S : 0));
      let x = (480 - u.total) / 2, out = '';
      u.items.forEach((it) => {
        if (it.disc) out += `<circle cx="${f2(x + it.d / 2)}" cy="${f2(base - it.d / 2 + 0.006 * S)}" r="${f2(it.d / 2)}" fill="${it.fill}"/>`;
        else out += OG.text(it.c, { x: f2(x - it.l), y: base, size: f2(S), family: TD_FONT, weight: 500, fill: TD.ink });
        x += it.d + u.gap;
      });
      return OG.logoSvg(out);
    },
    icon: () => OG.iconSvg(`<circle cx="17" cy="32" r="14.5" fill="${TD.red}"/><circle cx="47" cy="32" r="14.5" fill="${TD.blue}"/>`),
  });

  /* 61. Pixel Didone ------------------------------------------------------ */

  const PD = { bg: '#0F0F10', ink: '#F1EBDD', gold: '#A8916A' };

  // A cubic Bezier as points.
  function bez(p0, p1, p2, p3, n = 48) {
    const out = [];
    for (let k = 0; k <= n; k++) {
      const t = k / n, u = 1 - t;
      out.push([0, 1].map((i) => u * u * u * p0[i] + 3 * u * u * t * p1[i] + 3 * u * t * t * p2[i] + t * t * t * p3[i]));
    }
    return out;
  }

  // Letters drawn with a flat nib, 4 pixels wide and 1 tall, so verticals
  // are stems and horizontals hairlines. Capitals 32 pixels tall, small
  // letters 20, tails to row 41. Each glyph is trimmed to its ink.
  const DIDONE = (() => {
    const nib = (pts) => ({ pts, rx: 2, ry: 0.55 });
    const ball = (x, y, r = 1.95) => ({ disc: [x, y, r] });
    const defs = {
      O: [nib(arc(14, 16, 11.4, 15.35, 0, 360))],
      G: [
        nib(arc(13.8, 16, 11.2, 15.35, -42, -330)),
        nib([[23.2, 3.6], [23.2, 10.2]]),
        nib([[23.9, 17.4], [23.9, 29.6]]),
        { rect: [19, 17, 10, 1] },
      ],
      S: [
        nib(bez([18.4, 5.6], [17.2, 0.9], [5.2, 0.3], [4.4, 7.4])),
        nib(bez([4.4, 7.4], [3.8, 13.6], [17.6, 16.4], [17.6, 23.6])),
        nib(bez([17.6, 23.6], [17.8, 30.8], [5.0, 31.8], [3.0, 25.6])),
        nib([[18.6, 4.0], [18.6, 9.2]]),
        nib([[2.8, 22.4], [2.8, 27.6]]),
      ],
      o: [nib(arc(11, 22, 8.6, 9.45, 0, 360))],
      g: [
        nib(arc(9.6, 17, 5.2, 4.6, 0, 360)),
        nib([[13.4, 13.5], [17.6, 12.6]]),
        ball(18.6, 12.9),
        nib(bez([6.6, 21.3], [2.6, 22.8], [3.0, 26.4], [8.6, 26.9])),
        nib(arc(10.8, 32.6, 8.2, 5.6, 0, 360)),
      ],
      s: [
        nib(bez([14.4, 15.6], [13.6, 12.3], [4.8, 11.9], [4.2, 16.5])),
        nib(bez([4.2, 16.5], [3.7, 21.2], [14.8, 21.3], [14.8, 26.4])),
        nib(bez([14.8, 26.4], [15.0, 31.7], [4.2, 31.9], [2.8, 27.8])),
        ball(14.5, 15.5),
        ball(2.7, 27.9),
      ],
    };
    const out = {};
    Object.keys(defs).forEach((k) => {
      const rows = raster(32, 42, defs[k]);
      const b = bbox(rows);
      out[k] = rows.map((r) => r.slice(b.x0, b.x1));
    });
    return out;
  })();

  OG.add({
    num: 61,
    id: 'pixel-didone',
    name: 'Pixel Didone',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Print & packaging'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: PD.bg,
    ink: PD.ink,
    palette: [PD.bg, PD.ink, PD.gold],
    fonts: ['Bodoni Moda:ital,wght@1,500'],
    type: 'A Didone drawn in square pixels with a flat nib: four-pixel stems, one-pixel hairlines, ball terminals as small clusters. The caption is Bodoni Moda Italic.',
    concept:
      'A fashion-house wordmark, the high contrast of a Bodoni, built at the resolution of a 320x200 screen. ' +
      'Every hairline is a single pixel, so the elegance and the pixel grid show at the same time.',
    logo: (w) => {
      const rows = layout(w, DIDONE, 4);
      const b = bbox(rows);
      const px = Math.min(4, 416 / b.w);
      const caps = /^[A-Z]+$/.test(w);
      // Center the capitals' band; tails hang below it.
      const top = 150 - 16 * px - (caps ? 0 : 3 * px) - 10;
      return OG.logoSvg(
        pixPath(rows, { '#': PD.ink }, { x: f2((480 - b.w * px) / 2 - b.x0 * px), y: f2(top), px }) +
          OG.text('320 × 200', { x: 240, y: f2(top + 41 * px + 30), size: 15, family: "'Bodoni Moda', serif", weight: 500, italic: true, fill: PD.gold, anchor: 'middle', ls: 4 }),
        { crisp: true }
      );
    },
    icon: () => {
      // A smaller O, so its hairlines stay one icon pixel wide.
      const rows = raster(16, 18, [{ pts: arc(8, 9, 5.6, 8.3, 0, 360), rx: 1.6, ry: 0.55 }]);
      return OG.iconSvg(`<rect width="64" height="64" fill="${PD.bg}"/>` + pixPath(rows, { '#': PD.ink }, { x: 4, y: 0, px: 3.5 }), { crisp: true });
    },
  });

  /* 66. Stencil ----------------------------------------------------------- */

  const ST = { bg: '#26281E', plate: '#59612F', plateDark: '#3F4520', paint: '#ECE5C6', rivet: '#7C8446' };
  const SAIRA = "'Saira Stencil One', 'Arial Black', sans-serif";

  // Spray paint: rough soft edges, a mist and speckle of overspray around
  // the letters, and worn spots where the paint has gone.
  const SPRAY =
    '<filter id="spray" x="-15%" y="-40%" width="130%" height="180%">' +
    '<feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="4" result="n"/>' +
    '<feDisplacementMap in="SourceGraphic" in2="n" scale="2.6" xChannelSelector="R" yChannelSelector="G" result="rough"/>' +
    '<feGaussianBlur in="rough" stdDeviation="0.55" result="soft"/>' +
    '<feTurbulence type="fractalNoise" baseFrequency="0.05 0.11" numOctaves="3" seed="11" result="wn"/>' +
    '<feColorMatrix in="wn" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  18 0 0 0 -11.6" result="wear"/>' +
    '<feComposite in="soft" in2="wear" operator="out" result="worn"/>' +
    '<feMorphology in="SourceAlpha" operator="dilate" radius="1.5" result="dil"/>' +
    '<feGaussianBlur in="dil" stdDeviation="4.5" result="halo"/>' +
    '<feTurbulence type="fractalNoise" baseFrequency="1.7" numOctaves="1" seed="9" result="n2"/>' +
    '<feColorMatrix in="n2" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  16 0 0 0 -10.2" result="dots"/>' +
    '<feComposite in="dots" in2="halo" operator="in" result="speck"/>' +
    `<feFlood flood-color="${ST.paint}" result="paint"/>` +
    '<feComposite in="paint" in2="speck" operator="in" result="speckle"/>' +
    '<feComposite in="paint" in2="halo" operator="in" result="mist0"/>' +
    '<feComponentTransfer in="mist0" result="mist"><feFuncA type="linear" slope="0.12"/></feComponentTransfer>' +
    '<feMerge><feMergeNode in="mist"/><feMergeNode in="speckle"/><feMergeNode in="worn"/></feMerge>' +
    '</filter>' +
    // The same spray, lighter, for small text: soft edges and a little mist.
    '<filter id="spray2" x="-5%" y="-60%" width="110%" height="220%">' +
    '<feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="5" result="n"/>' +
    '<feDisplacementMap in="SourceGraphic" in2="n" scale="1.4" xChannelSelector="R" yChannelSelector="G" result="rough"/>' +
    '<feGaussianBlur in="rough" stdDeviation="0.35" result="soft"/>' +
    '<feGaussianBlur in="SourceAlpha" stdDeviation="2.5" result="halo"/>' +
    `<feFlood flood-color="${ST.paint}" flood-opacity="0.2"/><feComposite in2="halo" operator="in" result="mist"/>` +
    '<feMerge><feMergeNode in="mist"/><feMergeNode in="soft"/></feMerge>' +
    '</filter>';

  function rivet(x, y, r) {
    return `<circle cx="${f2(x + 0.8)}" cy="${f2(y + 1.2)}" r="${r}" fill="#000" opacity="0.35"/><circle cx="${x}" cy="${y}" r="${r}" fill="url(#rvt)"/>`;
  }

  OG.add({
    num: 66,
    id: 'stencil',
    name: 'Stencil',
    caps: 'OGOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    scene: false,
    bg: ST.bg,
    ink: ST.paint,
    palette: [ST.plate, ST.paint, ST.bg, ST.rivet],
    fonts: ['Saira Stencil One'],
    type: 'Saira Stencil One, bridges and all, sprayed through a filter that roughens the edges, adds a fine overspray speckle and wears a few spots away.',
    concept:
      'The name sprayed through a stencil onto an olive-drab equipment plate, riveted at the corners, with a model and serial line beneath. ' +
      'OgOs as a tool built to run on old hardware for a long time: rugged, plain and made to be used.',
    logo: (w) => {
      const x = 32, y = 54, W = 416, H = 192;
      let out =
        '<defs>' + SPRAY +
        '<radialGradient id="rvt" cx="35%" cy="30%" r="75%"><stop offset="0" stop-color="#C9CF96"/><stop offset="0.45" stop-color="#7C8446"/><stop offset="1" stop-color="#2E3316"/></radialGradient>' +
        '<filter id="grain"><feTurbulence type="fractalNoise" baseFrequency="0.6" numOctaves="3" seed="2"/><feColorMatrix type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.9 -0.35"/><feComposite in2="SourceGraphic" operator="in"/></filter>' +
        '</defs>' +
        `<rect x="${x}" y="${y + 4}" width="${W}" height="${H}" rx="10" fill="#000" opacity="0.4"/>` +
        `<rect x="${x}" y="${y}" width="${W}" height="${H}" rx="10" fill="${ST.plate}"/>` +
        `<rect x="${x}" y="${y}" width="${W}" height="${H}" rx="10" fill="#000" filter="url(#grain)" opacity="0.5"/>` +
        `<rect x="${x + 1}" y="${y + 1}" width="${W - 2}" height="${H - 2}" rx="9" fill="none" stroke="#9BA562" stroke-opacity="0.45" stroke-width="2"/>` +
        `<rect x="${x + 12}" y="${y + 12}" width="${W - 24}" height="${H - 24}" rx="4" fill="none" stroke="${ST.plateDark}" stroke-width="2"/>`;
      [[x + 26, y + 26], [x + W - 26, y + 26], [x + 26, y + H - 26], [x + W - 26, y + H - 26]].forEach(([a, b]) => (out += rivet(a, b, 7)));
      out +=
        `<g filter="url(#spray)">` +
        OG.text(w, { x: 240, y: 178, size: 112, family: SAIRA, fill: ST.paint, anchor: 'middle', ls: 2 }) +
        '</g>' +
        `<g filter="url(#spray2)">` +
        OG.text('MODEL OGOS-32  ·  SER. 01-0000-55AA', { x: 240, y: 218, size: 14, family: SAIRA, fill: ST.paint, anchor: 'middle', ls: 2.5 }) +
        '</g>';
      return OG.logoSvg(out);
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="8" fill="${ST.plate}"/>` +
          `<defs><mask id="sb"><rect width="64" height="64" fill="#fff"/><rect x="29" y="0" width="6" height="64" fill="#000"/></mask></defs>` +
          `<path d="M22 9h20a12 12 0 0 1 12 12v22a12 12 0 0 1 -12 12h-20a12 12 0 0 1 -12 -12v-22a12 12 0 0 1 12 -12zM24 21v22h16v-22z" fill="${ST.paint}" fill-rule="evenodd" mask="url(#sb)"/>`
      ),
  });

  /* 71. Weight Ramp ------------------------------------------------------- */

  // A filament heating up: dull red to white hot.
  const WR = { bg: '#0D0B0A', heat: ['#A2301E', '#E6571E', '#FFB13A', '#FFF3D6'], weights: [200, 400, 700, 900] };
  // Unbounded's advance and ink, per 100 units of size, at each slot's weight.
  const WR_M = {
    0: { O: [93.0, 5.2, 87.8], o: [73.6, 4.4, 69.2] },
    1: { g: [77.9, 3.7, 71.3], G: [94.6, 4.5, 88.2] },
    2: { O: [96.5, 3.7, 92.8], o: [79.2, 3.0, 76.2] },
    3: { S: [87.0, 3.5, 83.9], s: [78.7, 2.2, 76.1] },
  };

  OG.add({
    num: 71,
    id: 'weight-ramp',
    name: 'Weight Ramp',
    caps: 'OgOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['OgOs internals'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: false,
    bg: WR.bg,
    ink: WR.heat[3],
    palette: [WR.bg, ...WR.heat],
    fonts: ['Unbounded:wght@200;400;700;900', 'JetBrains Mono:wght@500'],
    type: 'Unbounded, a variable font, at ExtraLight, Regular, Bold and Black, one weight per letter; the weights are labelled beneath in JetBrains Mono.',
    concept:
      'The name powers up from left to right: each letter heavier than the last and hotter in color, dull red to white, like a filament warming or a machine coming on. ' +
      'A bar under each letter grows with it, a meter for the weight.',
    logo: (w) => {
      const L = [...w], size = 104, sp = 6;
      const m = L.map((c, i) => WR_M[i][c] || WR_M[i][c.toUpperCase()] || WR_M[i][c.toLowerCase()]);
      const total = m.reduce((a, v) => a + (v[0] * size) / 100, 0) + sp * 3;
      let x = 240 - total / 2, out = '';
      const base = 162;
      L.forEach((c, i) => {
        const [adv, l, r] = m[i].map((v) => (v * size) / 100);
        out += OG.text(c, { x: f2(x), y: base, size, family: "'Unbounded', sans-serif", weight: WR.weights[i], fill: WR.heat[i] });
        const bh = [2, 4, 7, 11][i];
        out += `<rect x="${f2(x + l)}" y="${f2(198 - bh)}" width="${f2(r - l)}" height="${bh}" fill="${WR.heat[i]}"/>`;
        out += OG.text(String(WR.weights[i]), { x: f2(x + l), y: 216, size: 10, family: JB, weight: 500, fill: '#6D645C', ls: 1 });
        x += adv + sp;
      });
      return OG.logoSvg(out);
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="12" fill="${WR.bg}"/>` +
          `<rect x="10" y="10" width="44" height="44" rx="20" fill="none" stroke="${WR.heat[2]}" stroke-width="11"/>` +
          `<rect x="25" y="25" width="14" height="14" rx="6" fill="none" stroke="${WR.heat[0]}" stroke-width="2.4"/>`
      ),
  });

  /* 76. Toy Box ----------------------------------------------------------- */

  const TB = {
    bg: '#FFF3E0',
    face: ['#FF5A5F', '#FFB400', '#2EC4B6', '#7B61FF'],
    side: ['#C9363D', '#D08600', '#1C9488', '#5636D6'],
  };
  // Fredoka Bold advances and ink, per 100 units of size.
  const TB_M = { o: [55.8, 1.4, 54.4], g: [54.6, 1.4, 51.6], s: [45.7, 1.5, 44.2], O: [72.1, 1.4, 70.7], G: [71.9, 3.5, 67.4], S: [54.2, 1.1, 52.8] };
  const FRED = "'Fredoka', 'Nunito', sans-serif";

  // Mixes a hex color toward white by t.
  const tint = (hex, t) => '#' + [1, 3, 5].map((i) => Math.round(parseInt(hex.substr(i, 2), 16) * (1 - t) + 255 * t).toString(16).padStart(2, '0')).join('');

  OG.add({
    num: 76,
    id: 'toy-box',
    name: 'Toy Box',
    caps: 'ogos',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Pastel',
    risk: 'Safe',
    scene: false,
    bg: TB.bg,
    ink: '#3A2E5C',
    palette: [TB.bg, ...TB.face],
    fonts: ['Fredoka:wght@700'],
    type: 'Fredoka Bold, round and soft, each letter its own candy color with a molded edge underneath and a light glaze on top.',
    concept:
      'OgOs as a box of toy blocks: bubbly letters in four candy colors, each pressed out of plastic with a thick bottom edge. ' +
      'It says the system is for play and for making games, and it is friendly enough for a kid\'s first program.',
    logo: (w) => {
      const L = [...w], size = 150, sp = 7;
      const m = L.map((c) => TB_M[c] || TB_M[c.toLowerCase()]);
      const total = m.reduce((a, v) => a + (v[0] * size) / 100, 0) + sp * 3;
      const caps = /[A-Z]/.test(w);
      const base = caps ? 196 : 182;
      let x = 240 - total / 2, defs = '<defs>', edge = '', face = '';
      L.forEach((c, i) => {
        defs += `<linearGradient id="tb${i}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${tint(TB.face[i], 0.38)}"/><stop offset="0.55" stop-color="${TB.face[i]}"/></linearGradient>`;
        const t = (dy, fill) => OG.text(c, { x: f2(x), y: f2(base + dy), size, family: FRED, weight: 700, fill });
        for (let k = 10; k >= 1; k -= 1.5) edge += t(k, TB.side[i]);
        face += t(0, `url(#tb${i})`);
        x += (m[i][0] * size) / 100 + sp;
      });
      defs += '<filter id="tbs" x="-10%" y="-10%" width="120%" height="140%"><feDropShadow dx="0" dy="9" stdDeviation="7" flood-color="#7A4A10" flood-opacity="0.22"/></filter></defs>';
      return OG.logoSvg(defs + `<g filter="url(#tbs)">${edge}</g>` + face);
    },
    icon: () =>
      OG.iconSvg(
        '<defs><linearGradient id="tbi" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="' + tint(TB.face[0], 0.38) + '"/><stop offset="0.55" stop-color="' + TB.face[0] + '"/></linearGradient></defs>' +
          `<circle cx="32" cy="37" r="20" fill="none" stroke="${TB.side[0]}" stroke-width="15"/>` +
          `<circle cx="32" cy="31" r="20" fill="none" stroke="url(#tbi)" stroke-width="15"/>`
      ),
  });

  /* 81. Voxel ------------------------------------------------------------- */

  // 5x7 letters: capitals on rows 0-6, small letters on 2-6, the g's tail to 8.
  const E2V = ['.....', '.....'];
  const FVOX = {
    O: ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.', ...E2V],
    G: ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.', ...E2V],
    S: ['.####', '#....', '#....', '.###.', '....#', '....#', '####.', ...E2V],
    o: [...E2V, '.###.', '#...#', '#...#', '#...#', '.###.', ...E2V],
    g: [...E2V, '.####', '#...#', '#...#', '#...#', '.####', '....#', '.###.'],
    s: [...E2V, '.####', '#....', '.###.', '....#', '####.', ...E2V],
  };
  const VX = {
    bg: '#16132A',
    // front, top, side for each letter
    c: [['#FF5964', '#FFA3A9', '#B42E3A'], ['#FFC53D', '#FFE6A3', '#BD8A12'], ['#3DDC97', '#A6F2CF', '#1E9463'], ['#4D8BFF', '#A9C6FF', '#2A55BE']],
  };

  // Each set pixel becomes a column of D cubes going back, the word rising
  // to the right in 2:1 pixel-art isometric. `fit` centers the projected
  // bounds on (cx, cy) inside w x h. Faces hidden by a neighbour are skipped.
  function voxels(rows, colorOf, fit, D = 2) {
    const H = rows.length, n = rows[0].length;
    const wU = n + D, hU = 0.5 * (n + D) + H * 1.12;
    const s = Math.min(fit.w / wU, fit.h / hU), h = s * 1.12;
    // At unit size the bounds run x from -D to n and y from -hU to 0.
    const ox = fit.cx - ((n - D) / 2) * s, oy = fit.cy + (hU / 2) * s;
    const P = (x, y, z) => [f2(ox + (x + z) * s), f2(oy + (z - x) * s * 0.5 - y * h)];
    const on = (i, j) => j >= 0 && j < H && i >= 0 && i < n && rows[j][i] === '#';
    const poly = (pts, fill) => `<path d="M${pts.map((p) => p.join(' ')).join('L')}Z" fill="${fill}"/>`;
    const cubes = [];
    rows.forEach((r, j) => [...r].forEach((v, i) => {
      if (v === '#') for (let z = -D; z < 0; z++) cubes.push([i, H - 1 - j, z, j]);
    }));
    // Back to front: nearer means smaller x, larger y and z.
    cubes.sort((a, b) => a[2] - a[0] + a[1] - (b[2] - b[0] + b[1]));
    let out = '';
    cubes.forEach(([x, y, z, j]) => {
      const [front, top, side] = colorOf(x);
      if (!on(x, j - 1)) out += poly([P(x, y + 1, z), P(x + 1, y + 1, z), P(x + 1, y + 1, z + 1), P(x, y + 1, z + 1)], top);
      if (!on(x - 1, j)) out += poly([P(x, y, z), P(x, y, z + 1), P(x, y + 1, z + 1), P(x, y + 1, z)], side);
      if (z === -1) out += poly([P(x, y, 0), P(x + 1, y, 0), P(x + 1, y + 1, 0), P(x, y + 1, 0)], front);
    });
    return out;
  }

  OG.add({
    num: 81,
    id: 'voxel',
    name: 'Voxel',
    caps: 'OGOS',
    flexCase: true,
    family: 'Pixel',
    sources: ['Games & arcade'],
    paletteTag: 'Rainbow',
    risk: 'Bold',
    scene: false,
    bg: VX.bg,
    ink: '#FFFFFF',
    palette: [VX.bg, VX.c[0][0], VX.c[1][0], VX.c[2][0], VX.c[3][0]],
    fonts: [],
    type: 'A 5x7 bitmap face, every pixel extruded into a cube with a light top, a mid front and a dark side, in 2:1 isometric.',
    concept:
      'The pixel letters stood up as a row of building blocks, each pixel a cube, rising across the frame like the title of an isometric game. ' +
      'Pixels with depth: the flat 8-bit look of OgOs, built into something you could walk around.',
    logo: (w) => {
      const rows = layout(w, FVOX, 1);
      const b = bbox(rows);
      const r = rows.slice(b.y0, b.y1);
      const letterOf = (i) => Math.min(3, Math.floor(i / 6));
      return OG.logoSvg(voxels(r, (i) => VX.c[letterOf(i)], { cx: 240, cy: 150, w: 432, h: 248 }, 1));
    },
    icon: () => {
      const O = ['.##.', '#..#', '#..#', '#..#', '.##.'];
      return OG.iconSvg(`<rect width="64" height="64" rx="12" fill="${VX.bg}"/>` + voxels(O, () => VX.c[0], { cx: 32, cy: 32, w: 54, h: 54 }, 1));
    },
  });

  /* 86. Blueprint --------------------------------------------------------- */

  const BP = { bg: '#164985', line: '#FFFFFF', fill: '#2A64A6', grid: '#FFFFFF' };

  // The letters are built from circles, arcs and bars, one stroke weight t.
  // Each glyph returns its width, its filled shapes and its construction marks.
  function bpGlyph(c, x0, yb, H, X, t, note) {
    const P = (cx, cy, r, a) => [f2(cx + r * Math.cos((a * Math.PI) / 180)), f2(cy + r * Math.sin((a * Math.PI) / 180))];
    const ring = (cx, cy, R, r) => `M${f2(cx - R)} ${f2(cy)}a${f2(R)} ${f2(R)} 0 1 0 ${f2(2 * R)} 0a${f2(R)} ${f2(R)} 0 1 0 ${f2(-2 * R)} 0zM${f2(cx - r)} ${f2(cy)}a${f2(r)} ${f2(r)} 0 1 0 ${f2(2 * r)} 0a${f2(r)} ${f2(r)} 0 1 0 ${f2(-2 * r)} 0z`;
    const sector = (cx, cy, R, r, a0, a1) => {
      const lg = a1 - a0 > 180 ? 1 : 0;
      const [p0, p1, p2, p3] = [P(cx, cy, R, a0), P(cx, cy, R, a1), P(cx, cy, r, a1), P(cx, cy, r, a0)];
      return `M${p0.join(' ')}A${f2(R)} ${f2(R)} 0 ${lg} 1 ${p1.join(' ')}L${p2.join(' ')}A${f2(r)} ${f2(r)} 0 ${lg} 0 ${p3.join(' ')}Z`;
    };
    const rect = (x, y, w, h) => `M${f2(x)} ${f2(y)}h${f2(w)}v${f2(h)}h${f2(-w)}z`;
    const dash = (cx, cy, r) => `<circle cx="${f2(cx)}" cy="${f2(cy)}" r="${f2(r)}" fill="none" stroke="${BP.line}" stroke-width="0.8" stroke-dasharray="7 3 1.5 3" opacity="0.6"/>`;
    const cross = (cx, cy, n = 7) => `<path d="M${f2(cx - n)} ${f2(cy)}h${2 * n}M${f2(cx)} ${f2(cy - n)}v${2 * n}" stroke="${BP.line}" stroke-width="0.9" opacity="0.8"/>`;
    const callout = (cx, cy, R, a, label) => {
      const [x1, y1] = P(cx, cy, R, a), [x2, y2] = P(cx, cy, R + 16, a);
      const right = Math.cos((a * Math.PI) / 180) > 0;
      return `<path d="M${f2(cx)} ${f2(cy)}L${x2} ${y2}h${right ? 14 : -14}" fill="none" stroke="${BP.line}" stroke-width="0.8" opacity="0.85"/><circle cx="${x1}" cy="${y1}" r="1.8" fill="${BP.line}"/>` +
        OG.text(label, { x: f2(+x2 + (right ? 17 : -17)), y: f2(+y2 + 3), size: 8.5, family: JB, weight: 500, fill: BP.line, anchor: right ? 'start' : 'end' });
    };
    const big = c === c.toUpperCase();
    const h = big ? H : X;
    const R = h / 2, cy = yb - R;
    if (c === 'O' || c === 'o') {
      const cx = x0 + R;
      return { w: 2 * R, d: [ring(cx, cy, R, R - t)], k: dash(cx, cy, R - t / 2) + cross(cx, cy) + (note ? callout(cx, cy, R, 48, `R${Math.round(R)}`) : '') };
    }
    if (c === 'G' || c === 'g') {
      const cx = x0 + R;
      if (big) {
        const k = dash(cx, cy, R - t / 2) + cross(cx, cy) +
          `<path d="M${f2(cx)} ${f2(cy)}L${P(cx, cy, R + 20, -40).join(' ')}M${f2(cx)} ${f2(cy)}L${f2(cx + R + 20)} ${f2(cy)}" stroke="${BP.line}" stroke-width="0.8" stroke-dasharray="3 2" opacity="0.7" fill="none"/>` +
          `<path d="M${P(cx, cy, R + 12, -40).join(' ')}A${f2(R + 12)} ${f2(R + 12)} 0 0 1 ${f2(cx + R + 12)} ${f2(cy)}" fill="none" stroke="${BP.line}" stroke-width="0.8"/>` +
          OG.text('40°', { x: f2(cx + R + 16), y: f2(cy - 16), size: 8.5, family: JB, weight: 500, fill: BP.line });
        return { w: 2 * R, d: [sector(cx, cy, R, R - t, 0, 320), rect(cx + t * 0.1, cy - t / 2, R - t * 0.1, t)], k };
      }
      const dd = X * 0.2;
      return {
        w: 2 * R,
        d: [ring(cx, cy, R, R - t), rect(cx + R - t, yb - X, t, X + dd), sector(cx, yb + dd, R, R - t, 0, 160)],
        k: dash(cx, cy, R - t / 2) + cross(cx, cy) + dash(cx, yb + dd, R - t / 2) + cross(cx, yb + dd, 5),
      };
    }
    // S and s: two arcs on stacked circles.
    const c2 = (h - t) / 4, cx = x0 + c2 + t / 2;
    const yu = yb - h + t / 2 + c2, yl = yu + 2 * c2;
    return {
      w: 2 * c2 + t,
      d: [sector(cx, yu, c2 + t / 2, c2 - t / 2, 90, 330), sector(cx, yl, c2 + t / 2, c2 - t / 2, -90, 150)],
      k: dash(cx, yu, c2) + dash(cx, yl, c2) + cross(cx, yu, 5) + cross(cx, yl, 5),
    };
  }

  OG.add({
    num: 86,
    id: 'blueprint',
    name: 'Blueprint',
    caps: 'OgOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Print & packaging'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: true,
    bg: BP.bg,
    ink: '#FFFFFF',
    palette: [BP.bg, '#FFFFFF', BP.fill],
    fonts: ['JetBrains Mono:wght@500;700'],
    type: 'Geometric letters constructed from circles, arcs and bars at one stroke weight, drawn in outline; callouts and the title block in JetBrains Mono.',
    concept:
      "The engineering drawing of the logo: the name's letters constructed from circles and bars on blueprint blue, with center marks, dashed construction circles, a radius callout, the G's 40 degree opening and a title block. " +
      'It shows that the mark is built, not drawn by hand, which suits a system people take apart to learn from.',
    logo: (w) => {
      // A drafting grid.
      let out =
        `<rect width="480" height="300" fill="${BP.bg}"/>` +
        '<defs><pattern id="bpg" width="12" height="12" patternUnits="userSpaceOnUse"><path d="M12 0H0V12" fill="none" stroke="#fff" stroke-width="0.5" opacity="0.13"/></pattern>' +
        '<pattern id="bpG" width="60" height="60" patternUnits="userSpaceOnUse"><path d="M60 0H0V60" fill="none" stroke="#fff" stroke-width="0.8" opacity="0.2"/></pattern></defs>' +
        '<rect width="480" height="300" fill="url(#bpg)"/><rect width="480" height="300" fill="url(#bpG)"/>' +
        `<rect x="10" y="10" width="460" height="280" fill="none" stroke="#fff" stroke-width="1.2" opacity="0.8"/>`;
      let H = 104, gapK = 0.13;
      const widthAt = (H) => [...w].reduce((a, c) => a + bpGlyph(c, 0, 0, H, H * 0.7, H * 0.19).w, 0) + gapK * H * (w.length - 1);
      if (widthAt(H) > 380) H = (H * 380) / widthAt(H);
      const X = H * 0.7, t = H * 0.19, total = widthAt(H);
      const x0 = 240 - total / 2, yb = 70 + H;
      // Guide lines.
      const guides = [[yb - H, 'CAP'], [yb, 'BASE']];
      if (/[a-z]/.test(w)) guides.push([yb - X, 'X']);
      guides.forEach(([y, label]) => {
        out += `<path d="M16 ${f2(y)}H464" stroke="#fff" stroke-width="0.7" stroke-dasharray="2 3" opacity="0.55"/>` +
          OG.text(label, { x: 16, y: f2(y - 3), size: 7, family: JB, weight: 500, fill: '#fff', ls: 1, attrs: 'opacity="0.75"' });
      });
      let x = x0, under = '', over = '', marks = '';
      [...w].forEach((c, i) => {
        const g = bpGlyph(c, x, yb, H, X, t, i === 2);
        g.d.forEach((d) => {
          under += `<path d="${d}" fill="#fff" stroke="#fff" stroke-width="3.2" fill-rule="evenodd"/>`;
          over += `<path d="${d}" fill="${BP.fill}" fill-rule="evenodd"/>`;
        });
        marks += g.k;
        x += g.w + gapK * H;
      });
      // Overall width, dimensioned above the capitals.
      const dy = yb - H - 16;
      out += under + over + marks +
        `<path d="M${f2(x0)} ${f2(dy - 6)}v12M${f2(x0 + total)} ${f2(dy - 6)}v12" stroke="#fff" stroke-width="0.8"/>` +
        `<path d="M${f2(x0)} ${f2(dy)}H${f2(x0 + total)}" stroke="#fff" stroke-width="0.8"/>` +
        `<path d="M${f2(x0)} ${f2(dy)}l7 -2.5v5zM${f2(x0 + total)} ${f2(dy)}l-7 -2.5v5z" fill="#fff"/>` +
        `<rect x="${f2(240 - 20)}" y="${f2(dy - 6)}" width="40" height="12" fill="${BP.bg}"/>` +
        OG.text(String(Math.round(total)), { x: 240, y: f2(dy + 3), size: 9, family: JB, weight: 700, fill: '#fff', anchor: 'middle' });
      // Title block.
      const tb = { x: 286, y: 248, w: 174, h: 32 };
      out += `<rect x="${tb.x}" y="${tb.y}" width="${tb.w}" height="${tb.h}" fill="${BP.bg}" stroke="#fff" stroke-width="1"/>` +
        `<path d="M${tb.x} ${tb.y + 16}H${tb.x + tb.w}M${tb.x + 118} ${tb.y}V${tb.y + 32}M${tb.x + 58} ${tb.y + 16}V${tb.y + 32}" stroke="#fff" stroke-width="0.8"/>` +
        OG.text('OGOS MARK', { x: tb.x + 5, y: tb.y + 12, size: 9, family: JB, weight: 700, fill: '#fff', ls: 1 }) +
        OG.text('REV A', { x: tb.x + 123, y: tb.y + 12, size: 8, family: JB, weight: 500, fill: '#fff', ls: 1 }) +
        OG.text('SCALE 1:1', { x: tb.x + 5, y: tb.y + 28, size: 7.5, family: JB, weight: 500, fill: '#fff' }) +
        OG.text('DWG 0x10000', { x: tb.x + 63, y: tb.y + 28, size: 7.5, family: JB, weight: 500, fill: '#fff' }) +
        OG.text('SHT 1/1', { x: tb.x + 123, y: tb.y + 28, size: 7.5, family: JB, weight: 500, fill: '#fff' });
      return OG.logoSvg(out);
    },
    icon: () => {
      const c = 32, R = 21, t = 8;
      return OG.iconSvg(
        `<rect width="64" height="64" rx="10" fill="${BP.bg}"/>` +
          `<path d="M4 32H60M32 4V60" stroke="#fff" stroke-width="1.6" stroke-dasharray="6 2.5 1.5 2.5" opacity="0.8"/>` +
          `<circle cx="${c}" cy="${c}" r="${R - t / 2}" fill="none" stroke="#fff" stroke-width="${t + 3}"/>` +
          `<circle cx="${c}" cy="${c}" r="${R - t / 2}" fill="none" stroke="${BP.fill}" stroke-width="${t - 0.5}"/>` +
          `<circle cx="${c}" cy="${c}" r="2.6" fill="#fff"/>`
      );
    },
  });

  /* 91. Pattern Fill ------------------------------------------------------ */

  // 8x8 one-bit fill patterns, '#' black.
  const PATS = (() => {
    const gen = (f) => Array.from({ length: 8 }, (_, j) => Array.from({ length: 8 }, (_, i) => (f(i, j) ? '#' : '.')).join(''));
    return {
      bricks: ['########', '#.......', '#.......', '#.......', '########', '....#...', '....#...', '....#...'],
      diagonal: gen((i, j) => (i + j) % 4 === 0),
      dots: gen((i, j) => (j % 4 === 0 && i % 4 === 0) || (j % 4 === 2 && i % 4 === 2)),
      checks: gen((i, j) => (i < 4) !== (j < 4)),
      weave: gen((i, j) => ((i < 4) !== (j < 4) ? j % 2 === 0 : i % 2 === 0)),
      gray: gen((i, j) => (i + j) % 2 === 0),
      light: gen((i, j) => i % 4 === 0 && j % 2 === 0 && (i + j) % 4 === 0),
      hlines: gen((i, j) => j % 4 === 0),
      vlines: gen((i, j) => i % 4 === 1),
      grid: gen((i, j) => i % 8 === 0 || j % 8 === 0),
      zigzag: gen((i, j) => (j + (i < 4 ? i : 7 - i)) % 4 === 0),
      solid: gen(() => true),
    };
  })();
  const PF_ORDER = ['solid', 'gray', 'light', 'bricks', 'diagonal', 'dots', 'checks', 'weave', 'hlines', 'vlines', 'grid', 'zigzag'];
  const PF_LETTER = ['bricks', 'weave', 'checks', 'diagonal'];

  // <pattern> definitions: white ground, black pixels, px units a pixel.
  function patDefs(px, prefix) {
    return PF_ORDER.map((k) => `<pattern id="${prefix}${k}" width="${8 * px}" height="${8 * px}" patternUnits="userSpaceOnUse"><rect width="${8 * px}" height="${8 * px}" fill="#fff"/>${pixPath(PATS[k], { '#': '#000' }, { px })}</pattern>`).join('');
  }

  // Fat round letters, drawn with a round nib 14 pixels wide: capitals 60
  // tall, small letters 44, tails to row 77.
  const FATS = (() => {
    const n = (pts, r = 7) => ({ pts, rx: r, ry: r });
    const defs = {
      O: [n(arc(24, 30, 17, 23, 0, 360))],
      G: [n(arc(24, 30, 17, 23, -40, -360)), n([[27, 31.5], [40.5, 31.5]])],
      S: [n(bez([36, 13.5], [33, 7.5], [12, 5], [9.5, 17.5])), n(bez([9.5, 17.5], [8, 28], [36, 29], [36, 42])), n(bez([36, 42], [36, 55.5], [11, 56], [8.5, 46]))],
      o: [n(arc(21, 38, 14, 15, 0, 360))],
      g: [n(arc(20, 37, 13, 14, 0, 360)), n([[33, 23], [33, 63]]), n(bez([33, 63], [33, 72.5], [14, 72.5], [10, 66]))],
      s: [n(bez([35, 28], [32.5, 22], [11, 21], [9, 28.5]), 6.5), n(bez([9, 28.5], [7.5, 35], [36, 37], [36, 45.5]), 6.5), n(bez([36, 45.5], [36, 54], [12, 54.5], [8, 47.5]), 6.5)],
    };
    const out = {};
    Object.keys(defs).forEach((k) => {
      const rows = raster(50, 78, defs[k]);
      const b = bbox(rows);
      out[k] = rows.map((r) => r.slice(b.x0, b.x1));
    });
    return out;
  })();

  // Splits a letter's mask into a 2-pixel black outline ('k') and its
  // inside ('i').
  function outlined(rows, wk = 2) {
    const H = rows.length, W = rows[0].length;
    const on = (i, j) => j >= 0 && j < H && i >= 0 && i < W && rows[j][i] === '#';
    return rows.map((r, j) => [...r].map((v, i) => {
      if (v !== '#') return '.';
      for (let dj = -wk; dj <= wk; dj++) for (let di = -wk; di <= wk; di++) if (Math.abs(di) + Math.abs(dj) <= wk && !on(i + di, j + dj)) return 'k';
      return 'i';
    }).join(''));
  }

  OG.add({
    num: 91,
    id: 'pattern-fill',
    name: 'Pattern Fill',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Mac OS Classic'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: '#FFFFFF',
    ink: '#000000',
    palette: ['#FFFFFF', '#000000'],
    fonts: [],
    type: 'Fat round letters drawn on a pixel grid with a round nib 14 pixels wide, outlined two pixels thick and filled with 8x8 one-bit patterns.',
    concept:
      "A wordmark from a 1-bit paint program: each letter outlined in black and filled with a different 8x8 pattern, bricks, weave, checks and diagonals, with the pattern palette laid out beneath. " +
      'Black and white, but never plain, the way early Mac artists got texture out of one bit.',
    logo: (w) => {
      const px = 2.4;
      const gl = [...w].map((c) => outlined(FATS[c] || FATS[c.toUpperCase()]));
      const gap = 5, W = gl.reduce((a, g) => a + g[0].length, 0) + gap * 3;
      const P = Math.min(px, 420 / W);
      let x = 240 - (W * P) / 2;
      const caps = /^[A-Z]+$/.test(w);
      const top = caps ? 48 : 30;
      let out = `<defs>${patDefs(P, 'pf')}</defs>`;
      gl.forEach((g, n) => {
        out += pixPath(g, { i: `url(#pf${PF_LETTER[n]})`, k: '#000' }, { x: f2(x), y: top, px: P });
        x += (g[0].length + gap) * P;
      });
      // The pattern palette, as a paint program lays it out.
      const sw = 30, sh = 20, n = PF_ORDER.length, sx = 240 - (n * sw) / 2, sy = 250;
      out += `<rect x="${f2(sx - 3)}" y="${sy - 3}" width="${n * sw + 6}" height="${sh + 6}" fill="#fff" stroke="#000" stroke-width="2"/>`;
      PF_ORDER.forEach((k, i) => {
        out += `<rect x="${f2(sx + i * sw + 2)}" y="${sy + 2}" width="${sw - 4}" height="${sh - 4}" fill="url(#pf${k})" stroke="#000" stroke-width="1"/>`;
      });
      const sel = PF_ORDER.indexOf('bricks');
      out += `<rect x="${f2(sx + sel * sw - 1)}" y="${sy - 1}" width="${sw + 2}" height="${sh + 2}" fill="none" stroke="#000" stroke-width="2.5"/>`;
      return OG.logoSvg(out, { crisp: true });
    },
    icon: () => {
      const rows = outlined(raster(30, 30, [{ pts: arc(15, 15, 9.5, 9.5, 0, 360), rx: 5, ry: 5 }]), 2);
      return OG.iconSvg(`<defs>${patDefs(2, 'pi')}</defs><rect width="64" height="64" fill="#fff"/>` + pixPath(rows, { i: 'url(#pibricks)', k: '#000' }, { x: 2, y: 2, px: 2 }), { crisp: true });
    },
  });

  /* 96. Eight Bars -------------------------------------------------------- */

  const EB = { bg: '#F6F7F9', blue: '#1238A6' };
  // Squared slab capitals, drawn as strokes 30 wide from 0 to 120. The bars
  // fall every 16 units from the cap line, and every horizontal stroke
  // spans exactly two of them. Each glyph returns its width and its strokes
  // and serif blocks.
  function ebGlyph(c, x) {
    const t = 30, h = t / 2;
    const top = 0, bot = 120, W = 118, r = 42;
    const L = x + h, R = x + W - h, T = top + h, B = bot - h;
    const sq = (x0, y0, x1, y1, r) => `M${x0 + r} ${y0}H${x1 - r}Q${x1} ${y0} ${x1} ${y0 + r}V${y1 - r}Q${x1} ${y1} ${x1 - r} ${y1}H${x0 + r}Q${x0} ${y1} ${x0} ${y1 - r}V${y0 + r}Q${x0} ${y0} ${x0 + r} ${y0}Z`;
    const st = (d) => ({ d }), blk = (x0, y0, w, h2) => ({ d: `M${x0} ${y0}h${w}v${h2}h${-w}z`, fill: true });
    if (c === 'O') return { w: W, p: [st(sq(L, T, R, B, r))] };
    if (c === 'G') {
      return {
        w: W,
        p: [
          st(`M${R} ${T + 4}Q${R} ${T} ${R - r} ${T}H${L + r}Q${L} ${T} ${L} ${T + r}V${B - r}Q${L} ${B} ${L + r} ${B}H${R - r}Q${R} ${B} ${R} ${B - r}V${60}`),
          blk(R - h, 0, t, 42),
          blk(x + W / 2 - 4, 45, R + h - (x + W / 2 - 4), t),
        ],
      };
    }
    // S: squared, with block serifs at both ends.
    const m = (T + B) / 2, rr = 40, sx = R - 8;
    return {
      w: W - 8,
      p: [
        st(`M${sx} ${T + 6}Q${sx} ${T} ${sx - rr} ${T}H${L + rr}Q${L} ${T} ${L} ${T + rr}V${m - rr}Q${L} ${m} ${L + rr} ${m}H${sx - rr}Q${sx} ${m} ${sx} ${m + rr}V${B - rr}Q${sx} ${B} ${sx - rr} ${B}H${L + rr}Q${L} ${B} ${L} ${B - 6}`),
        blk(sx - h, top, t, 42),
        blk(L - h, 78, t, 42),
      ],
    };
  }

  OG.add({
    num: 96,
    id: 'eight-bars',
    name: 'Eight Bars',
    caps: 'OGOS',
    flexCase: false,
    family: 'Wordmark',
    sources: ['IBM & OS/2'],
    paletteTag: 'Corporate',
    risk: 'Safe',
    scene: false,
    bg: EB.bg,
    ink: EB.blue,
    palette: [EB.bg, EB.blue],
    fonts: ['Familjen Grotesk:wght@600'],
    type: 'Squared slab letters drawn for this mark, one heavy stroke weight with block serifs, cut into eight bars that thin toward the baseline; the descriptor in Familjen Grotesk.',
    concept:
      'Enterprise-grade retro: heavy squared slab capitals in one deep blue, sliced into eight horizontal bars, the way the big machine makers of the 70s striped their names. ' +
      'Here the bars thin as they fall, so the name reads like a fast scan, serious and a little restless.',
    logo: (w) => {
      const gap = 16;
      const gl = [];
      let x = 0;
      // Capitals only: a letter needs all eight bars.
      [...w.toUpperCase()].forEach((c) => { const g = ebGlyph(c, x); gl.push(g); x += g.w + gap; });
      const total = x - gap;
      const k = Math.min(0.86, 410 / total);
      const ox = 240 - (total * k) / 2, oy = 150 - 60 * k - 14;
      // Eight bars, each a little thinner than the one above.
      let bars = '';
      for (let i = 0; i < 8; i++) bars += `<rect x="-20" y="${i * 16}" width="${f2(total + 40)}" height="${f2(12.6 - i * 0.62)}" fill="#fff"/>`;
      let body = '';
      gl.forEach((g) => g.p.forEach((p) => {
        body += p.fill ? `<path d="${p.d}" fill="${EB.blue}"/>` : `<path d="${p.d}" fill="none" stroke="${EB.blue}" stroke-width="30"/>`;
      }));
      return OG.logoSvg(
        `<defs><mask id="ebm" maskUnits="userSpaceOnUse" x="-40" y="-20" width="${f2(total + 80)}" height="220">${bars}</mask></defs>` +
          `<g transform="translate(${f2(ox)} ${f2(oy)}) scale(${f2(k)})"><g mask="url(#ebm)">${body}</g></g>` +
          OG.text('32-BIT OPERATING SYSTEM', { x: 240, y: f2(oy + 120 * k + 34), size: 11, family: "'Familjen Grotesk', sans-serif", weight: 600, fill: EB.blue, anchor: 'middle', ls: 5 })
      );
    },
    icon: () => {
      // The O alone, cut by the same bars.
      const g = ebGlyph('O', 0);
      let bars = '';
      for (let i = 0; i < 8; i++) bars += `<rect x="-20" y="${i * 16}" width="160" height="${f2(12.6 - i * 0.62)}" fill="#fff"/>`;
      return OG.iconSvg(
        `<defs><mask id="ebi" maskUnits="userSpaceOnUse" x="-20" y="-20" width="170" height="170">${bars}</mask></defs>` +
          `<g transform="translate(5.5 6.5) scale(0.45)"><g mask="url(#ebi)"><path d="${g.p[0].d}" fill="none" stroke="${EB.blue}" stroke-width="30"/></g></g>`
      );
    },
  });
})();
