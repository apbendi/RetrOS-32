/* Batch E: Beyond computing. Directions 7, 12, 17 ... 97. */
(function () {
  const { C } = OG;

  /* ---------------------------------------------------------------- helpers */

  const f = (n) => +n.toFixed(2);
  const pathOf = (pts, close = true) =>
    pts.map((p, i) => (i ? 'L' : 'M') + f(p[0]) + ' ' + f(p[1])).join('') + (close ? 'Z' : '');

  // A closed path through points, smoothed with quadratic curves between midpoints.
  function smoothClosed(pts) {
    const n = pts.length;
    const mid = (a, b) => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
    let d = 'M' + mid(pts[n - 1], pts[0]).map(f).join(' ');
    for (let i = 0; i < n; i++) {
      const p = pts[i], m = mid(p, pts[(i + 1) % n]);
      d += `Q${f(p[0])} ${f(p[1])} ${f(m[0])} ${f(m[1])}`;
    }
    return d + 'Z';
  }

  // Points around a rounded rectangle, each pushed in or out a little at random:
  // a rough stone or a hand-cut shape.
  function roughRect(x, y, w, h, r, jitter, seed, step = 14) {
    const rnd = OG.rng(seed), pts = [];
    const corners = [[x + w - r, y + r, -90], [x + w - r, y + h - r, 0], [x + r, y + h - r, 90], [x + r, y + r, 180]];
    const edge = (ax, ay, bx, by) => {
      const len = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.round(len / step));
      for (let i = 0; i < n; i++) {
        const t = i / n, nx = (by - ay) / len, ny = -(bx - ax) / len, j = (rnd() - 0.5) * 2 * jitter;
        pts.push([ax + (bx - ax) * t + nx * j, ay + (by - ay) * t + ny * j]);
      }
    };
    edge(x + r, y, x + w - r, y);
    corners.forEach(([cx, cy, a0], k) => {
      for (let i = 0; i < 3; i++) {
        const a = ((a0 + i * 30) * Math.PI) / 180, j = (rnd() - 0.5) * 2 * jitter;
        pts.push([cx + Math.cos(a) * (r + j), cy + Math.sin(a) * (r + j)]);
      }
      const nxt = [[x + w, y + r, x + w, y + h - r], [x + w - r, y + h, x + r, y + h], [x, y + h - r, x, y + r], null][k];
      if (nxt) edge(...nxt);
    });
    return pts;
  }

  // Scales a bitmap up by k in both directions.
  const upscale = (rows, k) => rows.flatMap((r) => Array(k).fill([...r].map((c) => c.repeat(k)).join('')));

  // Lays glyph bitmaps (all the same height) side by side with `gap` blank columns.
  function setRows(glyphs, gap) {
    const h = glyphs[0].length;
    const rows = Array(h).fill('');
    glyphs.forEach((g, n) => {
      for (let r = 0; r < h; r++) rows[r] += (n ? '.'.repeat(gap) : '') + g[r];
    });
    return rows;
  }

  // Centers each row string in a field `w` wide (for symmetric sprites).
  const centerRows = (rows, w) =>
    rows.map((r) => {
      const l = Math.floor((w - r.length) / 2);
      return '.'.repeat(l) + r + '.'.repeat(w - r.length - l);
    });

  // A noise texture filter: dark or light grain at a given strength.
  const grain = (id, freq, seed, dark, alpha) =>
    `<filter id="${id}" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="${freq}" numOctaves="3" seed="${seed}"/>` +
    `<feColorMatrix values="0 0 0 0 ${dark ? 0 : 1}  0 0 0 0 ${dark ? 0 : 1}  0 0 0 0 ${dark ? 0 : 1}  ${alpha} 0 0 0 ${-alpha / 2}"/></filter>`;

  // Advance widths at 100 px for the letters of the name, measured in Chrome,
  // so a lockup can size any capitalization of the name to a width.
  const ADV = {
    Sacramento: { O: 77, G: 87, S: 82, o: 32, g: 30, s: 25, ' ': 20 },
    'Sedgwick Ave Display': { O: 55, G: 59, S: 62, o: 48, g: 50, s: 53, ' ': 32 },
    Anton: { O: 49, G: 48, S: 46, o: 50, g: 50, s: 47, ' ': 21 },
    'Alfa Slab One': { O: 79, G: 78, S: 67, o: 63, g: 65, s: 52, ' ': 30 },
    'Climate Crisis': { O: 112, G: 113, S: 108, o: 92, g: 91, s: 77, ' ': 20 },
    Righteous: { O: 78, G: 67, S: 61, o: 58, g: 61, s: 52, ' ': 28 },
    'Fjalla One': { O: 52, G: 50, S: 46, o: 46, g: 48, s: 40, ' ': 22 },
    'Inter Tight 800': { O: 76, G: 74, S: 63, o: 59, g: 61, s: 55, ' ': 20 },
    Cinzel: { O: 88, G: 84, S: 58, o: 78, g: 73, s: 52, ' ': 25 },
    'Inter Tight': { O: 75, G: 73, S: 62, o: 58, g: 60, s: 53, ' ': 20 },
    'IBM Plex Sans': { O: 69, G: 69, S: 58, o: 54, g: 53, s: 49, ' ': 24 },
    Jost: { O: 82, G: 81, S: 63, o: 59, g: 61, s: 46, ' ': 25 },
  };
  const advance = (font, w) => [...w].reduce((a, ch) => a + (ADV[font][ch] || 60), 0) / 100;
  // The font size that sets w at most `width` wide, and no bigger than `max`.
  const fitSize = (font, w, width, max) => Math.min(max, width / advance(font, w));

  /* 7. Ogham ------------------------------------------------------------ */

  // Each letter's strokes, in units along a stem at y = 0 (y grows downward).
  // Vowels cross the stem with short strokes, the M group with long
  // diagonals, and the B group hangs strokes below it.
  const OGHAM = {
    O: { w: 10, lines: [[0, -14, 0, 14], [10, -14, 10, 14]] },
    G: { w: 26, lines: [[0, 24, 12, -24], [14, 24, 26, -24]] },
    S: { w: 30, lines: [[0, 0, 0, 25], [10, 0, 10, 25], [20, 0, 20, 25], [30, 0, 30, 25]] },
  };

  // Lays out ᚛ letters ᚜ and returns its grooves as line segments, the
  // letter centers, and the total width, in units.
  function oghamLayout(word) {
    const lines = [], centers = [];
    let x = 0;
    lines.push([x, -13, x + 15, 0], [x, 13, x + 15, 0]);
    const stem0 = x + 15;
    x += 15 + 16;
    [...word.toUpperCase()].forEach((ch, i) => {
      const g = OGHAM[ch] || OGHAM.O;
      if (i) x += 22;
      g.lines.forEach(([a, b, c, d]) => lines.push([x + a, b, x + c, d]));
      centers.push([x + g.w / 2, ch]);
      x += g.w;
    });
    x += 16;
    const stem1 = x;
    lines.push([x + 15, -13, x, 0], [x + 15, 13, x, 0]);
    lines.unshift([stem0 - 15, 0, stem1 + 15, 0]);
    return { lines, centers, width: x + 15 };
  }

  // A carved groove: a lit lower lip, a dark cut, and its floor.
  function grooves(lines, sw) {
    const seg = (l, dx, dy, color, w) =>
      `<line x1="${f(l[0] + dx)}" y1="${f(l[1] + dy)}" x2="${f(l[2] + dx)}" y2="${f(l[3] + dy)}" stroke="${color}" stroke-width="${f(w)}" stroke-linecap="round"/>`;
    return (
      lines.map((l) => seg(l, sw * 0.28, sw * 0.28, '#D6D6CC', sw)).join('') +
      lines.map((l) => seg(l, 0, 0, '#2A2D29', sw)).join('') +
      lines.map((l) => seg(l, sw * 0.18, sw * 0.2, '#555A52', sw * 0.42)).join('')
    );
  }

  // Lichen: little clusters of yellow-green and orange dots near a shape's edge.
  function lichen(pts, seed, n, skip) {
    const rnd = OG.rng(seed);
    const cols = ['#C8CB78', '#AEB45C', '#D9A548', '#E2D99A'];
    let out = '';
    for (let i = 0; i < n; i++) {
      const p = pts[Math.floor(rnd() * pts.length)];
      const cx = p[0] + (rnd() - 0.5) * 30, cy = p[1] + (rnd() - 0.5) * 30;
      if (skip && skip(cx, cy)) continue;
      const col = cols[Math.floor(rnd() * cols.length)];
      for (let k = 0; k < 4; k++) {
        const r = 1 + rnd() * 3.2;
        out += `<circle cx="${f(cx + (rnd() - 0.5) * 10)}" cy="${f(cy + (rnd() - 0.5) * 8)}" r="${f(r)}" fill="${col}" opacity="${f(0.55 + rnd() * 0.4)}"/>`;
      }
    }
    return out;
  }

  function stone(id, pts) {
    const d = smoothClosed(pts);
    return (
      `<defs><linearGradient id="${id}g" x1="0" y1="0" x2="0.4" y2="1"><stop offset="0" stop-color="#9EA19A"/><stop offset="1" stop-color="#6C706A"/></linearGradient>` +
      grain(id + 'n', 0.75, 4, true, 1.6) + grain(id + 'm', 0.5, 9, false, 1.1) +
      `<clipPath id="${id}c"><path d="${d}"/></clipPath></defs>` +
      `<path d="${d}" fill="url(#${id}g)"/>` +
      `<filter id="${id}b"><feGaussianBlur stdDeviation="1.6"/></filter>` +
      `<g clip-path="url(#${id}c)"><rect width="480" height="300" filter="url(#${id}n)" opacity="0.35"/><rect width="480" height="300" filter="url(#${id}m)" opacity="0.22"/>` +
      `<g filter="url(#${id}b)"><path d="${d}" fill="none" stroke="#D3D6CC" stroke-width="4" transform="translate(2.5 3)" opacity="0.8"/><path d="${d}" fill="none" stroke="#3E423C" stroke-width="5" transform="translate(-3 -3.5)" opacity="0.7"/></g></g>` +
      `<path d="${d}" fill="none" stroke="#3E423C" stroke-width="2.5"/>`
    );
  }

  OG.add({
    num: 7,
    id: 'ogham',
    name: 'Ogham',
    caps: 'OGOS',
    flexCase: false,
    family: 'Experimental',
    sources: ['Beyond computing'],
    paletteTag: 'Earthy',
    risk: 'Wild',
    bg: '#25302B',
    ink: '#E9E3CF',
    palette: ['#25302B', '#868A82', '#2A2D29', '#C8CB78', '#D9A548'],
    fonts: ['Marcellus SC'],
    type: 'The name cut in Ogham strokes along a stem, between its feather marks, with each letter transliterated beneath in Marcellus SC.',
    concept:
      'OGOS carved in Ogham, the stroke alphabet of early Irish standing stones: two notches for O, two long diagonals for G, four strokes for S. ' +
      'Ogham is a binary-looking code of marks and gaps, close kin to a barcode or a row of pixels, and the name is old enough to deserve it.',
    logo: () => {
      const L = oghamLayout('OGOS'), u = 1.62, x0 = 240 - (L.width * u) / 2, y0 = 142;
      const lines = L.lines.map(([a, b, c, d]) => [x0 + a * u, y0 + b * u, x0 + c * u, y0 + d * u]);
      const pts = roughRect(28, 82, 424, 124, 30, 3.2, 11, 16);
      const latin = L.centers
        .map(([cx, ch]) => OG.text(ch, { x: f(x0 + cx * u), y: 252, size: 24, family: "'Marcellus SC', serif", fill: '#E9E3CF', anchor: 'middle' }))
        .join('');
      const ticks = L.centers.map(([cx]) => `<rect x="${f(x0 + cx * u - 0.75)}" y="216" width="1.5" height="10" fill="#7E8A80"/>`).join('');
      return OG.logoSvg(
        `<ellipse cx="244" cy="212" rx="200" ry="10" fill="#000" opacity="0.28"/>` +
          stone('st', pts) +
          `<g clip-path="url(#stc)">${lichen(pts, 21, 40, (x, y) => Math.abs(y - y0) < 48 && x > x0 - 8 && x < x0 + L.width * u + 8)}</g>` +
          grooves(lines, 7.2) +
          ticks +
          latin
      );
    },
    icon: () => {
      const pts = roughRect(14, 3, 36, 58, 12, 1.4, 5, 8);
      // The stem runs up the stone, as on a real one: O, then G.
      const x = 32, s = [
        [x, 56, x, 8],
        [x - 7, 50, x + 7, 50], [x - 7, 43, x + 7, 43],
        [x - 9, 33, x + 9, 25], [x - 9, 24, x + 9, 16],
      ];
      return OG.iconSvg(stone('si', pts) + grooves(s, 4.2));
    },
  });

  /* 12. Og --------------------------------------------------------------- */

  const OG_PAL = {
    k: '#2A1A10', h: '#7A4420', H: '#A2622C', b: '#4A2410', s: '#EBA877', S: '#C47F52', w: '#FFFFFF', e: '#2A1A10',
    m: '#7A1E14', t: '#FFF4E0', l: '#E9A23B', L: '#6A3A14', g: '#CFCAB9', G: '#A29C8C', D: '#6E695F', n: '#E4DFD0',
  };

  // Og's head, 16 pixels wide: a mop of hair, one brow, a big grin, a beard.
  const OG_HEAD = centerRows([
    'kkkkkkkk',
    'kkhhHhhHhhkk',
    'khhHhhhhhhHhhk',
    'khhhhhHhhhhhhhhk',
    'khhhhhhhhhhhhhhk',
    'khhsssssssssshhk',
    'khsbbbbssbbbbshk',
    'khswesssssswesHk',
    'khssssssSssssshk',
    'khssssSSSSssssHk',
    'kbbssssssssssbbk',
    'khhkttttttttkhhk',
    'khhhkmmmmmmkhhhk',
    '.khhhkkkkkkhhhk.',
    '..khhhhhhhhhhk..',
    '...kkhhhhhhkk...',
  ], 22);

  // The rest of him: a leopard tunic, and a stone tablet cut like a floppy disk.
  const OG_BODY = [
    '..kkllLllllllLlsssk...',
    '.kllllllLlllllssssk...',
    '.ksskkkkkkkkkkkkk.sk..',
    '.ksskgGGDDDDDDGGk.sk..',
    '.ksskgGGDnnDDDGGGksk..',
    '.ksskgGGDnnDDDGGGksk..',
    '.ksskgGGDDDDDDGGGksk..',
    '.kssssGGGGGGGGGGssssk.',
    'kssssgnnnnnnnnnGssssk.',
    'kssssgnDDDnDDnnGssssk.',
    '.kkkkgnnnnnnnnnGkkkk..',
    '....kgnDDnDDDnnGk.....',
    '....kgnnnnnnnnnGk.....',
    '....kgGGGGGGGGGDk.....',
    '....kkkkkkkkkkkkk.....',
    '.....klLllllLllk......',
    '.....kLlkllkllLk......',
    '......kssk.kssk.......',
    '......kssk.kssk.......',
    '.....ksssk.ksssk......',
    '....kkkkkk.kkkkkk.....',
  ];

  // Pixel letters for the slab, five pixels wide; each is doubled and cut
  // as a V-groove, so every stroke has a shadowed and a lit half.
  const OG_GLYPHS = {
    O: ['.####.', '#....#', '#....#', '#....#', '#....#', '#....#', '.####.', '......', '......'],
    G: ['.####.', '#....#', '#.....', '#..###', '#....#', '#....#', '.####.', '......', '......'],
    S: ['.####.', '#....#', '#.....', '.####.', '.....#', '#....#', '.####.', '......', '......'],
    o: ['......', '......', '.####.', '#....#', '#....#', '#....#', '.####.', '......', '......'],
    g: ['......', '......', '.#####', '#....#', '#....#', '#....#', '.#####', '.....#', '.####.'],
    s: ['......', '......', '.#####', '#.....', '.####.', '.....#', '#####.', '......', '......'],
  };


  // Marks each set pixel by which edge it is on: 'x' shadowed (open above or
  // to the left), 'y' lit (open below or to the right), 'z' the floor; and
  // 'l' for the stone just below or right of a cut, the lip that catches light.
  function chisel(rows) {
    const at = (r, c) => r >= 0 && r < rows.length && c >= 0 && c < rows[r].length && rows[r][c] === '#';
    return rows.map((row, r) =>
      [...row]
        .map((ch, c) =>
          ch !== '#'
            ? at(r - 1, c) || at(r, c - 1) ? 'l' : '.'
            : !at(r - 1, c) || !at(r, c - 1) ? 'x' : !at(r + 1, c) || !at(r, c + 1) ? 'y' : 'z'
        )
        .join('')
    );
  }

  // A slab of stone in pixels, w by h, with chipped corners, a lit top-left
  // rim, a shaded bottom-right rim, speckles and a crack.
  function slab(w, h, seed) {
    const rnd = OG.rng(seed);
    const inset = (r) => (r === 0 ? 2 : r === 1 ? 1 : r === h - 1 ? 1 : 0);
    const rows = [];
    for (let r = 0; r < h; r++) {
      let s = '';
      const a = inset(r), b = inset(r) + (r > h - 6 ? 1 : 0);
      for (let c = 0; c < w; c++) {
        if (c < a || c >= w - b) s += '.';
        else if (c === a || c === w - b - 1 || r === 0 || r === h - 1) s += 'K';
        else if (r === 1 || c === a + 1) s += 'A';
        else if (r === h - 2 || c === w - b - 2) s += 'C';
        else s += rnd() < 0.05 ? 'd' : rnd() < 0.03 ? 'A' : 'B';
      }
      rows.push(s);
    }
    return rows;
  }

  OG.add({
    num: 12,
    id: 'og',
    name: 'Og',
    caps: 'OGOS',
    flexCase: true,
    family: 'Mascot',
    sources: ['Beyond computing', 'Games & arcade'],
    paletteTag: 'Earthy',
    risk: 'Wild',
    bg: '#EDCB8E',
    ink: '#3B2416',
    palette: ['#EDCB8E', '#A39B8A', '#E9A23B', '#7A4420', '#2A1A10'],
    fonts: [],
    type: 'Six-pixel stone letters, doubled and cut as V-grooves (a dark wall and a lit wall per stroke) with a lit lip below; the caption is the system font.',
    concept:
      'Og, a cheerful caveman in pixel art, shows off his stone tablet, cut in the shape of a floppy disk, beside a slab with the name chiseled into it. ' +
      'The joke is in the name itself: OgOs as the first operating system, older than the wheel.',
    logo: (w) => {
      const px = 5;
      const word = [...w].map((ch) => OG_GLYPHS[ch] || OG_GLYPHS.O);
      // Only the rows the word uses, so any case sits centered on the slab.
      let rows = setRows(word, 1);
      const used = rows.map((r) => r.includes('#'));
      rows = rows.slice(used.indexOf(true), used.lastIndexOf(true) + 1);
      const cut = chisel(upscale(rows, 2));
      const ox = 7, oy = 8; // Og, in pixels
      const sw = 64, sh = 26, sx = 25, sy = oy + 37 - sh; // the slab
      const lx = sx + Math.round((sw - cut[0].length) / 2), ly = sy + Math.round((sh - cut.length) / 2);
      const body = [...OG_HEAD, ...OG_BODY];
      const crack = ['.K', 'K.', 'K.', '.K'];
      const P = { K: '#3E372E', A: '#CFC8B6', B: '#AEA695', C: '#7D7565', d: '#A0988A', x: '#2B251E', y: '#4C4436', z: '#3A3329', l: '#DCD5C4' };
      const ground = centerRows(['g'.repeat(80), 'h'.repeat(84)], 86);
      const cap = 'COMPUTING SINCE 10,000 BC', cw = OG.pixTextWidth(cap, { px: 2, prop: true });
      return OG.logoSvg(
        OG.bitmap(ground, { g: '#C99A5B', h: '#DDB676' }, { x: 5 * px, y: (oy + 37) * px, px }) +
          OG.bitmap(slab(sw, sh, 3), P, { x: sx * px, y: sy * px, px }) +
          OG.bitmap(crack, P, { x: (sx + sw - 9) * px, y: (sy + sh - 5) * px, px }) +
          OG.bitmap(cut, P, { x: lx * px, y: ly * px, px }) +
          OG.bitmap(body, OG_PAL, { x: ox * px, y: oy * px, px }) +
          OG.pixText(cap, { x: 240 - cw / 2, y: (oy + 41) * px, px: 2, prop: true, fill: '#5A3A22' }),
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="6" fill="#EDCB8E"/>` + OG.bitmap(OG_HEAD.map((r) => r.slice(3, 19)), OG_PAL, { x: 0, y: 2, px: 4 }),
        { crisp: true }
      ),
  });

  /* 17. Constellation ----------------------------------------------------- */

  // Each letter as stars: [x, y, magnitude] in units (cap height 10, y up from
  // the baseline), and the lines between them as pairs of star indexes.
  const loop = (n) => Array.from({ length: n }, (_, i) => [i, (i + 1) % n]);
  const chain = (n) => Array.from({ length: n - 1 }, (_, i) => [i, i + 1]);
  const STARS = {
    O: { w: 6.6, s: [[3.2, 10, 1], [5.6, 8.9, 3], [6.6, 5.4, 2], [5.7, 1.4, 3], [3, 0, 2], [0.8, 1.6, 3], [0, 5.2, 2], [1, 8.6, 3]], e: loop(8) },
    G: { w: 6.4, s: [[6.2, 8.4, 3], [4, 10, 2], [1.2, 9, 3], [0, 5.4, 1], [1, 1.5, 3], [3.4, 0, 2], [6, 1, 3], [6.4, 4.4, 2], [3.8, 4.6, 3]], e: chain(9) },
    S: { w: 6.0, s: [[5.8, 8.8, 3], [3.4, 10, 2], [0.8, 9, 3], [0.4, 6.6, 3], [3, 5.2, 1], [5.6, 3.8, 3], [6, 1.4, 3], [3.4, 0, 2], [0.4, 1, 3]], e: chain(9) },
    o: { w: 5.4, s: [[2.6, 7, 2], [4.8, 6, 3], [5.4, 3.4, 2], [4.2, 0.6, 3], [2, 0, 2], [0.2, 1.8, 3], [0, 4.6, 3]], e: loop(7) },
    g: { w: 5.4, s: [[5.4, 7, 2], [2.4, 7, 3], [0, 4.4, 2], [1, 1, 3], [3.6, 0.3, 3], [5.4, 2.4, 3], [5.2, -1.4, 3], [3, -3, 2], [0.6, -2, 3]], e: [[0, 1], [1, 2], [2, 3], [3, 4], [4, 5], [0, 5], [5, 6], [6, 7], [7, 8]] },
    s: { w: 4.8, s: [[4.8, 6.4, 3], [2.4, 7, 2], [0.3, 5.6, 3], [1.6, 3.8, 3], [4, 2.8, 2], [4.8, 1, 3], [2.6, 0, 2], [0, 0.8, 3]], e: chain(8) },
  };

  // A star: a soft halo, a disc, and for the brightest a four-point glint.
  function star(x, y, mag, color, glint) {
    const r = [0, 4.4, 3.1, 2.1][mag];
    let out = `<circle cx="${f(x)}" cy="${f(y)}" r="${f(r * 3.2)}" fill="url(#sh)"/>` + `<circle cx="${f(x)}" cy="${f(y)}" r="${f(r)}" fill="${color}"/>`;
    if (mag === 1 && glint !== false) {
      const L = r * 4.2;
      out += `<path d="M${f(x - L)} ${f(y)}L${f(x)} ${f(y - 0.9)}L${f(x + L)} ${f(y)}L${f(x)} ${f(y + 0.9)}ZM${f(x)} ${f(y - L)}L${f(x + 0.9)} ${f(y)}L${f(x)} ${f(y + L)}L${f(x - 0.9)} ${f(y)}Z" fill="${color}"${glint === 'twinkle' ? '><animate attributeName="opacity" values="1;0.25;1" dur="2.6s" repeatCount="indefinite"/></path>' : '/>'}`;
    }
    return out;
  }

  // Lines between stars, stopping short of each star as on a printed chart.
  function starLines(pts, edges, gap, color, width) {
    return edges
      .map(([a, b]) => {
        const [x1, y1] = pts[a], [x2, y2] = pts[b], d = Math.hypot(x2 - x1, y2 - y1);
        const ux = (x2 - x1) / d, uy = (y2 - y1) / d;
        return `<line x1="${f(x1 + ux * gap)}" y1="${f(y1 + uy * gap)}" x2="${f(x2 - ux * gap)}" y2="${f(y2 - uy * gap)}" stroke="${color}" stroke-width="${width}" stroke-linecap="round"/>`;
      })
      .join('');
  }

  const STAR_HALO = `<radialGradient id="sh"><stop offset="0" stop-color="#BFD4FF" stop-opacity="0.55"/><stop offset="1" stop-color="#BFD4FF" stop-opacity="0"/></radialGradient>`;

  OG.add({
    num: 17,
    id: 'constellation',
    name: 'Constellation',
    caps: 'OgOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Beyond computing'],
    paletteTag: 'Mono',
    risk: 'Bold',
    bg: '#0B1736',
    ink: '#E8EEFF',
    palette: ['#0B1736', '#FFFFFF', '#8FB0F0', '#FFE3A6', '#3A5A9A'],
    fonts: ['IM Fell English:ital@0;1'],
    type: 'Letters drawn as constellations: stars of three magnitudes joined by hairlines that stop short of each star; the label is IM Fell English italic.',
    concept:
      'The name as a constellation on an old star chart, with faint coordinate lines, a field of dim stars, Bayer letters by the brightest and a label: OgOS Major. ' +
      'It suits a system that runs on old machines under new skies: something to navigate by, and to find your way back to.',
    logo: (w) => {
      const u = 12.2, gapU = 2.2, base = 186;
      const glyphs = [...w].map((ch) => STARS[ch] || STARS.O);
      const total = glyphs.reduce((a, g) => a + g.w, 0) + gapU * (glyphs.length - 1);
      let x = 240 - (total * u) / 2;
      const rnd = OG.rng(17);
      let field = '';
      for (let i = 0; i < 150; i++) {
        const sx = 8 + rnd() * 464, sy = 8 + rnd() * 284, r = 0.4 + rnd() * rnd() * 1.4;
        field += `<circle cx="${f(sx)}" cy="${f(sy)}" r="${f(r)}" fill="#DCE6FF" opacity="${f(0.25 + rnd() * 0.5)}"/>`;
      }
      // Coordinate lines: declination arcs and hour lines about a far pole.
      let grid = '';
      [700, 770, 840, 910].forEach((r) => (grid += `<circle cx="240" cy="910" r="${r}" fill="none" stroke="#5B7BC0" stroke-width="0.8" opacity="0.3"/>`));
      [-0.42, -0.21, 0, 0.21, 0.42].forEach((a) => {
        const cx = 240, cy = 910;
        grid += `<line x1="${f(cx + Math.sin(a) * 600)}" y1="${f(cy - Math.cos(a) * 600)}" x2="${f(cx + Math.sin(a) * 940)}" y2="${f(cy - Math.cos(a) * 940)}" stroke="#5B7BC0" stroke-width="0.8" opacity="0.32"/>`;
      });
      grid += `<path d="M0 232C140 196 330 120 480 70" fill="none" stroke="#C9A85E" stroke-width="0.9" stroke-dasharray="5 4" opacity="0.3"/>`;
      let lines = '', stars = '', bayer = '';
      const greek = ['α', 'β', 'γ', 'δ'];
      glyphs.forEach((g, n) => {
        const pts = g.s.map(([sx, sy]) => [x + sx * u, base - sy * u]);
        lines += starLines(pts, g.e, 6.5, '#9DBBF5', 1.6);
        g.s.forEach(([, , m], i) => {
          const warm = (n + i) % 5 === 2;
          stars += star(pts[i][0], pts[i][1], m, warm ? '#FFE3A6' : '#FFFFFF', n === 0 && m === 1 ? 'twinkle' : true);
        });
        const bi = g.s.findIndex((p) => p[2] === 1);
        const bp = pts[bi >= 0 ? bi : 0];
        bayer += OG.text(greek[n % 4], { x: f(bp[0] + 9), y: f(bp[1] - 8), size: 13, family: "'IM Fell English', serif", italic: true, fill: '#9DB4E4' });
        x += (g.w + gapU) * u;
      });
      return OG.logoSvg(
        `<defs>${STAR_HALO}</defs>` + grid + field + lines + stars + bayer +
          OG.text(w + ' Major', { x: 240, y: 268, size: 21, family: "'IM Fell English', serif", italic: true, fill: '#C9D6F2', anchor: 'middle', ls: 1.5 })
      );
    },
    icon: () => {
      const pts = [[32, 9, 1], [47, 15, 3], [53, 31, 2], [47, 47, 3], [32, 55, 2], [17, 48, 3], [11, 32, 2], [17, 15, 3]];
      return OG.iconSvg(
        `<defs>${STAR_HALO}</defs><rect width="64" height="64" rx="10" fill="#0B1736"/>` +
          starLines(pts, loop(8), 4.5, '#8FB0F0', 2.2) +
          pts.map(([x, y, m]) => star(x, y, m, '#FFFFFF', false)).join('')
      );
    },
  });

  /* 22. Bauhaus ---------------------------------------------------------- */

  const BH = { red: '#D52B1E', yellow: '#F4B400', blue: '#1D4E9E', black: '#141414', paper: '#F1EADB' };

  // An annulus from angle a0 to a1 (degrees, clockwise from 3 o'clock), or a full ring.
  function arcBand(cx, cy, R, r, a0, a1) {
    if (a1 - a0 >= 360) {
      return `M${f(cx + R)} ${f(cy)}A${R} ${R} 0 1 1 ${f(cx - R)} ${f(cy)}A${R} ${R} 0 1 1 ${f(cx + R)} ${f(cy)}Z` +
        (r > 0 ? `M${f(cx + r)} ${f(cy)}A${r} ${r} 0 1 0 ${f(cx - r)} ${f(cy)}A${r} ${r} 0 1 0 ${f(cx + r)} ${f(cy)}Z` : '');
    }
    const p = (rad, a) => [cx + rad * Math.cos((a * Math.PI) / 180), cy + rad * Math.sin((a * Math.PI) / 180)];
    const large = a1 - a0 > 180 ? 1 : 0;
    const [x0, y0] = p(R, a0), [x1, y1] = p(R, a1), [x2, y2] = p(r, a1), [x3, y3] = p(r, a0);
    return `M${f(x0)} ${f(y0)}A${R} ${R} 0 ${large} 1 ${f(x1)} ${f(y1)}` + (r > 0 ? `L${f(x2)} ${f(y2)}A${r} ${r} 0 ${large} 0 ${f(x3)} ${f(y3)}Z` : `L${f(cx)} ${f(cy)}Z`);
  }

  // Bauhaus letters, each built at the origin (x from 0, baseline at y = 0)
  // from rings, half discs and bars. X is the x-height; caps are 1.42 X.
  function bauhausGlyph(ch, X) {
    const t = X * 0.31, P = (d, c) => `<path d="${d}" fill="${c}" fill-rule="evenodd"/>`;
    const R = X / 2;
    if (ch === 'o' || ch === 'O') {
      const r = ch === 'O' ? X * 0.71 : R, th = ch === 'O' ? t * 1.15 : t;
      return { w: 2 * r, svg: P(arcBand(r, -r, r, r - th, 0, 360), BH.red) };
    }
    if (ch === 'g') {
      return {
        w: 2 * R,
        svg:
          P(arcBand(R, -R, R, R - t, 0, 360), BH.yellow) +
          P(arcBand(R, 0, R, R - t, 0, 90), BH.blue) +
          `<rect x="${f(2 * R - t)}" y="${f(-X)}" width="${f(t)}" height="${f(X)}" fill="${BH.black}"/>`,
      };
    }
    if (ch === 'G') {
      const r = X * 0.71, th = t * 1.15;
      return {
        w: 2 * r,
        svg:
          P(arcBand(r, -r, r, r - th, 0, 300), BH.yellow) +
          `<rect x="${f(r * 0.92)}" y="${f(-r - th * 0.1)}" width="${f(r * 1.08)}" height="${f(th)}" fill="${BH.black}"/>`,
      };
    }
    // s and S: a half ring opening right stacked on one opening left,
    // sharing the middle stroke, each end carried on by a short bar.
    const H = ch === 'S' ? X * 1.42 : X, th = ch === 'S' ? t * 0.9 : t * 0.7;
    const Ro = (H + th) / 4, rx = Ro * 1.18;
    const half = (cy, a0, a1) =>
      `M${f(rx + (a0 === 90 ? 0 : 0))} ${f(cy - Ro)}` +
      (a0 === 90
        ? `A${f(rx)} ${f(Ro)} 0 0 0 ${f(rx)} ${f(cy + Ro)}L${f(rx)} ${f(cy + Ro - th)}A${f(rx - th)} ${f(Ro - th)} 0 0 1 ${f(rx)} ${f(cy - Ro + th)}Z`
        : `A${f(rx)} ${f(Ro)} 0 0 1 ${f(rx)} ${f(cy + Ro)}L${f(rx)} ${f(cy + Ro - th)}A${f(rx - th)} ${f(Ro - th)} 0 0 0 ${f(rx)} ${f(cy - Ro + th)}Z`);
    const bar = rx * 0.62;
    return {
      w: 2 * rx,
      svg:
        `<path d="${half(-H + Ro, 90, 270)}" fill="${BH.blue}"/>` +
        `<rect x="${f(rx - 0.5)}" y="${f(-H)}" width="${f(bar)}" height="${f(th)}" fill="${BH.blue}"/>` +
        `<path d="${half(-Ro, -90, 90)}" fill="${BH.black}"/>` +
        `<rect x="${f(rx - bar + 0.5)}" y="${f(-th)}" width="${f(bar)}" height="${f(th)}" fill="${BH.black}"/>`,
    };
  }

  OG.add({
    num: 22,
    id: 'bauhaus',
    name: 'Bauhaus',
    caps: 'ogos',
    flexCase: true,
    family: 'Geometric',
    sources: ['Beyond computing'],
    paletteTag: 'Primary',
    risk: 'Bold',
    bg: BH.paper,
    ink: BH.black,
    palette: [BH.red, BH.yellow, BH.blue, BH.black, BH.paper],
    fonts: [],
    type: 'Built, not set: each letter is a ring, a quarter ring, a half ring or a bar, in a primary color.',
    concept:
      'The name assembled from the Bauhaus workshop kit: o a red ring, g a yellow ring with a black stem and a blue quarter-circle tail, s two half rings swung apart. ' +
      'Form from function, which is also how a 32-bit kernel gets built: from a few simple parts, carefully fitted.',
    logo: (w) => {
      const X = 104, gap = 14;
      const gl = [...w].map((ch) => bauhausGlyph(ch, X));
      const total = gl.reduce((a, g) => a + g.w, 0) + gap * (gl.length - 1);
      const hasCap = /[OGS]/.test(w), hasDesc = w.includes('g');
      const top = hasCap ? X * 1.42 : X, bot = hasDesc ? X / 2 : 0;
      const k = Math.min(1, 400 / total, 230 / (top + bot));
      const base = 150 + ((top - bot) * k) / 2;
      let x = 240 - (total * k) / 2, out = '';
      gl.forEach((g) => {
        out += `<g transform="translate(${f(x)} ${f(base)}) scale(${f(k)})">${g.svg}</g>`;
        x += (g.w + gap) * k;
      });
      return OG.logoSvg(out);
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="${BH.paper}"/>` +
          `<circle cx="22" cy="22" r="15" fill="${BH.blue}"/>` +
          `<rect x="34" y="8" width="24" height="24" fill="${BH.red}"/>` +
          `<path d="M8 58L26 34L44 58Z" fill="${BH.yellow}"/>` +
          `<rect x="46" y="38" width="10" height="20" fill="${BH.black}"/>`
      ),
  });

  /* 27. Neon Diner ------------------------------------------------------- */

  // A text or path drawn as a lit neon tube: a shadow on the wall, a wide
  // glow, the colored tube and its hot white core.
  function neonLayers(el, color, core, wTube) {
    const at = (stroke, sw, extra) => el.replace('/>', ` fill="none" stroke="${stroke}" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round" ${extra || ''}/>`);
    const tfill = (fill, stroke, sw, extra) => el.replace('/>', ` fill="${fill}" stroke="${stroke}" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round" ${extra || ''}/>`);
    const isText = el.startsWith('<text');
    const mk = isText ? (c, sw, extra) => tfill(c, c, sw, extra) : at;
    return (
      mk('#000', wTube + 2, 'transform="translate(5 7)" filter="url(#nb2)" opacity="0.55"') +
      mk(color, wTube * 3.2, 'filter="url(#nb8)" opacity="0.75"') +
      mk(color, wTube, '') +
      mk(core, wTube * 0.32, 'opacity="0.95"')
    );
  }

  const NEON_DEFS =
    `<filter id="nb8" x="-30%" y="-60%" width="160%" height="220%"><feGaussianBlur stdDeviation="9"/></filter>` +
    `<filter id="nb2" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="2.2"/></filter>`;

  OG.add({
    num: 27,
    id: 'neon-diner',
    name: 'Neon Diner',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Neon',
    risk: 'Wild',
    scene: true,
    bg: '#1A0E10',
    ink: '#FF5FB0',
    palette: ['#FF3FA4', '#3FF2FF', '#FF6A3D', '#1A0E10', '#3B1D1B'],
    fonts: ['Sacramento', 'Tilt Neon'],
    type: 'Sacramento, a thin monoline script, thickened into bent glass tube: a wall shadow, a wide glow, the pink tube and a white-hot core; OPEN is Tilt Neon.',
    concept:
      'The name as a neon sign over a diner counter at 2 a.m.: hot-pink script, a cyan underline held on with clips, a small OPEN sign that flickers, all on dark brick. ' +
      'Retro coding as a late-night hangout that is always open.',
    logo: (w) => {
      const lower = !/[A-Z]/.test(w);
      const size = fitSize('Sacramento', w, 330, lower ? 190 : 152), y = lower ? 150 : 162;
      const wordEl = `<text x="240" y="${y}" font-size="${f(size)}" font-family="Sacramento, cursive" text-anchor="middle"/>`;
      const layers = neonLayers(wordEl, '#FF3FA4', '#FFE4F2', 4.2).replace(/<text([^>]*)\/>/g, `<text$1>${OG.esc(w)}</text>`);
      const under = `<path d="M104 212C180 198 290 196 362 205"/>`;
      const open = `<rect x="376" y="240" width="68" height="30" rx="7"/>`;
      const openText = `<text x="410" y="262" font-size="19" font-family="'Tilt Neon', sans-serif" text-anchor="middle" letter-spacing="2"/>`;
      const clip = (x, y) => `<rect x="${x - 3}" y="${y - 6}" width="6" height="12" rx="1.5" fill="#8C8C94"/><rect x="${x - 3}" y="${y - 6}" width="6" height="3" rx="1" fill="#C8C8D0"/>`;
      return OG.logoSvg(
        `<defs><pattern id="br" width="52" height="26" patternUnits="userSpaceOnUse">` +
          `<rect width="52" height="26" fill="#120809"/><rect x="1" y="1" width="50" height="11" rx="1" fill="#3A1C1A"/>` +
          `<rect x="-25" y="14" width="50" height="11" rx="1" fill="#331818"/><rect x="27" y="14" width="50" height="11" rx="1" fill="#3E201E"/></pattern>` +
          `<radialGradient id="spill" cx="0.5" cy="0.48" r="0.6"><stop offset="0" stop-color="#FF3FA4" stop-opacity="0.32"/><stop offset="0.6" stop-color="#FF3FA4" stop-opacity="0.08"/><stop offset="1" stop-color="#FF3FA4" stop-opacity="0"/></radialGradient>` +
          `<radialGradient id="vig" cx="0.5" cy="0.45" r="0.75"><stop offset="0.55" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.7"/></radialGradient>` +
          grain('bg', 0.9, 3, true, 1.4) + NEON_DEFS + `</defs>` +
          `<rect width="480" height="300" fill="url(#br)"/><rect width="480" height="300" filter="url(#bg)" opacity="0.5"/>` +
          `<rect width="480" height="300" fill="url(#spill)"/><rect width="480" height="300" fill="url(#vig)"/>` +
          layers +
          neonLayers(under, '#3FF2FF', '#E6FFFF', 4) +
          `<rect x="94" y="207" width="12" height="9" rx="2" fill="#2A2A30"/><rect x="360" y="200" width="12" height="9" rx="2" fill="#2A2A30"/>` +
          clip(160, 203) + clip(310, 200) +
          `<g><animate attributeName="opacity" values="1;1;1;0.35;1;0.4;1;1" keyTimes="0;0.6;0.62;0.64;0.66;0.69;0.71;1" dur="5s" repeatCount="indefinite"/>` +
          neonLayers(open, '#FF6A3D', '#FFE2D6', 2.4) +
          neonLayers(openText, '#FF6A3D', '#FFE2D6', 1.2).replace(/<text([^>]*)\/>/g, '<text$1>OPEN</text>') +
          `</g>` + clip(396, 240) + clip(424, 240)
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs>${NEON_DEFS.replace('stdDeviation="9"', 'stdDeviation="4"')}<radialGradient id="isp"><stop offset="0" stop-color="#FF3FA4" stop-opacity="0.35"/><stop offset="1" stop-color="#FF3FA4" stop-opacity="0"/></radialGradient></defs>` +
          `<rect width="64" height="64" rx="10" fill="#1A0E10"/><circle cx="32" cy="32" r="30" fill="url(#isp)"/>` +
          neonLayers(`<ellipse cx="32" cy="32" rx="15" ry="18"/>`, '#FF3FA4', '#FFE4F2', 5.5)
      ),
  });

  /* 32. Constructivist --------------------------------------------------- */

  // Blocky condensed letters as polygons, in units: cap height 10 (y down
  // from the cap line), x-height from y = 3, descender to y = 13.
  function cvGlyph(ch) {
    const W = 4.6, T = 1.75, c = 1.0;
    const oct = (x0, y0, x1, y1, k) => [[x0 + k, y0], [x1 - k, y0], [x1, y0 + k], [x1, y1 - k], [x1 - k, y1], [x0 + k, y1], [x0, y1 - k], [x0, y0 + k]];
    const ring = (y0, y1) => [oct(0, y0, W, y1, c), oct(T, y0 + T, W - T, y1 - T, 0.35)];
    const sPoly = (y0, y1) => {
      const m = (y0 + y1 - T) / 2;
      return [[[c, y0], [W, y0], [W, y0 + T], [T, y0 + T], [T, m], [W - c, m], [W, m + c], [W, y1 - c], [W - c, y1], [0, y1], [0, y1 - T], [W - T, y1 - T], [W - T, m + T], [c, m + T], [0, m + T - c], [0, y0 + c]]];
    };
    switch (ch) {
      case 'O': return ring(0, 10);
      case 'o': return ring(3, 10);
      case 'G':
        return [[[c, 0], [W - c, 0], [W, c], [W, 3.3], [W - T, 3.3], [W - T, T], [T, T], [T, 10 - T], [W - T, 10 - T], [W - T, 6.1], [2.2, 6.1], [2.2, 4.5], [W, 4.5], [W, 10 - c], [W - c, 10], [c, 10], [0, 10 - c], [0, c]]];
      case 'g':
        return [[[c, 3], [W, 3], [W, 13 - c], [W - c, 13], [0.4, 13], [0.4, 13 - T], [W - T, 13 - T], [W - T, 10], [c, 10], [0, 10 - c], [0, 3 + c]], [[T, 3 + T], [W - T, 3 + T], [W - T, 10 - T], [T, 10 - T]]];
      case 'S': return sPoly(0, 10);
      default: return sPoly(3, 10);
    }
  }

  const CV = { red: '#D3271C', black: '#16130F', paper: '#EEE3C8' };

  OG.add({
    num: 32,
    id: 'constructivist',
    name: 'Constructivist',
    caps: 'OGOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Primary',
    risk: 'Bold',
    bg: CV.paper,
    ink: CV.black,
    palette: [CV.red, CV.black, CV.paper],
    fonts: ['Bebas Neue'],
    type: 'Hand-built condensed block capitals with chamfered corners, drawn as polygons on a 4.6 by 10 grid; the small line is Bebas Neue.',
    concept:
      'A 1920s avant-garde poster: heavy block letters climbing a diagonal across a red sun, a black bar under them and a red wedge driving in. ' +
      'Pure geometric energy, without any of the politics, for a system that wants to be built and rebuilt by its users.',
    logo: (w) => {
      const u = 13.2, gap = 0.95, W = 4.6;
      const glyphs = [...w].map(cvGlyph);
      const total = glyphs.length * W + (glyphs.length - 1) * gap;
      const word = glyphs
        .map((g, i) => `<path d="${g.map((poly) => pathOf(poly.map(([x, y]) => [(i * (W + gap) + x) * u, y * u]))).join('')}" fill-rule="evenodd"/>`)
        .join('');
      const wpx = total * u;
      return OG.logoSvg(
        `<circle cx="332" cy="112" r="82" fill="${CV.red}"/>` +
          `<path d="M28 272L300 128L312 146Z" fill="${CV.red}"/>` +
          `<g transform="translate(244 150) rotate(-17) translate(${f(-wpx / 2)} -72)">` +
          `<rect x="-40" y="${f(10 * u + 12)}" width="${f(wpx + 90)}" height="16" fill="${CV.black}"/>` +
          `<rect x="${f(wpx * 0.35)}" y="${f(10 * u + 34)}" width="${f(wpx * 0.65 + 50)}" height="5" fill="${CV.black}"/>` +
          `<g fill="${CV.black}">${word}</g>` +
          OG.text('OPERATING SYSTEM · 32 BIT', { x: -34, y: f(10 * u + 25.5), size: 13, family: "'Bebas Neue', sans-serif", fill: CV.paper, ls: 3 }) +
          `</g>`
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="${CV.paper}"/><circle cx="38" cy="26" r="20" fill="${CV.black}"/>` +
          `<path d="M4 60L36 30L42 38Z" fill="${CV.red}"/>`
      ),
  });

  /* 37. Psychedelic ------------------------------------------------------ */

  // A smooth curve through points (Catmull-Rom), n samples per span.
  function catmull(pts, n) {
    const out = [];
    for (let i = 0; i < pts.length - 1; i++) {
      const p0 = pts[Math.max(0, i - 1)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[Math.min(pts.length - 1, i + 2)];
      for (let k = 0; k < n; k++) {
        const t = k / n, t2 = t * t, t3 = t2 * t;
        out.push([0, 1].map((j) => 0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2 + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3)));
      }
    }
    out.push(pts[pts.length - 1]);
    return out;
  }

  // The outline of a stroke along a centerline, width wid, with round ends.
  function ribbon(c, wid) {
    const L = [], R = [], tan = [];
    c.forEach((p, i) => {
      const a = c[Math.max(0, i - 1)], b = c[Math.min(c.length - 1, i + 1)];
      const d = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1, tx = (b[0] - a[0]) / d, ty = (b[1] - a[1]) / d, h = wid / 2;
      tan.push([tx, ty]);
      L.push([p[0] - ty * h, p[1] + tx * h]);
      R.push([p[0] + ty * h, p[1] - tx * h]);
    });
    const cap = (p, [tx, ty], sgn) => {
      const out = [], h = wid / 2;
      for (let k = 1; k < 8; k++) {
        const a = (k / 8) * Math.PI;
        out.push([p[0] + sgn * (-ty * h * Math.cos(a) + tx * h * Math.sin(a)), p[1] + sgn * (tx * h * Math.cos(a) + ty * h * Math.sin(a))]);
      }
      return out;
    };
    const n = c.length - 1;
    return [...L, ...cap(c[n], tan[n], 1), ...R.reverse(), ...cap(c[0], tan[0], -1)];
  }

  // An elliptical ring (outer and inner contours) from angle a0 to a1, or whole.
  const ellipsePts = (cx, cy, rx, ry, a0, a1, n) =>
    Array.from({ length: n + 1 }, (_, i) => {
      const a = ((a0 + ((a1 - a0) * i) / n) * Math.PI) / 180;
      return [cx + rx * Math.cos(a), cy + ry * Math.sin(a)];
    });

  // Psychedelic letters in font units (cap line y = 0, x-height 30, baseline
  // 100, descender 134): each is a list of shapes, a shape a list of contours.
  function psyGlyph(ch) {
    const t = 25;
    const ring = (cx, cy, rx, ry) => [[ellipsePts(cx, cy, rx + t / 2, ry + t / 2, 0, 360, 48), ellipsePts(cx, cy, rx - t / 2, ry - t / 2, 0, 360, 36).reverse()]];
    switch (ch) {
      case 'o': return { w: 72, top: 30, bot: 100, shapes: ring(36, 65, 23.5, 22.5) };
      case 'O': return { w: 86, top: 0, bot: 100, shapes: ring(43, 50, 30.5, 37.5) };
      case 'g':
        return { w: 72, top: 30, bot: 134, shapes: [...ring(33, 64, 20.5, 21.5), [ribbon(catmull([[57, 40], [58, 80], [57, 112], [42, 124], [22, 122], [12, 114]], 8), t)]] };
      case 'G':
        return {
          w: 86, top: 0, bot: 100,
          shapes: [[ribbon(ellipsePts(43, 50, 30.5, 37.5, -40, -330, 40).concat([[73.5, 62], [73.5, 80]]).reverse(), t)], [ribbon([[44, 54], [60, 54], [72, 54]], t * 0.9)]],
        };
      case 'S':
        return { w: 74, top: 0, bot: 100, shapes: [[ribbon(catmull([[62, 18], [50, 9], [33, 9], [17, 16], [15, 32], [30, 44], [46, 54], [60, 68], [58, 86], [41, 93], [24, 92], [11, 83]], 6), t)]] };
      default:
        return { w: 64, top: 30, bot: 100, shapes: [[ribbon(catmull([[52, 44], [42, 39], [29, 39], [17, 44], [17, 56], [30, 63], [43, 69], [52, 79], [47, 89], [32, 92], [19, 90], [10, 84]], 6), t * 0.92)]] };
    }
  }

  // Sets the word, then bends it into a swelling, sagging envelope.
  function psychedelic(w, box) {
    const gl = [...w].map(psyGlyph), sp = 2;
    const W = gl.reduce((a, g) => a + g.w, 0) + sp * (gl.length - 1);
    const top = Math.min(...gl.map((g) => g.top)) - 13, bot = Math.max(...gl.map((g) => g.bot)) + 13;
    const { x0, x1, cy, h0, h1 } = box;
    const warp = ([x, y]) => {
      const u = x / W, v = Math.min(1, Math.max(0, (y - top) / (bot - top)));
      const uu = u - 0.05 * Math.sin(2 * Math.PI * u);
      const H = h0 + (h1 - h0) * Math.pow(Math.sin(Math.PI * Math.min(1, Math.max(0, u))), 1.3);
      const c = cy + 7 * Math.sin(2 * Math.PI * u + 0.6);
      const vv = Math.pow(v, 0.82);
      return [x0 + (x1 - x0) * uu, c - H / 2 + vv * H];
    };
    let x = 0;
    const shapes = [];
    gl.forEach((g) => {
      g.shapes.forEach((sh) => shapes.push(sh.map((ctr) => ctr.map(([px, py]) => warp([x + px, py])))));
      x += g.w + sp;
    });
    // The envelope's outline, for the echo lines around the word.
    const env = (d) => {
      const tp = [], bt = [];
      for (let i = 0; i <= 40; i++) {
        const [ax, ay] = warp([(W * i) / 40, top]), [, by] = warp([(W * i) / 40, bot]);
        tp.push([ax, ay - d]);
        bt.push([ax, by + d]);
      }
      const [lx, ly] = warp([0, (top + bot) / 2]), [rx, ry] = warp([W, (top + bot) / 2]);
      return smoothClosed([...tp, [rx + d * 0.9, ry], ...bt.reverse(), [lx - d * 0.9, ly]]);
    };
    return { shapes, env };
  }

  const PSY = { purple: '#4E1381', orange: '#FF8A1C', pink: '#FF3E9E', deep: '#24063F', lime: '#B6F03C' };

  OG.add({
    num: 37,
    id: 'psychedelic',
    name: 'Psychedelic',
    caps: 'ogos',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Neon',
    risk: 'Wild',
    bg: PSY.purple,
    ink: PSY.orange,
    palette: [PSY.purple, PSY.orange, PSY.pink, PSY.deep],
    fonts: [],
    type: 'Hand-built: fat round strokes drawn as outlines, then the whole word bent through a warp that swells the middle letters and lets their bottoms sag.',
    concept:
      'A 1967 ballroom poster: the letters swell to fill a bulging shape and sag as if melting, orange vibrating against purple, ringed by echo lines. ' +
      'The Summer of Love is as far from 1997 as 1997 is from now, and a hobby OS is a happening of its own.',
    logo: (w) => {
      const P = psychedelic(w, { x0: 74, x1: 406, cy: 150, h0: 118, h1: 196 });
      const d = (sh) => sh.map((ctr) => pathOf(ctr)).join('');
      const echoes = [[16, PSY.pink, 5], [31, PSY.orange, 4], [44, PSY.pink, 2.5]]
        .map(([dd, c, sw]) => `<path d="${P.env(dd)}" fill="none" stroke="${c}" stroke-width="${sw}"/>`).join('');
      return OG.logoSvg(
        echoes +
          `<g transform="translate(4 5)" fill="${PSY.deep}">${P.shapes.map((sh) => `<path d="${d(sh)}" fill-rule="evenodd"/>`).join('')}</g>` +
          `<g fill="${PSY.orange}" stroke="${PSY.orange}" stroke-width="1" stroke-linejoin="round">${P.shapes.map((sh) => `<path d="${d(sh)}" fill-rule="evenodd"/>`).join('')}</g>`
      );
    },
    icon: () => {
      // An o whose stroke runs once round, then curls inward and ends in a ball.
      const c = [], N = 120, turns = 1.72;
      for (let i = 0; i <= N; i++) {
        const k = i / N, a = k * turns * 2 * Math.PI - Math.PI / 2;
        const r = k < 0.56 ? 20 : 20 - 12.5 * Math.pow((k - 0.56) / 0.44, 0.9);
        c.push([32 + r * Math.cos(a) * 0.94, 33 + r * Math.sin(a)]);
      }
      const L = [], R = [];
      c.forEach((p, i) => {
        const a = c[Math.max(0, i - 1)], b = c[Math.min(c.length - 1, i + 1)], dd = Math.hypot(b[0] - a[0], b[1] - a[1]);
        const tx = (b[0] - a[0]) / dd, ty = (b[1] - a[1]) / dd, h = (i / N < 0.56 ? 5.2 : 5.2 - 2.4 * ((i / N - 0.56) / 0.44));
        L.push([p[0] - ty * h, p[1] + tx * h]);
        R.push([p[0] + ty * h, p[1] - tx * h]);
      });
      const e = c[N];
      return OG.iconSvg(
        `<rect width="64" height="64" rx="12" fill="${PSY.purple}"/><circle cx="32" cy="33" r="29" fill="none" stroke="${PSY.pink}" stroke-width="2.5"/>` +
          `<path d="${pathOf([...L, ...R.reverse()])}" fill="${PSY.orange}" stroke="${PSY.orange}" stroke-width="1" stroke-linejoin="round"/>` +
          `<circle cx="${f(e[0])}" cy="${f(e[1])}" r="4.2" fill="${PSY.orange}"/>`
      );
    },
  });

  /* 42. Night Owl --------------------------------------------------------- */

  const OWL = { night: '#111A3A', body: '#5B3F7A', face: '#EADCB8', ring: '#2B1B33', iris: '#FFB627', beak: '#F28A2E', branch: '#6B4423', leaf: '#4E8A5C', moon: '#F4E9B8' };

  // An owl's eye that is also the letter O: a dark ring, an amber iris, a
  // pupil looking up and to the right, and a catchlight.
  function owlEye(cx, cy, R, t) {
    return (
      `<circle cx="${f(cx)}" cy="${f(cy)}" r="${f(R - t / 2)}" fill="none" stroke="${OWL.ring}" stroke-width="${f(t)}"/>` +
      `<circle cx="${f(cx)}" cy="${f(cy)}" r="${f(R - t)}" fill="${OWL.iris}"/>` +
      `<circle cx="${f(cx + R * 0.1)}" cy="${f(cy - R * 0.08)}" r="${f((R - t) * 0.55)}" fill="#120A08"/>` +
      `<circle cx="${f(cx + R * 0.22)}" cy="${f(cy - R * 0.25)}" r="${f(R * 0.09)}" fill="#FFF"/>`
    );
  }

  OG.add({
    num: 42,
    id: 'night-owl',
    name: 'Night Owl',
    caps: 'OgOs',
    flexCase: true,
    family: 'Mascot',
    sources: ['Beyond computing'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    bg: OWL.night,
    ink: OWL.face,
    palette: [OWL.night, OWL.body, OWL.face, OWL.iris, OWL.beak],
    fonts: ['Righteous'],
    type: 'The two O\'s are drawn as owl eyes to the weight of Righteous, which sets the g (the beak, in orange) and the s.',
    concept:
      'An owl made of the name: its two O\'s are the eyes, the g between them is the beak, and the s sits beside it on the branch that underlines the word. ' +
      'A mascot for the people who write kernels after midnight.',
    logo: (w) => {
      const S = 116, t = S * 0.15, gap = S * 0.06;
      const chars = [...w];
      const eyeR = (ch) => (ch === 'O' ? S * 0.37 : S * 0.29);
      const widthOf = (ch) => (/[Oo]/.test(ch) ? eyeR(ch) * 2 : (ADV.Righteous[ch] * S) / 100);
      const total = chars.reduce((a, ch) => a + widthOf(ch), 0) + gap * (chars.length - 1);
      const base = 176;
      // Letters after the second eye stand clear of the owl's body.
      const lastEye = Math.max(chars.lastIndexOf('O'), chars.lastIndexOf('o'));
      const outside = chars.length - 1 - lastEye, extra = 26;
      let x = 240 - (total + outside * extra) / 2 + 4;
      const slots = chars.map((ch, i) => {
        if (i > lastEye) x += extra;
        const wd = widthOf(ch), sl = { ch, x, wd };
        x += wd + gap;
        return sl;
      });
      const eyes = slots.filter((sl) => /[Oo]/.test(sl.ch));
      const e1 = eyes[0], e2 = eyes[eyes.length - 1];
      const R1 = eyeR(e1.ch), R2 = eyeR(e2.ch), Rm = Math.max(R1, R2);
      const c1 = [e1.x + R1, base - R1], c2 = [e2.x + R2, base - R2];
      // The head and body: a rounded shape around both eyes, with ear tufts.
      const hx0 = e1.x - 18, hx1 = e2.x + 2 * R2 + 18, top = base - 2 * Rm - 22, bot = base + 48;
      const body =
        `<path d="M${f(hx0 + 30)} ${f(top)}L${f(hx0 - 4)} ${f(top - 34)}L${f(hx0 + 62)} ${f(top + 2)}` +
        `L${f(hx1 - 62)} ${f(top + 2)}L${f(hx1 + 4)} ${f(top - 34)}L${f(hx1 - 30)} ${f(top)}` +
        `Q${f(hx1 + 6)} ${f(top + 14)} ${f(hx1 + 2)} ${f(base - Rm)}Q${f(hx1 + 6)} ${f(bot - 10)} ${f((hx0 + hx1) / 2 + 30)} ${f(bot)}` +
        `L${f((hx0 + hx1) / 2 - 30)} ${f(bot)}Q${f(hx0 - 6)} ${f(bot - 10)} ${f(hx0 - 2)} ${f(base - Rm)}Q${f(hx0 - 6)} ${f(top + 14)} ${f(hx0 + 30)} ${f(top)}Z" fill="${OWL.body}"/>`;
      const disc = `<circle cx="${f(c1[0])}" cy="${f(c1[1])}" r="${f(R1 + 12)}" fill="${OWL.face}"/><circle cx="${f(c2[0])}" cy="${f(c2[1])}" r="${f(R2 + 12)}" fill="${OWL.face}"/>`;
      // Chest feathers: small chevrons under the face.
      let chest = '';
      const mid = (hx0 + hx1) / 2;
      [[-44, 28], [-14, 30], [16, 28], [46, 30], [-28, 40], [2, 41], [32, 40]].forEach(([dx, dy]) => {
        chest += `<path d="M${f(mid + dx - 7)} ${f(base + dy - 4)}L${f(mid + dx)} ${f(base + dy + 2)}L${f(mid + dx + 7)} ${f(base + dy - 4)}" fill="none" stroke="${OWL.face}" stroke-width="2.4" stroke-linecap="round" opacity="0.45"/>`;
      });
      const by = bot + 4;
      const branch =
        `<path d="M32 ${f(by + 2)}Q240 ${f(by - 8)} 452 ${f(by + 4)}L452 ${f(by + 13)}Q240 ${f(by + 3)} 32 ${f(by + 12)}Z" fill="${OWL.branch}"/>` +
        `<path d="M400 ${f(by + 3)}Q420 ${f(by - 18)} 446 ${f(by - 22)}" fill="none" stroke="${OWL.branch}" stroke-width="4" stroke-linecap="round"/>` +
        `<path d="M446 ${f(by - 22)}q-4 -16 10 -22q4 14 -10 22Z" fill="${OWL.leaf}"/><path d="M60 ${f(by + 8)}q-10 14 -26 14q8 -14 26 -14Z" fill="${OWL.leaf}"/>`;
      const talons = [-26, -14, 14, 26].map((dx) => `<rect x="${f(mid + dx - 3.5)}" y="${f(by - 6)}" width="7" height="12" rx="3.5" fill="${OWL.beak}"/>`).join('');
      const letters = slots
        .filter((sl) => !/[Oo]/.test(sl.ch))
        .map((sl) => {
          const inHead = sl.x > e1.x && sl.x < e2.x;
          return OG.text(sl.ch, { x: f(sl.x + sl.wd / 2), y: base, size: S, family: 'Righteous, sans-serif', fill: inHead ? OWL.beak : OWL.face, anchor: 'middle' });
        })
        .join('');
      const moon = `<circle cx="420" cy="52" r="20" fill="${OWL.moon}"/><circle cx="430" cy="45" r="18" fill="${OWL.night}"/>`;
      const rnd = OG.rng(42);
      let stars = '';
      for (let i = 0; i < 14; i++) stars += `<circle cx="${f(30 + rnd() * 420)}" cy="${f(14 + rnd() * 60)}" r="${f(0.8 + rnd() * 1.1)}" fill="#DDE4FF" opacity="${f(0.4 + rnd() * 0.5)}"/>`;
      return OG.logoSvg(
        stars + moon + branch + body + disc + chest + talons +
          owlEye(c1[0], c1[1], R1, t) + owlEye(c2[0], c2[1], R2, t) + letters
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="12" fill="${OWL.night}"/>` +
          `<path d="M14 18L8 4L24 13L40 13L56 4L50 18Q58 30 54 44Q48 60 32 60Q16 60 10 44Q6 30 14 18Z" fill="${OWL.body}"/>` +
          `<circle cx="21" cy="30" r="13" fill="${OWL.face}"/><circle cx="43" cy="30" r="13" fill="${OWL.face}"/>` +
          owlEye(21, 30, 10, 3.4) + owlEye(43, 30, 10, 3.4) +
          `<path d="M28 39L36 39L32 49Z" fill="${OWL.beak}"/>`
      ),
  });

  /* 47. Tag -------------------------------------------------------------- */

  OG.add({
    num: 47,
    id: 'tag',
    name: 'Tag',
    caps: 'OG OS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Beyond computing'],
    paletteTag: 'Neon',
    risk: 'Wild',
    bg: '#8D8B85',
    ink: '#141414',
    palette: ['#FFE14D', '#FF8A1C', '#F0388E', '#141414', '#8D8B85'],
    fonts: ['Sedgwick Ave Display'],
    type: 'Sedgwick Ave Display, a handstyle drawn from real tags, built up as a throw-up: fade fill, black outline, a 3D block, a white cutback and drips.',
    concept:
      'OG OS thrown up on a buffed concrete wall: a yellow to pink fade, a hard black outline, a 3D block and paint dripping off the bottoms. ' +
      'OG as in original: the system that was here first, and still writes its name on the wall.',
    logo: (w) => {
      const size = fitSize('Sedgwick Ave Display', w, 330, 150);
      const fam = "'Sedgwick Ave Display', cursive";
      const T = (attrs) => `<text x="0" y="0" font-size="${f(size)}" font-family="${fam}" text-anchor="middle" ${attrs}>${OG.esc(w)}</text>`;
      // Letter centers from the measured advances, for the drips.
      const adv = [...w].map((ch) => (ADV['Sedgwick Ave Display'][ch] || 55) * size / 100);
      const tw = adv.reduce((a, b) => a + b, 0);
      let ax = -tw / 2;
      const centers = adv.map((a, i) => { const c = ax + a / 2; ax += a; return [c, w[i]]; }).filter(([, ch]) => ch !== ' ');
      const drips = centers
        .filter((_, i) => i % 2 === 0 || i === centers.length - 1)
        .map(([cx], i) => {
          const len = [34, 22, 46, 28][i % 4], dx = cx + [-6, 8, -2, 6][i % 4], wd = 9;
          return `<path d="M${f(dx - wd / 2)} -12L${f(dx - wd / 2)} ${f(len)}a${f(wd / 2)} ${f(wd / 2)} 0 0 0 ${f(wd)} 0L${f(dx + wd / 2)} -12Z"/>`;
        })
        .join('');
      const block = [1, 2, 3, 4, 5, 6, 7].map((k) => `<g transform="translate(${k} ${k})">${T('fill="#3A1A52" stroke="#3A1A52" stroke-width="15" stroke-linejoin="round"')}</g>`).join('');
      const buff = smoothClosed(roughRect(30, 52, 420, 196, 10, 5, 47, 22));
      return OG.logoSvg(
        `<defs><linearGradient id="fade" x1="0" y1="0" x2="0" y2="1"><stop offset="0.12" stop-color="#FFE14D"/><stop offset="0.5" stop-color="#FF8A1C"/><stop offset="0.9" stop-color="#F0388E"/></linearGradient>` +
          grain('wall', 0.65, 12, true, 1.5) +
          `<filter id="spray" x="-20%" y="-30%" width="140%" height="160%"><feGaussianBlur stdDeviation="3"/></filter>` +
          `<clipPath id="tc"><g transform="translate(240 ${f(150 + size * 0.3)}) rotate(-6)">${T('')}</g></clipPath></defs>` +
          `<path d="${buff}" fill="#A19E97"/><path d="${buff}" fill="#000" filter="url(#wall)" opacity="0.25"/>` +
          `<g transform="translate(240 ${f(150 + size * 0.3)}) rotate(-6)">` +
          `<g filter="url(#spray)" opacity="0.5">${T('fill="#F0388E" stroke="#F0388E" stroke-width="34" stroke-linejoin="round"')}</g>` +
          `<g transform="translate(4 4)">${T('fill="#F7F5EE" stroke="#F7F5EE" stroke-width="28" stroke-linejoin="round"')}</g>` +
          block +
          `<g fill="url(#fade)" stroke="#141414" stroke-width="3">${drips}</g>` +
          T('fill="#141414" stroke="#141414" stroke-width="14" stroke-linejoin="round"') +
          T('fill="url(#fade)"') +
          `</g>` +
          `<g clip-path="url(#tc)"><g transform="translate(240 ${f(150 + size * 0.3)}) rotate(-6)"><g transform="translate(-4 -5)" opacity="0.85">${T('fill="none" stroke="#FFFFFF" stroke-width="3"')}</g></g></g>` +
          `<path d="M${f(240 - tw / 2 + 4)} ${f(150 - size * 0.42)}l6 -14l2 14l13 3l-13 4l-3 14l-4 -14l-12 -3Z" fill="#FFF"/>`
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs><linearGradient id="ifade" x1="0" y1="0" x2="0" y2="1"><stop offset="0.15" stop-color="#FFE14D"/><stop offset="0.55" stop-color="#FF8A1C"/><stop offset="0.9" stop-color="#F0388E"/></linearGradient></defs>` +
          `<rect width="64" height="64" rx="10" fill="#8D8B85"/>` +
          `<ellipse cx="32" cy="27" rx="19" ry="20" fill="none" stroke="#141414" stroke-width="14"/>` +
          `<path d="M17 36L17 56a3.5 3.5 0 0 0 7 0L24 36Z" fill="url(#ifade)" stroke="#141414" stroke-width="2.5"/>` +
          `<path d="M39 42L39 50a3 3 0 0 0 6 0L45 40Z" fill="url(#ifade)" stroke="#141414" stroke-width="2.5"/>` +
          `<ellipse cx="32" cy="27" rx="19" ry="20" fill="none" stroke="url(#ifade)" stroke-width="7.5"/>` +
          `<path d="M19 15q5 -6 12 -7" fill="none" stroke="#FFF" stroke-width="2.5" stroke-linecap="round"/>`
      ),
  });

  /* 52. Mission Patch ----------------------------------------------------- */

  const PATCH = { gold: '#E0A92E', goldDark: '#9C6E12', navy: '#1C2C63', space: '#0A1029', white: '#F5F2E8', red: '#E0452B', cyan: '#7FD8F0' };

  // A small pixel CRT for the middle of the patch.
  const PATCH_CRT = [
    '.kkkkkkkkkkkkkk.',
    'kbbbbbbbbbbbbbbk',
    'kbkkkkkkkkkkkkbk',
    'kbk' + 'ssssssssss' + 'kbk',
    'kbk' + 'sswwwsssss' + 'kbk',
    'kbk' + 'swssswssss' + 'kbk',
    'kbk' + 'swssswssss' + 'kbk',
    'kbk' + 'swssswssss' + 'kbk',
    'kbk' + 'sswwwseees' + 'kbk',
    'kbk' + 'ssssssssss' + 'kbk',
    'kbkkkkkkkkkkkkbk',
    'kbbbbbbbbbbbrbbk',
    '.kkkkkkkkkkkkkk.',
    '.....kbbbbk.....',
    '...kkbbbbbbkk...',
    '...kkkkkkkkkk...',
  ];

  // A five-point star.
  const star5 = (cx, cy, r, fill) =>
    `<path d="${pathOf(Array.from({ length: 10 }, (_, i) => {
      const a = -Math.PI / 2 + (i * Math.PI) / 5, rr = i % 2 ? r * 0.42 : r;
      return [cx + rr * Math.cos(a), cy + rr * Math.sin(a)];
    }))}" fill="${fill}"/>`;

  OG.add({
    num: 52,
    id: 'mission-patch',
    name: 'Mission Patch',
    caps: 'OGOS',
    flexCase: true,
    family: 'Emblem',
    sources: ['Beyond computing'],
    paletteTag: 'Primary',
    risk: 'Bold',
    bg: '#2A2F3A',
    ink: '#F5F2E8',
    palette: [PATCH.navy, PATCH.gold, PATCH.space, PATCH.white, PATCH.red],
    fonts: ['Jost:wght@700'],
    type: 'Jost Bold, a Futura-like geometric, set around the band in embroidered white thread; the CRT in the middle is pixel art.',
    concept:
      'An embroidered mission patch: a gold merrowed edge, the name around a navy band, and in the middle a little CRT in orbit with a red moon. ' +
      'Every hobby OS is a moonshot run by a tiny crew; this is the badge for its first mission.',
    logo: (w) => {
      const cx = 240, cy = 150, R = 128;
      let satin = '';
      for (let i = 0; i < 160; i++) {
        const a = (i / 160) * Math.PI * 2, c = Math.cos(a), sn = Math.sin(a);
        satin += `<line x1="${f(cx + c * (R - 13))}" y1="${f(cy + sn * (R - 13))}" x2="${f(cx + c * (R - 1) + sn * 3)}" y2="${f(cy + sn * (R - 1) - c * 3)}"/>`;
      }
      const rt = 79, rb = 101; // text baselines: the top runs clockwise, the bottom anticlockwise
      const size = Math.min(40, 150 / advance('Jost', w));
      const orbit = (half) => `<path d="${half}" fill="none" stroke="${PATCH.cyan}" stroke-width="2.5" transform="rotate(-18 ${cx} ${cy + 6})"/>`;
      return OG.logoSvg(
        `<defs><path id="pt" d="M${cx - rt} ${cy}A${rt} ${rt} 0 0 1 ${cx + rt} ${cy}"/><path id="pb" d="M${cx - rb} ${cy}A${rb} ${rb} 0 0 0 ${cx + rb} ${cy}"/>` +
          `<pattern id="tw" width="4" height="4" patternUnits="userSpaceOnUse" patternTransform="rotate(35)"><rect width="4" height="2" fill="#FFF" opacity="0.07"/></pattern>` +
          `<filter id="ps" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="4"/></filter></defs>` +
          `<circle cx="${cx + 3}" cy="${cy + 6}" r="${R}" fill="#000" opacity="0.45" filter="url(#ps)"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${R}" fill="${PATCH.gold}"/>` +
          `<g stroke="${PATCH.goldDark}" stroke-width="1.6" opacity="0.6">${satin}</g>` +
          `<circle cx="${cx}" cy="${cy}" r="${R - 14}" fill="${PATCH.navy}"/><circle cx="${cx}" cy="${cy}" r="${R - 14}" fill="url(#tw)"/>` +
          `<circle cx="${cx}" cy="${cy}" r="${R - 19}" fill="none" stroke="${PATCH.white}" stroke-width="1.3" stroke-dasharray="4 3" opacity="0.6"/>` +
          `<circle cx="${cx}" cy="${cy}" r="70" fill="${PATCH.gold}"/><circle cx="${cx}" cy="${cy}" r="66" fill="${PATCH.space}"/>` +
          [[-40, -30, 1.4], [44, -38, 1.1], [-50, 22, 1], [30, 44, 1.3], [52, 10, 0.9], [-20, -50, 1], [12, -54, 0.8], [-34, 46, 0.9]]
            .map(([dx, dy, r]) => `<circle cx="${cx + dx}" cy="${cy + dy}" r="${r * 1.6}" fill="${PATCH.white}"/>`).join('') +
          orbit(`M${cx - 58} ${cy + 6}A58 18 0 0 1 ${cx + 58} ${cy + 6}`) +
          OG.bitmap(PATCH_CRT, { k: '#1A1A22', b: '#D9CFB6', s: '#2B8FA3', w: PATCH.white, e: '#9EE6F5', r: PATCH.red }, { x: cx - 24, y: cy - 26, px: 3 }) +
          orbit(`M${cx + 58} ${cy + 6}A58 18 0 0 1 ${cx - 58} ${cy + 6}`) +
          `<circle cx="${cx + 47}" cy="${cy + 30}" r="7" fill="${PATCH.red}"/><circle cx="${cx + 45}" cy="${cy + 28}" r="2.5" fill="#FF8A6A"/>` +
          `<text font-size="${f(size)}" font-family="Jost, sans-serif" font-weight="700" fill="${PATCH.white}" letter-spacing="${f(size * 0.16)}" text-anchor="middle"><textPath href="#pt" startOffset="50%">${OG.esc(w)}</textPath></text>` +
          `<text font-size="17" font-family="Jost, sans-serif" font-weight="700" fill="${PATCH.gold}" letter-spacing="3" text-anchor="middle"><textPath href="#pb" startOffset="50%">32 · MISSION 1</textPath></text>` +
          star5(cx - 100, cy + 4, 7, PATCH.white) + star5(cx + 100, cy + 4, 7, PATCH.white)
      );
    },
    icon: () =>
      OG.iconSvg(
        `<circle cx="32" cy="32" r="31" fill="${PATCH.gold}"/><circle cx="32" cy="32" r="25" fill="${PATCH.navy}"/>` +
          `<circle cx="32" cy="32" r="22" fill="none" stroke="${PATCH.white}" stroke-width="1.5" stroke-dasharray="3 2" opacity="0.7"/>` +
          `<circle cx="32" cy="32" r="16" fill="${PATCH.space}"/>` +
          OG.bitmap(PATCH_CRT.slice(0, 13), { k: '#1A1A22', b: '#D9CFB6', s: '#2B8FA3', w: PATCH.white, e: '#9EE6F5', r: PATCH.red }, { x: 24, y: 25, px: 1 }) +
          `<ellipse cx="32" cy="34" rx="20" ry="6" fill="none" stroke="${PATCH.cyan}" stroke-width="2" transform="rotate(-18 32 34)"/>` +
          `<circle cx="47" cy="40" r="3.5" fill="${PATCH.red}"/>`
      ),
  });

  /* 57. Red Dot ----------------------------------------------------------- */

  // Where the dot sits in each letter it can live in, as fractions of the
  // letter's advance (x) and the font size (y above the baseline).
  const DOT_AT = { g: [0.56, 0.4], G: [0.61, 0.48] };

  OG.add({
    num: 57,
    id: 'red-dot',
    name: 'Red Dot',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['IBM & OS/2'],
    paletteTag: 'Corporate',
    risk: 'Safe',
    bg: '#161616',
    ink: '#E8E8E8',
    palette: ['#161616', '#E8E8E8', '#E4002B', '#5C5C5C'],
    fonts: ['IBM Plex Sans:ital,wght@1,600'],
    type: 'IBM Plex Sans SemiBold Italic, each letter placed by its measured advance, with a red dot set into the g.',
    concept:
      'The restraint of a black business laptop: one serious italic sans, matte black, and one red dot. ' +
      'On the keyboard the pointing stick sits by the G key, so here the dot sits in the g, where its counter would be.',
    logo: (w) => {
      const size = fitSize('IBM Plex Sans', w, 360, 118);
      const adv = [...w].map((ch) => (ADV['IBM Plex Sans'][ch] * size) / 100);
      const tw = adv.reduce((a, b) => a + b, 0);
      let x = 240 - tw / 2;
      const base = 150 + size * 0.3;
      let out = '', dot = '';
      [...w].forEach((ch, i) => {
        out += OG.text(ch, { x: f(x), y: f(base), size: f(size), family: "'IBM Plex Sans', sans-serif", weight: 600, italic: true, fill: '#E8E8E8' });
        if (DOT_AT[ch] && !dot) {
          const [fx, fy] = DOT_AT[ch], r = size * (ch === 'g' ? 0.085 : 0.075);
          const dx = x + adv[i] * fx, dy = base - size * fy;
          dot = `<circle cx="${f(dx)}" cy="${f(dy)}" r="${f(r + 3)}" fill="#161616"/><circle cx="${f(dx)}" cy="${f(dy)}" r="${f(r)}" fill="url(#nub)"/>`;
        }
        x += adv[i];
      });
      return OG.logoSvg(
        `<defs><radialGradient id="nub" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#FF3B4E"/><stop offset="0.7" stop-color="#E4002B"/><stop offset="1" stop-color="#A3001C"/></radialGradient></defs>` +
          out + dot
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs><radialGradient id="inub" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#FF3B4E"/><stop offset="0.7" stop-color="#E4002B"/><stop offset="1" stop-color="#A3001C"/></radialGradient></defs>` +
          `<rect width="64" height="64" rx="8" fill="#161616"/><rect x="0.5" y="0.5" width="63" height="63" rx="7.5" fill="none" stroke="#3A3A3A"/>` +
          `<circle cx="32" cy="32" r="13" fill="#0C0C0C"/><circle cx="32" cy="32" r="11" fill="url(#inub)"/>`
      ),
  });

  /* 62. Op Art ------------------------------------------------------------ */

  // Concentric black rings, period P; inside the clip they run half a period
  // out of step (and a touch heavier), so shapes appear only as a phase shift.
  function opRings(cx, cy, P, rMax, shifted, weight) {
    let out = '';
    for (let r = P / 4 + (shifted ? P / 2 : 0); r < rMax; r += P) out += `<circle cx="${cx}" cy="${cy}" r="${f(r)}"/>`;
    return `<g fill="none" stroke="#111" stroke-width="${f(P * weight)}">${out}</g>`;
  }

  OG.add({
    num: 62,
    id: 'op-art',
    name: 'Op Art',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Beyond computing'],
    paletteTag: 'Mono',
    risk: 'Wild',
    bg: '#F3F0E8',
    ink: '#111111',
    palette: ['#111111', '#F3F0E8', '#8A8780'],
    fonts: ['Anton'],
    type: 'Anton, used only as a mask: inside the letters the rings run half a period out of step and a little heavier, so no letterform is drawn at all.',
    concept:
      'Black and white rings spread from the center, and the name exists only where they slip half a step out of phase, as in a 1965 op-art canvas. ' +
      'The letters vibrate in and out of view, the way an interlaced CRT image shimmers when you look too closely.',
    logo: (w) => {
      const size = fitSize('Anton', w, 330, 172);
      const asc = /[OGS]/.test(w) ? 0.87 : 0.74, desc = w.includes('g') ? 0.13 : 0;
      const base = 150 + ((asc - desc) / 2) * size;
      const P = 8;
      return OG.logoSvg(
        `<defs><clipPath id="fd"><rect x="22" y="30" width="436" height="240" rx="120"/></clipPath>` +
          `<clipPath id="tx"><text x="240" y="${f(base)}" font-size="${f(size)}" font-family="Anton, sans-serif" text-anchor="middle">${OG.esc(w)}</text></clipPath></defs>` +
          `<g clip-path="url(#fd)"><rect width="480" height="300" fill="#F3F0E8"/>${opRings(240, 150, P, 260, false, 0.42)}` +
          `<g clip-path="url(#tx)"><rect width="480" height="300" fill="#F3F0E8"/>${opRings(240, 150, P, 260, true, 0.74)}</g></g>` +
          `<rect x="22" y="30" width="436" height="240" rx="120" fill="none" stroke="#111" stroke-width="3"/>`
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs><clipPath id="ic"><circle cx="32" cy="32" r="31"/></clipPath><clipPath id="io"><path d="M32 7A25 25 0 1 1 31.9 7ZM32 19A13 13 0 1 0 32.1 19Z" clip-rule="evenodd"/></clipPath></defs>` +
          `<g clip-path="url(#ic)"><rect width="64" height="64" fill="#F3F0E8"/>${opRings(32, 32, 6, 46, false, 0.38)}` +
          `<g clip-path="url(#io)"><rect width="64" height="64" fill="#F3F0E8"/>${opRings(32, 32, 6, 46, true, 0.78)}</g></g>` +
          `<circle cx="32" cy="32" r="30.5" fill="none" stroke="#111" stroke-width="1.5"/>`
      ),
  });

  /* 67. Fair Isle ---------------------------------------------------------- */

  const KNIT = { n: '#1F2A4C', c: '#F0E6CF', r: '#B32631', N: '#2B3A63' };

  // One knit stitch, 14 by 10: two leaning lobes, a V.
  const STITCH =
    `<g id="st"><path d="M7 11.2C3.8 10.4 0.4 6.4 0.5 1C0.6 -0.6 2.4 -1 3.8 0C5.8 1.8 7 6 7 11.2Z"/><path d="M7 11.2C10.2 10.4 13.6 6.4 13.5 1C13.4 -0.6 11.6 -1 10.2 0C8.2 1.8 7 6 7 11.2Z"/>` +
    `<path d="M2 1.5C2.6 4.6 4.4 7.4 6.2 8.6C5.4 5.6 4.4 2.6 2 1.5ZM12 1.5C11.4 4.6 9.6 7.4 7.8 8.6C8.6 5.6 9.6 2.6 12 1.5Z" fill="#FFF" opacity="0.16"/></g>`;

  // Knitting letters, five stitches wide; caps on rows 0 to 6, a g's tail on 7 and 8.
  const KNIT_GLYPHS = {
    O: ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.', '.....', '.....'],
    G: ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.', '.....', '.....'],
    S: ['.###.', '#...#', '#....', '.###.', '....#', '#...#', '.###.', '.....', '.....'],
    o: ['.....', '.....', '.###.', '#...#', '#...#', '#...#', '.###.', '.....', '.....'],
    g: ['.....', '.....', '.####', '#...#', '#...#', '#...#', '.####', '....#', '.###.'],
    s: ['.....', '.....', '.####', '#....', '.###.', '....#', '####.', '.....', '.....'],
  };

  // A row of a repeating motif, as many columns as needed.
  const rep = (unit, n) => unit.repeat(Math.ceil(n / unit.length)).slice(0, n);

  function sweater(w) {
    const cols = 34;
    let word = setRows([...w].map((ch) => KNIT_GLYPHS[ch] || KNIT_GLYPHS.O), 2);
    const used = word.map((r) => r.includes('#'));
    word = word.slice(used.indexOf(true), used.lastIndexOf(true) + 1);
    const top = [
      rep('nnncnn', cols), rep('n', cols), rep('r', cols), rep('c', cols),
      rep('ccnccc', cols), rep('cncncc', cols), rep('ccnccc', cols),
      rep('c', cols), rep('r', cols), rep('n', cols),
    ];
    const bottom = [
      rep('n', cols), rep('r', cols), rep('c', cols),
      rep('rcccrc', cols), rep('crcrcc', cols), rep('ccrccc', cols),
      rep('c', cols), rep('r', cols), rep('n', cols), rep('nnncnn', cols),
    ];
    // The middle band: 11 rows of red, a seed-stitch edge, and the word.
    const band = Array.from({ length: 11 }, (_, i) => (i === 0 || i === 10 ? rep('rN', cols) : rep('r', cols)));
    const y0 = 1 + Math.floor((9 - word.length) / 2), x0 = Math.floor((cols - word[0].length) / 2);
    word.forEach((row, j) => {
      const b = [...band[y0 + j]];
      [...row].forEach((ch, i) => { if (ch === '#') b[x0 + i] = 'c'; });
      band[y0 + j] = b.join('');
    });
    return [...top, ...band, ...bottom];
  }

  function knit(rows, x0, y0, cw, ch, colors) {
    return rows
      .map((row, j) => [...row].map((k, i) => `<use href="#st" x="${f(x0 + i * cw)}" y="${f(y0 + j * ch)}" fill="${colors[k]}"/>`).join(''))
      .join('');
  }

  OG.add({
    num: 67,
    id: 'fair-isle',
    name: 'Fair Isle',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Beyond computing'],
    paletteTag: 'Earthy',
    risk: 'Wild',
    scene: true,
    bg: KNIT.n,
    ink: KNIT.c,
    palette: [KNIT.r, KNIT.c, KNIT.n],
    fonts: [],
    type: 'Five-stitch knitting letters in cream on the red band; every pixel of the design is one V-shaped stitch.',
    concept:
      'A Fair Isle sweater: bands of little peerie motifs in red, cream and navy, and the name knitted into the middle band. ' +
      'Knitting charts are pixel art, a stitch to a cell, older than any screen; cozy computing for long winter evenings.',
    logo: (w) =>
      OG.logoSvg(
        `<defs>${STITCH}${grain('yarn', 1.4, 5, false, 1.2)}</defs><rect width="480" height="300" fill="#120E10"/>` +
          knit(sweater(w), 2, -5, 14, 10, KNIT) +
          `<rect width="480" height="300" filter="url(#yarn)" opacity="0.18"/>`
      ),
    icon: () => {
      const rows = ['nnnnnnn', 'rrcccrr', 'rcrrrcr', 'rcrrrcr', 'rcrrrcr', 'rcrrrcr', 'rcrrrcr', 'rrcccrr', 'nnnnnnn'];
      return OG.iconSvg(
        `<defs>${STITCH}</defs><rect width="64" height="64" rx="8" fill="#120E10"/>` +
          `<g transform="scale(0.653 0.705)">${knit(rows, 0, 0.5, 14, 10, KNIT)}</g>`
      );
    },
  });

  /* 72. Graph Paper ------------------------------------------------------- */

  // A kid's pixel letters, six squares wide with two-square strokes; caps on
  // rows 0 to 7, a g's tail on rows 8 and 9.
  const GP_GLYPHS = {
    O: ['.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', '......', '......'],
    G: ['.####.', '##..##', '##....', '##.###', '##..##', '##..##', '##..##', '.####.', '......', '......'],
    S: ['.####.', '##..##', '##....', '.####.', '....##', '....##', '##..##', '.####.', '......', '......'],
    o: ['......', '......', '.####.', '##..##', '##..##', '##..##', '##..##', '.####.', '......', '......'],
    g: ['......', '......', '.#####', '##..##', '##..##', '##..##', '##..##', '.#####', '....##', '.####.'],
    s: ['......', '......', '.#####', '##....', '.####.', '....##', '....##', '#####.', '......', '......'],
  };

  // Graphite: the shape's edges roughened, and paper grain showing through.
  const PENCIL =
    `<filter id="pen" x="-5%" y="-5%" width="110%" height="110%"><feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="8" result="n"/>` +
    `<feDisplacementMap in="SourceGraphic" in2="n" scale="1.4" xChannelSelector="R" yChannelSelector="G" result="d"/>` +
    `<feTurbulence type="fractalNoise" baseFrequency="1.6" numOctaves="1" seed="2" result="g"/>` +
    `<feColorMatrix in="g" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  -1.6 0 0 0 1.85" result="a"/><feComposite in="d" in2="a" operator="in"/></filter>`;

  // Squares filled by hand: each a little off the grid, a little off size.
  function shade(cells, cell, color, seed, op) {
    const rnd = OG.rng(seed);
    return cells
      .map(([c, r]) => {
        const j = () => (rnd() - 0.5) * 1.1;
        return `<rect x="${f(c * cell + 0.5 + j())}" y="${f(r * cell + 0.5 + j())}" width="${f(cell - 0.6 + j())}" height="${f(cell - 0.6 + j())}" fill="${color}" opacity="${f(op - rnd() * 0.15)}"/>`;
      })
      .join('');
  }

  const graphGrid = (id, cell, w, h) =>
    `<pattern id="${id}" width="${cell}" height="${cell}" patternUnits="userSpaceOnUse"><path d="M${cell} 0V${cell}M0 ${cell}H${cell}" fill="none" stroke="#8DB6E2" stroke-width="0.9"/></pattern>` +
    `<rect width="${w}" height="${h}" fill="url(#${id})"/>`;

  OG.add({
    num: 72,
    id: 'graph-paper',
    name: 'Graph Paper',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Print & packaging', 'Beyond computing'],
    paletteTag: 'Pastel',
    risk: 'Bold',
    scene: true,
    bg: '#F6F3EA',
    ink: '#4A4A50',
    palette: ['#F6F3EA', '#8DB6E2', '#4A4A50', '#2F6FD0', '#D9443A'],
    fonts: ['Gochi Hand'],
    type: 'Six-square pixel letters with two-square strokes, shaded in by hand in pencil with a blue colored-pencil shadow; the notes are Gochi Hand.',
    concept:
      'A page from a kid\'s 1985 notebook: the logo planned square by square on graph paper before typing it in, with a colored-pencil shadow and notes in the margin. ' +
      'Every pixel logo started like this, and OgOs is still made by hand.',
    logo: (w) => {
      const cell = 12;
      let rows = setRows([...w].map((ch) => GP_GLYPHS[ch] || GP_GLYPHS.O), 1);
      const used = rows.map((r) => r.includes('#'));
      rows = rows.slice(used.indexOf(true), used.lastIndexOf(true) + 1);
      const c0 = Math.round((40 - rows[0].length) / 2) + 1, r0 = Math.floor((25 - rows.length) / 2) - 1;
      const on = new Set(), cells = [], shadow = [];
      rows.forEach((row, j) => [...row].forEach((ch, i) => { if (ch === '#') { on.add(`${c0 + i},${r0 + j}`); cells.push([c0 + i, r0 + j]); } }));
      cells.forEach(([c, r]) => { if (!on.has(`${c + 1},${r + 1}`) && !shadow.some(([a, b]) => a === c + 1 && b === r + 1)) shadow.push([c + 1, r + 1]); });
      const hand = (t, x, y, size, rot, fill) => `<text x="${x}" y="${y}" font-size="${size}" font-family="'Gochi Hand', cursive" fill="${fill}" transform="rotate(${rot} ${x} ${y})">${OG.esc(t)}</text>`;
      const sw = [[30, 2, '#D9443A'], [31, 2, '#F2C53D'], [32, 2, '#3FA34D'], [33, 2, '#2F6FD0']];
      const yb = (r0 + rows.length + 1) * cell;
      return OG.logoSvg(
        `<defs>${PENCIL}${grain('pp', 0.8, 6, true, 1.2)}</defs><rect width="480" height="300" fill="#F6F3EA"/>` +
          graphGrid('gp', cell, 480, 300) +
          `<rect x="47" width="1.6" height="300" fill="#E58A8A"/>` +
          [50, 150, 250].map((y) => `<circle cx="22" cy="${y}" r="9" fill="#3A3631"/><circle cx="22" cy="${y - 1.5}" r="9" fill="#2A2622"/>`).join('') +
          `<g filter="url(#pen)">${shade(shadow, cell, '#5D95E2', 72, 0.72)}${shade(cells, cell, '#47474E', 7, 0.9)}${sw.map(([c, r, col], i) => shade([[c, r]], cell, col, 90 + i, 0.85)).join('')}</g>` +
          `<g filter="url(#pen)" fill="none" stroke="#5A5A60" stroke-width="1.6" stroke-linecap="round">` +
          `<path d="M352 50Q338 54 334 44"/><path d="M336 36L333 44L341 43"/>` +
          `<path d="M${f(c0 * cell + 18)} ${f(yb + 26)}Q${f(c0 * cell - 4)} ${f(yb + 20)} ${f(c0 * cell + 6)} ${f(yb - 4)}"/><path d="M${f(c0 * cell + 1)} ${f(yb + 1)}L${f(c0 * cell + 6)} ${f(yb - 5)}L${f(c0 * cell + 11)} ${f(yb + 1)}"/></g>` +
          `<g filter="url(#pen)">${hand('16 colors!', 356, 58, 22, -4, '#4A4A50')}${hand('blue = shadow', c0 * cell + 24, yb + 34, 19, -2, '#2F6FD0')}${hand('logo #3', 64, 34, 18, -3, '#4A4A50')}</g>` +
          `<rect width="480" height="300" filter="url(#pp)" opacity="0.12"/>`
      );
    },
    icon: () => {
      const rows = ['.####.', '##..##', '##..##', '##..##', '##..##', '.####.'];
      const cells = [];
      rows.forEach((row, j) => [...row].forEach((ch, i) => { if (ch === '#') cells.push([i + 1, j + 1]); }));
      return OG.iconSvg(
        `<defs>${PENCIL}</defs><rect width="64" height="64" rx="6" fill="#F6F3EA"/>` + graphGrid('igp', 8, 64, 64) +
          `<g filter="url(#pen)">${shade(cells.map(([c, r]) => [c + 1, r + 1]).filter(([c, r]) => !cells.some(([a, b]) => a === c && b === r)), 8, '#2F6FD0', 3, 0.75)}${shade(cells, 8, '#4A4A50', 4, 0.95)}</g>`
      );
    },
  });

  /* 77. Heraldry --------------------------------------------------------- */

  const HER = { blue: '#0000AA', yellow: '#FFFF55', red: '#AA0000', green: '#00AA00', ink: '#24180F', parch: '#EFE5CB', gold: '#C9A23C', cream: '#F5ECD3' };

  const HER_FLOPPY = [
    'kkkkkkkkkkk.',
    'kllgggggllkk',
    'kllgkkgglllk',
    'kllgkkgglllk',
    'kllggggglllk',
    'kllllllllllk',
    'klwwwwwwwwlk',
    'klwkkkkkkwlk',
    'klwwwwwwwwlk',
    'kkkkkkkkkkkk',
  ];
  const HER_CURSOR = ['k.......', 'kk......', 'kwk.....', 'kwwk....', 'kwwwk...', 'kwwwwk..', 'kwwwwwk.', 'kwwwwwwk', 'kwwwkkkk', 'kwkwk...', 'kk.kwk..', '...kwk..', '....kk..'];
  const HER_CRT = [
    '..kkkkkkkkkkkk..',
    '.kbbbbbbbbbbbbk.',
    'kbbkkkkkkkkkkbbk',
    'kbk' + 'ssssssssss' + 'kbk',
    'kbk' + 'sgssssssss' + 'kbk',
    'kbk' + 'ssgsssssss' + 'kbk',
    'kbk' + 'sgssgggsss' + 'kbk',
    'kbk' + 'ssssssssss' + 'kbk',
    'kbbkkkkkkkkkkbbk',
    'kbbbbbbbbbbbrbbk',
    '.kkkkkkkkkkkkkk.',
    '.....kbbbbk.....',
    '...kbbbbbbbbk...',
    '...kkkkkkkkkk...',
  ];

  function achievement(x, y, k) {
    // The shield, k = scale; quartered blue, yellow, red, green.
    const W = 128 * k, H = 146 * k, cx = x + W / 2;
    const d = `M${f(x)} ${f(y)}H${f(x + W)}V${f(y + 62 * k)}C${f(x + W)} ${f(y + 112 * k)} ${f(cx + 24 * k)} ${f(y + 134 * k)} ${f(cx)} ${f(y + H)}C${f(cx - 24 * k)} ${f(y + 134 * k)} ${f(x)} ${f(y + 112 * k)} ${f(x)} ${f(y + 62 * k)}Z`;
    const my = y + 64 * k;
    const P = { k: HER.ink, g: '#9AA0A8', w: '#FFFFFF', l: '#DCDCDC', b: '#E7DDC4', s: '#123C2A', r: '#FF3030' };
    return {
      d,
      svg:
        `<clipPath id="shc"><path d="${d}"/></clipPath>` +
        `<g clip-path="url(#shc)"><rect x="${f(x)}" y="${f(y)}" width="${f(W / 2)}" height="${f(my - y)}" fill="${HER.blue}"/><rect x="${f(cx)}" y="${f(y)}" width="${f(W / 2)}" height="${f(my - y)}" fill="${HER.yellow}"/>` +
        `<rect x="${f(x)}" y="${f(my)}" width="${f(W / 2)}" height="${f(H)}" fill="${HER.red}"/><rect x="${f(cx)}" y="${f(my)}" width="${f(W / 2)}" height="${f(H)}" fill="${HER.green}"/></g>` +
        `<g shape-rendering="crispEdges">${OG.bitmap(HER_FLOPPY, P, { x: x + W / 4 - 18 * k, y: y + 32 * k - 15 * k, px: 3 * k })}` +
        OG.bitmap(HER_CURSOR, P, { x: cx + W / 4 - 11 * k, y: my + 30 * k - 20 * k, px: 3 * k }) + `</g>` +
        `<path d="${d}" fill="none" stroke="${HER.ink}" stroke-width="${f(4 * k)}" stroke-linejoin="round"/>` +
        `<path d="${d}" fill="none" stroke="${HER.gold}" stroke-width="${f(1.5 * k)}" transform="translate(${f(cx)} ${f(y + H / 2)}) scale(0.94) translate(${f(-cx)} ${f(-y - H / 2)})"/>`,
    };
  }

  OG.add({
    num: 77,
    id: 'heraldry',
    name: 'Heraldry',
    caps: 'OGOS',
    flexCase: true,
    family: 'Emblem',
    sources: ['Beyond computing', 'Windows 9x'],
    paletteTag: 'VGA 16',
    risk: 'Wild',
    bg: HER.parch,
    ink: HER.ink,
    palette: [HER.blue, HER.yellow, HER.red, HER.green, HER.parch, HER.ink],
    fonts: ['Cinzel:wght@400;700'],
    type: 'Cinzel, an inscriptional Roman, for the name and the motto; the charges and the crest are pixel art.',
    concept:
      'A coat of arms for the system: a shield quartered in four VGA colors, charged with a floppy disk and an arrow pointer, a CRT for a helm on a twisted torse, and the motto Pixelis Aeternum, pixels forever. ' +
      'Grand and a little absurd, as a family crest for a hobby OS should be.',
    logo: (w) => {
      const A = achievement(56, 82, 1);
      const P = { k: HER.ink, b: '#E2D6B8', s: '#123C2A', g: '#4BE38A', r: '#E53B2E' };
      const torse = [0, 1, 2, 3, 4, 5].map((i) => `<path d="M${76 + i * 15} 67h15l-5 10h-15z" fill="${i % 2 ? HER.yellow : HER.blue}" stroke="${HER.ink}" stroke-width="1.6" stroke-linejoin="round"/>`).join('');
      // Mantling: cloth leaves falling from the helm on both sides, blue lined with yellow.
      const leaf = 'M86 72Q62 62 48 78Q40 70 30 74Q42 84 42 100Q32 98 22 106Q36 114 36 130Q26 132 20 144Q34 148 40 160Q38 170 46 182Q54 172 58 168L58 84Z';
      const lining = 'M80 76Q64 72 54 84Q50 80 44 80Q50 90 50 104Q44 106 38 110Q46 118 46 132Q40 136 36 144Q46 150 50 160Q50 166 52 172L58 166L58 86Z';
      const mantle = (tf) => `<g transform="${tf}"><path d="${leaf}" fill="${HER.blue}" stroke="${HER.ink}" stroke-width="2" stroke-linejoin="round"/><path d="${lining}" fill="${HER.yellow}"/></g>`;
      const mantling = mantle('') + mantle('translate(240 0) scale(-1 1)');
      const rb = 'M30 236Q120 262 210 236';
      const ribbon =
        `<path d="M22 232L34 230L36 252L22 256L28 245Z" fill="#D9CBA6" stroke="${HER.ink}" stroke-width="1.8" stroke-linejoin="round"/>` +
        `<path d="M218 232L206 230L204 252L218 256L212 245Z" fill="#D9CBA6" stroke="${HER.ink}" stroke-width="1.8" stroke-linejoin="round"/>` +
        `<path d="M30 226Q120 252 210 226L210 248Q120 274 30 248Z" fill="${HER.cream}" stroke="${HER.ink}" stroke-width="2" stroke-linejoin="round"/>` +
        `<path id="mt" d="${rb}" fill="none"/>` +
        `<text font-size="12.5" font-family="Cinzel, serif" font-weight="700" fill="${HER.red}" letter-spacing="1.6" text-anchor="middle"><textPath href="#mt" startOffset="50%">PIXELIS AETERNUM</textPath></text>`;
      const size = fitSize('Cinzel', w, 210, 80);
      return OG.logoSvg(
        mantling + `<g shape-rendering="crispEdges">${OG.bitmap(HER_CRT, P, { x: 92.8, y: 16, px: 3.4 })}</g>` +
          A.svg + torse + ribbon +
          OG.text('THE ARMS OF', { x: 340, y: 106, size: 13, family: 'Cinzel, serif', weight: 400, fill: HER.ink, anchor: 'middle', ls: 4 }) +
          OG.text(w, { x: 340, y: f(130 + size * 0.71), size: f(size), family: 'Cinzel, serif', weight: 700, fill: HER.ink, anchor: 'middle', ls: 2 }) +
          `<rect x="252" y="${f(144 + size * 0.71)}" width="176" height="1.5" fill="${HER.ink}"/><rect x="252" y="${f(148 + size * 0.71)}" width="176" height="0.8" fill="${HER.ink}"/>` +
          OG.text('ANNO MMXXVI', { x: 340, y: f(168 + size * 0.71), size: 12, family: 'Cinzel, serif', weight: 400, fill: HER.ink, anchor: 'middle', ls: 4 })
      );
    },
    icon: () => OG.iconSvg(achievement(7, 2, 0.39).svg.replace(/shc/g, 'ishc')),
  });

  /* 82. Rubber Stamp ------------------------------------------------------ */

  // Worn ink: a mask with speckled holes and patchy pressure, and roughened edges.
  const WEAR =
    `<filter id="rough" x="-5%" y="-5%" width="110%" height="110%"><feTurbulence type="fractalNoise" baseFrequency="0.7" numOctaves="2" seed="4"/><feDisplacementMap in="SourceGraphic" scale="2.4" xChannelSelector="R" yChannelSelector="G"/></filter>` +
    `<filter id="holes" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="0.55" numOctaves="3" seed="11"/>` +
    `<feColorMatrix values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  3.2 0 0 0 -1.75"/></filter>` +
    `<filter id="patch" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="0.018" numOctaves="2" seed="3"/>` +
    `<feColorMatrix values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  1.6 0 0 0 -0.75"/></filter>` +
    `<mask id="wear" x="0" y="0" width="100%" height="100%"><rect width="480" height="300" fill="#FFF"/><rect width="480" height="300" filter="url(#holes)"/><rect width="480" height="300" filter="url(#patch)"/></mask>`;

  OG.add({
    num: 82,
    id: 'rubber-stamp',
    name: 'Rubber Stamp',
    caps: 'OGOS',
    flexCase: true,
    family: 'Emblem',
    sources: ['Print & packaging'],
    paletteTag: 'Mono',
    risk: 'Bold',
    bg: '#F2EEE3',
    ink: '#C42430',
    palette: ['#C42430', '#F2EEE3', '#2B2B2B'],
    fonts: ['Alfa Slab One', 'Black Ops One'],
    type: 'Alfa Slab One for the name, Black Ops One for the inspection lines, all inked once and worn by a speckled mask.',
    concept:
      'A red rubber stamp thumped onto the page at an angle: double border, the name big, APPROVED FOR RETRO USE knocked out of a band, ink missing where the pad ran dry. ' +
      'Every disk, manual and box in the 90s carried a stamp like it; this one certifies the software as properly old-school.',
    logo: (w) => {
      const red = '#C42430';
      const size = fitSize('Alfa Slab One', w, 300, 96);
      return OG.logoSvg(
        `<defs>${WEAR}</defs>` +
          `<g mask="url(#wear)"><g filter="url(#rough)" transform="rotate(-6 240 150)" fill="${red}">` +
          `<rect x="50" y="60" width="380" height="180" rx="12" fill="none" stroke="${red}" stroke-width="6"/>` +
          `<rect x="61" y="71" width="358" height="158" rx="5" fill="none" stroke="${red}" stroke-width="2"/>` +
          OG.text('★ OFFICIAL · 32-BIT ★', { x: 240, y: 98, size: 16, family: "'Black Ops One', sans-serif", fill: red, anchor: 'middle', ls: 3 }) +
          OG.text(w, { x: 240, y: f(110 + size * 0.79), size: f(size), family: "'Alfa Slab One', serif", fill: red, anchor: 'middle', ls: 3 }) +
          `<rect x="61" y="193" width="358" height="36" fill="${red}"/>` +
          OG.text('APPROVED FOR RETRO USE', { x: 240, y: 218, size: 19, family: "'Black Ops One', sans-serif", fill: '#F2EEE3', anchor: 'middle', ls: 2 }) +
          `</g></g>`
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs>${WEAR.replace(/480/g, '64').replace(/300/g, '64')}</defs>` +
          `<g mask="url(#wear)"><g transform="rotate(-10 32 32)" fill="#C42430">` +
          `<circle cx="32" cy="32" r="29" fill="none" stroke="#C42430" stroke-width="4"/><circle cx="32" cy="32" r="23" fill="none" stroke="#C42430" stroke-width="1.6"/>` +
          OG.text('OG', { x: 32, y: 41, size: 24, family: "'Alfa Slab One', serif", fill: '#C42430', anchor: 'middle' }) +
          `<path d="M32 10l1.5 3h3.2l-2.6 2 1 3.2-3.1-2-3.1 2 1-3.2-2.6-2h3.2z"/><path d="M32 46l1.5 3h3.2l-2.6 2 1 3.2-3.1-2-3.1 2 1-3.2-2.6-2h3.2z"/>` +
          `</g></g>`
      ),
  });

  /* 87. Label Maker ------------------------------------------------------- */

  // A strip of embossing tape with V-notched ends, and raised letters, one per cell.
  function tape(x, y, cell, h, text, opts) {
    const { color, dark, size, family, seed, pad } = opts;
    const W = text.length * cell + 2 * pad, n = h * 0.18;
    const d = `M${f(x)} ${f(y)}H${f(x + W)}L${f(x + W - n)} ${f(y + h / 2)}L${f(x + W)} ${f(y + h)}H${f(x)}L${f(x + n)} ${f(y + h / 2)}Z`;
    const rnd = OG.rng(seed);
    const id = 'tp' + seed;
    let letters = '';
    [...text].forEach((ch, i) => {
      if (ch === ' ') return;
      const cx = x + pad + cell * (i + 0.5), rot = (rnd() - 0.5) * 5, dy = (rnd() - 0.5) * 3;
      const by = y + h / 2 + size * 0.42 + dy;
      const t = (fill, dx, ddy, extra) => `<text x="${f(cx + dx)}" y="${f(by + ddy)}" font-size="${f(size)}" font-family="${family}" fill="${fill}" text-anchor="middle" transform="rotate(${f(rot)} ${f(cx)} ${f(by)})" ${extra || ''}>${OG.esc(ch)}</text>`;
      letters += t('#FFFFFF', 0, 0, `filter="url(#${id}h)" opacity="0.45"`) + t(dark, 1.4, 1.8, '') + t('#FFFFFF', -0.6, -0.7, 'opacity="0.9"') + t('#F3EFEE', 0, 0, '');
    });
    return (
      `<defs><linearGradient id="${id}g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${color[0]}"/><stop offset="1" stop-color="${color[1]}"/></linearGradient>` +
      `<filter id="${id}h" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="2.6"/></filter>` +
      `<filter id="${id}s" x="-10%" y="-30%" width="120%" height="160%"><feGaussianBlur stdDeviation="2.5"/></filter></defs>` +
      `<path d="${d}" fill="#000" opacity="0.3" transform="translate(2 4)" filter="url(#${id}s)"/>` +
      `<path d="${d}" fill="url(#${id}g)"/>` +
      `<rect x="${f(x + n)}" y="${f(y + h * 0.1)}" width="${f(W - 2 * n)}" height="${f(h * 0.07)}" fill="#FFF" opacity="0.11"/>` +
      `<rect x="${f(x + n)}" y="${f(y + h * 0.86)}" width="${f(W - 2 * n)}" height="${f(h * 0.05)}" fill="#000" opacity="0.12"/>` +
      letters
    );
  }

  OG.add({
    num: 87,
    id: 'label-maker',
    name: 'Label Maker',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['Print & packaging'],
    paletteTag: 'Primary',
    risk: 'Bold',
    bg: '#D9D0BC',
    ink: '#2A2622',
    palette: ['#D2232C', '#F3EFEE', '#1E1E22', '#D9D0BC'],
    fonts: ['Fjalla One'],
    type: 'Fjalla One, one letter per fixed-pitch cell as the label maker\'s wheel prints them, each a little crooked and raised: a dark edge below, a highlight above, a pale halo of stressed plastic.',
    concept:
      'Embossed label tape stuck on a beige PC case: red tape, raised white letters, a little uneven, V-notched ends, and a smaller black tape below it. ' +
      'Everything on a 90s desk got labeled like this, the computer most of all.',
    logo: (w) => {
      const cell = 56, pad = 38, W1 = w.length * cell + 2 * pad;
      const sub = '32-BIT RETRO OS', cell2 = 15.5, W2 = sub.length * cell2 + 2 * 16;
      return OG.logoSvg(
        tape(240 - W1 / 2, 88, cell, 90, w, { color: ['#DB2C35', '#BF1B25'], dark: '#7A0B13', size: 68, family: "'Fjalla One', sans-serif", seed: 1, pad }) +
          `<g transform="rotate(2.5 ${f(240 + W1 / 2 - W2 / 2)} 214)">` +
          tape(240 + W1 / 2 - W2 - 6, 200, cell2, 34, sub, { color: ['#2C2C31', '#141417'], dark: '#000', size: 21, family: "'Fjalla One', sans-serif", seed: 2, pad: 16 }) +
          `</g>`
      );
    },
    icon: () =>
      OG.iconSvg(tape(2, 15, 25, 34, 'OG', { color: ['#E1323B', '#B5151F'], dark: '#7A0B13', size: 28, family: "'Fjalla One', sans-serif", seed: 3, pad: 5 })),
  });

  /* 92. Masthead ---------------------------------------------------------- */

  OG.add({
    num: 92,
    id: 'masthead',
    name: 'Masthead',
    caps: 'OGOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Print & packaging'],
    paletteTag: 'Primary',
    risk: 'Bold',
    scene: true,
    bg: '#F7F2E7',
    ink: '#1A1714',
    palette: ['#D7262B', '#F7F2E7', '#1A1714', '#F6C431'],
    fonts: ['Anton', 'Spectral:ital,wght@1,600'],
    type: 'Anton, a heavy condensed grotesque, knocked out of the red band; cover lines in Anton and Spectral SemiBold Italic.',
    concept:
      'The top of a 1990s computer-magazine cover: the name knocked out of a red band, the issue line, and cover lines promising raycasters, TinyCC and FAT16. ' +
      'OgOs as a whole magazine of a system, with something to type in every month.',
    logo: (w) => {
      const size = 150, red = '#D7262B', paper = '#F7F2E7', ink = '#1A1714';
      const wd = advance('Anton', w) * size + (w.length - 1) * -2;
      const right = 24 + wd + 16;
      const stack = ['THE RETRO', 'CODING', 'MAGAZINE'];
      return OG.logoSvg(
        `<rect width="480" height="300" fill="${paper}"/>` +
          OG.text('VOL. 1 · NO. 32', { x: 24, y: 24, size: 12, family: 'Anton, sans-serif', fill: ink, ls: 1.5 }) +
          OG.text('OCTOBER 2026 · $3.95 · £2.50', { x: 456, y: 24, size: 12, family: 'Anton, sans-serif', fill: ink, anchor: 'end', ls: 1.5 }) +
          `<rect y="32" width="480" height="140" fill="${red}"/>` +
          OG.text(w, { x: 24, y: f(32 + 140 - 10 - (w.includes('g') ? size * 0.1 : 0)), size, family: 'Anton, sans-serif', fill: paper, ls: -2 }) +
          `<rect x="${f(right)}" y="52" width="3" height="100" fill="${paper}"/>` +
          stack.map((t, i) => OG.text(t, { x: f(right + 14), y: 82 + i * 30, size: 27, family: 'Anton, sans-serif', fill: paper, ls: 1 })).join('') +
          `<rect y="172" width="480" height="5" fill="${ink}"/>` +
          OG.text('INSIDE THE KERNEL', { x: 24, y: 202, size: 12, family: 'Anton, sans-serif', fill: red, ls: 2 }) +
          OG.text('BUILD A RAYCASTER IN 32-BIT C', { x: 24, y: 230, size: 25, family: 'Anton, sans-serif', fill: ink }) +
          OG.text('TinyCC: the compiler that fits on a floppy', { x: 24, y: 256, size: 16, family: "'Spectral', serif", weight: 600, italic: true, fill: ink }) +
          OG.text('FAT16, byte by byte  ·  Alt+Tab, explained', { x: 24, y: 280, size: 16, family: "'Spectral', serif", weight: 600, italic: true, fill: ink }) +
          `<g transform="rotate(-10 404 238)"><circle cx="404" cy="238" r="46" fill="#F6C431"/><circle cx="404" cy="238" r="41" fill="none" stroke="${red}" stroke-width="1.5" stroke-dasharray="3 3"/>` +
          OG.text('FREE', { x: 404, y: 229, size: 24, family: 'Anton, sans-serif', fill: red, anchor: 'middle' }) +
          OG.text('3½" DISK', { x: 404, y: 252, size: 18, family: 'Anton, sans-serif', fill: ink, anchor: 'middle' }) +
          OG.text('INSIDE!', { x: 404, y: 270, size: 13, family: 'Anton, sans-serif', fill: ink, anchor: 'middle', ls: 1 }) + `</g>`
      );
    },
    icon: () =>
      OG.iconSvg(`<rect width="64" height="64" fill="#D7262B"/><rect y="56" width="64" height="8" fill="#1A1714"/>` + OG.text('OG', { x: 32, y: 49, size: 50, family: 'Anton, sans-serif', fill: '#F7F2E7', anchor: 'middle', ls: -1 })),
  });

  /* 97. Manual ------------------------------------------------------------ */

  const MAN = { cream: '#EEE7D6', ink: '#1C1A17', stripes: ['#4E2E1A', '#7A3F1C', '#A9501E', '#D2691E', '#E98A27', '#F2AE35', '#F6CF4A'] };

  function manualStripes(x, y, w, h, n) {
    const gap = h / n * 0.28, sh = (h - gap * (n - 1)) / n;
    return MAN.stripes.slice(0, n).map((c, i) => `<rect x="${f(x)}" y="${f(y + i * (sh + gap))}" width="${f(w)}" height="${f(sh)}" fill="${c}"/>`).join('');
  }

  OG.add({
    num: 97,
    id: 'manual',
    name: 'Manual',
    caps: 'OgOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Print & packaging'],
    paletteTag: 'Earthy',
    risk: 'Safe',
    scene: true,
    bg: MAN.cream,
    ink: MAN.ink,
    palette: [MAN.cream, MAN.stripes[0], MAN.stripes[3], MAN.stripes[6], MAN.ink],
    fonts: ['Inter Tight:wght@400;800'],
    type: 'Inter Tight, a neo-grotesque in the Swiss manner, ExtraBold for the name and Regular for the rest, all hung from one left edge.',
    concept:
      'The cover of a 1983 computer manual: a Swiss grid, flush-left type and a stack of brown, orange and yellow stripes running off the right edge. ' +
      'Calm, competent and warm, the binder you would actually read: the system as a well-documented tool.',
    logo: (w) => {
      const size = fitSize('Inter Tight 800', w, 236, 84), L = 32, tb = 130;
      return OG.logoSvg(
        `<rect width="480" height="300" fill="${MAN.cream}"/>` +
          manualStripes(296, 46, 184, 208, 7) +
          OG.text('32-BIT OPERATING SYSTEM FOR i686 PERSONAL COMPUTERS', { x: L, y: 40, size: 8.5, family: "'Inter Tight', sans-serif", weight: 400, fill: MAN.ink, ls: 1.1 }) +
          `<rect x="${L}" y="48" width="240" height="1.5" fill="${MAN.ink}"/>` +
          OG.text(w, { x: L - 3, y: tb, size: f(size), family: "'Inter Tight', sans-serif", weight: 800, fill: MAN.ink, ls: -size * 0.03 }) +
          OG.text("User's Guide", { x: L, y: tb + 56, size: 27, family: "'Inter Tight', sans-serif", weight: 400, fill: MAN.ink, ls: -0.5 }) +
          OG.text('Volume 1', { x: L, y: tb + 82, size: 17, family: "'Inter Tight', sans-serif", weight: 400, fill: MAN.stripes[2] }) +
          `<rect x="${L}" y="246" width="240" height="1" fill="${MAN.ink}"/>` +
          OG.text('Part No. 0x10000 · First Edition', { x: L, y: 262, size: 8.5, family: "'Inter Tight', sans-serif", weight: 400, fill: MAN.ink, ls: 0.8 })
      );
    },
    icon: () =>
      OG.iconSvg(`<rect width="64" height="64" rx="6" fill="${MAN.cream}"/>` + `<g transform="translate(8 8)">${manualStripes(0, 0, 48, 48, 6)}</g>`),
  });
})();
