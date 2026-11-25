/*
 * Round two: directions 101 to 110, from the feedback on the first hundred.
 *
 * They share one palette: the desktop blue #1F5F9E, with #E0452B and #FF6B2C
 * as the bright accents, cream and a blue-black ink. Where a design keeps the
 * old RetrOS logo's bright element, it moves to the bottom left.
 */
(function () {
  'use strict';

  /* --------------------------------------------------------------- helpers */

  const f2 = (n) => +(+n).toFixed(2);
  const rect = (x, y, w, h, fill, extra = '') =>
    `<rect x="${f2(x)}" y="${f2(y)}" width="${f2(w)}" height="${f2(h)}" fill="${fill}"${extra ? ' ' + extra : ''}/>`;

  const P = {
    blue: '#1F5F9E', blueDeep: '#163F6B', blueLight: '#4F8FD0', sky: '#8EC3EE',
    red: '#E0452B', orange: '#FF6B2C', peach: '#FFB08A',
    cream: '#EFEADB', paper: '#F4F1E8', white: '#FFFDF6',
    ink: '#0F1B2D', screen: '#12243A', glyph: '#E9E6DA',
    steel: '#9DB0C6', steelDark: '#7A8FA8',
  };

  // Draws a bitmap as one <path> per color.
  function pixPath(rows, map, opts = {}) {
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

  // Sets a word in a bitmap alphabet; a missing letter falls back to its other case.
  function layout(word, font, gap = 1) {
    const gl = [...word].map((c) => font[c] || font[c.toUpperCase()] || font[c.toLowerCase()]);
    const rows = gl[0].map(() => '');
    gl.forEach((g, n) => g.forEach((r, j) => (rows[j] += (n ? '.'.repeat(gap) : '') + r)));
    return rows;
  }

  // The bounding box of a bitmap's set cells, with exclusive ends.
  function bbox(rows) {
    let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
    rows.forEach((r, j) => [...r].forEach((c, i) => {
      if (c !== '.' && c !== ' ') { x0 = Math.min(x0, i); x1 = Math.max(x1, i + 1); y0 = Math.min(y0, j); y1 = Math.max(y1, j + 1); }
    }));
    return { x0, y0, x1, y1, w: x1 - x0, h: y1 - y0 };
  }

  // Advance widths at 100 px, measured in Chrome, for fitting the name.
  const ADV = {
    saira: { O: 69.9, G: 66.4, S: 61.2, o: 57.2, g: 58.4, s: 50.1 },
    jost: { O: 82.1, G: 81.4, S: 62.5, o: 58.9, g: 61, s: 45.6 },
    archivo: { O: 83.3, G: 83.3, S: 72.2, o: 66.7, g: 66.6, s: 61.1 },
  };
  // The width of str at `size`, with `track` em of letter-spacing after each letter but the last.
  const tw = (font, str, size, track = 0) =>
    ([...str].reduce((a, c) => a + (ADV[font][c] || 60), 0) / 100 + track * (str.length - 1)) * size;
  const fit = (font, str, size, maxW, track = 0) => Math.min(size, maxW / tw(font, str, 1, track));

  const SAIRA = "'Saira', sans-serif";
  const JOST = "'Jost', sans-serif";
  const ARCHIVO = "'Archivo', sans-serif";

  /* ---------------------------------------------------------- bitmap faces */

  // The old logo's screen face: 8 wide and 14 tall, capitals on rows 0-10,
  // small letters on 3-10 and the g's tail to 13.
  const E3 = ['........', '........', '........'];
  const BIG = {
    O: ['.######.', '########', '##....##', '##....##', '##....##', '##....##', '##....##', '##....##', '##....##', '########', '.######.', ...E3],
    G: ['.######.', '########', '##....##', '##......', '##......', '##..####', '##..####', '##....##', '##....##', '########', '.######.', ...E3],
    S: ['.######.', '########', '##....##', '##......', '##......', '#######.', '.#######', '......##', '##....##', '########', '.######.', ...E3],
    o: [...E3, '.######.', '########', '##....##', '##....##', '##....##', '##....##', '########', '.######.', ...E3],
    g: [...E3, '.#######', '########', '##....##', '##....##', '##....##', '##....##', '########', '.#######', '......##', '########', '#######.'],
    s: [...E3, '.#######', '########', '##......', '#######.', '.#######', '......##', '########', '#######.', ...E3],
  };
  const BIG_BASE = 11; // the row under the baseline

  // The old logo's name face: 6 wide and 11 tall, two pixels a stroke.
  const E2 = ['......', '......'];
  const MID = {
    O: ['.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E2],
    G: ['.####.', '##..##', '##....', '##....', '##.###', '##..##', '##..##', '##..##', '.####.', ...E2],
    S: ['.####.', '##..##', '##....', '##....', '.####.', '....##', '....##', '##..##', '.####.', ...E2],
    o: [...E2, '.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', ...E2],
    g: [...E2, '.#####', '##..##', '##..##', '##..##', '##..##', '##..##', '.#####', '....##', '.####.'],
    s: [...E2, '.#####', '##....', '##....', '.####.', '....##', '....##', '#####.', ...E2],
  };

  // A 3x5 micro face for small print.
  const MICRO = {
    0: ['###', '#.#', '#.#', '#.#', '###'], 1: ['.#.', '##.', '.#.', '.#.', '###'], 2: ['##.', '..#', '.#.', '#..', '###'],
    3: ['##.', '..#', '.#.', '..#', '##.'], '-': ['...', '...', '###', '...', '...'], ' ': ['.', '.', '.', '.', '.'],
    A: ['.#.', '#.#', '###', '#.#', '#.#'], B: ['##.', '#.#', '##.', '#.#', '##.'], I: ['###', '.#.', '.#.', '.#.', '###'],
    T: ['###', '.#.', '.#.', '.#.', '.#.'], O: ['###', '#.#', '#.#', '#.#', '###'], S: ['###', '#..', '###', '..#', '###'],
    M: ['#.#', '###', '###', '#.#', '#.#'], N: ['##.', '#.#', '#.#', '#.#', '#.#'], E: ['###', '#..', '##.', '#..', '###'],
    X: ['#.#', '#.#', '.#.', '#.#', '#.#'], x: ['...', '#.#', '.#.', '#.#', '...'], 8: ['###', '#.#', '###', '#.#', '###'],
  };

  /* ------------------------------------------------------------ the frame */

  // The RetrOS monitor, front on: a dark outline, a cream frame, a dark
  // screen and a steel foot. Returns the markup and the screen's and chin's
  // boxes. o: line, inset (outer edge to screen), screenH, foot, colors.
  function monitor(x, y, W, H, o) {
    const line = o.line || 6, inset = o.inset || 18, foot = o.foot == null ? 22 : o.foot;
    const c = Object.assign({ line: P.ink, frame: P.cream, screen: P.screen, foot: P.steel, footTop: P.steelDark }, o.colors);
    const sx = x + inset, sy = y + inset, sw = W - 2 * inset, sh = o.screenH;
    let s = rect(x, y, W, H, c.line) + rect(x + line, y + line, W - 2 * line, H - 2 * line, c.frame);
    if (foot) {
      s += rect(x + line, y + H - line - foot, W - 2 * line, foot, c.foot);
      s += rect(x + line, y + H - line - foot, W - 2 * line, Math.max(1, Math.round(foot / 5)), c.footTop);
    }
    s += rect(sx, sy, sw, sh, c.screen);
    return { svg: s, screen: { x: sx, y: sy, w: sw, h: sh }, chin: { y0: sy + sh, y1: y + H - line - foot } };
  }

  // A sun rising at the bottom left of a cols x rows grid: centered cx cells
  // in from the left edge, on the bottom edge, with radius r. Orange above
  // `split` of the radius and red below, cut by blank rows (counted up from
  // the bottom) that widen toward the bottom.
  function sunRows(cols, rows, r, gaps, cx = 0, split = 0.5) {
    const out = [];
    for (let j = 0; j < rows; j++) {
      const k = rows - 1 - j;
      let s = '';
      for (let i = 0; i < cols; i++) {
        if (Math.hypot(i + 0.5 - cx, k + 0.5) > r || gaps.includes(k)) s += '.';
        else s += k + 0.5 >= r * split ? 'o' : 'r';
      }
      out.push(s);
    }
    return out;
  }
  const SUN = { o: P.orange, r: P.red };

  /* ------------------------------------------------------------ isometric */

  /*
   * Rasterizes an isometric scene into W x H cells of color keys. World
   * axes: X runs right and down the screen, Y left and down, Z up, so
   * a point projects to (X - Y, (X + Y) / 2 - Z), offset by (ox, oy).
   * Each face is a rectangle on a plane: {p: 'x' | 'y' | 'z', at, u: [lo,
   * hi], v: [lo, hi], c}, where (u, v) is (Y, Z) on an x plane, (X, Z) on a
   * y plane and (X, Y) on a z plane. c is a key, or a function of the point
   * relative to (u[0], v[0]) that returns one (or nothing, to leave the cell).
   * Faces paint in order, so list them back to front.
   */
  function isoScene(W, H, ox, oy, faces) {
    const g = Array.from({ length: H }, () => Array(W).fill('.'));
    for (let j = 0; j < H; j++) {
      for (let i = 0; i < W; i++) {
        const sx = i + 0.5 - ox, sy = j + 0.5 - oy;
        for (const f of faces) {
          let u, v;
          if (f.p === 'z') {
            const s = 2 * (sy + f.at);
            u = (sx + s) / 2;
            v = (s - sx) / 2;
          } else if (f.p === 'y') {
            u = sx + f.at;
            v = (u + f.at) / 2 - sy;
          } else {
            u = f.at - sx;
            v = (f.at + u) / 2 - sy;
          }
          if (u >= f.u[0] && u < f.u[1] && v >= f.v[0] && v < f.v[1]) {
            const k = typeof f.c === 'function' ? f.c(u - f.u[0], v - f.v[0]) : f.c;
            if (k) g[j][i] = k;
          }
        }
      }
    }
    return g;
  }

  // The three faces of a box you can see: its top, its front (the y plane
  // at y1, facing lower left) and its side (the x plane at x1, lower right).
  const box = (x0, x1, y0, y1, z0, z1, c) => [
    { p: 'z', at: z1, u: [x0, x1], v: [y0, y1], c: c.top },
    { p: 'y', at: y1, u: [x0, x1], v: [z0, z1], c: c.front },
    { p: 'x', at: x1, u: [y0, y1], v: [z0, z1], c: c.side },
  ];

  // Outlines a rasterized scene: every empty cell beside a filled one
  // becomes `k`. Returns rows of strings for OG.bitmap.
  function outlined(g, k) {
    const H = g.length, W = g[0].length;
    const solid = (i, j) => j >= 0 && j < H && i >= 0 && i < W && g[j][i] !== '.' && g[j][i] !== k;
    return g.map((row, j) => row.map((c, i) => {
      if (c !== '.') return c;
      return [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([di, dj]) => solid(i + di, j + dj)) ? k : '.';
    }).join(''));
  }

  // The cream plastic of a 90s case, lit from the upper left.
  const CREAM_BACK = { top: 'F', front: 'E', side: 'Q' };
  const ISO = {
    k: P.ink, T: '#FFF8E6', F: '#F0E7CF', E: '#E2D7BC', R: '#C9BB99', Q: '#B3A482',
    S: P.screen, s: '#0A1626', L: P.orange, l: P.peach, r: P.red,
    B: P.steel, b: P.steelDark, d: '#5E6F86',
    U: P.blueLight, V: P.blue, W: P.blueDeep, Z: '#2A6FB2',
    K: '#D9CDB0', N: '#FFFDF4',
  };

  // The RetrOS monitor in the round: a cream slab with the screen, the LED
  // at its bottom left and a steel foot, and the tube's housing behind.
  // (x, y) is the slab's left front corner on the floor, z its base height.
  function isoMonitor(x, y, z, o = {}) {
    const w = o.w || 24, h = o.h || 27, d = o.d || 7, bd = o.bd || 9, sun = !!o.sun;
    const yf = y, yb = y - d;
    const sx0 = 2, sx1 = w - 2, sz0 = Math.round(h * 0.4), sz1 = h - 2;
    const front = (u, v) => {
      if (v < 2.5) return v >= 2 ? 'b' : 'B'; // the foot, its top edge darker
      if (u >= sx0 && u < sx1 && v >= sz0 && v < sz1) {
        const su = u - sx0, sv = v - sz0;
        if (sun) {
          if (Math.hypot(su - 2, sv) < 6) return sv >= 2 ? 'L' : 'r';
        } else if (su < 4 && sv < 2) return sv >= 1.2 ? 'l' : 'L';
        if (sv > sz1 - sz0 - 1 || su < 1) return 's'; // the inner shadow along the top and left
        return 'S';
      }
      if (o.slot && v >= sz0 - 4 && v < sz0 - 3 && u >= w - 11 && u < w - 3) return 'd';
      return 'F';
    };
    return [
      ...box(x + 4, x + w - 4, yb - bd, yb, z + Math.round(h * 0.2), z + h - 8, CREAM_BACK),
      { p: 'z', at: z + h, u: [x, x + w], v: [yb, yf], c: (u, v) => (v > d - 1 ? 'N' : 'T') },
      { p: 'y', at: yf, u: [x, x + w], v: [z, z + h], c: front },
      { p: 'x', at: x + w, u: [yb, yf], v: [z, z + h], c: (u, v) => (v < 2.5 ? 'd' : 'R') },
    ];
  }

  /* ================================================== 101. Sunrise Screen */

  OG.add({
    num: 101,
    id: 'sunrise-screen',
    name: 'Sunrise Screen',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Safe',
    scene: false,
    bg: P.blue,
    ink: P.cream,
    palette: [P.blue, P.cream, P.screen, P.orange, P.red, P.steel],
    fonts: [],
    type: "The old logo's 6x9 name face, two pixels a stroke, set once on the chin. The sun is drawn on the same 6-pixel grid.",
    concept:
      "The RetrOS frame on the desktop blue, with the screen no longer spelling the name a second time. " +
      "The old logo's red LED moves to the bottom left and grows into a striped sun rising in that corner: the bright element and the screen's fill are now one thing, and a computer coming on reads as a sunrise. " +
      'Round two, from #46.',
    logo: (w) => {
      const m = monitor(123, 18, 234, 264, { screenH: 144 });
      const px = 6, cols = Math.floor(m.screen.w / px), rows = m.screen.h / px;
      let s = m.svg + OG.bitmap(sunRows(cols, rows, 13, [7, 4, 2, 1], 9.5), SUN, { x: m.screen.x, y: m.screen.y, px });
      const name = layout(w, MID, 1), nb = bbox(name), np = 5;
      const base = Math.min(...[...w].map((c) => (/[OGS]/.test(c) ? 0 : 2)));
      s += pixPath(name, { '#': P.ink }, { x: f2(240 - (nb.w * np) / 2 - nb.x0 * np), y: f2((m.chin.y0 + m.chin.y1) / 2 - ((9 + base) / 2) * np), px: np });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => {
      const m = monitor(4, 2, 56, 60, { line: 3, inset: 7, screenH: 32, foot: 6 });
      return OG.iconSvg(m.svg + OG.bitmap(sunRows(21, 16, 9, [4, 2, 1], 6.5), SUN, { x: m.screen.x, y: m.screen.y, px: 2 }), { crisp: true });
    },
  });

  /* ======================================================= 102. Widescreen */

  OG.add({
    num: 102,
    id: 'widescreen',
    name: 'Widescreen',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Safe',
    scene: false,
    bg: P.blue,
    ink: P.cream,
    palette: [P.blue, P.cream, P.screen, P.glyph, P.orange, P.steel],
    fonts: [],
    type: "The old logo's chunky 8x14 screen face, with an S drawn to match, at 8 screen pixels a pixel.",
    concept:
      'The RetrOS frame turned on its side, VGA-shaped, so the whole name fits on the screen and nothing has to be said twice. ' +
      'The orange LED sits in the bottom-left corner of the screen, and the chin shrinks to a strip above the foot. ' +
      'Round two, from #46.',
    logo: (w) => {
      const m = monitor(30, 40, 420, 222, { screenH: 150, foot: 16 });
      const rows = layout(w, BIG, 2), b = bbox(rows), px = 8;
      const mid = m.screen.y + m.screen.h / 2 + 2;
      const x = 240 - (b.w * px) / 2 - b.x0 * px, y = mid - ((b.y0 + BIG_BASE) / 2) * px;
      return OG.logoSvg(
        m.svg + pixPath(rows, { '#': P.glyph }, { x: f2(x), y: f2(y), px }) + rect(m.screen.x, m.screen.y + m.screen.h - 16, 32, 16, P.orange),
        { crisp: true }
      );
    },
    icon: () => {
      const m = monitor(2, 6, 60, 52, { line: 3, inset: 7, screenH: 32, foot: 5 });
      return OG.iconSvg(
        m.svg + pixPath(BIG.O.slice(0, 11), { '#': P.glyph }, { x: 22, y: 15.5, px: 2.5 }) +
          rect(m.screen.x, m.screen.y + m.screen.h - 5, 10, 5, P.orange),
        { crisp: true }
      );
    },
  });

  /* ==================================================== 103. Letter Blocks */

  const BLOCK_COLORS = [P.blue, P.orange, P.red, P.ink];

  // Each letter on a tile cut to its own height: capitals stand tall, small
  // letters sit low, and the g's tile drops below the line with its tail.
  // Returns the markup and the block's size. (x, y) is the top left of the
  // cap height; px is the size of a font pixel.
  function blocks(w, x, y, px, o = {}) {
    const pad = o.pad == null ? 2 : o.pad, gap = o.gap == null ? 1 : o.gap, cut = o.cut || P.paper;
    let s = '', cx = x;
    [...w].forEach((ch, n) => {
      const g = BIG[ch] || BIG[ch.toUpperCase()];
      const top = /[A-Z]/.test(ch) ? 0 : 3, bottom = ch === 'g' ? 14 : BIG_BASE;
      const tw0 = 8 + 2 * pad;
      s += rect(cx, y + (top - pad) * px, tw0 * px, (bottom - top + 2 * pad) * px, (o.colors || BLOCK_COLORS)[n % 4]);
      s += pixPath(g, { '#': cut }, { x: cx + pad * px, y, px });
      cx += (tw0 + gap) * px;
    });
    return { svg: s, w: cx - x - gap * px };
  }

  OG.add({
    num: 103,
    id: 'letter-blocks',
    name: 'Letter Blocks',
    caps: 'OgOs',
    flexCase: true,
    family: 'Geometric',
    sources: ['Print & packaging', 'Windows 9x'],
    paletteTag: 'Desktop Blue',
    risk: 'Bold',
    scene: false,
    bg: P.paper,
    ink: P.ink,
    palette: [P.paper, P.blue, P.orange, P.red, P.ink],
    fonts: [],
    type: "The old logo's chunky screen face, cut out of each tile in the paper color.",
    concept:
      "Four letters, four colored squares: #78's squares grown up into the name itself. " +
      'Each tile is cut to its letter, so capitals stand tall, small letters sit low and the g hangs below the line. ' +
      'The skyline changes with the case: OgOs, OGOS and ogos each make their own silhouette, which makes the case decision a visual one. ' +
      'Round two, from #1 and #78.',
    logo: (w) => {
      const px = 7;
      const probe = blocks(w, 0, 0, px);
      const caps = /[A-Z]/.test(w), tall = w.includes('g');
      const top = (caps ? 0 : 3) - 2, bottom = (tall ? 14 : BIG_BASE) + 2;
      const y = 150 - ((top + bottom) / 2) * px;
      return OG.logoSvg(blocks(w, 240 - probe.w / 2, y, px).svg, { crisp: true });
    },
    icon: () => {
      const b = blocks('Og', 0, 0, 2.5);
      return OG.iconSvg(blocks('Og', 32 - b.w / 2, 32 - 7.5 * 2.5, 2.5).svg, { crisp: true });
    },
  });

  /* ===================================================== 104. Iso Monitor */

  function isoMonitorRows() {
    const faces = [
      // The tile it stands on.
      ...box(-4, 28, -1, 25, -4, 0, { top: (u, v) => (u < 1 || v < 1 ? 'Z' : 'U'), front: 'V', side: 'W' }),
      ...isoMonitor(0, 20, 0, { slot: true }),
    ];
    return outlined(isoScene(66, 60, 33, 26, faces), 'k');
  }

  OG.add({
    num: 104,
    id: 'iso-monitor',
    name: 'Iso Monitor',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['BeOS', 'OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Bold',
    scene: false,
    bg: P.paper,
    ink: P.ink,
    palette: [P.cream, P.screen, P.orange, P.blue, P.ink],
    fonts: ['Saira:wght@800'],
    type: 'Saira ExtraBold in ink, square-shouldered and plain, as in #53, so the icon carries the color.',
    concept:
      "#53's bright isometric pixel style, with the random ring replaced by the thing the OS runs on: the RetrOS monitor itself, turned three-quarters, cream plastic with the tube's housing behind. " +
      'The orange LED sits at the bottom left of the screen, and it stands on a tile of the desktop blue. ' +
      'Round two, from #53 and #46.',
    logo: (w) => {
      const rows = isoMonitorRows(), px = 3.5, mx = 26, my = 46;
      let s = OG.bitmap(rows, ISO, { x: mx, y: my, px });
      const size = fit('saira', w, 88, 454 - (mx + 66 * px + 14));
      s += OG.text(w, { x: mx + 66 * px + 14, y: 178, size: f2(size), family: SAIRA, weight: 800, fill: P.ink });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(isoMonitorRows(), ISO, { x: -1, y: 2, px: 1 }), { crisp: true }),
  });

  /* ======================================================== 105. Desk Set */

  // A desktop case with the monitor on it and a keyboard in front, on a
  // shadow on the floor.
  function deskRows() {
    // Rows of keys, with darker grooves between them.
    const keys = (u, v) => (u < 1 || u > 26 || v < 1 || v > 7 ? 'T' : (v - 1) % 2 < 1 ? 'K' : 'N');
    const caseFront = (u, v) => {
      if (u >= 2 && u < 6 && v >= 1.5 && v < 4) return v >= 3.2 ? 'l' : 'L'; // the power light, bottom left
      if (u >= 17 && u < 27 && v >= 4 && v < 5) return 'd'; // the floppy slot
      if (u >= 24 && u < 26 && v >= 2 && v < 3) return 'b'; // its eject button
      return 'F';
    };
    const faces = [
      ...box(0, 32, 0, 24, 0, 8, { top: 'T', front: caseFront, side: 'R' }),
      ...isoMonitor(4, 18, 8, { w: 24, h: 25, d: 6, bd: 8, sun: true }),
      ...box(2, 29, 27, 35, 0, 2, { top: keys, front: 'F', side: 'R' }),
    ];
    return outlined(isoScene(68, 64, 35, 29, faces), 'k');
  }

  OG.add({
    num: 105,
    id: 'desk-set',
    name: 'Desk Set',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['BeOS', 'Home computers'],
    paletteTag: 'Desktop Blue',
    risk: 'Bold',
    scene: false,
    bg: P.blue,
    ink: P.paper,
    palette: [P.blue, P.cream, P.orange, P.red, P.screen, P.ink],
    fonts: ['Saira:wght@800'],
    type: 'Saira ExtraBold in cream with a hard ink shadow, sized to sit beside the icon.',
    concept:
      'A whole computer in the bright isometric style: a cream desktop case with a floppy slot and an orange power light at the bottom left, the monitor on top with the sun of #101 coming up in its corner, and a keyboard in front. ' +
      'It is the machine the OS is for, drawn the way a 90s desktop drew its own icons. ' +
      'Round two, from #53.',
    logo: (w) => {
      const rows = deskRows(), px = 3.4, mx = 22, my = 42;
      let s = OG.bitmap(rows, ISO, { x: mx, y: my, px });
      const tx = mx + 68 * px + 10, size = fit('saira', w, 84, 460 - tx);
      s += OG.text(w, { x: tx + 4, y: 186, size: f2(size), family: SAIRA, weight: 800, fill: P.ink });
      s += OG.text(w, { x: tx, y: 182, size: f2(size), family: SAIRA, weight: 800, fill: P.paper });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(deskRows(), ISO, { x: -2, y: -1, px: 1 }), { crisp: true }),
  });

  /* ================================================= 106. Monitor Patch */

  const SPACE = '#0C1C31';

  // A row of short diagonal stitches around a rounded rectangle, for the
  // merrowed edge of a patch.
  function merrowRect(x, y, w, h, r, t, color) {
    return `<rect x="${f2(x + t / 2)}" y="${f2(y + t / 2)}" width="${f2(w - t)}" height="${f2(h - t)}" rx="${f2(r - t / 2)}" fill="none" stroke="${color}" stroke-width="${f2(t)}" stroke-dasharray="1.4 2.2" opacity="0.5"/>`;
  }

  OG.add({
    num: 106,
    id: 'monitor-patch',
    name: 'Monitor Patch',
    caps: 'OGOS',
    flexCase: true,
    family: 'Emblem',
    sources: ['Beyond computing', 'OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Bold',
    scene: false,
    bg: '#2A303C',
    ink: P.cream,
    palette: [P.blue, P.cream, SPACE, P.red, P.orange, P.ink],
    fonts: ['Jost:wght@500;700'],
    type: 'Jost Bold, a Futura cut, set in widely tracked capitals as patches are; Jost Medium for the mission line.',
    concept:
      "A mission patch cut to the shape of the RetrOS frame, the way Apollo 8's was cut to its capsule. " +
      'The screen is a window on space: a red planet rising from the bottom-left corner, one clean orbit, and a single cream pixel riding it. ' +
      'The name is stitched on the chin and the mission on the foot. Flat thread colors and one line weight keep it grown-up. ' +
      'Round two, from #52 and #46.',
    logo: (w) => {
      const x = 134, y = 14, W = 212, H = 272, R = 24, b = 11;
      const ix = x + b, iy = y + b, iw = W - 2 * b, ih = H - 2 * b;
      const sx = ix + 14, sy = iy + 14, sw = iw - 28, sh = 140;
      const footH = 30, footY = iy + ih - footH;
      const track = 0.2, size = fit('jost', w, 40, iw - 40, track);
      const pcx = sx + 6, pcy = sy + sh + 46, pr = 104;
      const stars = [[0.62, 0.14, 1.6], [0.86, 0.3, 1.1], [0.4, 0.22, 1], [0.72, 0.52, 1.2], [0.9, 0.08, 1], [0.24, 0.1, 1.3], [0.52, 0.4, 0.9]];
      return OG.logoSvg(
        `<defs><clipPath id="mpf"><rect x="${ix}" y="${iy}" width="${iw}" height="${ih}" rx="${R - b}"/></clipPath>` +
          `<clipPath id="mps"><rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6"/></clipPath>` +
          `<filter id="mpd" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="3"/></filter></defs>` +
          `<rect x="${x + 2}" y="${y + 5}" width="${W}" height="${H}" rx="${R}" fill="#000" opacity="0.28" filter="url(#mpd)"/>` +
          `<rect x="${x}" y="${y}" width="${W}" height="${H}" rx="${R}" fill="${P.cream}"/>` +
          merrowRect(x + 1.5, y + 1.5, W - 3, H - 3, R - 1.5, b - 3, '#B8AE92') +
          `<g clip-path="url(#mpf)">` +
          rect(ix, iy, iw, ih, P.blue) +
          rect(ix, footY, iw, footH, P.ink) +
          `</g>` +
          `<rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6" fill="${SPACE}"/>` +
          `<g clip-path="url(#mps)">` +
          stars.map(([fx, fy, r]) => `<circle cx="${f2(sx + fx * sw)}" cy="${f2(sy + fy * sh)}" r="${r}" fill="${P.cream}" opacity="0.85"/>`).join('') +
          `<ellipse cx="${pcx}" cy="${pcy}" rx="${pr + 44}" ry="${pr - 30}" fill="none" stroke="${P.cream}" stroke-width="1.6" opacity="0.9" transform="rotate(-28 ${pcx} ${pcy})"/>` +
          `<circle cx="${pcx}" cy="${pcy}" r="${pr}" fill="${P.red}"/>` +
          `<circle cx="${pcx}" cy="${pcy}" r="${pr - 3}" fill="none" stroke="${P.orange}" stroke-width="6"/>` +
          `</g>` +
          rect(sx + sw - 52, sy + 26, 7, 7, P.cream) +
          `<rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6" fill="none" stroke="${P.cream}" stroke-width="2"/>` +
          OG.text(w, { x: 240 + (size * track) / 2, y: f2((sy + sh + footY) / 2 + size * 0.36), size: f2(size), family: JOST, weight: 700, fill: P.cream, anchor: 'middle', ls: f2(size * track) }) +
          OG.text('MISSION 32', { x: 242, y: footY + 19.5, size: 12, family: JOST, weight: 500, fill: P.steel, anchor: 'middle', ls: 4 })
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs><clipPath id="mpi"><rect x="12" y="10" width="40" height="32" rx="3"/></clipPath></defs>` +
          `<rect x="5" y="2" width="54" height="60" rx="9" fill="${P.cream}"/><rect x="8" y="5" width="48" height="54" rx="6" fill="${P.blue}"/>` +
          `<rect x="8" y="49" width="48" height="10" fill="${P.ink}"/><rect x="5" y="56" width="54" height="6" fill="${P.cream}"/>` +
          `<rect x="12" y="10" width="40" height="32" rx="3" fill="${SPACE}"/>` +
          `<g clip-path="url(#mpi)"><ellipse cx="13" cy="54" rx="44" ry="22" fill="none" stroke="${P.cream}" stroke-width="1.2" transform="rotate(-28 13 54)"/><circle cx="13" cy="54" r="27" fill="${P.red}"/><circle cx="13" cy="54" r="26" fill="none" stroke="${P.orange}" stroke-width="2.5"/></g>` +
          `<rect x="38" y="17" width="3" height="3" fill="${P.cream}"/>` +
          `<rect x="12" y="10" width="40" height="32" rx="3" fill="none" stroke="${P.cream}" stroke-width="1"/>`
      ),
  });

  /* ==================================================== 107. Round Patch */

  OG.add({
    num: 107,
    id: 'round-patch',
    name: 'Round Patch',
    caps: 'OGOS',
    flexCase: true,
    family: 'Emblem',
    sources: ['Beyond computing'],
    paletteTag: 'Desktop Blue',
    risk: 'Safe',
    scene: false,
    bg: '#2A303C',
    ink: P.cream,
    palette: [P.blue, P.cream, SPACE, P.orange, P.red, P.ink],
    fonts: ['Jost:wght@500;700'],
    type: 'Jost Bold capitals around the band, tracked wide; the mission line in Jost Medium.',
    concept:
      "#52's patch, redrawn as a crew would actually wear it: a cream edge, the name around a blue band, and in the middle the RetrOS monitor drawn flat, hanging over a planet's limb as the sun comes up behind it at the lower left. " +
      'No twill, no cartoon stars, no orbit through the screen; two orange dots mark the band. ' +
      'Round two, from #52.',
    logo: (w) => {
      const cx = 240, cy = 150, R = 136, band = 36, fr = R - 12 - band;
      const mid = R - 12 - band / 2;
      const track = 0.24, size = Math.min(27, (Math.PI * mid * 0.62) / tw('jost', w, 1, track));
      const rt = mid - size * 0.36, rb = mid + 12 * 0.36;
      const mw = 84, mh = 92, mx = cx - mw / 2, my = cy - mh / 2 - 12;
      const m = monitor(mx, my, mw, mh, { line: 3, inset: 11, screenH: 46, foot: 9, colors: { screen: '#0A1628' } });
      const lcx = cx + 30, lcy = cy + 240, lr = 196;
      return OG.logoSvg(
        `<defs><path id="rpt" d="M${cx - rt} ${cy}A${rt} ${rt} 0 0 1 ${cx + rt} ${cy}"/><path id="rpb" d="M${cx - rb} ${cy}A${rb} ${rb} 0 0 0 ${cx + rb} ${cy}"/>` +
          `<clipPath id="rpc"><circle cx="${cx}" cy="${cy}" r="${fr}"/></clipPath>` +
          `<linearGradient id="rpg" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="${P.orange}"/><stop offset="0.55" stop-color="${P.orange}" stop-opacity="0"/></linearGradient>` +
          `<filter id="rpd" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="3"/></filter></defs>` +
          `<circle cx="${cx + 2}" cy="${cy + 5}" r="${R}" fill="#000" opacity="0.28" filter="url(#rpd)"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${R}" fill="${P.cream}"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${R - 5}" fill="none" stroke="#B8AE92" stroke-width="7" stroke-dasharray="1.4 2.2" opacity="0.5"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${R - 12}" fill="${P.blue}"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${fr}" fill="${SPACE}"/>` +
          `<g clip-path="url(#rpc)">` +
          [[-46, -58, 1.3], [52, -52, 1], [-66, -10, 1], [64, -6, 1.2], [20, -70, 0.9], [-24, -72, 1.1]]
            .map(([dx, dy, r]) => `<circle cx="${cx + dx}" cy="${cy + dy}" r="${r}" fill="${P.cream}" opacity="0.8"/>`).join('') +
          `<circle cx="${cx - 52}" cy="${cy + 52}" r="16" fill="${P.orange}"/>` +
          `<circle cx="${lcx}" cy="${lcy}" r="${lr}" fill="${P.blueDeep}"/>` +
          `<circle cx="${lcx}" cy="${lcy}" r="${lr - 1.5}" fill="none" stroke="url(#rpg)" stroke-width="3"/>` +
          `</g>` +
          `<circle cx="${cx}" cy="${cy}" r="${fr}" fill="none" stroke="${P.cream}" stroke-width="2"/>` +
          m.svg + rect(m.screen.x, m.screen.y + m.screen.h - 6, 12, 6, P.orange) +
          `<text font-size="${f2(size)}" font-family="${JOST}" font-weight="700" fill="${P.cream}" letter-spacing="${f2(size * track)}" text-anchor="middle"><textPath href="#rpt" startOffset="50%">${OG.esc(w)}</textPath></text>` +
          `<text font-size="12" font-family="${JOST}" font-weight="500" fill="${P.sky}" letter-spacing="4" text-anchor="middle"><textPath href="#rpb" startOffset="50%">MISSION 32</textPath></text>` +
          `<circle cx="${cx - mid}" cy="${cy}" r="4" fill="${P.orange}"/><circle cx="${cx + mid}" cy="${cy}" r="4" fill="${P.orange}"/>`
      );
    },
    icon: () => {
      const m = monitor(21, 18, 22, 24, { line: 0.01, inset: 3, screenH: 12, foot: 3, colors: { line: P.cream, screen: '#0A1628' } });
      return OG.iconSvg(
        `<defs><clipPath id="rpi"><circle cx="32" cy="32" r="20"/></clipPath></defs>` +
          `<circle cx="32" cy="32" r="31" fill="${P.cream}"/><circle cx="32" cy="32" r="28" fill="${P.blue}"/><circle cx="32" cy="32" r="20" fill="${SPACE}"/>` +
          `<g clip-path="url(#rpi)"><circle cx="20" cy="45" r="5" fill="${P.orange}"/><circle cx="38" cy="88" r="44" fill="${P.blueDeep}"/></g>` +
          `<circle cx="32" cy="32" r="20" fill="none" stroke="${P.cream}" stroke-width="1"/>` +
          m.svg + rect(m.screen.x, m.screen.y + m.screen.h - 2, 4, 2, P.orange) +
          `<circle cx="8" cy="32" r="1.8" fill="${P.orange}"/><circle cx="56" cy="32" r="1.8" fill="${P.orange}"/>`
      );
    },
  });

  /* ===================================================== 108. My Computer */

  // A 32x32 desktop icon of the RetrOS monitor, 9x style: beveled cream
  // plastic, the sunrise on its screen and a floppy slot on the chin.
  function computerIcon() {
    const g = Array.from({ length: 32 }, () => Array(32).fill('.'));
    const fill = (x0, y0, x1, y1, c) => { for (let j = y0; j <= y1; j++) for (let i = x0; i <= x1; i++) g[j][i] = c; };
    fill(2, 1, 29, 29, 'k');
    fill(3, 2, 28, 28, 'c');
    fill(3, 2, 27, 2, 'w'); fill(3, 2, 3, 27, 'w');
    fill(28, 3, 28, 28, 'd'); fill(4, 28, 28, 28, 'd');
    fill(5, 4, 26, 18, 'k');
    fill(6, 5, 26, 18, 's');
    fill(6, 5, 26, 5, 'q'); fill(6, 5, 6, 18, 'q');
    // The sun in the bottom-left corner of the screen.
    for (let j = 6; j <= 18; j++) {
      for (let i = 7; i <= 26; i++) {
        const k = 18 - j, d = Math.hypot(i - 10.5, k + 0.5);
        if (d <= 7.6 && k !== 1 && k !== 3) g[j][i] = k >= 4 ? 'o' : 'r';
      }
    }
    fill(5, 19, 26, 19, 'w');
    fill(18, 22, 25, 22, 'k'); fill(18, 23, 25, 23, 'w');
    fill(3, 25, 27, 27, 't'); fill(3, 25, 27, 25, 'T');
    fill(28, 25, 28, 28, 'T');
    return g.map((r) => r.join(''));
  }
  const ICON_MAP = { k: P.ink, c: P.cream, w: P.white, d: '#B9AE90', s: P.screen, q: '#060E1A', o: P.orange, r: P.red, t: P.steel, T: P.steelDark };

  // The 9x arrow pointer.
  const POINTER = [
    'k...........', 'kk..........', 'kwk.........', 'kwwk........', 'kwwwk.......', 'kwwwwk......',
    'kwwwwwk.....', 'kwwwwwwk....', 'kwwwwwwwk...', 'kwwwwwwwwk..', 'kwwwwwwwwwk.', 'kwwwwwwkkkkk',
    'kwwwkwwk....', 'kwwkkwwk....', 'kwk..kwwk...', 'kk...kwwk...', 'k.....kwwk..', '......kwwk..', '.......kk...',
  ];

  OG.add({
    num: 108,
    id: 'my-computer',
    name: 'My Computer',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Windows 9x', 'OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Safe',
    scene: false,
    bg: P.blue,
    ink: P.white,
    palette: [P.blue, P.cream, P.red, P.orange, P.screen, P.ink],
    fonts: [],
    type: "The kernel's own 8x8 font (lib/font8.c), spaced proportionally at 3 screen pixels a pixel, as a desktop icon's label.",
    concept:
      "The logo is the one icon on a fresh desktop, just clicked: a 32x32 RetrOS monitor with the sunrise on its screen, its label selected in red with the dotted focus ring, and the pointer still on it. " +
      "It keeps #03's 9x feel but tells a story: the first thing you do on OgOs is open the computer. " +
      'Round two, from #03.',
    logo: (w) => {
      const px = 5, ix = 240 - 80, iy = 22;
      let s = OG.bitmap(computerIcon(), ICON_MAP, { x: ix, y: iy, px });
      const rows = OG.textRows(w, { prop: true }), lp = 3;
      const lw = rows[0].length * lp, bw = lw + 8 * lp, bh = 8 * lp + 6 * lp;
      const bx = Math.round(240 - bw / 2), by = iy + 32 * px + 12;
      s += rect(bx, by, bw, bh, P.red);
      // The dotted focus ring, one pixel in from the edge.
      let dots = '';
      for (let i = 0; i < bw / lp; i += 2) dots += rect(bx + i * lp, by, lp, lp, P.white) + rect(bx + i * lp, by + bh - lp, lp, lp, P.white);
      for (let j = 2; j < bh / lp - 1; j += 2) dots += rect(bx, by + j * lp, lp, lp, P.white) + rect(bx + bw - lp, by + j * lp, lp, lp, P.white);
      s += dots;
      s += OG.bitmap(rows, { '#': P.white }, { x: bx + 4 * lp, y: by + 3 * lp, px: lp });
      s += OG.bitmap(POINTER, { k: P.ink, w: P.white }, { x: ix + 25 * px, y: iy + 22 * px, px: 3 });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(computerIcon(), ICON_MAP, { x: 0, y: 0, px: 2 }), { crisp: true }),
  });

  /* ========================================================== 109. Button */

  // The 88x31 badge, at `px` screen pixels a badge pixel.
  function badge(x, y, px, w) {
    const r = (bx, by, bw, bh, c) => rect(x + bx * px, y + by * px, bw * px, bh * px, c);
    let s = r(0, 0, 88, 31, P.ink);
    // The icon well: desktop blue, beveled.
    s += r(1, 1, 29, 29, P.blue) + r(1, 1, 29, 1, '#4E86C2') + r(1, 1, 1, 29, '#4E86C2') + r(1, 29, 29, 1, P.blueDeep) + r(29, 1, 1, 29, P.blueDeep);
    // The monitor, 19 by 21, with its LED at the bottom left of the screen.
    s += r(6, 5, 19, 21, P.ink) + r(7, 6, 17, 19, P.cream) + r(9, 8, 13, 10, P.screen) + r(9, 16, 4, 2, P.orange);
    s += r(16, 20, 6, 1, P.ink) + r(7, 22, 17, 3, P.steel) + r(7, 22, 17, 1, P.steelDark);
    // Two stripes between the well and the panel.
    s += r(30, 1, 1, 29, P.red) + r(31, 1, 1, 29, P.orange);
    // The panel: cream, beveled.
    s += r(32, 1, 55, 29, P.cream) + r(32, 1, 55, 1, P.white) + r(32, 1, 1, 29, P.white) + r(32, 29, 55, 1, '#B9B29C') + r(86, 1, 1, 29, '#B9B29C');
    // The name in the old logo's name face, and the small print.
    const rows = layout(w, MID, 1), nb = bbox(rows);
    const nx = 32 + Math.floor((55 - nb.w) / 2) - nb.x0;
    const ny = /[A-Z]/.test(w) ? 6 : 4;
    s += OG.bitmap(rows, { '#': P.ink }, { x: x + nx * px, y: y + ny * px, px });
    const micro = layout('32-BIT OS', MICRO, 1), mw = micro[0].length;
    s += OG.bitmap(micro, { '#': P.red }, { x: x + (32 + Math.floor((55 - mw) / 2)) * px, y: y + 21 * px, px });
    return s;
  }

  OG.add({
    num: 109,
    id: 'button',
    name: 'Button',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['Demoscene & early web', 'OgOs internals'],
    paletteTag: 'Desktop Blue',
    risk: 'Safe',
    scene: false,
    bg: P.paper,
    ink: P.ink,
    palette: [P.blue, P.cream, P.red, P.orange, P.screen, P.ink],
    fonts: [],
    type: "Drawn at 88 by 31 pixels: the name in the old logo's 6x9 name face, the small print in a 3x5 micro face.",
    concept:
      "#60's idea, one badge instead of three: an 88x31 button whose left half is the RetrOS monitor on the desktop blue, LED at the bottom left, and whose right half is the name. " +
      'Shown big, then at the sizes it is used, where it stays crisp because it was drawn there. ' +
      'Round two, from #60.',
    logo: (w) =>
      OG.logoSvg(
        badge(64, 36, 4, w) +
          badge(64, 196, 2, w) +
          badge(272, 211, 1, w) +
          OG.pixText('2x', { x: 64, y: 266, px: 1.5, fill: P.steelDark }) +
          OG.pixText('1x', { x: 272, y: 250, px: 1.5, fill: P.steelDark }),
        { crisp: true }
      ),
    icon: () =>
      OG.iconSvg(
        rect(0, 0, 64, 64, P.ink) + rect(2, 2, 60, 60, P.blue) + rect(2, 2, 60, 2, '#4E86C2') + rect(2, 2, 2, 60, '#4E86C2') +
          rect(2, 60, 60, 2, P.blueDeep) + rect(60, 2, 2, 60, P.blueDeep) +
          rect(12, 10, 40, 44, P.ink) + rect(14, 12, 36, 40, P.cream) + rect(18, 16, 28, 20, P.screen) + rect(18, 32, 8, 4, P.orange) +
          rect(32, 40, 12, 2, P.ink) + rect(14, 44, 36, 8, P.steel) + rect(14, 44, 36, 2, P.steelDark),
        { crisp: true }
      ),
  });

  /* =================================================== 110. Racing Stripes */

  const STRIPES = [P.cream, P.orange, P.red, P.sky];

  // Four stripes running in from the left, breaking into loose squares at
  // their tails. (x1, y) is where they end and the top of the first.
  function stripes(x1, y, h, gap, seed, sq) {
    const rnd = OG.rng(seed);
    let s = '';
    STRIPES.forEach((c, n) => {
      const sy = y + n * (h + gap), start = 70 + n * 18;
      s += rect(start, sy, x1 - start, h, c);
      // The tail: squares the stripe's height, thinning out to the left.
      for (let k = 1; k <= 4; k++) {
        if (k > 1 && rnd() < 0.25) continue;
        s += rect(start - k * (sq + sq * 0.6), sy + (h - sq) / 2, sq, sq, c, `opacity="${f2(1 - k * 0.2)}"`);
      }
    });
    return s;
  }

  OG.add({
    num: 110,
    id: 'racing-stripes',
    name: 'Racing Stripes',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Home computers', 'Windows 9x'],
    paletteTag: 'Desktop Blue',
    risk: 'Bold',
    scene: false,
    bg: P.blue,
    ink: P.paper,
    palette: [P.blue, P.cream, P.orange, P.red, P.sky, P.ink],
    fonts: ['Archivo:ital,wght@1,900'],
    type: "Archivo Black Italic with #1's hard one-step ink shadow.",
    concept:
      "#1's energy without the flag: the mark coming in fast is the RetrOS monitor itself, leaning forward, trailing four stripes in the brand's colors that break into pixels at their tails like a late-80s livery. " +
      'Round two, from #1 and #78.',
    logo: (w) => {
      const mx = 146, my = 84, mw = 104, mh = 116;
      const m = monitor(mx, my, mw, mh, { line: 5, inset: 13, screenH: 58, foot: 13 });
      const skew = (inner) => `<g transform="translate(0 ${my + mh / 2}) skewX(-12) translate(0 ${-(my + mh / 2)})">${inner}</g>`;
      const size = fit('archivo', w, 84, 184), tx = 268, base = 182;
      return OG.logoSvg(
        skew(stripes(mx + 10, my + 16, 14, 6, 7, 10) + m.svg + rect(m.screen.x, m.screen.y + m.screen.h - 8, 16, 8, P.orange)) +
          OG.text(w, { x: tx + 5, y: base + 5, size: f2(size), family: ARCHIVO, weight: 900, italic: true, fill: P.ink, ls: -1 }) +
          OG.text(w, { x: tx, y: base, size: f2(size), family: ARCHIVO, weight: 900, italic: true, fill: P.paper, ls: -1 })
      );
    },
    icon: () => {
      const m = monitor(20, 12, 36, 40, { line: 2.5, inset: 6, screenH: 18, foot: 5 });
      let s = '';
      STRIPES.forEach((c, n) => (s += rect(2 + n * 3, 18 + n * 6, 22 - n * 3, 4, c)));
      return OG.iconSvg(`<g transform="translate(0 32) skewX(-12) translate(0 -32)">${s + m.svg + rect(m.screen.x, m.screen.y + m.screen.h - 4, 7, 4, P.orange)}</g>`);
    },
  });
})();
