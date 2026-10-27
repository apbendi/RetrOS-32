/* Batch C: games, demoscene and the early web (directions 5, 10, 15 ... 100). */
(function () {
  const { C } = OG;

  /* ================================================================ helpers */

  const n2 = (v) => +(+v).toFixed(2);
  const isUp = (c) => c !== c.toLowerCase();

  // The glyph for a character, falling back to the other case.
  const glyphOf = (alpha, ch) => alpha[ch] || alpha[ch.toUpperCase()] || alpha[ch.toLowerCase()];

  // Lays a word out from an alphabet of equal-height bitmaps.
  function layWord(alpha, w, gap = 1) {
    let x = 0;
    const glyphs = [];
    [...w].forEach((ch) => {
      const rows = glyphOf(alpha, ch);
      if (!rows) return;
      if (glyphs.length) x += gap;
      glyphs.push({ ch, rows, x });
      x += rows[0].length;
    });
    return { glyphs, width: x, height: glyphs.length ? glyphs[0].rows.length : 0 };
  }

  // A word as one set of rows ('#' set, '.' clear).
  function wordRows(alpha, w, gap = 1) {
    const L = layWord(alpha, w, gap);
    const rows = [];
    for (let j = 0; j < L.height; j++) {
      let s = '';
      L.glyphs.forEach((g, k) => (s += (k ? '.'.repeat(gap) : '') + g.rows[j]));
      rows.push(s);
    }
    return rows;
  }

  const blank = (w, h, ch = '.') => Array.from({ length: h }, () => Array(w).fill(ch));
  const toRows = (cv) => cv.map((r) => r.join(''));
  const on = (cv, i, j) => j >= 0 && j < cv.length && i >= 0 && i < cv[0].length && cv[j][i] !== '.';

  // The pixels just outside a shape (8-neighbourhood, or 4 with diag false).
  function outlineOf(cv, ch = 'k', diag = true) {
    const H = cv.length, W = cv[0].length;
    const out = blank(W, H);
    for (let j = 0; j < H; j++)
      for (let i = 0; i < W; i++) {
        if (on(cv, i, j)) continue;
        let hit = false;
        for (let dy = -1; dy <= 1 && !hit; dy++)
          for (let dx = -1; dx <= 1 && !hit; dx++) {
            if (!diag && dx && dy) continue;
            if (on(cv, i + dx, j + dy)) hit = true;
          }
        if (hit) out[j][i] = ch;
      }
    return out;
  }

  // Mixes two #RRGGBB colors; t = 0 gives a, 1 gives b.
  function mix(a, b, t) {
    const p = (c, k) => parseInt(c.substr(1 + 2 * k, 2), 16);
    return '#' + [0, 1, 2].map((k) => Math.round(p(a, k) + (p(b, k) - p(a, k)) * t).toString(16).padStart(2, '0')).join('').toUpperCase();
  }

  // A 4-point sparkle centred on x, y.
  const sparkle = (x, y, r, fill, extra = '') =>
    `<path d="M${n2(x)} ${n2(y - r)}Q${n2(x + r * 0.12)} ${n2(y - r * 0.12)} ${n2(x + r)} ${n2(y)}Q${n2(x + r * 0.12)} ${n2(y + r * 0.12)} ${n2(x)} ${n2(y + r)}Q${n2(x - r * 0.12)} ${n2(y + r * 0.12)} ${n2(x - r)} ${n2(y)}Q${n2(x - r * 0.12)} ${n2(y - r * 0.12)} ${n2(x)} ${n2(y - r)}Z" fill="${fill}" ${extra}/>`;

  /*
   * Stroke-skeleton letters for big pixel masks: O, G and S (and o, g, s)
   * as superellipse arcs of an even stroke, rasterized onto a cell grid.
   * Units: cap height 1, y = 0 at the cap top, 1 at the baseline.
   *   o: t (stroke), n (superellipse exponent), cw / lw (cap / lowercase
   *   width), xh (x-height), ds (descender), lt (lowercase stroke factor)
   */
  function supPt(cx, cy, a, b, th, n) {
    const c = Math.cos(th), s = Math.sin(th), e = 2 / n;
    return [cx + a * Math.sign(c) * Math.pow(Math.abs(c), e), cy - b * Math.sign(s) * Math.pow(Math.abs(s), e)];
  }
  function arcPts(cx, cy, a, b, t0, t1, n, steps = 48) {
    const pts = [];
    for (let k = 0; k <= steps; k++) pts.push(supPt(cx, cy, a, b, ((t0 + ((t1 - t0) * k) / steps) * Math.PI) / 180, n));
    return pts;
  }
  function skeleton(ch, o) {
    const up = isUp(ch);
    const W = up ? o.cw : o.lw;
    let t = up ? o.t : o.t * (o.lt || 0.74), n = o.n;
    const top = up ? 0 : 1 - o.xh, bot = 1, cx = W / 2;
    const L = ch.toLowerCase();
    if (L === 's' && !up) { t *= 0.86; n = 2.2; }
    const h = t / 2, a = W / 2 - h, cy = (top + bot) / 2, b = (bot - top) / 2 - h;
    if (L === 'o') return { w: W, t, p: [arcPts(cx, cy, a, b, 0, 360, n)] };
    if (ch === 'G') return { w: W, t, p: [arcPts(cx, cy, a, b, 34, 360, n), [[cx + a, cy + 0.03], [cx - 0.02, cy + 0.03]]] };
    if (ch === 'g') {
      const sb = 1 + o.ds * 0.1;
      return { w: W, t, p: [arcPts(cx, cy, a, b, 0, 360, n), [[cx + a, cy], [cx + a, sb]], arcPts(cx, sb, a, Math.max(0.01, 1 + o.ds - h - sb), 0, -160, n)] };
    }
    if (L === 's') {
      const mid = (top + bot) / 2, op = up ? 20 : 25;
      const b1 = (mid - (top + h)) / 2, b2 = (bot - h - mid) / 2;
      return { w: W, t, p: [arcPts(cx, top + h + b1, a, b1, op, 270, n), arcPts(cx, mid + b2, a, b2, 90, -(180 - op), n)] };
    }
    return null;
  }
  function segDist(px, py, ax, ay, bx, by) {
    const dx = bx - ax, dy = by - ay, l = dx * dx + dy * dy;
    const u = l ? Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / l)) : 0;
    return Math.hypot(ax + u * dx - px, ay + u * dy - py);
  }
  // A glyph as rows of '#'/'.', capRows cells to the cap height, square cells.
  const rasterCache = {};
  function rasterGlyph(ch, capRows, o) {
    const key = ch + capRows + JSON.stringify(o);
    if (rasterCache[key]) return rasterCache[key];
    const sk = skeleton(ch, o);
    const cs = 1 / capRows, cols = Math.round(sk.w / cs), rows = Math.round((1 + o.ds) / cs);
    const out = [];
    for (let j = 0; j < rows; j++) {
      let s = '';
      for (let i = 0; i < cols; i++) {
        const x = (i + 0.5) * (sk.w / cols), y = (j + 0.5) * cs;
        let d = 9;
        sk.p.forEach((pl) => { for (let k = 1; k < pl.length; k++) d = Math.min(d, segDist(x, y, pl[k - 1][0], pl[k - 1][1], pl[k][0], pl[k][1])); });
        s += d <= sk.t / 2 ? '#' : '.';
      }
      out.push(s);
    }
    return (rasterCache[key] = out);
  }
  // An alphabet of rasterized glyphs for layWord.
  const rasterAlpha = (capRows, o) => {
    const a = {};
    ['O', 'G', 'S', 'o', 'g', 's'].forEach((c) => (a[c] = rasterGlyph(c, capRows, o)));
    return a;
  };

  // A tiny 3x5 pixel font for captions and badge lettering.
  const TINY = {
    0: ['###', '#.#', '#.#', '#.#', '###'], 1: ['.#.', '##.', '.#.', '.#.', '###'], 2: ['###', '..#', '###', '#..', '###'],
    3: ['###', '..#', '.##', '..#', '###'], 4: ['#.#', '#.#', '###', '..#', '..#'], 5: ['###', '#..', '###', '..#', '###'],
    6: ['###', '#..', '###', '#.#', '###'], 7: ['###', '..#', '.#.', '.#.', '.#.'], 8: ['###', '#.#', '###', '#.#', '###'],
    9: ['###', '#.#', '###', '..#', '###'], A: ['.#.', '#.#', '###', '#.#', '#.#'], B: ['##.', '#.#', '##.', '#.#', '##.'],
    C: ['.##', '#..', '#..', '#..', '.##'], D: ['##.', '#.#', '#.#', '#.#', '##.'], E: ['###', '#..', '##.', '#..', '###'],
    F: ['###', '#..', '##.', '#..', '#..'], G: ['.##', '#..', '#.#', '#.#', '.##'], H: ['#.#', '#.#', '###', '#.#', '#.#'],
    I: ['###', '.#.', '.#.', '.#.', '###'], K: ['#.#', '#.#', '##.', '#.#', '#.#'], L: ['#..', '#..', '#..', '#..', '###'],
    M: ['#...#', '##.##', '#.#.#', '#...#', '#...#'], N: ['#..#', '##.#', '#.##', '#..#', '#..#'], O: ['.#.', '#.#', '#.#', '#.#', '.#.'],
    P: ['##.', '#.#', '##.', '#..', '#..'], R: ['##.', '#.#', '##.', '#.#', '#.#'], S: ['.##', '#..', '.#.', '..#', '##.'],
    T: ['###', '.#.', '.#.', '.#.', '.#.'], U: ['#.#', '#.#', '#.#', '#.#', '###'], V: ['#.#', '#.#', '#.#', '#.#', '.#.'],
    W: ['#...#', '#...#', '#.#.#', '##.##', '#...#'], X: ['#.#', '#.#', '.#.', '#.#', '#.#'], Y: ['#.#', '#.#', '.#.', '.#.', '.#.'],
    x: ['...', '#.#', '.#.', '#.#', '...'], ' ': ['..', '..', '..', '..', '..'], '!': ['#', '#', '#', '.', '#'], '.': ['.', '.', '.', '.', '#'],
    ':': ['.', '#', '.', '#', '.'], '-': ['...', '...', '###', '...', '...'], '/': ['..#', '..#', '.#.', '#..', '#..'],
  };
  const tinyRows = (str) => {
    const rows = ['', '', '', '', ''];
    [...str].forEach((ch, k) => {
      const g = TINY[ch] || TINY[ch.toUpperCase()] || TINY[' '];
      for (let r = 0; r < 5; r++) rows[r] += (k ? '.' : '') + g[r];
    });
    return rows;
  };
  const tinyWidth = (str) => tinyRows(str)[0].length;
  const tiny = (str, x, y, px, fill) => OG.bitmap(tinyRows(str), { '#': fill }, { x, y, px });

  /* ============================================================ 5. Title Screen */

  // A chunky 16-bit title face: caps 11x12, lowercase x-height 9, descender 3.
  const B3 = '...........';
  const TS = {
    O: ['...#####...', '.#########.', '###########', '####...####', '###.....###', '###.....###', '###.....###', '###.....###', '####...####', '###########', '.#########.', '...#####...', B3, B3, B3],
    G: ['...######..', '.##########', '###########', '####.......', '###........', '###...#####', '###...#####', '###.....###', '####...####', '###########', '.##########', '...#####...', B3, B3, B3],
    S: ['..#######..', '.#########.', '####...####', '###........', '####.......', '.########..', '..########.', '.......####', '........###', '####...####', '.#########.', '..#######..', B3, B3, B3],
    o: ['..........', '..........', '..........', '..######..', '.########.', '####..####', '###....###', '###....###', '###....###', '####..####', '.########.', '..######..', '..........', '..........', '..........'],
    g: ['..........', '..........', '..........', '..########', '.#########', '####..####', '###....###', '###....###', '####..####', '.#########', '..########', '.......###', '###...####', '.#########', '..#######.'],
    s: ['..........', '..........', '..........', '..#######.', '.#########', '####...###', '#####.....', '.########.', '.....#####', '###...####', '#########.', '.#######..', '..........', '..........', '..........'],
  };

  // The word as pixel art: shading tones, a black outline and a hard shadow.
  function titleArt(w, bounce) {
    const L = layWord(TS, w, 1);
    const M = 3;
    const W = L.width + 2 * M, H = L.height + 2 * M + 2;
    const fill = blank(W, H);
    L.glyphs.forEach((g, k) => {
      const dy = M + 1 + bounce[k % bounce.length];
      let top = 99, bot = -1;
      g.rows.forEach((r, j) => { if (r.includes('#')) { top = Math.min(top, j); bot = Math.max(bot, j); } });
      g.rows.forEach((r, j) => [...r].forEach((c, i) => {
        if (c !== '#') return;
        const t = (j - top) / (bot - top + 1);
        const edge = !(j > 0 && g.rows[j - 1][i] === '#');
        const low = !(j < g.rows.length - 1 && g.rows[j + 1][i] === '#');
        fill[dy + j][M + g.x + i] = edge ? 'h' : low ? 'd' : t < 0.4 ? 'a' : t < 0.7 ? 'b' : 'c';
      }));
    });
    const line = outlineOf(fill, 'k');
    const body = fill.map((r, j) => r.map((c, i) => (c !== '.' || line[j][i] !== '.' ? '#' : '.')));
    const shadow = blank(W, H);
    for (let j = 0; j < H; j++) for (let i = 0; i < W; i++) if (on(body, i - 1, j - 1) && !on(body, i, j)) shadow[j][i] = 's';
    return { W, H, fill: toRows(fill), line: toRows(line), shadow: toRows(shadow) };
  }

  const TS_COL = { h: '#FFFBD6', a: '#FFE14D', b: '#FFB320', c: '#F27A12', d: '#C9480E', k: '#000000', s: '#0A1452' };

  OG.add({
    num: 5,
    id: 'title-screen',
    name: 'Title Screen',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Games & arcade'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    scene: false,
    bg: '#2347C4',
    ink: '#FFFFFF',
    palette: ['#2347C4', '#FFE14D', '#F27A12', '#000000', '#FFFFFF'],
    fonts: ['Press Start 2P'],
    type: 'Hand-drawn chunky pixel letters, 11 by 12, with a one-pixel black outline, top-lit gold shading and a hard navy shadow; Press Start 2P for the prompt.',
    concept:
      'The name as the title of a 16-bit cartridge game: fat gold letters hopping on their baselines, a blinking PRESS START, a copyright line. ' +
      'OgOs is a system for making and playing games, so its logo starts like one.',
    logo: (w) => {
      const art = titleArt(w, [0, -1, 1, 0]);
      const px = 8;
      const x = Math.round((480 - art.W * px) / 2) + 4, y = 34;
      const map = TS_COL;
      // Sparkles at the first letter's top-left and the last letter's top-right.
      const sx = x + 3 * px, ex = x + (art.W - 3) * px;
      return OG.logoSvg(
        OG.bitmap(art.shadow, map, { x, y, px }) +
          OG.bitmap(art.line, map, { x, y, px }) +
          OG.bitmap(art.fill, map, { x, y, px }) +
          OG.bitmap(['..#..', '..#..', '##.##', '..#..', '..#..'], { '#': '#FFFFFF' }, { x: sx - 14, y: y + 6, px: 4 }) +
          OG.bitmap(['.#.', '#.#', '.#.'], { '#': '#FFFBD6' }, { x: ex + 2, y: y + 30, px: 4 }) +
          `<g>${OG.text('PRESS START', { x: 242, y: 244, size: 15, family: "'Press Start 2P', monospace", fill: '#0A1452', anchor: 'middle' })}` +
          OG.text('PRESS START', { x: 240, y: 242, size: 15, family: "'Press Start 2P', monospace", fill: '#FFFFFF', anchor: 'middle' }) +
          `<animate attributeName="opacity" values="1;0" dur="1.1s" calcMode="discrete" repeatCount="indefinite"/></g>` +
          OG.text('© 2026 OGOS', { x: 240, y: 272, size: 8, family: "'Press Start 2P', monospace", fill: '#9FB4FF', anchor: 'middle' }),
        { crisp: true }
      );
    },
    icon: () => {
      const art = titleArt('O', [0]);
      const px = 4;
      return OG.iconSvg(
        `<rect width="64" height="64" rx="10" fill="#2347C4"/>` +
          OG.bitmap(art.shadow, TS_COL, { x: (64 - art.W * px) / 2 + 2, y: (64 - art.H * px) / 2 + 2, px }) +
          OG.bitmap(art.line, TS_COL, { x: (64 - art.W * px) / 2 + 2, y: (64 - art.H * px) / 2 + 2, px }) +
          OG.bitmap(art.fill, TS_COL, { x: (64 - art.W * px) / 2 + 2, y: (64 - art.H * px) / 2 + 2, px }),
        { crisp: true }
      );
    },
  });

  /* ==================================================================== 10. DMG */

  const GB = OG.GAMEBOY; // darkest to lightest
  const B7 = '.......', B6 = '......';
  const DM = {
    O: ['.#####.', '#######', '##...##', '##...##', '##...##', '##...##', '#######', '.#####.', B7, B7],
    G: ['.######', '#######', '##.....', '##..###', '##..###', '##...##', '#######', '.#####.', B7, B7],
    S: ['.######', '#######', '##.....', '######.', '.######', '.....##', '#######', '######.', B7, B7],
    o: [B6, B6, '.####.', '######', '##..##', '##..##', '######', '.####.', B6, B6],
    g: [B6, B6, '.#####', '######', '##..##', '######', '.#####', '....##', '######', '#####.'],
    s: [B6, B6, '.#####', '##....', '#####.', '.#####', '....##', '#####.', B6, B6],
  };

  // Lit LCD cells: each pixel a square with a gap, so the matrix shows.
  function lcdCells(rows, fill, x, y, p, cell) {
    let out = '';
    rows.forEach((r, j) => {
      for (let i = 0; i < r.length; i++) if (r[i] === '#') out += `<rect x="${x + i * p}" y="${y + j * p}" width="${cell}" height="${cell}" fill="${fill}"/>`;
    });
    return out;
  }

  OG.add({
    num: 10,
    id: 'dmg',
    name: 'DMG',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Games & arcade'],
    paletteTag: 'Phosphor',
    risk: 'Safe',
    scene: true,
    bg: GB[3],
    ink: GB[0],
    palette: [GB[3], GB[0], GB[1], GB[2]],
    fonts: [],
    type: 'A bold pixel face drawn for an 8-row LCD, two pixels a stroke, every pixel a separate cell of the dot matrix.',
    concept:
      'A four-shade handheld LCD at power-on, the name settling into place while the slow liquid crystal still holds the frames it fell through. ' +
      'Calm, green and instantly familiar to anyone who played under a blanket.',
    logo: (w) => {
      const rows = wordRows(DM, w, 1);
      const p = 13, cell = 11;
      const W = rows[0].length;
      const x = Math.round((480 - (W * p - 2)) / 2), y = 100;
      const ox = ((x % p) + p) % p, oy = ((y % p) + p) % p;
      const grid =
        `<defs><pattern id="mx" x="${ox}" y="${oy}" width="${p}" height="${p}" patternUnits="userSpaceOnUse"><rect width="${cell}" height="${cell}" fill="#93B50F"/></pattern>` +
        `<linearGradient id="sh" x1="0" y1="0" x2="0.35" y2="1"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0.10"/><stop offset="0.5" stop-color="#FFFFFF" stop-opacity="0"/><stop offset="1" stop-color="#0F380F" stop-opacity="0.08"/></linearGradient></defs>` +
        `<rect width="480" height="300" fill="${GB[3]}"/><rect width="480" height="300" fill="url(#mx)"/>`;
      const ghosts =
        `<g opacity="0.55">${lcdCells(rows, GB[2], x, y - 6 * p, p, cell)}</g>` +
        `<g>${lcdCells(rows, GB[2], x, y - 3 * p, p, cell)}</g>`;
      // Lit pixels cast a faint shadow on the reflector behind the liquid crystal.
      const shade = `<g opacity="0.75">${lcdCells(rows, GB[2], x + 2, y + 2, p, cell)}</g>`;
      return OG.logoSvg(grid + ghosts + shade + lcdCells(rows, GB[0], x, y, p, cell) + `<rect width="480" height="300" fill="url(#sh)"/>`, { crisp: true });
    },
    icon: () => {
      const rows = DM.O.slice(0, 8);
      const p = 7, cell = 6;
      return OG.iconSvg(`<rect width="64" height="64" rx="8" fill="${GB[3]}"/>` + lcdCells(rows, GB[0], 8, 4, p, cell), { crisp: true });
    },
  });

  /* ================================================================ 15. Marquee */

  const MQ_FONT = "'Bowlby One', 'Arial Black', sans-serif";
  const marqueeDefs = (top, bot) =>
    `<defs>` +
    `<linearGradient id="hz" gradientUnits="userSpaceOnUse" x1="0" y1="${top}" x2="0" y2="${bot}">` +
    `<stop offset="0" stop-color="#1B3FB0"/><stop offset="0.28" stop-color="#4FA8F5"/><stop offset="0.5" stop-color="#D9F3FF"/><stop offset="0.53" stop-color="#FFFFFF"/>` +
    `<stop offset="0.535" stop-color="#3A0838"/><stop offset="0.62" stop-color="#9C1F7A"/><stop offset="0.8" stop-color="#F0561E"/><stop offset="1" stop-color="#FFC94A"/></linearGradient>` +
    `<linearGradient id="rim" gradientUnits="userSpaceOnUse" x1="0" y1="${top - 10}" x2="0" y2="${bot + 10}">` +
    `<stop offset="0" stop-color="#FFFFFF"/><stop offset="0.35" stop-color="#8E97B0"/><stop offset="0.52" stop-color="#2A2A3C"/><stop offset="0.56" stop-color="#E6E9F5"/><stop offset="1" stop-color="#6D7390"/></linearGradient>` +
    `<radialGradient id="gf" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#fff" stop-opacity="1"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>` +
    `<mask id="gm"><rect x="0" y="20" width="480" height="260" fill="url(#gf)"/></mask>` +
    `<pattern id="gp" width="24" height="24" patternUnits="userSpaceOnUse" x="0" y="6"><path d="M0 0.5H24M0.5 0V24" stroke="#FF4FB8" stroke-width="1" fill="none"/></pattern>` +
    `<filter id="gw" x="-1" y="-1" width="3" height="3"><feGaussianBlur stdDeviation="2.2"/></filter>` +
    `</defs>`;

  // A long four-point glint with a soft halo.
  const glint = (x, y, r) =>
    `<circle cx="${n2(x)}" cy="${n2(y)}" r="${n2(r * 0.42)}" fill="#fff" filter="url(#gw)" opacity="0.9"/>` +
    sparkle(x, y, r, '#FFFFFF') +
    `<g transform="rotate(45 ${n2(x)} ${n2(y)})">${sparkle(x, y, r * 0.45, '#FFFFFF')}</g>`;

  OG.add({
    num: 15,
    id: 'marquee',
    name: 'Marquee',
    caps: 'OGOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Games & arcade'],
    paletteTag: 'Metallic',
    risk: 'Bold',
    scene: false,
    bg: '#050309',
    ink: '#FFFFFF',
    palette: ['#050309', '#4FA8F5', '#9C1F7A', '#F0561E', '#FFC94A', '#E6E9F5'],
    fonts: ['Bowlby One'],
    type: 'Bowlby One slanted 12 degrees, filled with a hard-horizon sky-to-sunset gradient inside a two-tone chrome rim.',
    concept:
      'The backlit header of an 80s arcade cabinet: chrome letters with a sunset reflected in their faces, glints on the corners, a faint lit grid behind the glass. ' +
      'It sells OgOs the way a cabinet sold a game, from across the room.',
    logo: (w) => {
      const len = [...w].reduce((s, c) => s + (isUp(c) ? 100 : 88), 0) - 10;
      const size = 124, base = 180;
      const capTop = base - 0.72 * size, xTop = base - 0.56 * size;
      const t = (attrs) =>
        `<text x="0" y="0" font-family="${MQ_FONT}" font-size="${size}" text-anchor="middle" textLength="${len}" lengthAdjust="spacingAndGlyphs" ${attrs}>${OG.esc(w)}</text>`;
      const firstTop = isUp(w[0]) ? capTop : xTop;
      const lastTop = isUp(w[w.length - 1]) ? capTop : xTop;
      const sk = Math.tan((12 * Math.PI) / 180);
      const gx1 = 240 - len / 2 + 14 + sk * (base - firstTop), gx2 = 240 + len / 2 - 16 + sk * (base - lastTop - 30);
      return OG.logoSvg(
        marqueeDefs(capTop - base, 0) +
          `<rect x="0" y="20" width="480" height="260" fill="url(#gp)" mask="url(#gm)" opacity="0.32"/>` +
          `<g transform="translate(240 ${base}) skewX(-12)">` +
          t('fill="#000" stroke="#000" stroke-width="24" stroke-linejoin="round"') +
          t('fill="none" stroke="url(#rim)" stroke-width="15" stroke-linejoin="round"') +
          t('fill="none" stroke="#14101E" stroke-width="4" stroke-linejoin="round"') +
          t('fill="url(#hz)"') +
          `</g>` +
          glint(gx1 - 6, firstTop + 2, 22) +
          glint(gx2, lastTop + 30, 15) +
          `<rect x="120" y="222" width="240" height="2" fill="#FF4FB8"/>` +
          OG.text('INSERT DISK', { x: 240, y: 254, size: 17, family: MQ_FONT, fill: '#FFC94A', anchor: 'middle', ls: 6 }),
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        marqueeDefs(-46, 0) +
          `<rect width="64" height="64" rx="10" fill="#050309"/>` +
          `<g transform="translate(32 52)">` +
          `<ellipse cx="0" cy="-22" rx="19" ry="21" fill="none" stroke="#000" stroke-width="16"/>` +
          `<ellipse cx="0" cy="-22" rx="19" ry="21" fill="none" stroke="url(#rim)" stroke-width="13"/>` +
          `<ellipse cx="0" cy="-22" rx="19" ry="21" fill="none" stroke="url(#hz)" stroke-width="8"/>` +
          `</g>` +
          sparkle(17, 13, 10, '#FFFFFF'),
        { crisp: false }
      ),
  });

  /* ================================================================= 20. Vector */

  // Angular letters from straight strokes, in units: caps 4 wide, 6 tall;
  // lowercase 3.4 wide on a 4-unit x-height, descending to 8.
  const VEC = {
    O: { w: 4, p: [[[1, 0], [3, 0], [4, 1], [4, 5], [3, 6], [1, 6], [0, 5], [0, 1], [1, 0]]] },
    G: { w: 4, p: [[[4, 1.3], [3, 0], [1, 0], [0, 1], [0, 5], [1, 6], [3, 6], [4, 5], [4, 3.3], [2.1, 3.3]]] },
    S: { w: 4, p: [[[4, 1], [3, 0], [1, 0], [0, 1], [0, 2], [1, 3], [3, 3], [4, 4], [4, 5], [3, 6], [1, 6], [0, 5]]] },
    o: { w: 3.4, p: [[[0.9, 2], [2.5, 2], [3.4, 2.9], [3.4, 5.1], [2.5, 6], [0.9, 6], [0, 5.1], [0, 2.9], [0.9, 2]]] },
    g: { w: 3.4, p: [[[3.4, 2], [0.9, 2], [0, 2.9], [0, 4.6], [0.9, 5.5], [3.4, 5.5]], [[3.4, 2], [3.4, 7.1], [2.5, 8], [0.9, 8], [0, 7.2]]] },
    s: { w: 3.4, p: [[[3.4, 2.8], [2.6, 2], [0.8, 2], [0, 2.8], [0, 3.4], [0.8, 4], [2.6, 4], [3.4, 4.6], [3.4, 5.2], [2.6, 6], [0.8, 6], [0, 5.2]]] },
  };

  // Polylines for a word: [{pts:[[x,y]...]}], in screen units.
  function vecWord(w, s, x0, y0, gap = 1.6) {
    let x = 0;
    const lines = [];
    [...w].forEach((ch, k) => {
      const g = glyphOf(VEC, ch);
      if (!g) return;
      if (k) x += gap;
      g.p.forEach((pl) => lines.push(pl.map(([u, v]) => [x0 + (x + u) * s, y0 + v * s])));
      x += g.w;
    });
    return { lines, width: x * s };
  }
  const vecWidth = (w, gap = 1.6) => [...w].reduce((t, ch, k) => t + (k ? gap : 0) + glyphOf(VEC, ch).w, 0);

  const poly = (pts) => 'M' + pts.map(([x, y]) => `${n2(x)} ${n2(y)}`).join('L');

  // Draws polylines as glowing vector strokes, with brighter dots where the beam turns.
  function vectorGlow(lines, sw, filterId = 'vg') {
    const d = lines.map(poly).join('');
    let dots = '';
    lines.forEach((pl) => pl.forEach(([x, y]) => (dots += `<circle cx="${n2(x)}" cy="${n2(y)}" r="${n2(sw * 0.75)}" fill="#FFFFFF"/>`)));
    return (
      `<path d="${d}" fill="none" stroke="#38E8FF" stroke-width="${n2(sw * 3.2)}" stroke-linecap="round" stroke-linejoin="round" filter="url(#${filterId})" opacity="0.85"/>` +
      `<path d="${d}" fill="none" stroke="#C8FBFF" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round"/>` +
      dots
    );
  }
  const vecDefs = (sd) => `<defs><filter id="vg" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="${sd}"/></filter></defs>`;

  // The ship, nose up at the origin's top, then rotated.
  const SHIP = [[0, -11], [7.5, 9], [3.5, 5.5], [-3.5, 5.5], [-7.5, 9], [0, -11]];
  const ROCK = [[0, -1], [0.55, -0.85], [0.95, -0.35], [0.7, 0.05], [1, 0.45], [0.55, 0.95], [0.05, 0.7], [-0.5, 0.95], [-0.95, 0.4], [-0.75, -0.1], [-0.95, -0.55], [-0.4, -0.95], [0, -1]];
  const place = (pts, x, y, s, deg) => {
    const a = (deg * Math.PI) / 180, c = Math.cos(a), si = Math.sin(a);
    return pts.map(([u, v]) => [x + (u * c - v * si) * s, y + (u * si + v * c) * s]);
  };

  OG.add({
    num: 20,
    id: 'vector',
    name: 'Vector',
    caps: 'OGOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Games & arcade'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: false,
    bg: '#000000',
    ink: '#C8FBFF',
    palette: ['#000000', '#C8FBFF', '#38E8FF', '#FFFFFF'],
    fonts: [],
    type: 'Angular letters drawn only from straight beam strokes, octagonal bowls, with brighter dots where the beam lingers at each corner.',
    concept:
      'A vector arcade monitor: no pixels, just a beam tracing lines in white-cyan light, with a little ship firing across the bottom at a drifting rock. ' +
      'Geometric and sharp, and drawn with the same kind of line maths a 32-bit hobby OS can do in a few hundred bytes.',
    logo: (w) => {
      const s = 18;
      const W = vecWidth(w) * s;
      const word = vecWord(w, s, 240 - W / 2, 78);
      const ship = place(SHIP, 62, 242, 1.15, 90);
      const flame = place([[-2.5, 6.5], [0, 13], [2.5, 6.5]], 62, 242, 1.15, 90);
      const rock = place(ROCK, 414, 240, 20, 10);
      const small = place(ROCK, 64, 52, 9, 70);
      const bullets = [118, 170, 222, 274, 326].map((x, k) => `<circle cx="${x}" cy="242" r="${k === 4 ? 2.6 : 2}" fill="#FFFFFF" opacity="${0.45 + k * 0.13}"/>`).join('');
      return OG.logoSvg(
        vecDefs(4) +
          vectorGlow(word.lines, 3) +
          vectorGlow([ship, flame, rock, small], 2) +
          `<g filter="url(#vg)">${bullets}</g>` + bullets,
        { crisp: false }
      );
    },
    icon: () => {
      const o = VEC.O.p[0].map(([u, v]) => [10 + u * 11, 5 + v * 9]);
      const ship = place(SHIP, 32, 32, 0.95, 35);
      return OG.iconSvg(
        vecDefs(2.5) + `<rect width="64" height="64" rx="10" fill="#000"/>` + vectorGlow([o], 4.2) + vectorGlow([ship], 3),
        { crisp: false }
      );
    },
  });

  /* ============================================================= 25. Tetrominoes */

  // Every fixed orientation of the seven pieces, each normalised so its first
  // cell in reading order is at (0, 0).
  const TETRO = (() => {
    const base = {
      I: [[0, 0], [1, 0], [2, 0], [3, 0]], O: [[0, 0], [1, 0], [0, 1], [1, 1]], T: [[0, 0], [1, 0], [2, 0], [1, 1]],
      S: [[1, 0], [2, 0], [0, 1], [1, 1]], Z: [[0, 0], [1, 0], [1, 1], [2, 1]], J: [[0, 0], [0, 1], [1, 1], [2, 1]], L: [[2, 0], [0, 1], [1, 1], [2, 1]],
    };
    const out = [];
    Object.entries(base).forEach(([t, cells]) => {
      const seen = new Set();
      let c = cells;
      for (let r = 0; r < 4; r++) {
        const s = c.slice().sort((a, b) => a[1] - b[1] || a[0] - b[0]);
        const nrm = s.map(([x, y]) => [x - s[0][0], y - s[0][1]]);
        const key = nrm.join(';');
        if (!seen.has(key)) { seen.add(key); out.push({ t, cells: nrm }); }
        c = c.map(([x, y]) => [-y, x]);
      }
    });
    return out;
  })();

  // Tiles a mask with tetrominoes (backtracking), preferring pieces unlike their neighbours.
  const tileCache = {};
  function tileMask(rows, seed) {
    const key = rows.join('/') + seed;
    if (tileCache[key]) return tileCache[key];
    const H = rows.length, W = rows[0].length;
    const own = rows.map((r) => [...r].map((c) => (c === '#' ? null : -1)));
    const pieces = [];
    const rnd = OG.rng(seed);
    const pref = TETRO.map((o) => ({ o, k: rnd() }));
    const same = (o, fx, fy) => {
      let n = 0;
      o.cells.forEach(([dx, dy]) => [[1, 0], [-1, 0], [0, 1], [0, -1]].forEach(([ax, ay]) => {
        const p = own[fy + dy + ay] && own[fy + dy + ay][fx + dx + ax];
        if (p != null && p >= 0 && pieces[p].t === o.t) n++;
      }));
      return n;
    };
    let steps = 0;
    const solve = () => {
      if (++steps > 200000) return false;
      let fx = -1, fy = -1;
      for (let y = 0; y < H && fx < 0; y++) for (let x = 0; x < W; x++) if (own[y][x] === null) { fx = x; fy = y; break; }
      if (fx < 0) return true;
      const cands = pref
        .filter(({ o }) => o.cells.every(([dx, dy]) => own[fy + dy] && own[fy + dy][fx + dx] === null))
        .map(({ o, k }) => ({ o, k: k + 4 * same(o, fx, fy) + 0.45 * pieces.filter((p) => p.t === o.t).length }))
        .sort((a, b) => a.k - b.k);
      for (const { o } of cands) {
        const id = pieces.length;
        pieces.push({ t: o.t, cells: o.cells.map(([dx, dy]) => [fx + dx, fy + dy]) });
        o.cells.forEach(([dx, dy]) => (own[fy + dy][fx + dx] = id));
        if (solve()) return true;
        o.cells.forEach(([dx, dy]) => (own[fy + dy][fx + dx] = null));
        pieces.pop();
      }
      return false;
    };
    const res = solve() ? pieces : null;
    if (!res) throw new Error('no tetromino tiling for a glyph');
    return (tileCache[key] = res);
  }

  const B6b = '......';
  const TM = {
    O: ['.####.', '######', '##..##', '##..##', '##..##', '##..##', '######', '.####.', B6b, B6b],
    G: ['.#####', '######', '##....', '##....', '##.###', '##.###', '######', '.#####', B6b, B6b],
    S: ['.#####', '######', '##....', '#####.', '.#####', '....##', '######', '#####.', B6b, B6b],
    o: [B6b, B6b, '.####.', '######', '##..##', '##..##', '######', '.####.', B6b, B6b],
    g: [B6b, B6b, '.####.', '######', '##..##', '######', '.#####', '....##', '.#####', '.####.'],
    s: [B6b, B6b, '.#####', '##....', '#####.', '.#####', '....##', '#####.', B6b, B6b],
  };
  const PIECE = { I: '#16D8E8', O: '#F5D90A', T: '#A22CE0', S: '#2FD03A', Z: '#EE2B2B', J: '#2754E8', L: '#F5901A' };

  // One beveled mino.
  function mino(x, y, c, col, b = 3) {
    const lt = mix(col, '#FFFFFF', 0.55), lt2 = mix(col, '#FFFFFF', 0.3), dk = mix(col, '#000000', 0.35), dk2 = mix(col, '#000000', 0.5);
    const e = c - 1;
    return (
      `<rect x="${x}" y="${y}" width="${e}" height="${e}" fill="${col}"/>` +
      `<path d="M${x} ${y}h${e}l-${b} ${b}h-${e - 2 * b}z" fill="${lt}"/>` +
      `<path d="M${x} ${y}v${e}l${b} -${b}v-${e - 2 * b}z" fill="${lt2}"/>` +
      `<path d="M${x} ${y + e}h${e}l-${b} -${b}h-${e - 2 * b}z" fill="${dk}"/>` +
      `<path d="M${x + e} ${y}v${e}l-${b} -${b}v-${e - 2 * b}z" fill="${dk2}"/>`
    );
  }

  function tetroWord(w, x0, y0, c) {
    const L = layWord(TM, w, 1);
    let out = '';
    L.glyphs.forEach((g, k) => {
      const pieces = tileMask(g.rows, 7 + k * 31 + g.ch.charCodeAt(0));
      const id = {};
      pieces.forEach((p, n) => p.cells.forEach(([i, j]) => (id[i + ',' + j] = n)));
      let seams = '';
      pieces.forEach((p, n) => p.cells.forEach(([i, j]) => {
        const X = x0 + (g.x + i) * c, Y = y0 + j * c;
        out += mino(X, Y, c, PIECE[p.t]);
        // A dark seam wherever this piece meets another or the outside.
        if (id[i + 1 + ',' + j] !== n) seams += `M${X + c - 0.5} ${Y - 0.5}v${c}`;
        if (id[i + ',' + (j + 1)] !== n) seams += `M${X - 0.5} ${Y + c - 0.5}h${c}`;
        if (id[i - 1 + ',' + j] !== n) seams += `M${X - 0.5} ${Y - 0.5}v${c}`;
        if (id[i + ',' + (j - 1)] !== n) seams += `M${X - 0.5} ${Y - 0.5}h${c}`;
      }));
      out += `<path d="${seams}" stroke="#05061A" stroke-width="2" fill="none" stroke-linecap="square"/>`;
    });
    return { svg: out, width: L.width * c };
  }

  OG.add({
    num: 25,
    id: 'tetrominoes',
    name: 'Tetrominoes',
    caps: 'OGOS',
    flexCase: true,
    family: 'Pixel',
    sources: ['Games & arcade'],
    paletteTag: 'Rainbow',
    risk: 'Safe',
    scene: false,
    bg: '#0E1030',
    ink: '#FFFFFF',
    palette: ['#0E1030', '#16D8E8', '#F5D90A', '#A22CE0', '#2FD03A', '#EE2B2B'],
    fonts: [],
    type: 'Two-cell-thick block letters, each one tiled exactly with the seven falling pieces by a small solver, every cell beveled.',
    concept:
      'The name stacked out of falling-block pieces in their classic colors, with one more piece still dropping in from above. ' +
      'It is a puzzle that fits together, which is what writing an OS feels like on a good day.',
    logo: (w) => {
      const c = 15;
      const L = layWord(TM, w, 1);
      const x0 = Math.round(240 - (L.width * c) / 2), y0 = 104;
      const word = tetroWord(w, x0, y0, c);
      // A T piece falling above the second letter, with its trail.
      const g2 = L.glyphs[Math.min(1, L.glyphs.length - 1)];
      const tx = x0 + (g2.x + 1) * c, ty = 38;
      let fall = '';
      [[0, 0], [1, 0], [2, 0], [1, 1]].forEach(([i, j]) => (fall += mino(tx + i * c, ty + j * c, c, PIECE.T)));
      let trail = '';
      [0, 1, 2].forEach((i) => (trail += `<rect x="${tx + i * c + 5}" y="${ty - 26 + (i === 1 ? -6 : 0)}" width="4" height="${i === 1 ? 26 : 18}" fill="#A22CE0" opacity="${i === 1 ? 0.55 : 0.35}"/>`));
      return OG.logoSvg(trail + fall + word.svg, { crisp: true });
    },
    icon: () => {
      const c = 28;
      return OG.iconSvg(mino(4, 4, c + 1, PIECE.O, 6) + mino(32, 4, c + 1, PIECE.O, 6) + mino(4, 32, c + 1, PIECE.O, 6) + mino(32, 32, c + 1, PIECE.O, 6), { crisp: true });
    },
  });
  /* ============================================================== 30. Raycaster */

  // Slab textures: each letter of the chunky title face, centred in a 16x16
  // block with a frame, and a shadow texel under every letter texel.
  function letterTexture(ch) {
    const g = glyphOf(TS, ch);
    const tex = blank(16, 16, 'w');
    for (let i = 0; i < 16; i++) { tex[0][i] = 'e'; tex[15][i] = 'e'; tex[i][0] = 'e'; tex[i][15] = 'e'; }
    let top = 99, bot = -1;
    g.forEach((r, j) => { if (r.includes('#')) { top = Math.min(top, j); bot = Math.max(bot, j); } });
    const ox = Math.floor((16 - g[0].length) / 2), oy = 1 + Math.floor((14 - (bot - top + 1)) / 2) - top;
    const set = (j, i, k) => { if (j >= 1 && j <= 14 && i >= 1 && i <= 14) tex[j][i] = k; };
    g.forEach((r, j) => [...r].forEach((c, i) => { if (c === '#') set(oy + j + 1, ox + i + 1, 's'); }));
    g.forEach((r, j) => [...r].forEach((c, i) => { if (c === '#') set(oy + j, ox + i, 'L'); }));
    return tex;
  }

  // Casts one ray through the grid (DDA) and returns what it hit.
  function castRay(px, py, dx, dy, wall) {
    let mx = Math.floor(px), my = Math.floor(py);
    const ddx = Math.abs(1 / dx), ddy = Math.abs(1 / dy);
    const sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    let sdx = dx < 0 ? (px - mx) * ddx : (mx + 1 - px) * ddx;
    let sdy = dy < 0 ? (py - my) * ddy : (my + 1 - py) * ddy;
    let side = 0;
    for (let k = 0; k < 80; k++) {
      if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
      if (wall(mx, my)) break;
    }
    const perp = side === 0 ? sdx - ddx : sdy - ddy;
    let u = side === 0 ? py + perp * dy : px + perp * dx;
    u -= Math.floor(u);
    if ((side === 0 && dx < 0) || (side === 1 && dy > 0)) u = 1 - u;
    return { mx, my, side, perp, u };
  }

  // A raycast view down a hall in which the letters stand as slabs, Wolfenstein
  // door style, each nearer and further, left to right.
  function raycastView(w, NC, W, H, cam) {
    const chars = [...w];
    const tex = chars.map(letterTexture);
    const slabs = cam.slabs.slice(0, chars.length).map(([x, y], k) => ({ x0: x, x1: x + cam.th, y0: y, y1: y + 1, k }));
    const wall = (x, y) => y <= cam.yN || y >= cam.yS || x >= cam.xE || x < -3;
    const HZ = H / 2, HP = cam.hp;
    const a = (cam.ang * Math.PI) / 180, dirX = Math.cos(a), dirY = Math.sin(a);
    const pl = Math.tan((cam.fov * Math.PI) / 360), plX = -dirY * pl, plY = dirX * pl;
    const cw = W / NC;
    const clip = (y) => Math.max(0, Math.min(H, y));
    let out = `<rect width="${W}" height="${HZ}" fill="url(#ceil)"/><rect y="${HZ}" width="${W}" height="${H - HZ}" fill="url(#floor)"/>`;
    for (let c = 0; c < NC; c++) {
      const camX = (2 * (c + 0.5)) / NC - 1;
      const dx = dirX + plX * camX, dy = dirY + plY * camX;
      const r = castRay(cam.x, cam.y, dx, dy, wall);
      let hit = { slab: -1, perp: r.perp, side: r.side, u: r.u };
      slabs.forEach((sl) => {
        const tx1 = (sl.x0 - cam.x) / dx, tx2 = (sl.x1 - cam.x) / dx;
        const ty1 = (sl.y0 - cam.y) / dy, ty2 = (sl.y1 - cam.y) / dy;
        const txn = Math.min(tx1, tx2), tyn = Math.min(ty1, ty2);
        const tn = Math.max(txn, tyn), tf = Math.min(Math.max(tx1, tx2), Math.max(ty1, ty2));
        if (tn < tf && tn > 0 && tn < hit.perp) {
          const side = txn > tyn ? 0 : 1;
          hit = { slab: sl.k, perp: tn, side, u: side === 0 ? cam.y + tn * dy - sl.y0 : 0 };
        }
      });
      const lh = HP / hit.perp, top = HZ - lh / 2;
      // Distance shading, as RetrOS's raycaster dims its walls.
      const f = Math.max(0.3, Math.min(1, 3.2 / hit.perp));
      const dim = (col) => mix('#000014', col, f);
      const X = n2(c * cw), Wd = n2(cw + 0.4);
      const col = (y0, y1, fill) => `<rect x="${X}" y="${n2(clip(y0))}" width="${Wd}" height="${n2(clip(y1) - clip(y0))}" fill="${fill}"/>`;
      if (hit.slab >= 0 && hit.side === 0) {
        const T = tex[hit.slab], tc = Math.max(0, Math.min(15, Math.floor(hit.u * 16)));
        const cols = { w: dim('#3352FF'), L: mix('#000014', '#FFF27A', Math.max(0.78, f)), s: dim('#0A1470'), e: dim('#8FA2FF') };
        let j = 0;
        while (j < 16) {
          const k0 = T[j][tc];
          let k = j;
          while (k + 1 < 16 && T[k + 1][tc] === k0) k++;
          out += col(top + (j * lh) / 16, top + ((k + 1) * lh) / 16, cols[k0]);
          j = k + 1;
        }
      } else if (hit.slab >= 0) {
        out += col(top, top + lh, dim('#121C78'));
      } else {
        out += col(top, top + lh, dim(r.side === 0 ? '#1F2FB8' : '#16228E'));
        // Plain stone: mortar lines every half block, joints staggered.
        const mort = dim('#0B1468');
        [0, 0.5].forEach((v) => (out += `<rect x="${X}" y="${n2(top + v * lh)}" width="${Wd}" height="${n2(Math.max(1, lh / 28))}" fill="${mort}"/>`));
        if ((hit.u * 2) % 1 < 0.06) out += col(top, top + lh / 2, mort);
        if ((hit.u * 2 + 0.5) % 1 < 0.06) out += col(top + lh / 2, top + lh, mort);
      }
    }
    return out;
  }
  const RC_DEFS =
    `<defs><linearGradient id="ceil" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFFFFF"/><stop offset="1" stop-color="#C9CCD6"/></linearGradient>` +
    `<linearGradient id="floor" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3A3A40"/><stop offset="1" stop-color="#6E6E76"/></linearGradient></defs>`;

  OG.add({
    num: 30,
    id: 'raycaster',
    name: 'Raycaster',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Games & arcade', 'OgOs internals'],
    paletteTag: 'VGA 16',
    risk: 'Wild',
    scene: true,
    bg: '#1A28A8',
    ink: '#FFF27A',
    palette: ['#2A3FE0', '#FFF27A', '#FFFFFF', '#55555C', '#0A1470'],
    fonts: [],
    type: 'The chunky pixel face from Title Screen (no. 5) painted into 16-texel slab textures, then actually raycast: 160 columns, each one texture column stretched by its distance.',
    concept:
      "A first-person hall rendered the way OgOs's own raycaster renders it, flat blue walls, gray floor, white ceiling, with the four letters standing in it as stone slabs, each one further down the hall. " +
      'The logo is the real output of a small grid raycaster, the kind of program OgOs exists to run.',
    logo: (w) =>
      OG.logoSvg(
        RC_DEFS +
          raycastView(w, 160, 480, 300, {
            x: 0, y: 0.5, ang: 0, fov: 72, hp: 330, th: 0.22, yN: -4, yS: 5, xE: 12,
            slabs: [[2.4, -1.08], [3.4, -0.4], [4.5, 0.73], [5.8, 2.61]],
          }),
        { crisp: true }
      ),
    icon: () => {
      // A one-point corridor: ceiling, floor, two walls, and the dark end.
      const q = (pts, fill) => `<polygon points="${pts}" fill="${fill}"/>`;
      return OG.iconSvg(
        q('0,0 64,0 40,24 24,24', '#FFFFFF') + q('0,64 64,64 40,40 24,40', '#55555C') +
          q('0,0 24,24 24,40 0,64', '#2A3FE0') + q('64,0 40,24 40,40 64,64', '#1A28A8') +
          `<rect x="24" y="24" width="16" height="16" fill="#0A1470"/><rect x="29" y="28" width="6" height="8" fill="#FFF27A"/>`,
        { crisp: true }
      );
    },
  });

  /* =================================================================== 35. Pong */

  // Blocky score glyphs, 4x5, as the first paddle games drew them; lowercase
  // sits on a 4-row x-height and the g drops 2 more.
  const PONG = {
    0: ['####', '#..#', '#..#', '#..#', '####', '....', '....'],
    O: ['####', '#..#', '#..#', '#..#', '####', '....', '....'],
    G: ['####', '#...', '#.##', '#..#', '####', '....', '....'],
    S: ['####', '#...', '####', '...#', '####', '....', '....'],
    o: ['....', '####', '#..#', '#..#', '####', '....', '....'],
    g: ['....', '####', '#..#', '#..#', '####', '...#', '####'],
    s: ['....', '####', '##..', '..##', '####', '....', '....'],
  };
  const pongHalves = (w) => {
    if (w.includes(' ')) return w.split(' ').filter(Boolean);
    const h = Math.ceil(w.length / 2);
    return [w.slice(0, h), w.slice(h)];
  };

  OG.add({
    num: 35,
    id: 'pong',
    name: 'Pong',
    caps: '0G 0S',
    flexCase: true,
    family: 'Experimental',
    sources: ['Games & arcade'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#FFFFFF', '#8A8A8A'],
    fonts: [],
    type: 'Score digits built from 4x5 blocks, the way the first paddle game drew its numbers; O and 0 share one glyph.',
    concept:
      'The name is the score: zero-G against zero-S, at the top of a dashed-net court with two paddles and a square ball in flight. ' +
      'Read it as OGOS or as nil-nil, either way the game has just started.',
    logo: (w) => {
      const [l, r] = pongHalves(w);
      const px = 15;
      const left = wordRows(PONG, l, 1), right = wordRows(PONG, r, 1);
      const lx = 240 - 34 - left[0].length * px, rx = 240 + 34;
      let net = '';
      for (let y = 6; y < 300; y += 24) net += `<rect x="236" y="${y}" width="8" height="12" fill="#FFFFFF"/>`;
      let trail = '';
      [[118, 202, 0.1], [160, 186, 0.18], [202, 170, 0.3], [244, 154, 0.45]].forEach(([x, y, o]) => (trail += `<rect x="${x}" y="${y}" width="12" height="12" fill="#FFFFFF" opacity="${o}"/>`));
      return OG.logoSvg(
        net +
          OG.bitmap(left, { '#': '#FFFFFF' }, { x: lx, y: 26, px }) +
          OG.bitmap(right, { '#': '#FFFFFF' }, { x: rx, y: 26, px }) +
          `<rect x="40" y="186" width="12" height="56" fill="#FFFFFF"/>` +
          `<rect x="428" y="122" width="12" height="56" fill="#FFFFFF"/>` +
          trail +
          `<rect x="290" y="135" width="14" height="14" fill="#FFFFFF"/>`,
        { crisp: true }
      );
    },
    icon: () => {
      let net = '';
      for (let y = 2; y < 64; y += 12) net += `<rect x="30" y="${y}" width="4" height="7" fill="#FFFFFF"/>`;
      return OG.iconSvg(
        `<rect width="64" height="64" rx="8" fill="#000"/>` + net +
          `<rect x="7" y="18" width="7" height="26" fill="#FFFFFF"/><rect x="44" y="30" width="9" height="9" fill="#FFFFFF"/>`,
        { crisp: true }
      );
    },
  });

  /* ============================================================ 40. Minesweeper */

  const B6m = '......';
  const MS = {
    O: ['######', '######', '##..##', '##..##', '##..##', '##..##', '######', '######', B6m, B6m],
    G: ['######', '######', '##....', '##....', '##.###', '##..##', '######', '######', B6m, B6m],
    S: ['######', '######', '##....', '######', '######', '....##', '######', '######', B6m, B6m],
    o: [B6m, B6m, '######', '######', '##..##', '##..##', '######', '######', B6m, B6m],
    g: [B6m, B6m, '######', '######', '##..##', '######', '######', '....##', '######', '######'],
    s: [B6m, B6m, '######', '##....', '######', '######', '....##', '######', B6m, B6m],
  };
  const MS_NUM = { 1: '#0000FF', 2: '#008000', 3: '#FF0000', 4: '#000080', 5: '#800000' };

  // A seven-segment digit; segs is which of a..g are lit.
  const SEG7 = { 0: 'abcdef', 1: 'bc', 2: 'abdeg', 3: 'abcdg', 4: 'bcfg', 5: 'acdfg', 6: 'acdefg', 7: 'abc', 8: 'abcdefg', 9: 'abcdfg' };
  function seg7(x, y, w, h, t, lit, onC, offC) {
    const m = h / 2;
    const S = {
      a: `M${x + 1} ${y}h${w - 2}l-${t} ${t}h-${w - 2 - 2 * t}z`,
      d: `M${x + 1} ${y + h}h${w - 2}l-${t} -${t}h-${w - 2 - 2 * t}z`,
      f: `M${x} ${y + 1}v${m - 2}l${t} -${t / 2}v-${m - 2 - 1.5 * t}z`,
      e: `M${x} ${y + m + 1}v${m - 2}l${t} -${t}v-${m - 2 - 1.5 * t}z`,
      b: `M${x + w} ${y + 1}v${m - 2}l-${t} -${t / 2}v-${m - 2 - 1.5 * t}z`,
      c: `M${x + w} ${y + m + 1}v${m - 2}l-${t} -${t}v-${m - 2 - 1.5 * t}z`,
      g: `M${x + 1} ${y + m}l${t} -${t / 2}h${w - 2 - 2 * t}l${t} ${t / 2}l-${t} ${t / 2}h-${w - 2 - 2 * t}z`,
    };
    return Object.keys(S).map((k) => `<path d="${S[k]}" fill="${lit.includes(k) ? onC : offC}"/>`).join('');
  }
  const ledNum = (str, x, y) => [...str].map((d, k) => seg7(x + 3 + k * 15, y + 3, 11, 21, 2.5, SEG7[d], '#FF1A1A', '#4A0000')).join('');

  // A 3D box: raised (white light at top-left) or sunken.
  const bevel = (x, y, w, h, t, raised, face) => {
    const lt = raised ? '#FFFFFF' : '#808080', dk = raised ? '#808080' : '#FFFFFF';
    return (
      (face ? `<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="${face}"/>` : '') +
      `<path d="M${x} ${y}h${w}l-${t} ${t}h-${w - 2 * t}v${h - 2 * t}l-${t} ${t}z" fill="${lt}"/>` +
      `<path d="M${x + w} ${y + h}h-${w}l${t} -${t}h${w - 2 * t}v-${h - 2 * t}l${t} -${t}z" fill="${dk}"/>`
    );
  };

  // The surprised face: a yellow 17x17 disc with an O for a mouth.
  const O_FACE = [
    '......#####......', '....##yyyyy##....', '...#yyyyyyyyy#...', '..#yyyyyyyyyyy#..', '.#yyyyyyyyyyyyy#.',
    '.#yyyy#yyy#yyyy#.', '#yyyyy#yyy#yyyyy#', '#yyyyyyyyyyyyyyy#', '#yyyyyy###yyyyyy#', '#yyyyy#yyy#yyyyy#',
    '#yyyyy#yyy#yyyyy#', '.#yyyyy###yyyyy#.', '.#yyyyyyyyyyyyy#.', '..#yyyyyyyyyyy#..', '...#yyyyyyyyy#...',
    '....##yyyyy##....', '......#####......',
  ];

  OG.add({
    num: 40,
    id: 'minesweeper',
    name: 'Minesweeper',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Windows 9x', 'Games & arcade'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: '#3A6EA5',
    ink: '#FFFFFF',
    palette: ['#C0C0C0', '#3A6EA5', '#FF0000', '#0000FF', '#008000', '#FFFF00'],
    fonts: ['Press Start 2P'],
    type: 'Square block letters two cells thick, made of the raised squares left in a cleared mine field; the counts are Press Start 2P in the classic number colors.',
    concept:
      'A mine field cleared everywhere except the name, so the raised squares still left spell OgOs, with counts in their classic colors along the edges, a flag or two and an O-mouthed face in the button slot. ' +
      'It is the game a generation learned the mouse on, right-click flags included.',
    logo: (w) => {
      const c = 15;
      const rows0 = wordRows(MS, w, 1);
      const hasDesc = rows0.slice(8).some((r) => r.includes('#'));
      const rows = (hasDesc ? rows0 : rows0.slice(0, 8)).map((r) => '.' + r + '.');
      rows.unshift('.'.repeat(rows[0].length));
      rows.push('.'.repeat(rows[0].length));
      const NX = rows[0].length, NY = rows.length;
      // The letters are the squares still covered; everything else is cleared.
      const cov = (i, j) => rows[j] && rows[j][i] === '#';
      const near = (i, j, fn) => { let k = 0; for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) if ((dx || dy) && fn(i + dx, j + dy)) k++; return k; };
      // A few mines under the letters; the squares beside them show counts.
      const rnd = OG.rng(w.length * 97 + w.charCodeAt(1) * 3);
      const mine = {};
      const isMine = (i, j) => !!mine[i + ',' + j];
      for (let j = 0; j < NY; j++) for (let i = 0; i < NX; i++) if (cov(i, j) && rnd() < 0.075) mine[i + ',' + j] = 1;
      const fw = NX * c, fh = NY * c;
      const fx = Math.round(240 - fw / 2), hdr = 40;
      const fy = Math.round(150 - (fh + hdr + 6) / 2) + hdr + 6;
      let field = '', nums = '', flags = '';
      let flagCount = 0;
      for (let j = 0; j < NY; j++)
        for (let i = 0; i < NX; i++) {
          const X = fx + i * c, Y = fy + j * c;
          if (!cov(i, j)) {
            field += `<rect x="${X}" y="${Y}" width="${c}" height="${c}" fill="#C0C0C0"/><path d="M${X} ${Y + c}V${Y}H${X + c}" stroke="#808080" fill="none"/>`;
            const k = near(i, j, isMine);
            if (k) nums += OG.text(String(k), { x: X + c / 2 + 0.5, y: Y + c - 3.5, size: 8, family: "'Press Start 2P', monospace", fill: MS_NUM[k], anchor: 'middle' });
          } else {
            field += bevel(X, Y, c, c, 3, true, '#C0C0C0');
            if (isMine(i, j) && flagCount < 2 && (i * 7 + j * 3) % 5 === 0) {
              flagCount++;
              flags += `<rect x="${X + 8}" y="${Y + 3}" width="1" height="8" fill="#000"/><path d="M${X + 9} ${Y + 3}v6l-6 -3z" fill="#FF0000"/><rect x="${X + 6}" y="${Y + 10}" width="5" height="1" fill="#000"/><rect x="${X + 4}" y="${Y + 11}" width="9" height="1.5" fill="#000"/>`;
            }
          }
        }
      // The window, its header with two counters and the face.
      const wx = fx - 9, wy = fy - hdr - 15, ww = fw + 18, wh = fh + hdr + 24;
      const hx = fx, hy = fy - hdr - 6, hw = fw;
      const face = OG.bitmap(O_FACE, { '#': '#000000', y: '#FFFF00' }, { x: 240 - 17 + 0.5, y: hy + 3, px: 2 });
      return OG.logoSvg(
        bevel(wx, wy, ww, wh, 3, true, '#C0C0C0') +
          bevel(hx, hy, hw, hdr, 2, false, '#C0C0C0') +
          `<rect x="${hx + 7}" y="${hy + 7}" width="47" height="27" fill="#000"/>` + ledNum('004', hx + 7, hy + 7) +
          `<rect x="${hx + hw - 54}" y="${hy + 7}" width="47" height="27" fill="#000"/>` + ledNum('032', hx + hw - 54, hy + 7) +
          bevel(240 - 18, hy + 2, 36, 36, 2, true, '#C0C0C0') + face +
          `<rect x="${fx - 3}" y="${fy - 3}" width="${fw + 6}" height="${fh + 6}" fill="#808080"/>` +
          `<path d="M${fx - 3} ${fy + fh + 3}H${fx + fw + 3}V${fy - 3}" stroke="#FFFFFF" stroke-width="3" fill="none"/>` +
          field + flags + nums,
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(bevel(0, 0, 64, 64, 4, true, '#C0C0C0') + OG.bitmap(O_FACE, { '#': '#000000', y: '#FFFF00' }, { x: 7.5, y: 7.5, px: 3 }), { crisp: true }),
  });

  /* =========================================================== 45. Sprite Sheet */

  // Og: a round 16x16 creature, in four frames of a walk.
  function ogSprite(frame) {
    const bob = frame % 2 === 1 ? -1 : 0;
    const cv = blank(16, 16);
    const cy = 7 + bob;
    for (let j = 0; j < 16; j++)
      for (let i = 0; i < 16; i++) {
        const dx = i - 7.5, dy = j - cy;
        if ((dx * dx) / 42 + (dy * dy) / 36 <= 1) cv[j][i] = dx + dy < -6 ? 'h' : dx + dy > 4.5 ? 'd' : 'b';
      }
    // Feet: one lifted in frames 0 and 2, both down in 1 and 3.
    const foot = (x, y) => { for (let i = 0; i < 3; i++) { cv[y][x + i] = 'f'; cv[y + 1][x + i] = 'f'; } };
    // [back x, back lift, front x, front lift] for each frame.
    const [bx, bl, fx, fl] = [[2, -1, 11, 0], [4, 0, 9, 0], [3, 0, 10, -1], [4, 0, 9, 0]][frame];
    foot(bx, 13 + bl);
    foot(fx, 13 + fl);
    const ol = outlineOf(cv, 'k', false);
    for (let j = 0; j < 16; j++) for (let i = 0; i < 16; i++) if (ol[j][i] === 'k') cv[j][i] = 'k';
    // Eyes looking ahead, and a small smile.
    [[8, 4], [11, 4]].forEach(([x, y]) => {
      for (let j = 0; j < 3; j++) for (let i = 0; i < 2; i++) cv[y + j + bob][x + i] = 'w';
      cv[y + 1 + bob][x + 1] = 'p'; cv[y + 2 + bob][x + 1] = 'p';
    });
    cv[9 + bob][9] = 'k'; cv[10 + bob][10] = 'k'; cv[10 + bob][11] = 'k'; cv[9 + bob][12] = 'k';
    return toRows(cv);
  }
  const OG_COL = { k: '#2A1B3D', b: '#FFB33B', h: '#FFE08A', d: '#E07422', w: '#FFFFFF', p: '#2A1B3D', f: '#7A3FB8' };

  // A bubbly pixel face for the name: caps 8x10, lowercase x-height 7, descender 3.
  const B8 = '........', B7b = '.......', B6c = '......';
  const BUB = {
    O: ['..####..', '.######.', '##....##', '##....##', '##....##', '##....##', '##....##', '##....##', '.######.', '..####..', B8, B8, B8],
    G: ['..####..', '.######.', '##....##', '##......', '##......', '##..####', '##....##', '##....##', '.######.', '..####..', B8, B8, B8],
    S: ['..####..', '.######.', '##....##', '##......', '.#####..', '..#####.', '......##', '##....##', '.######.', '..####..', B8, B8, B8],
    o: [B7b, B7b, B7b, '..###..', '.#####.', '##...##', '##...##', '##...##', '.#####.', '..###..', B7b, B7b, B7b],
    g: [B7b, B7b, B7b, '..#####', '.######', '##...##', '##...##', '##...##', '.######', '..#####', '.....##', '.#####.', '..###..'],
    s: [B6c, B6c, B6c, '.####.', '##..##', '###...', '.####.', '...###', '##..##', '.####.', B6c, B6c, B6c],
  };

  OG.add({
    num: 45,
    id: 'sprite-sheet',
    name: 'Sprite Sheet',
    caps: 'ogos',
    flexCase: true,
    family: 'Mascot',
    sources: ['Games & arcade'],
    paletteTag: 'Pastel',
    risk: 'Bold',
    scene: false,
    bg: '#22202E',
    ink: '#FFFFFF',
    palette: ['#22202E', '#FFB33B', '#7A3FB8', '#FFFFFF', '#E07422'],
    fonts: [],
    type: 'A round, bubbly pixel face drawn to match the mascot, two pixels a stroke, with a one-pixel purple drop; frame numbers in a 3x5 micro font.',
    concept:
      'Meet Og, a round little creature with purple shoes, laid out as a four-frame walk cycle on a sprite sheet with its frame numbers and palette. ' +
      'It says OgOs is where you make the game, not only play it, and gives the system a character to put on a boot screen.',
    logo: (w) => {
      const px = 5, cell = 16 * px, gap = 12;
      const sw = 4 * cell + 3 * gap, sx = Math.round(240 - sw / 2), sy = 38;
      let sheet = `<defs><pattern id="ck" width="20" height="20" patternUnits="userSpaceOnUse" x="${sx}" y="${sy}"><rect width="20" height="20" fill="#3A3750"/><rect width="10" height="10" fill="#45425E"/><rect x="10" y="10" width="10" height="10" fill="#45425E"/></pattern></defs>`;
      for (let f = 0; f < 4; f++) {
        const x = sx + f * (cell + gap);
        sheet += `<rect x="${x - 2}" y="${sy - 2}" width="${cell + 4}" height="${cell + 4}" fill="#5D5A7A"/><rect x="${x}" y="${sy}" width="${cell}" height="${cell}" fill="url(#ck)"/>`;
        sheet += OG.bitmap(ogSprite(f), OG_COL, { x, y: sy, px });
        sheet += tiny('0' + f, x, sy - 14, 2, '#8D89B0');
      }
      // The palette strip under the frames.
      let pal = '';
      ['k', 'b', 'h', 'd', 'f', 'w'].forEach((k, i) => (pal += `<rect x="${sx + i * 14}" y="${sy + cell + 10}" width="12" height="12" fill="${OG_COL[k]}"/>`));
      pal += tiny('PAL 0', sx + 6 * 14 + 4, sy + cell + 13, 2, '#8D89B0');
      pal += tiny('16x16 WALK', sx + sw - tinyWidth('16x16 WALK') * 2, sy + cell + 13, 2, '#8D89B0');
      const name = wordRows(BUB, w, 2);
      const npx = 8, nx = Math.round(240 - (name[0].length * npx) / 2), ny = 156;
      return OG.logoSvg(
        sheet + pal +
          OG.bitmap(name, { '#': '#7A3FB8' }, { x: nx + npx / 2, y: ny + npx / 2, px: npx }) +
          OG.bitmap(name, { '#': '#FFFFFF' }, { x: nx, y: ny, px: npx }),
        { crisp: true }
      );
    },
    icon: () => OG.iconSvg(OG.bitmap(ogSprite(0), OG_COL, { x: 0, y: 0, px: 4 }), { crisp: true }),
  });

  /* ============================================================ 50. Copper Bars */

  const CB_FONT = "'Russo One', 'Arial Black', sans-serif";
  const COPPER = ['#FF2A2A', '#FF8C1A', '#FFE21A', '#3DFF5A', '#1AE6FF', '#3D6BFF', '#E23DFF'];
  const copperDefs = () =>
    `<defs>` +
    COPPER.map((c, k) =>
      `<linearGradient id="cb${k}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${mix(c, '#000000', 0.88)}"/><stop offset="0.3" stop-color="${c}"/><stop offset="0.5" stop-color="${mix(c, '#FFFFFF', 0.75)}"/><stop offset="0.7" stop-color="${c}"/><stop offset="1" stop-color="${mix(c, '#000000', 0.88)}"/></linearGradient>`
    ).join('') +
    `<linearGradient id="cu" x1="0" y1="0" x2="0" y2="1">` +
    [['#FFF6CF', 0, 0.2], ['#FFD27A', 0.2, 0.38], ['#E89A3C', 0.38, 0.52], ['#9A4F1C', 0.52, 0.6], ['#C9742C', 0.6, 0.74], ['#F2B25A', 0.74, 0.88], ['#FFE3A0', 0.88, 1]]
      .map(([c, a, b]) => `<stop offset="${a}" stop-color="${c}"/><stop offset="${b}" stop-color="${c}"/>`).join('') +
    `</linearGradient>` +
    `<linearGradient id="sc" gradientUnits="userSpaceOnUse" x1="60" y1="0" x2="420" y2="0">${COPPER.map((c, k) => `<stop offset="${(k / 6).toFixed(3)}" stop-color="${c}"/>`).join('')}</linearGradient>` +
    `</defs>`;

  OG.add({
    num: 50,
    id: 'copper-bars',
    name: 'Copper Bars',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Demoscene & early web'],
    paletteTag: 'Rainbow',
    risk: 'Bold',
    scene: false,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#E89A3C', '#FF2A2A', '#1AE6FF', '#E23DFF', '#FFE21A'],
    fonts: ['Russo One', 'Press Start 2P'],
    type: 'Russo One in a stepped copper-metal fill with a heavy black keyline; the scroller is Press Start 2P, one letter at a time on a sine.',
    concept:
      'A cracktro frozen mid-frame: raster bars rolling behind a metal logo, and a sine scroller underneath sending greetings. ' +
      'The demoscene is where people learned to squeeze a machine, which is OgOs in a sentence.',
    logo: (w) => {
      let bars = '';
      COPPER.forEach((c, k) => {
        const y = 112 + 58 * Math.sin(0.86 * k + 0.5);
        bars += `<rect x="0" y="${n2(y - 11)}" width="480" height="22" fill="url(#cb${k})"/>`;
      });
      const len = [...w].reduce((s, c) => s + (isUp(c) ? 100 : 86), 0);
      const t = (attrs) => `<text x="240" y="166" font-family="${CB_FONT}" font-size="122" text-anchor="middle" textLength="${len}" lengthAdjust="spacingAndGlyphs" ${attrs}>${OG.esc(w)}</text>`;
      const msg = 'GREETINGS TO ALL RETRO CODERS';
      let scroll = '';
      [...msg].forEach((ch, i) => {
        if (ch === ' ') return;
        const x = 66 + i * 12.4, y = 250 + 11 * Math.sin(i * 0.42 + 0.6);
        scroll += `<text x="${n2(x)}" y="${n2(y)}" font-family="'Press Start 2P', monospace" font-size="12" fill="url(#sc)">${ch}</text>`;
      });
      return OG.logoSvg(
        copperDefs() + bars +
          t('fill="#000" stroke="#000" stroke-width="12" stroke-linejoin="round"') +
          t('fill="url(#cu)" stroke="#FFF3D6" stroke-width="1.5"') +
          scroll,
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        copperDefs() + `<rect width="64" height="64" rx="10" fill="#000"/>` +
          [[1, 12], [4, 29], [6, 46]].map(([k, y]) => `<rect x="0" y="${y - 6}" width="64" height="12" fill="url(#cb${k})"/>`).join('') +
          `<ellipse cx="32" cy="32" rx="17" ry="20" fill="none" stroke="#000" stroke-width="16"/>` +
          `<ellipse cx="32" cy="32" rx="17" ry="20" fill="none" stroke="url(#cu)" stroke-width="10"/>`,
        { crisp: false }
      ),
  });

  /* ================================================================= 55. Plasma */

  const PL_FONT = { t: 0.21, n: 2.6, cw: 0.78, lw: 0.72, xh: 0.7, ds: 0.3 };
  const PL_PAL = (() => {
    const keys = ['#3B0A8C', '#E0218A', '#FF8A1F', '#FFE45C', '#2BD9FE', '#2A4BD7'];
    const out = [];
    const N = 48;
    for (let k = 0; k < N; k++) {
      const f = (k / N) * keys.length, i = Math.floor(f);
      out.push(mix(keys[i], keys[(i + 1) % keys.length], f - i));
    }
    return out;
  })();
  const plasmaAt = (x, y) => {
    const v = Math.sin(x / 5.2) + Math.sin(y / 3.7 + x / 9) + Math.sin((x + y) / 6.1) + Math.sin(Math.hypot(x - 30, y - 8) / 4.3);
    const f = (((v + 4) / 8) * 2) % 1;
    return PL_PAL[Math.floor(f * PL_PAL.length) % PL_PAL.length];
  };
  // Cells inside the mask get the plasma; a ring of dim cells around them glows.
  function plasmaCells(rows, x0, y0, px, ox = 0, oy = 0) {
    let out = '', halo = '';
    const H = rows.length, W = rows[0].length;
    const at = (i, j) => j >= 0 && j < H && i >= 0 && i < W && rows[j][i] === '#';
    for (let j = 0; j < H; j++)
      for (let i = 0; i < W; i++) {
        const X = x0 + i * px, Y = y0 + j * px;
        if (at(i, j)) out += `<rect x="${X}" y="${Y}" width="${px}" height="${px}" fill="${plasmaAt(i + ox, j + oy)}"/>`;
        else if (at(i - 1, j) || at(i + 1, j) || at(i, j - 1) || at(i, j + 1)) halo += `<rect x="${X}" y="${Y}" width="${px}" height="${px}" fill="${plasmaAt(i + ox, j + oy)}"/>`;
      }
    return { out, halo };
  }
  const PL_DEFS =
    `<defs><filter id="cyc" color-interpolation-filters="sRGB"><feColorMatrix type="hueRotate" values="0"><animate attributeName="values" values="0;360" dur="9s" repeatCount="indefinite"/></feColorMatrix></filter></defs>`;

  OG.add({
    num: 55,
    id: 'plasma',
    name: 'Plasma',
    caps: 'OgOs',
    flexCase: true,
    family: 'Experimental',
    sources: ['Demoscene & early web'],
    paletteTag: 'Neon',
    risk: 'Wild',
    scene: false,
    bg: '#07040F',
    ink: '#FFE45C',
    palette: ['#07040F', '#E0218A', '#FF8A1F', '#FFE45C', '#2BD9FE', '#3B0A8C'],
    fonts: [],
    type: 'Geometric letters built from superellipse strokes, rasterized to 6-pixel cells and used only as a mask for the effect.',
    concept:
      'The oldest trick in the demo book, a plasma of summed sine waves run through a cycling palette, poured into the letters and nowhere else. ' +
      'Every cell is computed, nothing is drawn by hand, and the palette slowly rotates the way VGA palette cycling did.',
    logo: (w) => {
      const A = rasterAlpha(20, PL_FONT);
      const rows = wordRows(A, w, 2);
      const px = 6, W = rows[0].length * px;
      const x0 = Math.round(240 - W / 2), y0 = 84;
      const p = plasmaCells(rows, x0, y0, px);
      return OG.logoSvg(PL_DEFS + `<g filter="url(#cyc)"><g opacity="0.22">${p.halo}</g>${p.out}</g>`, { crisp: true });
    },
    icon: () => {
      const rows = rasterGlyph('O', 14, { t: 0.27, n: 2.6, cw: 0.8, lw: 0.72, xh: 0.7, ds: 0 });
      const px = 4, W = rows[0].length * px, H = rows.length * px;
      const p = plasmaCells(rows, (64 - W) / 2, (64 - H) / 2, px, 6, 4);
      return OG.iconSvg(`<rect width="64" height="64" rx="10" fill="#07040F"/>` + p.out, { crisp: true });
    },
  });

  /* ================================================================== 60. 88x31 */

  // The badge's own lettering, in badge pixels: caps 7x9, lowercase 6 wide.
  const B7c = '.......', B6d = '......';
  const BADGE = {
    O: ['.#####.', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '.#####.', B7c, B7c],
    G: ['.#####.', '##...##', '##.....', '##.....', '##.####', '##...##', '##...##', '##...##', '.#####.', B7c, B7c],
    S: ['.#####.', '##...##', '##.....', '.####..', '..####.', '.....##', '.....##', '##...##', '.#####.', B7c, B7c],
    o: [B6d, B6d, B6d, '.####.', '##..##', '##..##', '##..##', '##..##', '.####.', B6d, B6d],
    g: [B6d, B6d, B6d, '.#####', '##..##', '##..##', '##..##', '.#####', '....##', '##..##', '.####.'],
    s: [B6d, B6d, B6d, '.#####', '##....', '.####.', '....##', '....##', '#####.', B6d, B6d],
  };

  // The badge icon, 21x21: a cyan ring with a lightning bolt through it.
  const BOLT_O = (() => {
    const bolt = [[13.5, 0.5], [5, 11.5], [9.5, 11.5], [6.5, 20.5], [16, 8.5], [11.5, 8.5], [15.5, 0.5]];
    const inPoly = (x, y) => {
      let c = false;
      for (let i = 0, j = bolt.length - 1; i < bolt.length; j = i++) {
        const [xi, yi] = bolt[i], [xj, yj] = bolt[j];
        if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) c = !c;
      }
      return c;
    };
    const cv = blank(21, 21);
    for (let j = 0; j < 21; j++)
      for (let i = 0; i < 21; i++) {
        const d = Math.hypot(i - 10, j - 10);
        if (d <= 9.6 && d >= 5.2) cv[j][i] = i + j < 16 ? 'a' : i + j > 24 ? 'c' : 'b';
      }
    const ringLine = outlineOf(cv, 'k', false);
    for (let j = 0; j < 21; j++) for (let i = 0; i < 21; i++) if (ringLine[j][i] === 'k') cv[j][i] = 'k';
    const b = blank(21, 21);
    for (let j = 0; j < 21; j++) for (let i = 0; i < 21; i++) if (inPoly(i + 0.5, j + 0.5)) b[j][i] = i > 10 ? 'z' : 'y';
    const bl = outlineOf(b, 'k', true);
    for (let j = 0; j < 21; j++) for (let i = 0; i < 21; i++) { if (bl[j][i] === 'k') cv[j][i] = 'k'; if (b[j][i] !== '.') cv[j][i] = b[j][i]; }
    return toRows(cv);
  })();
  const BOLT_COL = { a: '#AAFFFF', b: '#55FFFF', c: '#00AAAA', k: '#000000', y: '#FFFF55', z: '#FFAA00' };

  // An 88x31 button, drawn in badge pixels and scaled up whole.
  function badge(x, y, s, inner) {
    return `<g transform="translate(${x} ${y}) scale(${s})">` +
      `<rect width="88" height="31" fill="#000"/>${inner}</g>`;
  }
  const DITHER = `<defs><pattern id="dh" width="2" height="2" patternUnits="userSpaceOnUse"><rect width="2" height="2" fill="#00005A"/><rect width="1" height="1" fill="#0000AA"/><rect x="1" y="1" width="1" height="1" fill="#0000AA"/></pattern>` +
    `<pattern id="dg" width="2" height="2" patternUnits="userSpaceOnUse"><rect width="2" height="2" fill="#808080"/><rect width="1" height="1" fill="#C0C0C0"/><rect x="1" y="1" width="1" height="1" fill="#C0C0C0"/></pattern>` +
    `<pattern id="dk" width="2" height="2" patternUnits="userSpaceOnUse"><rect width="2" height="2" fill="#000000"/><rect width="1" height="1" fill="#003A00"/><rect x="1" y="1" width="1" height="1" fill="#003A00"/></pattern></defs>`;

  function poweredBadge(w) {
    const name = wordRows(BADGE, w, 1);
    const nx = 32 + Math.round((55 - name[0].length) / 2);
    const hasDesc = name.slice(9).some((r) => r.includes('#'));
    const ny = hasDesc ? 14 : 16;
    return (
      // Right panel: navy, dithered into a brighter blue at the top.
      `<rect x="1" y="1" width="86" height="29" fill="#00005A"/><rect x="31" y="1" width="56" height="9" fill="#0000AA"/><rect x="31" y="10" width="56" height="5" fill="url(#dh)"/>` +
      `<rect x="1" y="1" width="86" height="1" fill="#5555FF"/><rect x="1" y="1" width="1" height="29" fill="#5555FF"/>` +
      // Left panel: silver, beveled, with the icon.
      `<rect x="1" y="1" width="29" height="29" fill="#C0C0C0"/><rect x="1" y="1" width="29" height="1" fill="#FFFFFF"/><rect x="1" y="1" width="1" height="29" fill="#FFFFFF"/>` +
      `<rect x="1" y="29" width="29" height="1" fill="#808080"/><rect x="29" y="1" width="1" height="29" fill="#808080"/><rect x="30" y="1" width="1" height="29" fill="#000"/>` +
      OG.bitmap(BOLT_O, BOLT_COL, { x: 5, y: 5, px: 1 }) +
      tiny('POWERED', 32 + Math.round((55 - tinyWidth('POWERED')) / 2), 4, 1, '#FFFF55') +
      OG.bitmap(name, { '#': '#000044' }, { x: nx + 1, y: ny + 1, px: 1 }) +
      OG.bitmap(name, { '#': '#FFFFFF' }, { x: nx, y: ny, px: 1 })
    );
  }
  const viewedBadge = () =>
    `<rect x="1" y="1" width="86" height="29" fill="#C0C0C0"/><rect x="1" y="18" width="86" height="12" fill="url(#dg)"/>` +
    `<rect x="1" y="1" width="86" height="1" fill="#FFFFFF"/><rect x="1" y="1" width="1" height="29" fill="#FFFFFF"/><rect x="1" y="29" width="86" height="1" fill="#404040"/><rect x="86" y="1" width="1" height="29" fill="#404040"/>` +
    // A little monitor.
    `<rect x="5" y="6" width="16" height="13" fill="#000"/><rect x="6" y="7" width="14" height="11" fill="#E0E0E0"/><rect x="8" y="9" width="10" height="7" fill="#0000AA"/><rect x="9" y="10" width="3" height="2" fill="#55FFFF"/><rect x="10" y="19" width="6" height="2" fill="#000"/><rect x="7" y="21" width="12" height="3" fill="#000"/><rect x="8" y="22" width="10" height="1" fill="#808080"/>` +
    tiny('BEST VIEWED', 26, 5, 1, '#000000') + tiny('AT', 26, 12, 1, '#000000') + tiny('640x480', 37, 12, 1, '#0000AA') +
    tiny('IN 16 COLORS', 26, 21, 1, '#FFFFFF');
  const tccBadge = () =>
    `<rect x="1" y="1" width="86" height="29" fill="#000"/><rect x="1" y="1" width="86" height="29" fill="url(#dk)"/>` +
    `<rect x="1" y="1" width="86" height="1" fill="#00AA00"/><rect x="1" y="1" width="1" height="29" fill="#00AA00"/><rect x="1" y="29" width="86" height="1" fill="#003A00"/><rect x="86" y="1" width="1" height="29" fill="#003A00"/>` +
    `<rect x="5" y="6" width="19" height="19" fill="#000"/><rect x="5" y="6" width="19" height="1" fill="#55FF55"/><rect x="5" y="24" width="19" height="1" fill="#55FF55"/><rect x="5" y="6" width="1" height="19" fill="#55FF55"/><rect x="23" y="6" width="1" height="19" fill="#55FF55"/>` +
    OG.bitmap(['#...', '.#..', '..#.', '.#..', '#...'], { '#': '#55FF55' }, { x: 8, y: 11, px: 1 }) + `<rect x="13" y="16" width="6" height="1" fill="#55FF55"/>` +
    tiny('MADE WITH', 29, 5, 1, '#55FF55') +
    OG.bitmap(tinyRows('TCC'), { '#': '#005500' }, { x: 30, y: 13, px: 2 }) + OG.bitmap(tinyRows('TCC'), { '#': '#AAFFAA' }, { x: 29, y: 12, px: 2 });

  OG.add({
    num: 60,
    id: '88x31',
    name: '88x31',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['Demoscene & early web'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: '#FFFFFF',
    ink: '#000080',
    palette: ['#0000AA', '#C0C0C0', '#FFFF55', '#55FFFF', '#55FF55', '#000000'],
    fonts: [],
    type: 'Everything is drawn at the badge\'s native 88 by 31 pixels: a 7-pixel bitmap face for the name, a 3x5 micro font for the small print.',
    concept:
      'The name as an early-web button, the kind every homepage lined up in its footer, with a bolt-through-an-O icon and two friends: best viewed at 640x480, made with TCC. ' +
      'A whole brand in 88 by 31 pixels, small enough to sit in any README or homepage footer.',
    logo: (w) => OG.logoSvg(DITHER + badge(64, 46, 4, poweredBadge(w)) + badge(60, 190, 2, viewedBadge()) + badge(244, 190, 2, tccBadge()), { crisp: true }),
    icon: () => OG.iconSvg(OG.bitmap(BOLT_O, BOLT_COL, { x: 0.5, y: 0.5, px: 3 }), { crisp: true }),
  });

  /* ================================================================ 65. WordArt */

  const WA_FONT = "'Tinos', 'Times New Roman', serif";
  const RAINBOW = ['#E8141E', '#FF8C00', '#FFE600', '#1EB43C', '#1E64F0', '#8C28C8'];
  const waDefs = (x1, x2) =>
    `<defs><linearGradient id="rb" gradientUnits="userSpaceOnUse" x1="${x1}" y1="0" x2="${x2}" y2="0">${RAINBOW.map((c, k) => `<stop offset="${(k / 5).toFixed(2)}" stop-color="${c}"/>`).join('')}</linearGradient>` +
    `<path id="arc" d="M36 236 A 290 290 0 0 1 444 236" fill="none"/></defs>`;

  OG.add({
    num: 65,
    id: 'wordart',
    name: 'WordArt',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Windows 9x', 'Demoscene & early web'],
    paletteTag: 'Rainbow',
    risk: 'Wild',
    scene: false,
    bg: '#FFFFFF',
    ink: '#1A1A6E',
    palette: ['#FFFFFF', '#E8141E', '#FFE600', '#1EB43C', '#1E64F0', '#8C28C8'],
    fonts: ['Tinos:wght@700'],
    type: 'Tinos Bold (metric twin of the Times New Roman in every 1997 newsletter), set on an arch, with a navy outline and a gray drop shadow.',
    concept:
      'The name in full 1995 office-suite glory: arched, rainbow-filled, outlined and drop-shadowed, exactly as a school newsletter would have set it. ' +
      'Gloriously tacky on purpose, and a reminder that the fun of these machines was making things look like this.',
    logo: (w) => {
      // Approximate advances of Tinos Bold, to size each case to the arch.
      const adv = { O: 0.78, G: 0.78, S: 0.56, o: 0.5, g: 0.5, s: 0.39 };
      const em = [...w].reduce((s, c) => s + (adv[c] || 0.6), 0);
      const size = Math.min(136, 340 / em), len = em * size;
      const tp = (attrs) => `<text font-family="${WA_FONT}" font-weight="700" font-size="${n2(size)}" ${attrs}><textPath href="#arc" startOffset="50%" text-anchor="middle">${OG.esc(w)}</textPath></text>`;
      return OG.logoSvg(
        waDefs(240 - len / 2, 240 + len / 2) +
          `<g transform="translate(7 7)">${tp('fill="#9A9AA6"')}</g>` +
          tp('fill="url(#rb)" stroke="#1A1A6E" stroke-width="3.5" stroke-linejoin="round" paint-order="stroke"') +
          tp('fill="url(#rb)"'),
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        waDefs(10, 54) +
          OG.text('O', { x: 35, y: 56, size: 68, family: WA_FONT, weight: 700, fill: '#9A9AA6', anchor: 'middle' }) +
          OG.text('O', { x: 31, y: 52, size: 68, family: WA_FONT, weight: 700, fill: 'url(#rb)', anchor: 'middle', attrs: 'stroke="#1A1A6E" stroke-width="3" paint-order="stroke"' }),
        { crisp: false }
      ),
  });

  /* ================================================================ 70. Tracker */

  const TR_FONT = "'Space Mono', 'Courier New', monospace";
  const TR = { cw: 7.5, ch: 13, cols: 64, rows: 17, top: 79 };
  const TR_GLYPH = { t: 0.22, n: 2.6, cw: 0.7, lw: 0.66, xh: 0.7, ds: 0.3 };
  // Rasterizes onto tall character cells.
  function rasterCells(ch, capRows, o, aspect) {
    const sk = skeleton(ch, o);
    const cs = 1 / capRows, cols = Math.round(sk.w / (cs * aspect)), rows = Math.round((1 + o.ds) / cs);
    const out = [];
    for (let j = 0; j < rows; j++) {
      let s = '';
      for (let i = 0; i < cols; i++) {
        const x = (i + 0.5) * (sk.w / cols), y = (j + 0.5) * cs;
        let d = 9;
        sk.p.forEach((pl) => { for (let k = 1; k < pl.length; k++) d = Math.min(d, segDist(x, y, pl[k - 1][0], pl[k - 1][1], pl[k][0], pl[k][1])); });
        s += d <= sk.t / 2 ? '#' : '.';
      }
      out.push(s);
    }
    return out;
  }
  // One row of pattern data, four channels of note, instrument, volume, effect.
  const NOTES = ['C-4', 'D#4', 'G-4', 'A#4', 'C-5', 'F-3', 'G-3', 'C-3'];
  function trackerRow(r, rnd) {
    let s = r.toString(16).toUpperCase().padStart(2, '0') + ' ';
    for (let k = 0; k < 4; k++) {
      const hit = r % 4 === 0 ? rnd() < 0.85 : rnd() < 0.42;
      const note = hit ? NOTES[Math.floor(rnd() * NOTES.length)] : '---';
      const ins = hit ? '0' + (k + 1) : '..';
      const vol = hit && rnd() < 0.6 ? (rnd() < 0.5 ? '40' : '2C') : '..';
      const fx = rnd() < 0.3 ? ['0F3', 'A04', '304', 'E91', 'C20'][Math.floor(rnd() * 5)] : '...';
      s += `|${note} ${ins} ${vol} ${fx}`;
    }
    return s.padEnd(TR.cols, ' ');
  }

  OG.add({
    num: 70,
    id: 'tracker',
    name: 'Tracker',
    caps: 'ogos',
    flexCase: true,
    family: 'Experimental',
    sources: ['Demoscene & early web'],
    paletteTag: 'Phosphor',
    risk: 'Wild',
    scene: true,
    bg: '#0B1030',
    ink: '#D8E4FF',
    palette: ['#0B1030', '#D8E4FF', '#7F9CFF', '#FFD84A', '#3DFF7A', '#FF4A4A'],
    fonts: ['Space Mono:wght@700'],
    type: 'Rows of pattern data in Space Mono Bold; the name is drawn by inverting whole character cells, letters rasterized onto the tall text grid.',
    concept:
      'A music tracker mid-song: four channels of notes, instruments and effects scrolling past, with the cells inverted so the pattern itself spells the name, and VU meters jumping above. ' +
      'Chiptunes were written like code, row by row in hex, so it belongs on a system for retro coding.',
    logo: (w) => {
      const { cw, ch, cols, rows, top } = TR;
      const A = {};
      ['O', 'G', 'S', 'o', 'g', 's'].forEach((c) => (A[c] = rasterCells(c, 10, TR_GLYPH, cw / ch)));
      const word = wordRows(A, w, 2);
      const hasDesc = word.slice(10).some((r) => r.includes('#'));
      const used = hasDesc ? word.length : 10;
      const gx = 3 + Math.round((cols - 3 - word[0].length) / 2), gy = Math.round((rows - used) / 2);
      const rnd = OG.rng(31);
      let bands = '', text = '', nums = '';
      for (let r = 0; r < rows; r++) {
        const y = top + r * ch;
        if (r % 4 === 0) bands += `<rect x="0" y="${y}" width="480" height="${ch}" fill="#141C4A"/>`;
        const line = trackerRow(r, rnd);
        text += `<text x="0" y="${y + 10}" font-size="11.4" textLength="${cols * cw}" lengthAdjust="spacing" xml:space="preserve">${OG.esc(line)}</text>`;
        nums += `<rect x="0" y="${y}" width="${2 * cw + 2}" height="${ch}" fill="${r % 4 === 0 ? '#3A2E10' : '#1E1A2A'}"/>`;
      }
      // The inverted cells that make the name.
      const mask = blank(cols, rows);
      word.forEach((rw, j) => [...rw].forEach((c, i) => { if (c === '#' && gy + j < rows) mask[gy + j][gx + i] = '#'; }));
      const inv = OG.bitmap(toRows(mask), { '#': '#D8E4FF' }, { x: 0, y: top, px: cw, py: ch });
      // Header: song info and four VU meters.
      let vu = '';
      [0.85, 0.55, 1, 0.4].forEach((lv, k) => {
        const x0 = 3 * cw + k * 15.25 * cw + 18;
        for (let s = 0; s < 14; s++) {
          const lit = s < Math.round(lv * 14);
          const col = s < 9 ? '#3DFF7A' : s < 12 ? '#FFD84A' : '#FF4A4A';
          vu += `<rect x="${n2(x0 + s * 6.5)}" y="38" width="5" height="22" fill="${col}" opacity="${lit ? 1 : 0.16}"/>`;
        }
        vu += OG.text(String(k + 1), { x: n2(x0 - 10), y: 55, size: 11, family: TR_FONT, weight: 700, fill: '#7F9CFF' });
      });
      return OG.logoSvg(
        `<defs><clipPath id="ic">${inv}</clipPath></defs>` +
          `<rect width="480" height="300" fill="#0B1030"/>` +
          `<rect width="480" height="${top - 3}" fill="#1E2546"/><rect y="${top - 3}" width="480" height="2" fill="#4A5694"/>` +
          OG.text('OGOS.XM', { x: 12, y: 24, size: 13, family: TR_FONT, weight: 700, fill: '#FFD84A' }) +
          OG.text('BPM 125  SPD 06  PAT 00/12', { x: 468, y: 24, size: 11, family: TR_FONT, weight: 700, fill: '#7F9CFF', anchor: 'end' }) +
          vu + bands + nums +
          `<g font-family="${OG.esc(TR_FONT)}" font-weight="700" fill="#6F86D8">${text}</g>` +
          inv +
          `<g font-family="${OG.esc(TR_FONT)}" font-weight="700" fill="#0B1030" clip-path="url(#ic)">${text}</g>`,
        { crisp: false }
      );
    },
    icon: () => {
      let s = `<rect width="64" height="64" rx="8" fill="#1E2546"/>`;
      [0.7, 1, 0.45, 0.8].forEach((lv, k) => {
        for (let q = 0; q < 6; q++) {
          const lit = q < Math.round(lv * 6);
          s += `<rect x="${8 + k * 13}" y="${52 - q * 8}" width="10" height="6" fill="${q < 3 ? '#3DFF7A' : q < 5 ? '#FFD84A' : '#FF4A4A'}" opacity="${lit ? 1 : 0.18}"/>`;
        }
      });
      return OG.iconSvg(s, { crisp: true });
    },
  });

  /* ============================================================== 75. Neon Grid */

  const NG_FONT = "'Mr Dafoe', 'Brush Script MT', cursive";
  const neonDefs = (sunR) =>
    `<defs>` +
    `<linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0B0120"/><stop offset="0.55" stop-color="#2A0B4E"/><stop offset="1" stop-color="#7A1A78"/></linearGradient>` +
    `<linearGradient id="sun" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFF27A"/><stop offset="0.45" stop-color="#FF9A3C"/><stop offset="1" stop-color="#FF2E88"/></linearGradient>` +
    `<linearGradient id="flr" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2A0040"/><stop offset="1" stop-color="#0A0018"/></linearGradient>` +
    `<filter id="ng" x="-30%" y="-60%" width="160%" height="220%"><feGaussianBlur stdDeviation="5"/></filter>` +
    `<filter id="lg" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="2"/></filter>` +
    `</defs>`;

  // A sun with its lower half sliced into ever-wider gaps.
  function stripedSun(cx, cy, r, horizon) {
    let cuts = '';
    let y = cy - r * 0.1, gap = r * 0.035, k = 0;
    while (y < horizon) {
      cuts += `<rect x="${cx - r}" y="${n2(y)}" width="${2 * r}" height="${n2(gap)}" fill="#000"/>`;
      y += gap + r * (0.16 - k * 0.012);
      gap += r * 0.03;
      k++;
    }
    return `<mask id="sm"><rect x="${cx - r}" y="${cy - r}" width="${2 * r}" height="${horizon - cy + r}" fill="#fff"/>${cuts}</mask>` +
      `<circle cx="${cx}" cy="${cy}" r="${r}" fill="url(#sun)" mask="url(#sm)"/>`;
  }
  function neonFloor(x0, w, horizon, h, vx, n, rows, col, sw) {
    let d = '';
    for (let i = -n; i <= n; i++) d += `M${n2(vx + i * (w / n) * 0.18)} ${horizon}L${n2(vx + i * (w / n) * 1.6)} ${horizon + h}`;
    for (let k = 1; k <= rows; k++) {
      const y = horizon + h * Math.pow(k / rows, 2.1);
      d += `M${x0} ${n2(y)}H${x0 + w}`;
    }
    return `<path d="${d}" stroke="${col}" stroke-width="${sw * 2.5}" fill="none" filter="url(#lg)" opacity="0.8"/><path d="${d}" stroke="${col}" stroke-width="${sw}" fill="none"/>`;
  }

  OG.add({
    num: 75,
    id: 'neon-grid',
    name: 'Neon Grid',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Demoscene & early web', 'OgOs internals'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: true,
    bg: '#14032E',
    ink: '#7FF8FF',
    palette: ['#14032E', '#FF2BD6', '#00F0FF', '#FFF27A', '#FF2E88'],
    fonts: ['Mr Dafoe', 'Orbitron:wght@700'],
    type: 'Mr Dafoe, a fast brush script, lit as white neon with a cyan halo, over a tracked Orbitron line.',
    concept:
      'Synthwave at dusk: a striped sun sinking behind a magenta grid, and the name written across it in cyan neon. ' +
      'It grows straight out of the RetrOS wallpaper, so the rename keeps the mood people already know.',
    logo: (w) => {
      const H = 196;
      const len = [...w].reduce((s, c) => s + (isUp(c) ? 92 : 74), 0);
      const t = (attrs) => `<text x="240" y="176" font-family="${NG_FONT}" font-size="132" text-anchor="middle" textLength="${len}" lengthAdjust="spacingAndGlyphs" transform="rotate(-7 240 160)" ${attrs}>${OG.esc(w)}</text>`;
      return OG.logoSvg(
        neonDefs() +
          `<rect width="480" height="${H}" fill="url(#sky)"/>` +
          `<defs>${''}</defs>` + stripedSun(240, 150, 96, H) +
          `<rect y="${H}" width="480" height="${300 - H}" fill="url(#flr)"/>` +
          neonFloor(0, 480, H, 104, 240, 9, 7, '#FF2BD6', 1.6) +
          `<rect y="${H - 1}" width="480" height="2" fill="#FF8AE8"/>` +
          t('fill="none" stroke="#00E5FF" stroke-width="9" filter="url(#ng)" opacity="0.95"') +
          t('fill="none" stroke="#00E5FF" stroke-width="3.5"') +
          t('fill="#F2FFFF"') +
          OG.text('RETRO  OPERATING  SYSTEM', { x: 240, y: 270, size: 12, family: "'Orbitron', sans-serif", weight: 700, fill: '#FF8AE8', anchor: 'middle', ls: 5 }),
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        neonDefs() + `<rect width="64" height="64" rx="10" fill="url(#sky)"/>` +
          `<g>${stripedSun(32, 30, 20, 40)}</g>` +
          `<rect y="40" width="64" height="24" fill="url(#flr)"/>` +
          neonFloor(0, 64, 40, 24, 32, 3, 3, '#FF2BD6', 1.6),
        { crisp: false }
      ),
  });

  /* =============================================================== 80. LCD Game */

  // A 14-segment cell. Segments: a b c d e f (outer), g1 g2 (middle),
  // h i j (upper diagonals and stem), k l m (lower).
  function seg14(x, y, w, h, t, lit, onC, offC, shadow) {
    const g = t * 0.28, m = y + h / 2, cx = x + w / 2;
    const hz = (x0, x1, yy) => `M${n2(x0 + g)} ${n2(yy)}l${n2(t / 2)} ${n2(-t / 2)}H${n2(x1 - g - t / 2)}l${n2(t / 2)} ${n2(t / 2)}l${n2(-t / 2)} ${n2(t / 2)}H${n2(x0 + g + t / 2)}z`;
    const vt = (xx, y0, y1) => `M${n2(xx)} ${n2(y0 + g)}l${n2(t / 2)} ${n2(t / 2)}V${n2(y1 - g - t / 2)}l${n2(-t / 2)} ${n2(t / 2)}l${n2(-t / 2)} ${n2(-t / 2)}V${n2(y0 + g + t / 2)}z`;
    const dg = (x0, y0, x1, y1) => {
      const k = t * 0.42;
      return `M${n2(x0)} ${n2(y0)}L${n2(x0 + k)} ${n2(y0)}L${n2(x1)} ${n2(y1 - k)}L${n2(x1)} ${n2(y1)}L${n2(x1 - k)} ${n2(y1)}L${n2(x0)} ${n2(y0 + k)}z`;
    };
    const ix = x + t + g, iy = y + t + g, ox = x + w - t - g, oy = y + h - t - g;
    const S = {
      a: hz(x + t / 2, x + w - t / 2, y + t / 2),
      d: hz(x + t / 2, x + w - t / 2, y + h - t / 2),
      g1: hz(x + t / 2, cx, m),
      g2: hz(cx, x + w - t / 2, m),
      f: vt(x + t / 2, y + t / 2, m),
      e: vt(x + t / 2, m, y + h - t / 2),
      b: vt(x + w - t / 2, y + t / 2, m),
      c: vt(x + w - t / 2, m, y + h - t / 2),
      i: vt(cx, y + t / 2, m),
      l: vt(cx, m, y + h - t / 2),
      h: dg(ix, iy, cx - t / 2 - g, m - t / 2 - g),
      j: '',
      k: '',
      m: dg(cx + t / 2 + g, m + t / 2 + g, ox, oy),
    };
    // The upper-right and lower-left diagonals lean the other way.
    S.j = `M${n2(ox)} ${n2(iy)}L${n2(ox)} ${n2(iy + t * 0.42)}L${n2(cx + t / 2 + g + t * 0.42)} ${n2(m - t / 2 - g)}L${n2(cx + t / 2 + g)} ${n2(m - t / 2 - g)}L${n2(cx + t / 2 + g)} ${n2(m - t / 2 - g - t * 0.42)}L${n2(ox - t * 0.42)} ${n2(iy)}z`;
    S.k = `M${n2(cx - t / 2 - g)} ${n2(m + t / 2 + g)}L${n2(cx - t / 2 - g)} ${n2(m + t / 2 + g + t * 0.42)}L${n2(ix + t * 0.42)} ${n2(oy)}L${n2(ix)} ${n2(oy)}L${n2(ix)} ${n2(oy - t * 0.42)}L${n2(cx - t / 2 - g - t * 0.42)} ${n2(m + t / 2 + g)}z`;
    let on = '', off = '';
    Object.keys(S).forEach((s) => (lit.includes(s) ? (on += S[s]) : (off += S[s])));
    return `<path d="${off}" fill="${offC}"/>` + (shadow ? `<path d="${on}" fill="${shadow}" transform="translate(3 3)"/>` : '') + `<path d="${on}" fill="${onC}"/>`;
  }
  const SEG14 = {
    O: ['a', 'b', 'c', 'd', 'e', 'f'], G: ['a', 'c', 'd', 'e', 'f', 'g2'], S: ['a', 'f', 'g1', 'g2', 'c', 'd'],
    o: ['c', 'd', 'e', 'g1', 'g2'], g: ['a', 'b', 'c', 'd', 'f', 'g1', 'g2'], s: ['a', 'f', 'g1', 'g2', 'c', 'd'],
  };
  // Little LCD pictograms.
  const HEART = 'M0 -3C0 -7 -6 -8 -7 -3C-8 1 -3 4 0 7C3 4 8 1 7 -3C6 -8 0 -7 0 -3Z';
  const BELL = 'M-6 4C-5 2 -5 -1 -5 -3C-5 -6 -3 -8 0 -8C3 -8 5 -6 5 -3C5 -1 5 2 6 4ZM-2 5.5A2 2 0 0 0 2 5.5Z';
  const NOTE = 'M-3 5A2.6 2 0 1 1 -3 4.9ZM-0.8 5V-7L6 -9V-6.5L1 -5V5Z';

  OG.add({
    num: 80,
    id: 'lcd-game',
    name: 'LCD Game',
    caps: 'OGOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Games & arcade'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: true,
    bg: '#B7BFA4',
    ink: '#1D2218',
    palette: ['#B7BFA4', '#1D2218', '#8E977C', '#C9465A'],
    fonts: ['Orbitron:wght@700'],
    type: 'Fourteen-segment LCD letters drawn as segment polygons, every unlit segment still faintly there; Orbitron for the printed labels.',
    concept:
      'The face of a pocket LCD game: the name in big fourteen-segment characters with their ghost segments showing, a score, a ball frozen at one of its seven fixed positions, and the little heart, bell and note pictograms that only ever had two states. ' +
      'Lowercase falls back to what segments can do, which is half the charm.',
    logo: (w) => {
      const chars = [...w];
      const cw = 80, chh = 124, gap = 22, t = 10.5;
      const total = chars.length * cw + (chars.length - 1) * gap;
      const x0 = 240 - total / 2, y0 = 92;
      let segs = '';
      chars.forEach((c, k) => (segs += seg14(x0 + k * (cw + gap), y0, cw, chh, t, glyphOf(SEG14, c), '#1D2218', 'rgba(29,34,24,0.075)', 'rgba(29,34,24,0.16)')));
      segs = `<g transform="translate(${n2(0.105 * (y0 + chh / 2))} 0) skewX(-6)">${segs}</g>`;
      // A small seven-segment score at top right.
      let score = '';
      '0032'.split('').forEach((d, k) => (score += seg7(372 + k * 21, 34, 15, 28, 3.6, SEG7[d], '#1D2218', 'rgba(29,34,24,0.08)')));
      const ico = (d, x, y, on) => `<path d="${d}" transform="translate(${x} ${y}) scale(1.3)" fill="${on ? '#1D2218' : 'rgba(29,34,24,0.08)'}"/>`;
      return OG.logoSvg(
        `<defs><linearGradient id="lcd" x1="0" y1="0" x2="0.3" y2="1"><stop offset="0" stop-color="#C6CEB2"/><stop offset="1" stop-color="#A9B296"/></linearGradient></defs>` +
          `<rect width="480" height="300" fill="url(#lcd)"/>` +
          `<rect x="12" y="12" width="456" height="276" rx="10" fill="none" stroke="#1D2218" stroke-width="2.5" opacity="0.85"/>` +
          ico(HEART, 44, 48, true) + ico(BELL, 80, 48, true) + ico(NOTE, 116, 48, false) +
          // The fixed positions a ball can take, one of them lit.
          [0, 1, 2, 3, 4, 5, 6].map((k) => {
            const a = Math.PI * (0.12 + (0.76 * k) / 6);
            return `<circle cx="${n2(258 - Math.cos(a) * 62)}" cy="${n2(76 - Math.sin(a) * 40)}" r="5.5" fill="${k === 2 ? '#1D2218' : 'rgba(29,34,24,0.08)'}"/>`;
          }).join('') +
          OG.text('ALARM', { x: 140, y: 53, size: 10, family: "'Orbitron', sans-serif", weight: 700, fill: 'rgba(29,34,24,0.1)' }) +
          OG.text('AM', { x: 344, y: 46, size: 10, family: "'Orbitron', sans-serif", weight: 700, fill: '#1D2218' }) +
          score + segs +
          `<path d="M36 252l7 -5v10z" fill="#1D2218"/>` +
          OG.text('GAME A', { x: 50, y: 256, size: 11, family: "'Orbitron', sans-serif", weight: 700, fill: '#1D2218', ls: 1 }) +
          `<path d="M140 252l7 -5v10z" fill="rgba(29,34,24,0.08)"/>` +
          OG.text('GAME B', { x: 154, y: 256, size: 11, family: "'Orbitron', sans-serif", weight: 700, fill: 'rgba(29,34,24,0.1)', ls: 1 }) +
          OG.text('32-BIT', { x: 444, y: 256, size: 11, family: "'Orbitron', sans-serif", weight: 700, fill: '#C9465A', anchor: 'end', ls: 2 }),
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="8" fill="#B7BFA4"/>` + seg14(14, 7, 36, 50, 7, SEG14.O, '#1D2218', 'rgba(29,34,24,0.12)', 'rgba(29,34,24,0.16)'),
        { crisp: false }
      ),
  });

  /* ================================================================ 85. Famicom */

  // オ, ゴ and ス, hand-drawn on a 14x14 grid, two pixels a stroke.
  const KANA = {
    o: ['........##....', '........##....', '##############', '##############', '.......###....', '......####....', '.....##.##....', '....##..##....', '...##...##....', '..##....##....', '.##.....##....', '##......##....', '.....#####....', '.....####.....'],
    go: ['.........##.##', '.........##.##', '..........##.#', '##########....', '##########....', '........##....', '........##....', '........##....', '........##....', '........##....', '........##....', '........##....', '##########....', '##########....'],
    su: ['..............', '.##########...', '.###########..', '.........###..', '........###...', '.......###....', '......###.....', '.....######...', '....###..###..', '...###....###.', '..###......###', '.###........##', '###...........', '##............'],
  };

  // Gold pixel letters with a dark-red extrusion and outline.
  function kanaArt(glyphs, gap) {
    const W = glyphs.length * 14 + (glyphs.length - 1) * gap + 6, H = 14 + 6;
    const fill = blank(W, H);
    glyphs.forEach((g, k) => KANA[g].forEach((r, j) => [...r].forEach((c, i) => {
      if (c !== '#') return;
      const top = !(j > 0 && KANA[g][j - 1][i] === '#');
      const bot = !(j < 13 && KANA[g][j + 1][i] === '#');
      fill[j + 2][2 + k * (14 + gap) + i] = top ? 'h' : bot ? 'd' : 'g';
    })));
    const ext = blank(W, H);
    for (let j = 0; j < H; j++) for (let i = 0; i < W; i++) if (!on(fill, i, j) && (on(fill, i - 1, j - 1) || on(fill, i - 2, j - 2))) ext[j][i] = 'x';
    const both = fill.map((r, j) => r.map((c, i) => (c !== '.' || ext[j][i] !== '.' ? '#' : '.')));
    const line = outlineOf(both, 'k', true);
    return { W, H, fill: toRows(fill), ext: toRows(ext), line: toRows(line) };
  }
  const FC = { h: '#FFF2B0', g: '#F7C948', d: '#D99A1E', x: '#6E0014', k: '#2A0008' };

  OG.add({
    num: 85,
    id: 'famicom',
    name: 'Famicom',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Games & arcade'],
    paletteTag: 'Primary',
    risk: 'Wild',
    scene: false,
    bg: '#C8102E',
    ink: '#F7C948',
    palette: ['#C8102E', '#F7C948', '#FFF2B0', '#6E0014', '#FFFFFF'],
    fonts: [],
    type: 'Katakana オゴス hand-drawn on a 14-pixel grid in gold with a two-step dark red extrusion; the Latin name below in the 88x31 bitmap face.',
    concept:
      'OgOs as it would read on a Japanese cartridge label: オゴス, o-go-su, big and gold on red, with the Latin name on a gold band beneath. ' +
      'It looks the furthest from home of these directions, and the closest to the cartridges that taught a generation what a title screen was.',
    logo: (w) => {
      const art = kanaArt(['o', 'go', 'su'], 3);
      const px = 8, x = Math.round(240 - (art.W * px) / 2) + 6, y = 36;
      const name = wordRows(BADGE, w, 1);
      const npx = 3, nw = name[0].length * npx;
      const hasDesc = name.slice(9).some((r) => r.includes('#'));
      const by = 208, bh = 44;
      const ny = by + Math.round((bh - (hasDesc ? 11 : 9) * npx) / 2);
      const star = (cx, cy) => OG.bitmap(['..#..', '..#..', '#####', '.###.', '##.##'], { '#': '#C8102E' }, { x: cx - 7, y: cy - 7, px: 3 });
      return OG.logoSvg(
        OG.bitmap(art.line, FC, { x, y, px }) + OG.bitmap(art.ext, FC, { x, y, px }) + OG.bitmap(art.fill, FC, { x, y, px }) +
          `<rect x="48" y="${by - 6}" width="384" height="3" fill="#F7C948"/>` +
          `<rect x="48" y="${by}" width="384" height="${bh}" fill="#F7C948"/>` +
          `<rect x="48" y="${by + bh + 3}" width="384" height="3" fill="#F7C948"/>` +
          OG.bitmap(name, { '#': '#C8102E' }, { x: Math.round(240 - nw / 2), y: ny, px: npx }) +
          star(240 - nw / 2 - 36, by + bh / 2) + star(240 + nw / 2 + 36, by + bh / 2) +
          [70, 82, 94].map((sx) => `<path d="M${sx} ${by}h6l-12 ${bh}h-6z" fill="#C8102E"/>`).join('') +
          [386, 398, 410].map((sx) => `<path d="M${sx + 12} ${by}h6l-12 ${bh}h-6z" fill="#C8102E"/>`).join(''),
        { crisp: true }
      );
    },
    icon: () => {
      const art = kanaArt(['o'], 0);
      const px = 3;
      return OG.iconSvg(`<rect width="64" height="64" rx="10" fill="#C8102E"/>` + OG.bitmap(art.line, FC, { x: 2, y: 2, px }) + OG.bitmap(art.ext, FC, { x: 2, y: 2, px }) + OG.bitmap(art.fill, FC, { x: 2, y: 2, px }), { crisp: true });
    },
  });

  /* ================================================================== 90. Pipes */

  // Letters as runs of straight pipe, in units of 10: caps 6x10, lowercase 5 wide.
  const PIPES = {
    O: { w: 6, p: [[0, 0], [6, 0], [6, 10], [0, 10], [0, 0]] },
    G: { w: 6, p: [[6, 2.6], [6, 0], [0, 0], [0, 10], [6, 10], [6, 5], [3, 5]] },
    S: { w: 6, p: [[6, 0], [0, 0], [0, 5], [6, 5], [6, 10], [0, 10]] },
    o: { w: 5, p: [[0, 3], [5, 3], [5, 10], [0, 10], [0, 3]] },
    g: { w: 5, p: [[5, 8], [0, 8], [0, 3], [5, 3], [5, 13], [0, 13]] },
    s: { w: 5, p: [[5, 3], [0, 3], [0, 6.5], [5, 6.5], [5, 10], [0, 10]] },
  };
  const PIPE_COL = ['#E8282B', '#25B84A', '#2F6BFF', '#F2C21B'];
  const pipeDefs = (cols) =>
    `<defs>` +
    cols.map((c, k) =>
      `<linearGradient id="ph${k}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${mix(c, '#000000', 0.6)}"/><stop offset="0.22" stop-color="${mix(c, '#FFFFFF', 0.65)}"/><stop offset="0.42" stop-color="${c}"/><stop offset="0.8" stop-color="${mix(c, '#000000', 0.35)}"/><stop offset="1" stop-color="${mix(c, '#000000', 0.75)}"/></linearGradient>` +
      `<linearGradient id="pv${k}" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="${mix(c, '#000000', 0.6)}"/><stop offset="0.22" stop-color="${mix(c, '#FFFFFF', 0.65)}"/><stop offset="0.42" stop-color="${c}"/><stop offset="0.8" stop-color="${mix(c, '#000000', 0.35)}"/><stop offset="1" stop-color="${mix(c, '#000000', 0.75)}"/></linearGradient>` +
      `<radialGradient id="pj${k}" cx="0.36" cy="0.32" r="0.72"><stop offset="0" stop-color="#FFFFFF"/><stop offset="0.25" stop-color="${mix(c, '#FFFFFF', 0.35)}"/><stop offset="0.65" stop-color="${c}"/><stop offset="1" stop-color="${mix(c, '#000000', 0.7)}"/></radialGradient>`
    ).join('') +
    `</defs>`;
  // One pipe: straight runs shaded across their width, balls at every bend.
  function pipe(pts, k, x0, y0, u, r) {
    let runs = '', balls = '';
    for (let i = 1; i < pts.length; i++) {
      const [ax, ay] = pts[i - 1], [bx, by] = pts[i];
      const X0 = x0 + Math.min(ax, bx) * u, X1 = x0 + Math.max(ax, bx) * u, Y0 = y0 + Math.min(ay, by) * u, Y1 = y0 + Math.max(ay, by) * u;
      if (ay === by) runs += `<rect x="${n2(X0)}" y="${n2(Y0 - r)}" width="${n2(X1 - X0)}" height="${2 * r}" fill="url(#ph${k})"/>`;
      else runs += `<rect x="${n2(X0 - r)}" y="${n2(Y0)}" width="${2 * r}" height="${n2(Y1 - Y0)}" fill="url(#pv${k})"/>`;
    }
    const closed = pts[0][0] === pts[pts.length - 1][0] && pts[0][1] === pts[pts.length - 1][1];
    pts.forEach(([x, y], i) => {
      if (closed && i === pts.length - 1) return;
      const end = !closed && (i === 0 || i === pts.length - 1);
      balls += `<circle cx="${n2(x0 + x * u)}" cy="${n2(y0 + y * u)}" r="${n2(r * (end ? 1.12 : 1.36))}" fill="url(#pj${k})"/>`;
    });
    return runs + balls;
  }

  OG.add({
    num: 90,
    id: 'pipes',
    name: 'Pipes',
    caps: 'OGOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Windows 9x'],
    paletteTag: 'Primary',
    risk: 'Bold',
    scene: false,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#E8282B', '#25B84A', '#2F6BFF', '#F2C21B'],
    fonts: [],
    type: 'Each letter is one length of plumbing: straight runs shaded as cylinders, a glossy ball joint at every bend.',
    concept:
      'The famous screensaver that grew shaded pipes across the screen, steered into spelling the name, one glossy color per letter. ' +
      'It is the moment a 90s PC first showed off real-time 3D to everyone in the office, left running all night.',
    logo: (w) => {
      const u = 10.5, r = 9.5, gap = 3;
      const chars = [...w];
      const width = chars.reduce((s, c, k) => s + (k ? gap : 0) + glyphOf(PIPES, c).w, 0) * u;
      const hasDesc = chars.includes('g');
      let x = 240 - width / 2;
      const y0 = hasDesc ? 72 : 92;
      let out = '';
      chars.forEach((c, k) => {
        const g = glyphOf(PIPES, c);
        out += pipe(g.p, k % 4, x, y0, u, r);
        x += (g.w + gap) * u;
      });
      return OG.logoSvg(pipeDefs(PIPE_COL) + out, { crisp: false });
    },
    icon: () =>
      OG.iconSvg(pipeDefs(['#E8282B']) + `<rect width="64" height="64" rx="10" fill="#000"/>` + pipe([[0, 2.2], [3.6, 2.2], [3.6, 6.4]], 0, -4, 0, 10, 9), { crisp: false }),
  });

  /* ==================================================================== 95. VHS */

  // The deck's on-screen display: 5x7 block characters.
  const OSD = {
    P: ['####.', '#...#', '#...#', '####.', '#....', '#....', '#....'], L: ['#....', '#....', '#....', '#....', '#....', '#....', '#####'],
    A: ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'], Y: ['#...#', '#...#', '.#.#.', '..#..', '..#..', '..#..', '..#..'],
    S: ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'], 0: ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
    1: ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'], 2: ['.###.', '#...#', '....#', '..##.', '.#...', '#....', '#####'],
    3: ['####.', '....#', '....#', '.###.', '....#', '....#', '####.'], 7: ['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'],
    ':': ['.', '.', '#', '.', '.', '#', '.'], ' ': ['..', '..', '..', '..', '..', '..', '..'],
  };
  const osdRows = (str) => {
    const rows = Array(7).fill('');
    [...str].forEach((ch, k) => { const g = OSD[ch] || OSD[' ']; for (let r = 0; r < 7; r++) rows[r] += (k ? '.' : '') + g[r]; });
    return rows;
  };
  const osd = (str, x, y, px) => OG.bitmap(osdRows(str), { '#': '#101010' }, { x: x + px, y: y + px, px }) + OG.bitmap(osdRows(str), { '#': '#FFFFFF' }, { x, y, px });
  const VHS_FONT = "'Rubik', 'Arial Black', sans-serif";

  OG.add({
    num: 95,
    id: 'vhs',
    name: 'VHS',
    caps: 'OgOS',
    flexCase: true,
    family: 'Experimental',
    sources: ['Beyond computing'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: true,
    bg: '#1424D8',
    ink: '#FFFFFF',
    palette: ['#1424D8', '#FFFFFF', '#FF2A4A', '#2AF0FF', '#101010'],
    fonts: ['Rubik:wght@900'],
    type: 'Rubik Black in white with a heavy black outline, red and cyan ghosts bleeding off its edges; the on-screen display is a 5x7 block font drawn by hand.',
    concept:
      'A rented tape on the family VCR: the blue screen, PLAY in the corner, the counter running, and the name riding through a band of tracking noise with its colors slipping. ' +
      'Retro beyond computers: the look of every tape that was watched until it wore thin.',
    logo: (w) => {
      const len = [...w].reduce((s, c) => s + (isUp(c) ? 92 : 78), 0);
      const base = 196;
      const t = (attrs) => `<text x="240" y="${base}" font-family="${VHS_FONT}" font-weight="900" font-size="112" text-anchor="middle" textLength="${len}" lengthAdjust="spacingAndGlyphs" ${attrs}>${OG.esc(w)}</text>`;
      const word = t('fill="#FF2A4A" transform="translate(-5 0)" opacity="0.85" filter="url(#vb)"') + t('fill="#2AF0FF" transform="translate(5 0)" opacity="0.8" filter="url(#vb)"') +
        t('fill="#FFFFFF" stroke="#101010" stroke-width="7" stroke-linejoin="round" paint-order="stroke"');
      // Tracking noise: streaks across one band, and the head-switch at the bottom.
      const rnd = OG.rng(95);
      const band0 = 162, band1 = 178;
      let noise = '';
      for (let k = 0; k < 46; k++) {
        const y = band0 - 4 + rnd() * (band1 - band0 + 8), x = rnd() * 480, wd = 8 + rnd() * 70;
        noise += `<rect x="${n2(x)}" y="${n2(y)}" width="${n2(wd)}" height="${n2(0.8 + rnd() * 1.6)}" fill="#FFFFFF" opacity="${n2(0.25 + rnd() * 0.6)}"/>`;
      }
      for (let k = 0; k < 26; k++) {
        const y = 284 + rnd() * 14, x = rnd() * 480, wd = 10 + rnd() * 90;
        noise += `<rect x="${n2(x)}" y="${n2(y)}" width="${n2(wd)}" height="1.5" fill="#FFFFFF" opacity="${n2(0.2 + rnd() * 0.5)}"/>`;
      }
      return OG.logoSvg(
        `<defs><radialGradient id="crt" cx="0.5" cy="0.5" r="0.75"><stop offset="0.6" stop-color="#1A2CF0"/><stop offset="1" stop-color="#0A148A"/></radialGradient>` +
          `<pattern id="sl" width="4" height="3" patternUnits="userSpaceOnUse"><rect width="4" height="1" fill="#000"/></pattern>` +
          `<filter id="vb" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="1.6 0.4"/></filter>` +
          `<clipPath id="above"><rect width="480" height="${band0}"/><rect y="${band1}" width="480" height="${300 - band1}"/></clipPath>` +
          `<clipPath id="inband"><rect y="${band0}" width="480" height="${band1 - band0}"/></clipPath></defs>` +
          `<rect width="480" height="300" fill="url(#crt)"/>` +
          `<rect y="284" width="480" height="16" fill="#0A148A"/>` +
          `<g clip-path="url(#above)">${word}</g>` +
          `<g clip-path="url(#inband)"><rect y="${band0}" width="480" height="${band1 - band0}" fill="#5A66FF" opacity="0.35"/><g transform="translate(14 0)">${word}</g></g>` +
          noise +
          osd('PLAY', 34, 30, 3) + `<path d="M137 30l17 10.5l-17 10.5z" fill="#101010" transform="translate(3 3)"/><path d="M137 30l17 10.5l-17 10.5z" fill="#FFFFFF"/>` +
          osd('SP', 34, 236, 3) + osd('0:03:27', 446 - osdRows('0:03:27')[0].length * 3, 236, 3) +
          `<rect width="480" height="300" fill="url(#sl)" opacity="0.18"/>`,
        { crisp: false }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="10" fill="#1424D8"/><path d="M22 14l26 16l-26 16z" fill="#101010" transform="translate(2 2)"/><path d="M22 14l26 16l-26 16z" fill="#FFFFFF"/>` +
          `<rect y="49" width="64" height="7" fill="#5A66FF"/><rect x="4" y="50" width="22" height="2" fill="#FFFFFF"/><rect x="31" y="53" width="27" height="2" fill="#FFFFFF" opacity="0.7"/><rect x="12" y="54" width="12" height="1.5" fill="#FFFFFF" opacity="0.6"/>`,
        { crisp: false }
      ),
  });

  /* ===================================================== 100. Under Construction */

  // Blocky sign letters: caps 8x8, lowercase 8 wide on a 6-row x-height.
  const B8b = '........';
  const BLK = {
    O: ['.######.', '########', '###..###', '###..###', '###..###', '###..###', '########', '.######.', B8b, B8b],
    G: ['.#######', '########', '###.....', '###.####', '###.####', '###..###', '########', '.######.', B8b, B8b],
    S: ['.#######', '########', '###.....', '#######.', '.#######', '.....###', '########', '#######.', B8b, B8b],
    o: [B8b, B8b, '.######.', '########', '###..###', '###..###', '########', '.######.', B8b, B8b],
    g: [B8b, B8b, '.#######', '########', '###..###', '########', '.#######', '.....###', '#######.', '######..'],
    s: [B8b, B8b, '.#######', '###.....', '#######.', '.#######', '.....###', '#######.', B8b, B8b],
  };
  // A road-sign worker, two frames: shovel in the ground, then thrown.
  const DIG = [
    [
      '..####................', '.######...............', '..####................', '...###................', '....##................',
      '...#####..............', '..#######.............', '..##.#######..........', '..##.##..#####........', '..##.##.....####......',
      '.....##........###....', '....####.........##...', '....##.##.........#...', '...##...##.......###..', '...##....##.....#####.',
      '..##......##...#######',
    ],
    [
      '..####......#..#......', '.######...##..#....#..', '..####..##............', '...###..##............', '....##.##.............',
      '...######.............', '..#######.............', '..##.##...............', '..##.##...............', '..##.##...............',
      '.....##...............', '....####..............', '....##.##.............', '...##...##.......#....', '...##....##.....###...',
      '..##......##...#####..',
    ],
  ];

  OG.add({
    num: 100,
    id: 'under-construction',
    name: 'Under Construction',
    caps: 'OgOs',
    flexCase: true,
    family: 'Emblem',
    sources: ['Demoscene & early web'],
    paletteTag: 'Primary',
    risk: 'Wild',
    scene: false,
    bg: '#C0C0C0',
    ink: '#000000',
    palette: ['#FFD400', '#000000', '#FF1A1A', '#C0C0C0', '#FFFFFF'],
    fonts: [],
    type: 'Heavy blocky pixel letters, three pixels a stroke, black on sign yellow; the small print is a 3x5 micro font.',
    concept:
      'A 1997 homepage that will be finished any day now: a hazard-striped sign, a worker digging away in two frames, the name in fat block letters and a NEW! burst stuck on the corner. ' +
      'An honest logo for a hobby OS, which is always under construction and proud of it.',
    logo: (w) => {
      const bx = 34, by = 62, bw = 412, bh = 176, st = 22;
      // Hazard stripes, clipped to the bands.
      let stripes = '';
      for (let x = bx - 40; x < bx + bw + 20; x += 28) stripes += `<path d="M${x} ${by + bh}l${st * 0} 0l14 0l${bh} -${bh}h-14z" fill="#000"/>`;
      const name = wordRows(BLK, w, 1);
      const hasDesc = name.slice(8).some((r) => r.includes('#'));
      const px = 7, nw = name[0].length * px;
      const nx = 150 + Math.round((282 - nw) / 2), ny = by + st + (hasDesc ? 18 : 26);
      const fig = (k) => OG.bitmap(DIG[k], { '#': '#000' }, { x: bx + 18, y: by + st + 22, px: 5 });
      const cap = 'UNDER CONSTRUCTION';
      // A NEW! starburst stuck on the corner.
      let burst = '';
      for (let i = 0; i < 28; i++) {
        const a = (i * Math.PI) / 14, rr = i % 2 ? 30 : 44;
        burst += (i ? 'L' : 'M') + n2(Math.cos(a) * rr) + ' ' + n2(Math.sin(a) * rr);
      }
      return OG.logoSvg(
        `<defs><clipPath id="bands"><rect x="${bx}" y="${by}" width="${bw}" height="${st}"/><rect x="${bx}" y="${by + bh - st}" width="${bw}" height="${st}"/></clipPath></defs>` +
          `<rect x="${bx + 5}" y="${by + 5}" width="${bw}" height="${bh}" fill="#808080"/>` +
          `<rect x="${bx}" y="${by}" width="${bw}" height="${bh}" fill="#FFD400"/>` +
          `<g clip-path="url(#bands)"><rect x="${bx}" y="${by}" width="${bw}" height="${bh}" fill="#FFD400"/>${stripes}</g>` +
          `<rect x="${bx}" y="${by}" width="${bw}" height="${bh}" fill="none" stroke="#000" stroke-width="4"/>` +
          `<rect x="${bx}" y="${by + st}" width="${bw}" height="3" fill="#000"/><rect x="${bx}" y="${by + bh - st - 3}" width="${bw}" height="3" fill="#000"/>` +
          `<g>${fig(0)}<animate attributeName="opacity" values="1;0" dur="0.9s" calcMode="discrete" repeatCount="indefinite"/></g>` +
          `<g opacity="0">${fig(1)}<animate attributeName="opacity" values="0;1" dur="0.9s" calcMode="discrete" repeatCount="indefinite"/></g>` +
          OG.bitmap(name, { '#': '#000' }, { x: nx, y: ny, px }) +
          tiny(cap, 150 + Math.round((282 - tinyWidth(cap) * 2) / 2), by + bh - st - 20, 2, '#000') +
          `<g transform="translate(424 66) rotate(-14)"><path d="${burst}Z" fill="#FF1A1A" stroke="#FFD400" stroke-width="3" stroke-linejoin="round"/>` +
          tiny('NEW!', -Math.round((tinyWidth('NEW!') * 3) / 2), -8, 3, '#FFFFFF') + `</g>`,
        { crisp: true }
      );
    },
    icon: () => {
      let s = `<defs><clipPath id="ic"><rect x="2" y="2" width="60" height="60" rx="8"/></clipPath></defs><g clip-path="url(#ic)"><rect width="64" height="64" fill="#FFD400"/>`;
      for (let x = -64; x < 64; x += 16) s += `<path d="M${x} 64l64 -64h8l-64 64z" fill="#000"/>`;
      s += `</g><rect x="2" y="2" width="60" height="60" rx="8" fill="none" stroke="#000" stroke-width="3"/>`;
      s += `<circle cx="32" cy="32" r="19" fill="#FFD400" stroke="#000" stroke-width="3"/>`;
      s += OG.bitmap(['.######.', '########', '###..###', '###..###', '###..###', '###..###', '########', '.######.'], { '#': '#000' }, { x: 20, y: 20, px: 3 });
      return OG.iconSvg(s, { crisp: true });
    },
  });

})();
