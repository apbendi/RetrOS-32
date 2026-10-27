/* Batch B: Machines & media. Directions 4, 9, 14 ... 99. */
(function () {
  const { C } = OG;

  /* ------------------------------------------------------------- helpers */

  const f2 = (v) => +(+v).toFixed(2);
  const pt = (p) => `${f2(p[0])} ${f2(p[1])}`;
  const rad = (d) => (d * Math.PI) / 180;

  // Lays text out in a monospace bitmap font with cells cw wide and h tall.
  // Characters the font lacks are blank cells.
  function mono(text, font, cw, h) {
    const rows = Array(h).fill('');
    for (const ch of text) {
      const g = font[ch] || [];
      for (let r = 0; r < h; r++) rows[r] += ((g[r] || '') + '.'.repeat(cw)).slice(0, cw);
    }
    return rows;
  }

  // Lays text out proportionally: each glyph trimmed to its ink, `gap`
  // blank columns between glyphs, `sp` columns for a space.
  function prop(text, font, h, gap = 1, sp = 3) {
    const rows = Array(h).fill('');
    [...text].forEach((ch, i) => {
      const g = font[ch];
      let lo = 99, hi = -1;
      if (g) g.forEach((r) => { for (let k = 0; k < r.length; k++) if (r[k] !== '.') { lo = Math.min(lo, k); hi = Math.max(hi, k); } });
      for (let r = 0; r < h; r++) {
        const part = hi < 0 ? '.'.repeat(sp) : (((g && g[r]) || '') + '.'.repeat(hi + 1)).slice(lo, hi + 1);
        rows[r] += (i ? '.'.repeat(gap) : '') + part;
      }
    });
    return rows;
  }

  // A font given as hex, one byte a row, bit 7 the leftmost pixel.
  function hexFont(def, h) {
    const out = {};
    Object.keys(def).forEach((k) => {
      const rows = [];
      for (let i = 0; i < h; i++) {
        const v = parseInt(def[k].substr(i * 2, 2) || '00', 16);
        let s = '';
        for (let b = 7; b >= 0; b--) s += (v >> b) & 1 ? '#' : '.';
        rows.push(s);
      }
      out[k] = rows;
    });
    return out;
  }

  // The textLength to give a name so it fits in maxW, or 0 when it already
  // does. `table` holds rough advance widths in em for O, G, S, o, g, s.
  function fitLen(str, size, table, maxW) {
    const est = [...str].reduce((a, c) => a + (table[c] || 0.6), 0) * size;
    return est > maxW ? maxW : 0;
  }
  const RUSSO_W = { O: 0.74, G: 0.72, S: 0.66, o: 0.62, g: 0.62, s: 0.55 };
  const KALAM_W = { O: 0.62, G: 0.56, S: 0.45, o: 0.48, g: 0.46, s: 0.38 };

  // Replaces each '#' with a character chosen per row, so OG.bitmap can
  // color a bitmap in horizontal bands.
  const band = (rows, pick) => rows.map((r, j) => r.replace(/#/g, pick(j)));

  /* ------------------------------------------------------- bitmap fonts */

  // A home-computer face: 6 pixels wide in an 8x8 cell, two-pixel stems.
  const C64 = {
    '*': ['........', '.##..##.', '..####..', '########', '..####..', '.##..##.', '........', '........'],
    '.': ['........', '........', '........', '........', '........', '...##...', '...##...', '........'],
    '0': ['..####..', '.##..##.', '.##.###.', '.###.##.', '.##..##.', '.##..##.', '..####..', '........'],
    '1': ['...##...', '..###...', '...##...', '...##...', '...##...', '...##...', '.######.', '........'],
    '2': ['..####..', '.##..##.', '.....##.', '....##..', '..##....', '.##.....', '.######.', '........'],
    '3': ['..####..', '.##..##.', '.....##.', '...###..', '.....##.', '.##..##.', '..####..', '........'],
    '6': ['..####..', '.##..##.', '.##.....', '.#####..', '.##..##.', '.##..##.', '..####..', '........'],
    '7': ['.######.', '.##..##.', '....##..', '...##...', '...##...', '...##...', '...##...', '........'],
    '8': ['..####..', '.##..##.', '.##..##.', '..####..', '.##..##.', '.##..##.', '..####..', '........'],
    A: ['...##...', '..####..', '.##..##.', '.######.', '.##..##.', '.##..##.', '.##..##.', '........'],
    B: ['.#####..', '.##..##.', '.##..##.', '.#####..', '.##..##.', '.##..##.', '.#####..', '........'],
    C: ['..####..', '.##..##.', '.##.....', '.##.....', '.##.....', '.##..##.', '..####..', '........'],
    D: ['.####...', '.##.##..', '.##..##.', '.##..##.', '.##..##.', '.##.##..', '.####...', '........'],
    E: ['.######.', '.##.....', '.##.....', '.####...', '.##.....', '.##.....', '.######.', '........'],
    G: ['..####..', '.##..##.', '.##.....', '.##.###.', '.##..##.', '.##..##.', '..####..', '........'],
    I: ['..####..', '...##...', '...##...', '...##...', '...##...', '...##...', '..####..', '........'],
    K: ['.##..##.', '.##.##..', '.####...', '.###....', '.####...', '.##.##..', '.##..##.', '........'],
    M: ['.##...##', '.###.###', '.#######', '.##.#.##', '.##...##', '.##...##', '.##...##', '........'],
    O: ['..####..', '.##..##.', '.##..##.', '.##..##.', '.##..##.', '.##..##.', '..####..', '........'],
    R: ['.#####..', '.##..##.', '.##..##.', '.#####..', '.####...', '.##.##..', '.##..##.', '........'],
    S: ['..####..', '.##..##.', '.##.....', '..####..', '.....##.', '.##..##.', '..####..', '........'],
    T: ['.######.', '...##...', '...##...', '...##...', '...##...', '...##...', '...##...', '........'],
    V: ['.##..##.', '.##..##.', '.##..##.', '.##..##.', '.##..##.', '..####..', '...##...', '........'],
    Y: ['.##..##.', '.##..##.', '.##..##.', '..####..', '...##...', '...##...', '...##...', '........'],
    o: ['........', '........', '..####..', '.##..##.', '.##..##.', '.##..##.', '..####..', '........'],
    g: ['........', '........', '..#####.', '.##..##.', '.##..##.', '..#####.', '.....##.', '.#####..'],
    s: ['........', '........', '..#####.', '.##.....', '..####..', '.....##.', '.#####..', '........'],
  };

  // A Topaz-like face: wider than the C64 one, 7 pixels in an 8x8 cell.
  const TOPAZ = {
    '.': ['........', '........', '........', '........', '........', '..##....', '..##....', '........'],
    '#': ['.##.##..', '.##.##..', '#######.', '.##.##..', '#######.', '.##.##..', '.##.##..', '........'],
    '0': ['.#####..', '##...##.', '##..###.', '##.#.##.', '###..##.', '##...##.', '.#####..', '........'],
    '1': ['...##...', '..###...', '...##...', '...##...', '...##...', '...##...', '.######.', '........'],
    '2': ['.#####..', '##...##.', '.....##.', '..####..', '.##.....', '##......', '#######.', '........'],
    F: ['#######.', '##......', '##......', '#####...', '##......', '##......', '##......', '........'],
    G: ['.#####..', '##...##.', '##......', '##..###.', '##...##.', '##...##.', '.######.', '........'],
    M: ['##...##.', '###.###.', '#######.', '##.#.##.', '##...##.', '##...##.', '##...##.', '........'],
    O: ['.#####..', '##...##.', '##...##.', '##...##.', '##...##.', '##...##.', '.#####..', '........'],
    P: ['######..', '##...##.', '##...##.', '######..', '##......', '##......', '##......', '........'],
    S: ['.#####..', '##...##.', '##......', '.#####..', '.....##.', '##...##.', '.#####..', '........'],
    a: ['........', '........', '.####...', '....##..', '.#####..', '##..##..', '.###.##.', '........'],
    b: ['##......', '##......', '######..', '##...##.', '##...##.', '##...##.', '######..', '........'],
    c: ['........', '........', '.#####..', '##...##.', '##......', '##...##.', '.#####..', '........'],
    d: ['.....##.', '.....##.', '.######.', '##...##.', '##...##.', '##...##.', '.######.', '........'],
    e: ['........', '........', '.#####..', '##...##.', '#######.', '##......', '.#####..', '........'],
    f: ['..####..', '.##..##.', '.##.....', '####....', '.##.....', '.##.....', '####....', '........'],
    g: ['........', '........', '.######.', '##...##.', '##...##.', '.######.', '.....##.', '.#####..'],
    i: ['...##...', '........', '..###...', '...##...', '...##...', '...##...', '..####..', '........'],
    l: ['..###...', '...##...', '...##...', '...##...', '...##...', '...##...', '..####..', '........'],
    m: ['........', '........', '##.##...', '#######.', '##.#.##.', '##.#.##.', '##...##.', '........'],
    n: ['........', '........', '######..', '##...##.', '##...##.', '##...##.', '##...##.', '........'],
    o: ['........', '........', '.#####..', '##...##.', '##...##.', '##...##.', '.#####..', '........'],
    r: ['........', '........', '##.###..', '###..##.', '##......', '##......', '##......', '........'],
    s: ['........', '........', '.######.', '##......', '.#####..', '.....##.', '######..', '........'],
    t: ['..##....', '..##....', '######..', '..##....', '..##....', '..##.##.', '...###..', '........'],
    u: ['........', '........', '##...##.', '##...##.', '##...##.', '##...##.', '.######.', '........'],
    w: ['........', '........', '##...##.', '##.#.##.', '##.#.##.', '#######.', '.##.##..', '........'],
  };

  // The VGA text-mode face, 8x16, for the glyphs the DOS directions use.
  const VGA = hexFont({
    ' ': '',
    C: '00003c66c2c0c0c0c0c2663c00000000',
    G: '00003c66c2c0c0dec6c6663a00000000',
    O: '00007cc6c6c6c6c6c6c6c67c00000000',
    S: '00007cc6c660380c06c6c67c00000000',
    V: '0000c6c6c6c6c6c6c66c381000000000',
    ':': '00000000181800000018180000000000',
    '\\': '00000080c0e070381c0e060200000000',
    '>': '0000006030180c060c18306000000000',
    '_': '00000000000000000000000000ff0000',
    '.': '00000000000000000000181800000000',
    '[': '00003c30303030303030303c00000000',
    ']': '00003c0c0c0c0c0c0c0c0c3c00000000',
    '0': '0000386cc6c6d6d6c6c66c3800000000',
    '1': '00001838781818181818187e00000000',
    '2': '00007cc6060c183060c0c6fe00000000',
    '3': '00007cc606063c060606c67c00000000',
    e: '00000000007cc6fec0c0c67c00000000',
    g: '000000000076cccccccccc7c0ccc7800',
    i: '00001818003818181818183c00000000',
    n: '0000000000dc66666666666600000000',
    o: '00000000007cc6c6c6c6c67c00000000',
    r: '0000000000dc7666606060f000000000',
    s: '00000000007cc660380cc67c00000000',
  }, 16);

  /* ------------------------------------------------- 4. READY. -------- */

  const C64_BLUE = '#40318D', C64_LIGHT = '#7869C4', C64_TEXT = '#9C90F0';
  const c64 = (text, x, y, px, fill) => OG.bitmap(mono(text, C64, 8, 8), { '#': fill }, { x, y, px });

  OG.add({
    num: 4,
    id: 'ready',
    name: 'READY.',
    caps: 'OGOS',
    flexCase: true,
    family: 'Interface',
    sources: ['Home computers'],
    paletteTag: 'Mono',
    risk: 'Safe',
    scene: true,
    bg: C64_LIGHT,
    ink: '#FFFFFF',
    palette: [C64_BLUE, C64_LIGHT, C64_TEXT],
    fonts: [],
    type: 'A chunky 8x8 home-computer face drawn for this set (two-pixel stems, six pixels wide, C64 in spirit but not its ROM); the name at 9x, the rest at 2x and 3x.',
    concept:
      'The screen an 8-bit home computer showed the moment you switched it on: light-blue border, blue screen, a banner, READY. and a blinking cursor. ' +
      'OgOs is a machine you turn on and start coding on, and this says it in the language of 1982.',
    logo: (w) => {
      const top = '**** 32 BASIC V1 ****', l2 = '32768K RAM SYSTEM';
      return OG.logoSvg(
        `<rect width="480" height="300" fill="${C64_LIGHT}"/><rect x="20" y="22" width="440" height="256" fill="${C64_BLUE}"/>` +
          c64(top, 240 - top.length * 8, 46, 2, C64_TEXT) +
          c64(w, 240 - w.length * 36, 76, 9, C64_TEXT) +
          c64(l2, 240 - l2.length * 8, 160, 2, C64_TEXT) +
          c64('READY.', 33, 196, 3, C64_TEXT) +
          `<rect x="36" y="220" width="18" height="21" fill="${C64_TEXT}"><animate attributeName="opacity" values="1;1;0;0" dur="1.2s" calcMode="discrete" repeatCount="indefinite"/></rect>`,
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="${C64_LIGHT}"/><rect x="5" y="5" width="54" height="54" fill="${C64_BLUE}"/>` +
          OG.bitmap(prop('OG', C64, 8, 1), { '#': C64_LIGHT }, { x: 12, y: 11, px: 3 }) +
          `<rect x="12" y="38" width="18" height="15" fill="${C64_LIGHT}"/>`,
        { crisp: true }
      ),
  });

  /* ------------------------------------------- 9. Rainbow Corner ------- */

  // Monoline letters with round ends, built from arcs: O, G, S in both
  // cases. X is the ink height (cap height or x-height), sw the stroke.
  function roundGlyph(ch, X, sw) {
    const r = (X - sw) / 2;
    const P = (cx, cy, rx, ry, deg) => [cx + rx * Math.cos(rad(deg)), cy + ry * Math.sin(rad(deg))];
    const lower = ch !== ch.toUpperCase();
    const c = ch.toLowerCase();
    const cx = sw / 2 + r, cy = -X / 2;
    const ring = `M ${f2(cx - r)} ${f2(cy)} A ${f2(r)} ${f2(r)} 0 1 0 ${f2(cx + r)} ${f2(cy)} A ${f2(r)} ${f2(r)} 0 1 0 ${f2(cx - r)} ${f2(cy)} Z`;
    if (c === 'o') return { d: ring, w: 2 * r + sw };
    if (c === 'g' && lower) {
      const xs = cx + r, yb = X * 0.42 - sw / 2, k = r * 0.95;
      return { d: ring + ` M ${f2(xs)} ${f2(-X + sw / 2)} V ${f2(yb - k)} A ${f2(k)} ${f2(k)} 0 0 1 ${f2(xs - k)} ${f2(yb)} H ${f2(cx - r * 0.5)}`, w: 2 * r + sw };
    }
    if (c === 'g') {
      return { d: `M ${pt(P(cx, cy, r, r, -40))} A ${f2(r)} ${f2(r)} 0 1 0 ${f2(cx + r)} ${f2(cy)} H ${f2(cx + r * 0.12)}`, w: 2 * r + sw };
    }
    // s and S: two stacked bowls that meet at the middle. Three strokes
    // stacked in the x-height leave the small s no room, so it gets a
    // lighter stroke and wider bowls, as heavy geometric faces do.
    const ss = lower ? sw * 0.8 : sw;
    const a = (X - ss) / 4, b = a * (lower ? 1.42 : 1.1);
    const sx = ss / 2 + b, yt = -X + ss / 2 + a, yb = -ss / 2 - a;
    return {
      d: `M ${pt(P(sx, yt, b, a, -30))} A ${f2(b)} ${f2(a)} 0 1 0 ${f2(sx)} ${f2(yt + a)} A ${f2(b)} ${f2(a)} 0 1 1 ${pt(P(sx, yb, b, a, 150))}`,
      w: 2 * b + ss, sw: ss,
    };
  }

  // A word of round letters: returns the path markup and the word's width,
  // with its baseline at y = 0 and its left edge at x = 0.
  function roundWord(word, xh, sw, gap) {
    let x = 0, d = '';
    [...word].forEach((ch, i) => {
      const lower = ch !== ch.toUpperCase();
      const g = roundGlyph(ch, lower ? xh : xh * 1.36, sw);
      d += `<path transform="translate(${f2(x)} 0)" d="${g.d}"${g.sw ? ` stroke-width="${f2(g.sw)}"` : ''}/>`;
      x += g.w + (i < word.length - 1 ? gap : 0);
    });
    return { d, w: x };
  }

  const SPECTRUM = ['#D8261C', '#F4C20D', '#2BA84A', '#1FA3D9'];

  // Four stripes across the bottom-right corner of a box, clipped to it.
  function rainbowCorner(x, y, size, r, id) {
    const n = [Math.cos(rad(30)), Math.sin(rad(30))]; // across the stripes
    const u = [Math.cos(rad(-60)), Math.sin(rad(-60))]; // along them
    const cx = x + size, cy = y + size, bw = size * 0.085, s0 = size * 0.15;
    let s = `<clipPath id="${id}"><rect x="${x}" y="${y}" width="${size}" height="${size}" rx="${r}"/></clipPath>`;
    s += `<rect x="${x}" y="${y}" width="${size}" height="${size}" rx="${r}" fill="#111"/><g clip-path="url(#${id})">`;
    [3, 2, 1, 0].forEach((i, k) => {
      const d0 = -(s0 + (i + 1) * bw), d1 = -(s0 + i * bw), L = size * 2;
      const p = (d, t) => [cx + n[0] * d + u[0] * t, cy + n[1] * d + u[1] * t];
      s += `<path d="M ${pt(p(d0, -L))} L ${pt(p(d0, L))} L ${pt(p(d1, L))} L ${pt(p(d1, -L))} Z" fill="${SPECTRUM[k]}"/>`;
    });
    return s + '</g>';
  }

  OG.add({
    num: 9,
    id: 'rainbow-corner',
    name: 'Rainbow Corner',
    caps: 'ogos',
    flexCase: true,
    family: 'Geometric',
    sources: ['Home computers'],
    paletteTag: 'Rainbow',
    risk: 'Safe',
    bg: '#ECE8DF',
    ink: '#111111',
    palette: ['#111111', '#D8261C', '#F4C20D', '#2BA84A', '#1FA3D9', '#ECE8DF'],
    fonts: [],
    type: 'Letters drawn as one thick round-ended stroke each, built from circles and arcs, then slanted 10 degrees.',
    concept:
      'A black case with four stripes running off its corner: the rubber-key home computer of 1982, reduced to a badge. ' +
      'The round, friendly lowercase says this is a machine for kids and hobbyists, not an office.',
    logo: (w) => {
      const sw = 25;
      let word = roundWord(w, 84, sw, 9);
      const maxW = 252, k = Math.min(1, maxW / word.w);
      const x0 = 186, base = 186;
      return OG.logoSvg(
        rainbowCorner(44, 84, 120, 14, 'rc') +
          `<g transform="translate(${x0} ${base}) skewX(-10) scale(${f2(k)})" fill="none" stroke="#111" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round">${word.d}</g>`
      );
    },
    icon: () => OG.iconSvg(rainbowCorner(2, 2, 60, 9, 'ri')),
  });

  /* --------------------------------------------------- 14. Boing ------- */

  // A checkered ball: facets of a latitude/longitude grid on a tilted
  // sphere, projected orthographically. Points on the far side are pushed
  // onto the rim, so facets that cross it end exactly there.
  function boingBall(cx, cy, R, { tilt = -0.36, lean = 0.3, spin = 0.1, red = '#E0182B', white = '#FFFFFF' } = {}) {
    const nLon = 16, nLat = 8, sub = 3;
    const proj = (lat, lon) => {
      const x = Math.cos(lat) * Math.sin(lon + spin), y = Math.sin(lat), z = Math.cos(lat) * Math.cos(lon + spin);
      const y2 = y * Math.cos(lean) - z * Math.sin(lean), z2 = y * Math.sin(lean) + z * Math.cos(lean);
      let sx = x * Math.cos(tilt) - y2 * Math.sin(tilt), sy = x * Math.sin(tilt) + y2 * Math.cos(tilt);
      if (z2 < 0) { const d = Math.hypot(sx, sy) || 1; sx /= d; sy /= d; }
      return [cx + sx * R, cy - sy * R, z2];
    };
    let s = `<circle cx="${f2(cx)}" cy="${f2(cy)}" r="${f2(R)}" fill="${white}"/>`;
    for (let i = 0; i < nLat; i++) {
      const a0 = -Math.PI / 2 + (i * Math.PI) / nLat, a1 = a0 + Math.PI / nLat;
      for (let j = 0; j < nLon; j++) {
        if ((i + j) % 2) continue;
        const b0 = (j * 2 * Math.PI) / nLon, b1 = b0 + (2 * Math.PI) / nLon;
        const pts = [];
        for (let k = 0; k <= sub; k++) pts.push(proj(a0, b0 + ((b1 - b0) * k) / sub));
        for (let k = 1; k <= sub; k++) pts.push(proj(a0 + ((a1 - a0) * k) / sub, b1));
        for (let k = 1; k <= sub; k++) pts.push(proj(a1, b1 - ((b1 - b0) * k) / sub));
        for (let k = 1; k < sub; k++) pts.push(proj(a1 - ((a1 - a0) * k) / sub, b0));
        if (pts.every((p) => p[2] < 0)) continue;
        s += `<path d="M ${pts.map(pt).join(' L ')} Z" fill="${red}"/>`;
      }
    }
    // Shading: a soft highlight and a darker rim.
    s += `<circle cx="${f2(cx)}" cy="${f2(cy)}" r="${f2(R)}" fill="url(#bshade)"/>`;
    return s;
  }

  const BALL_DEFS =
    `<radialGradient id="bshade" cx="0.36" cy="0.32" r="0.75">` +
    `<stop offset="0" stop-color="#fff" stop-opacity="0.45"/><stop offset="0.35" stop-color="#fff" stop-opacity="0"/>` +
    `<stop offset="0.8" stop-color="#000" stop-opacity="0.06"/><stop offset="1" stop-color="#000" stop-opacity="0.38"/></radialGradient>` +
    `<filter id="bsoft" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="5"/></filter>`;

  const AMIGA_GRID = '#9B1FA8';

  OG.add({
    num: 14,
    id: 'boing',
    name: 'Boing',
    caps: 'OgOs',
    flexCase: true,
    family: 'Geometric',
    sources: ['Home computers'],
    paletteTag: 'Primary',
    risk: 'Bold',
    bg: '#A9A9A9',
    ink: '#FFFFFF',
    palette: ['#E0182B', '#FFFFFF', AMIGA_GRID, '#A9A9A9', '#1B1B3A'],
    fonts: [],
    type: 'The O\'s are checkered spheres computed from a tilted latitude/longitude grid; g and s are a Topaz-like bitmap face drawn for this set.',
    concept:
      'Both O\'s become the red and white checkered ball that bounced in front of a purple grid at every computer show in 1985, shadows and all. ' +
      'It is the famous demo of what a home computer could suddenly do, and the name bounces along with it.',
    logo: (w) => {
      const px = 11, base = 198;
      // The wall grid and a floor in perspective.
      let g = '';
      const fl = 1.19;
      for (let x = 48; x <= 432; x += 32) g += `<path d="M ${x} 26 V 206 L ${f2(240 + (x - 240) * fl)} 274"/>`;
      for (let y = 26; y <= 206; y += 30) g += `<path d="M 48 ${y} H 432"/>`;
      [226, 250].forEach((y) => { const t = (y - 206) / 68, h = 192 * (1 + (fl - 1) * t); g += `<path d="M ${f2(240 - h)} ${y} H ${f2(240 + h)}"/>`; });
      g += `<path d="M ${f2(240 - 192 * fl)} 274 H ${f2(240 + 192 * fl)}"/>`;
      // Lay the word out: balls for o and O, bitmap letters for the rest.
      const items = [...w].map((ch) => {
        if (ch === 'O' || ch === 'o') { const R = ch === 'O' ? 42 : 31; return { ball: true, R, w: 2 * R }; }
        const rows = prop(ch, TOPAZ, 8, 0);
        return { rows, w: rows[0].length * px };
      });
      const gap = 12, total = items.reduce((a, it) => a + it.w, 0) + gap * (items.length - 1);
      let x = 240 - total / 2, shadows = '', fg = '';
      items.forEach((it) => {
        if (it.ball) {
          const cx = x + it.R, cy = base - it.R + 3;
          shadows += `<circle cx="${f2(cx + 16)}" cy="${f2(cy + 8)}" r="${it.R}" fill="#000" opacity="0.32" filter="url(#bsoft)"/>`;
          fg += boingBall(cx, cy, it.R);
        } else {
          const y = base - 7 * px;
          shadows += OG.bitmap(it.rows, { '#': '#000' }, { x: x + 10, y: y + 6, px }).replace(/<rect /g, '<rect opacity="0.3" ');
          fg += OG.bitmap(it.rows, { '#': '#1B1B3A' }, { x: x + 3, y: y + 3, px }) + OG.bitmap(it.rows, { '#': '#FFFFFF' }, { x, y, px });
        }
        x += it.w + gap;
      });
      return OG.logoSvg(
        `<defs>${BALL_DEFS}</defs><g fill="none" stroke="${AMIGA_GRID}" stroke-width="2">${g}</g>` +
          `<g filter="url(#bsoft)">${shadows}</g>` + fg
      );
    },
    icon: () =>
      OG.iconSvg(
        `<defs>${BALL_DEFS}</defs>` + boingBall(32, 32, 29),
      ),
  });

  /* ---------------------------------------------------- 19. Guru ------- */

  const GURU_RED = '#FF2A14';
  const topaz = (text, x, y, px, fill) => OG.bitmap(mono(text, TOPAZ, 8, 8), { '#': fill }, { x, y, px });
  const topazC = (text, cy, px, fill) => topaz(text, Math.round(240 - (text.length * 8 - 1) * px / 2), cy, px, fill);

  OG.add({
    num: 19,
    id: 'guru',
    name: 'Guru',
    caps: 'OGOS',
    flexCase: true,
    family: 'Interface',
    sources: ['Home computers'],
    paletteTag: 'Mono',
    risk: 'Wild',
    scene: true,
    bg: '#000000',
    ink: GURU_RED,
    palette: ['#000000', GURU_RED, '#7A0E06'],
    fonts: [],
    type: 'A Topaz-like 8x8 bitmap face drawn for this set: seven pixels wide, two-pixel stems; the name at 8x, the message at 1x.',
    concept:
      'The most famous crash screen of the home-computer era, recast as a boast: a blinking red box, "Software Failure", and a meditation number that now starts with the name. ' +
      'A wink for everyone who has stared at a kernel panic, which is everyone who writes an OS.',
    logo: (w) => {
      const px = 8;
      const anim = `<animate attributeName="stroke" values="${GURU_RED};${GURU_RED};#4A0904;#4A0904" dur="1.6s" calcMode="discrete" repeatCount="indefinite"/>`;
      return OG.logoSvg(
        `<rect width="480" height="300" fill="#000"/>` +
          `<rect x="32" y="44" width="416" height="212" fill="none" stroke="${GURU_RED}" stroke-width="8">${anim}</rect>` +
          topazC('Software Failure.', 78, 1, GURU_RED) +
          topazC('Press left mouse button to continue.', 92, 1, GURU_RED) +
          topazC(w, 126, px, GURU_RED) +
          topazC('Meditation #00000020.00010000', 208, 1, GURU_RED),
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="#000"/><rect x="3" y="3" width="58" height="58" fill="none" stroke="${GURU_RED}" stroke-width="6"/>` +
          OG.bitmap(mono('OG', TOPAZ, 8, 8), { '#': GURU_RED }, { x: 9, y: 22, px: 3 }),
        { crisp: true }
      ),
  });

  /* ------------------------------------------------ 24. Teletext ------- */

  // Big letters on teletext's mosaic grid: one bitmap pixel is a sextant,
  // a character cell is 2 sextants wide and 3 tall. Caps fill rows 0-11,
  // lowercase rows 3-11, and descenders rows 12-14.
  const TT = {
    O: ['...####...', '.########.', '###....###', '##......##', '##......##', '##......##', '##......##', '##......##', '##......##', '###....###', '.########.', '...####...'],
    G: ['...#####..', '.########.', '###....###', '##........', '##........', '##........', '##...#####', '##...#####', '##......##', '###....###', '.########.', '...####...'],
    S: ['..######..', '.########.', '###....###', '##........', '###.......', '.#######..', '..#######.', '.......###', '........##', '###....###', '.########.', '..######..'],
    o: ['', '', '', '...####...', '.########.', '###....###', '##......##', '##......##', '##......##', '###....###', '.########.', '...####...'],
    g: ['', '', '', '...####.##', '.#########', '###....###', '##......##', '##......##', '###....###', '.#########', '...####.##', '........##', '###....###', '.########.', '...####...'],
    s: ['', '', '', '..#######.', '.#########', '##........', '###.......', '.########.', '.......###', '........##', '#########.', '.#######..'],
  };
  const TT_COLORS = ['#FF0000', '#FFFF00', '#00FF00', '#00FFFF', '#FF00FF'];

  function ttWord(w) {
    const rows = mono(w, Object.fromEntries(Object.entries(TT).map(([k, v]) => [k, v.map((r) => r || '..........')])), 12, 15);
    // mono pads each glyph to 12 columns: 10 of ink and one cell of space.
    return rows.map((r) => r.slice(0, r.length - 2));
  }

  OG.add({
    num: 24,
    id: 'teletext',
    name: 'Teletext',
    caps: 'OgOS',
    flexCase: true,
    family: 'Pixel',
    sources: ['Home computers'],
    paletteTag: 'Rainbow',
    risk: 'Bold',
    scene: true,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#FF0000', '#FFFF00', '#00FF00', '#00FFFF', '#FF00FF'],
    fonts: [],
    type: 'Mosaic letters drawn on teletext\'s 2x3 sextant grid, one color per character row as the control codes allowed; the header in the system font.',
    concept:
      'Page 100 of a broadcast teletext service, where the masthead had to be built out of block graphics, one color per row. ' +
      'Chunky, bright and readable from across a room, it ties OgOs to the same 40-column world as the BBC Micro.',
    logo: (w) => {
      const s = 9, rows = ttWord(w);
      const x = Math.round(240 - (rows[0].length * s) / 2), y = 50;
      const banded = band(rows, (j) => 'abcde'[Math.floor(j / 3)]);
      const map = { a: TT_COLORS[0], b: TT_COLORS[1], c: TT_COLORS[2], d: TT_COLORS[3], e: TT_COLORS[4] };
      const head = OG.pixText('P100', { x: 24, y: 16, px: 1.5, fill: '#fff' }) +
        OG.pixText(' OGOS 100  Tue 27 Oct', { x: 24 + 4 * 12, y: 16, px: 1.5, fill: '#fff' }) +
        OG.pixText('12:00/00', { x: 456 - 8 * 12, y: 16, px: 1.5, fill: '#FFFF00' });
      const fast = [['News', '#FF0000'], ['Code', '#00FF00'], ['Games', '#FFFF00'], ['Index', '#00FFFF']];
      let fx = 24, foot = '';
      fast.forEach(([t, c]) => { foot += OG.pixText(t, { x: fx, y: 272, px: 1.5, fill: c }); fx += 112; });
      return OG.logoSvg(
        `<rect width="480" height="300" fill="#000"/>` + head +
          OG.bitmap(banded, map, { x, y, px: s }) +
          `<rect x="24" y="210" width="432" height="24" fill="#0000FF"/>` +
          OG.pixText('The retro operating system', { x: 36, y: 216, px: 1.5, fill: '#FFFFFF' }) +
          OG.pixText('32', { x: 432, y: 216, px: 1.5, fill: '#FFFF00' }) +
          foot,
        { crisp: true }
      );
    },
    icon: () => {
      const rows = band(TT.O, (j) => 'abcd'[Math.floor(j / 3)]);
      return OG.iconSvg(
        `<rect width="64" height="64" fill="#000"/>` +
          OG.bitmap(rows, { a: TT_COLORS[0], b: TT_COLORS[1], c: TT_COLORS[2], d: TT_COLORS[3] }, { x: 7, y: 8, px: 5, py: 4 }),
        { crisp: true }
      );
    },
  });

  /* ----------------------------------------------- 29. Cartridge ------- */

  const CART_BANDS = ['#FF2E63', '#FF8A00', '#FFD400', '#00C2FF'];

  function cartridge(w, k) {
    // k = 1 for the logo; the icon draws its own simpler shape.
    const shell = 'M 88 34 H 392 Q 410 34 410 52 V 222 L 398 230 V 266 H 82 V 230 L 70 222 V 52 Q 70 34 88 34 Z';
    let s = `<defs><linearGradient id="csh" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#55575E"/><stop offset="1" stop-color="#2A2B30"/></linearGradient>` +
      `<clipPath id="clab"><rect x="96" y="82" width="288" height="146" rx="5"/></clipPath>` +
      `<filter id="cdrop" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="6"/></filter></defs>`;
    s += `<path d="${shell}" transform="translate(6 10)" fill="#000" opacity="0.35" filter="url(#cdrop)"/>`;
    s += `<path d="${shell}" fill="url(#csh)"/>`;
    s += `<path d="${shell}" fill="none" stroke="#fff" stroke-opacity="0.18" stroke-width="2"/>`;
    // Grip ridges along the top.
    for (let i = 0; i < 4; i++) {
      const y = 44 + i * 7;
      s += `<rect x="112" y="${y}" width="256" height="3" rx="1.5" fill="#1C1D21"/><rect x="112" y="${y + 3}" width="256" height="1.5" fill="#fff" opacity="0.14"/>`;
    }
    // The recess and the label.
    s += `<rect x="90" y="76" width="300" height="158" rx="8" fill="#1E1F23"/>`;
    s += `<g clip-path="url(#clab)"><rect x="96" y="82" width="288" height="146" fill="#14121F"/>`;
    s += `<g transform="rotate(-24 240 160)">`;
    CART_BANDS.forEach((c, i) => { s += `<rect x="-60" y="${150 + i * 17}" width="600" height="13" fill="${c}"/>`; });
    s += `</g>`;
    s += OG.text(w, { x: 236, y: 186, size: 100, family: "'Barlow Condensed', sans-serif", weight: 900, italic: true, fill: '#000', anchor: 'middle', ls: 1, attrs: 'opacity="0.55"' });
    s += OG.text(w, { x: 230, y: 180, size: 100, family: "'Barlow Condensed', sans-serif", weight: 900, italic: true, fill: '#FFFFFF', anchor: 'middle', ls: 1, attrs: 'stroke="#14121F" stroke-width="5" paint-order="stroke" stroke-linejoin="round"' });
    s += OG.text('SYSTEM CARTRIDGE', { x: 110, y: 101, size: 11, family: "'Barlow Condensed', sans-serif", weight: 800, italic: true, fill: '#fff', ls: 2.5, attrs: 'opacity="0.85"' });
    s += `</g>`;
    // The 32-BIT badge.
    s += `<g transform="rotate(-12 357 104)"><circle cx="357" cy="104" r="25" fill="#FFD400" stroke="#14121F" stroke-width="3"/>` +
      OG.text('32-BIT', { x: 357, y: 110, size: 16, family: "'Barlow Condensed', sans-serif", weight: 800, italic: true, fill: '#14121F', anchor: 'middle' }) + `</g>`;
    // An embossed arrow.
    s += `<path d="M 228 244 H 252 L 240 256 Z" fill="#202125" stroke="#fff" stroke-opacity="0.12"/>`;
    return s;
  }

  OG.add({
    num: 29,
    id: 'cartridge',
    name: 'Cartridge',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['Home computers', 'Games & arcade'],
    paletteTag: 'Neon',
    risk: 'Bold',
    bg: '#E9E4D8',
    ink: '#14121F',
    palette: ['#3F4046', '#14121F', '#FF2E63', '#FF8A00', '#FFD400', '#00C2FF'],
    fonts: ['Barlow Condensed:ital,wght@1,600;1,800;1,900'],
    type: 'Barlow Condensed Black Italic, outlined and drop-shadowed like a box-art title; the shell and label are drawn.',
    concept:
      'A game cartridge with ridged grey plastic and a loud label: you plug OgOs in and it boots straight into play. ' +
      'It speaks to the half of the project that is about making and playing retro-style games.',
    logo: (w) => OG.logoSvg(cartridge(w)),
    icon: () => {
      const shell = 'M 14 4 H 50 Q 56 4 56 10 V 50 L 52 53 V 60 H 12 V 53 L 8 50 V 10 Q 8 4 14 4 Z';
      let s = `<defs><clipPath id="cil"><rect x="14" y="18" width="36" height="30" rx="2"/></clipPath></defs>` +
        `<path d="${shell}" fill="#45474D"/><rect x="16" y="8" width="32" height="2.5" fill="#222"/><rect x="16" y="12" width="32" height="2.5" fill="#222"/>` +
        `<g clip-path="url(#cil)"><rect x="14" y="18" width="36" height="30" fill="#14121F"/><g transform="rotate(-24 32 33)">`;
      CART_BANDS.forEach((c, i) => { s += `<rect x="0" y="${30 + i * 5}" width="64" height="4" fill="${c}"/>`; });
      return OG.iconSvg(s + `</g></g>`);
    },
  });

  /* ---------------------------------------------------- 34. C:\> ------- */

  const vga = (text, x, y, px, fill) => OG.bitmap(mono(text, VGA, 8, 16), { '#': fill }, { x, y, px });
  const DOS_GRAY = '#AAAAAA';

  OG.add({
    num: 34,
    id: 'c-prompt',
    name: 'C:\\>',
    caps: 'OGOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['DOS & BIOS'],
    paletteTag: 'Mono',
    risk: 'Safe',
    bg: '#000000',
    ink: DOS_GRAY,
    palette: ['#000000', DOS_GRAY, '#FFFFFF'],
    fonts: [],
    type: 'The VGA text-mode face, 8x16 cells as the video BIOS drew them, at 6x for the prompt and 2x for the version line.',
    concept:
      'A DOS prompt sitting in the OgOs directory, waiting for you to type. ' +
      'Nothing but light grey on black: the most austere direction, and the one every PC user of the era read a thousand times a day.',
    logo: (w) => {
      const prompt = 'C:\\' + w + '>';
      const px = 6, x = 240 - ((prompt.length + 1) * 8 * px) / 2, y = 112;
      const cx = x + prompt.length * 8 * px;
      return OG.logoSvg(
        vga(w + ' 32 [Version 1.00]', x, 64, 2, DOS_GRAY) +
          vga(prompt, x, y, px, DOS_GRAY) +
          `<rect x="${cx}" y="${y + 13 * px}" width="${8 * px}" height="${2 * px}" fill="${DOS_GRAY}"><animate attributeName="opacity" values="1;1;0;0" dur="1.1s" calcMode="discrete" repeatCount="indefinite"/></rect>`,
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" rx="6" fill="#000"/>` +
          OG.bitmap(['##....', '.##...', '..##..', '...##.', '..##..', '.##...', '##....'], { '#': DOS_GRAY }, { x: 9, y: 17, px: 4 }) +
          `<rect x="33" y="37" width="22" height="7" fill="${DOS_GRAY}"/>`,
        { crisp: true }
      ),
  });
  /* ---------------------------------------------------- 39. POST ------- */

  // Rasterizes a picture onto a pixel grid: fn(x, y) gets a pixel's center
  // and returns a map key, or nothing for an empty pixel.
  function raster(w, h, fn) {
    const rows = [];
    for (let j = 0; j < h; j++) {
      let s = '';
      for (let i = 0; i < w; i++) s += fn(i + 0.5, j + 0.5) || '.';
      rows.push(s);
    }
    return rows;
  }
  function inPoly(x, y, P) {
    let c = false;
    for (let i = 0, j = P.length - 1; i < P.length; j = i++) {
      if ((P[i][1] > y) !== (P[j][1] > y) && x < ((P[j][0] - P[i][0]) * (y - P[i][1])) / (P[j][1] - P[i][1]) + P[i][0]) c = !c;
    }
    return c;
  }
  // Adds a one-pixel outline of `k` around every pixel that isn't '.'.
  function outline(rows, k) {
    return rows.map((r, j) => [...r].map((ch, i) => {
      if (ch !== '.') return ch;
      for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
        const q = rows[j + dj] && rows[j + dj][i + di];
        if (q && q !== '.' && q !== k) return k;
      }
      return '.';
    }).join(''));
  }

  // The power-on badge: a ring O struck by a bolt, a g, a star and a swoosh.
  function postBadge(icon) {
    const W = icon ? 16 : 36, H = icon ? 16 : 24;
    const ring = icon ? { cx: 8, cy: 8, ro: 7.6, ri: 3.4 } : { cx: 11, cy: 11.5, ro: 10.6, ri: 5 };
    const sc = icon ? 16 / 23 : 1;
    const bolt = [[15, 0], [7, 12.5], [12, 12.5], [7.5, 23.5], [18.5, 9.5], [13.5, 9.5], [19, 0]].map(([x, y]) => icon ? [x * sc - 1.6, y * sc] : [x, y]);
    const gRows = TOPAZ.g;
    const star = (x, y, cx, cy, R) => {
      const P = [];
      for (let k = 0; k < 10; k++) { const a = rad(-90 + k * 36), r = k % 2 ? R * 0.45 : R; P.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]); }
      return inPoly(x, y, P);
    };
    let rows = raster(W, H, (x, y) => {
      if (inPoly(x, y, bolt)) return x + y > (icon ? 15 : 22) ? 'y' : 'Y';
      const d = Math.hypot(x - ring.cx, y - ring.cy);
      if (!icon) {
        if (star(x, y, 32, 4.5, 4.4)) return 'W';
        const gx = Math.floor((x - 21) / 2), gy = Math.floor((y - 3) / 2);
        if (gx >= 0 && gx < 8 && gy >= 0 && gy < 8 && gRows[gy][gx] === '#') return gy > 4 ? 'c' : 'C';
        const e = Math.hypot((x - 19) / 18, (y + 10) / 33);
        if (y > 13 && e > 1.0 && e < 1.11 && d > ring.ro + 0.6) return x < 20 ? 'M' : 'R';
      }
      if (d < ring.ro && d > ring.ri) return x - ring.cx + (y - ring.cy) * 1.2 > 3 ? 'g' : 'G';
      return '';
    });
    return outline(rows, 'k');
  }
  const POST_MAP = { Y: '#FFFF55', y: '#FFAA00', G: '#55FF55', g: '#00AA00', C: '#55FFFF', c: '#00AAAA', W: '#FFFFFF', M: '#FF55FF', R: '#FF5555', k: '#000000' };

  OG.add({
    num: 39,
    id: 'post',
    name: 'POST',
    caps: 'OgOS',
    flexCase: true,
    family: 'Interface',
    sources: ['DOS & BIOS'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: true,
    bg: '#000000',
    ink: '#FFFFFF',
    palette: ['#000000', '#AAAAAA', '#FFFFFF', '#55FF55', '#FFFF55', '#FF55FF'],
    fonts: ['VT323'],
    type: 'VT323, a terminal face from the same years, for every line; the badge is pixel art rasterized from a ring, a bolt, a star and a swoosh.',
    concept:
      'The first second of a Pentium II era PC: the BIOS counts its memory, names its processor, and puts a bright pixel badge in the corner. ' +
      'Here the badge is OgOs\'s own, and the machine under test is the one the project targets.',
    logo: (w) => {
      const vt = (t, x, y, size, fill, extra = {}) => OG.text(t, Object.assign({ x, y, size, family: "'VT323', monospace", fill }, extra));
      return OG.logoSvg(
        `<rect width="480" height="300" fill="#000"/>` +
          `<g shape-rendering="crispEdges">${OG.bitmap(postBadge(false), POST_MAP, { x: 348, y: 22, px: 3 })}</g>` +
          vt(w, 20, 120, 140, '#FFFFFF') +
          vt('BIOS v1.00', 252, 120, 34, '#FFFF55') +
          vt('Copyright (C) 2026, the OgOs project', 24, 164, 20, '#AAAAAA') +
          vt('Main Processor :', 24, 198, 22, '#AAAAAA') + vt('Pentium II 400MHz', 182, 198, 22, '#FFFFFF') +
          vt('Memory Testing :', 24, 220, 22, '#AAAAAA') + vt('32768K OK', 182, 220, 22, '#FFFFFF') +
          vt('Press DEL to enter SETUP', 24, 256, 20, '#AAAAAA') +
          vt('10/27/26-i440FX-OGOS-00', 24, 279, 16, '#555555')
      );
    },
    icon: () => OG.iconSvg(OG.bitmap(postBadge(true), POST_MAP, { x: 0, y: 0, px: 4 }), { crisp: true }),
  });

  /* ---------------------------------------------------- 44. ANSI ------- */

  // Block letters on a grid of half cells: each text cell holds two of
  // these pixels, one above the other, drawn as a full block or a half
  // block. Caps fill rows 0-9, lowercase rows 2-9, descenders rows 10-13.
  const ANSI = {
    O: ['.#####.', '#######', '##...##', '##...##', '##...##', '##...##', '##...##', '##...##', '#######', '.#####.'],
    G: ['.######', '#######', '##.....', '##.....', '##..###', '##..###', '##...##', '##...##', '#######', '.#####.'],
    S: ['.######', '#######', '##.....', '##.....', '######.', '.######', '.....##', '.....##', '#######', '######.'],
    o: ['', '', '.#####.', '#######', '##...##', '##...##', '##...##', '##...##', '#######', '.#####.'],
    g: ['', '', '.######', '#######', '##...##', '##...##', '##...##', '##...##', '#######', '.######', '.....##', '.....##', '#######', '######.'],
    s: ['', '', '.######', '#######', '##.....', '######.', '.######', '.....##', '#######', '######.'],
  };
  const ANSI_SIGN = Object.assign({}, VGA, hexFont({ b: '0000e06060786c666666667c00000000', '-': '00000000000000fe0000000000000000' }, 16));

  // Each text row's look: a solid color, or a 50% dither of two colors,
  // which is how ANSI art faked gradients with 16 colors.
  const ANSI_ROWS = [['#FF55FF'], ['#FF55FF', '#5555FF'], ['#5555FF'], ['#5555FF', '#55FFFF'], ['#55FFFF'], ['#55FFFF', '#00AAAA'], ['#00AAAA']];

  function ansiArt(w, x0, y0, cw) {
    const rows = prop(w, ANSI, 14, 1, 3).map((r) => r.replace(/[^#]/g, '.'));
    const W = rows[0].length, ch = cw * 2, q = cw / 4;
    const px = (r, c) => !!(rows[r] && rows[r][c] === '#');
    const any = (k, c) => px(2 * k, c) || px(2 * k + 1, c);
    // Empty cells reachable from outside the letters, so counters stay black.
    const outside = new Set(), stack = [[0, -1]];
    while (stack.length) {
      const [k, c] = stack.pop(), key = k + ',' + c;
      if (k < -1 || k > 7 || c < -1 || c > W + 1 || outside.has(key) || (k >= 0 && c >= 0 && any(k, c))) continue;
      outside.add(key);
      stack.push([k + 1, c], [k - 1, c], [k, c + 1], [k, c - 1]);
    }
    let defs = '', out = '';
    ANSI_ROWS.forEach((v, k) => {
      if (v.length > 1) {
        defs += `<pattern id="am${k}" x="${x0}" y="${y0}" width="${2 * q}" height="${2 * q}" patternUnits="userSpaceOnUse">` +
          `<rect width="${2 * q}" height="${2 * q}" fill="${v[1]}"/><rect width="${q}" height="${q}" fill="${v[0]}"/><rect x="${q}" y="${q}" width="${q}" height="${q}" fill="${v[0]}"/></pattern>`;
      }
    });
    // The shade block, light shade (25%), for the drop shadow.
    defs += `<pattern id="ash" x="${x0}" y="${y0}" width="${4 * q}" height="${2 * q}" patternUnits="userSpaceOnUse">` +
      `<rect width="${q}" height="${q}" fill="#3C3CC8"/><rect x="${2 * q}" y="${q}" width="${q}" height="${q}" fill="#3C3CC8"/></pattern>`;
    for (let k = 0; k < 7; k++) {
      const fill = ANSI_ROWS[k].length > 1 ? `url(#am${k})` : ANSI_ROWS[k][0];
      for (let c = 0; c <= W; c++) {
        const x = x0 + c * cw, y = y0 + k * ch, t = px(2 * k, c), b = px(2 * k + 1, c);
        if (t || b) {
          out += `<rect x="${x}" y="${t ? y : y + cw}" width="${cw}" height="${t && b ? ch : cw}" fill="${fill}"/>`;
        } else if (k > 0 && c > 0 && any(k - 1, c - 1) && outside.has(k + ',' + c)) {
          out += `<rect x="${x}" y="${y}" width="${cw}" height="${ch}" fill="url(#ash)"/>`;
        }
      }
    }
    return { svg: `<defs>${defs}</defs>${out}`, w: (W + 1) * cw };
  }

  OG.add({
    num: 44,
    id: 'ansi',
    name: 'ANSI',
    caps: 'OGOS',
    flexCase: true,
    family: 'Pixel',
    sources: ['DOS & BIOS'],
    paletteTag: 'Neon',
    risk: 'Bold',
    bg: '#000000',
    ink: '#55FFFF',
    palette: ['#000000', '#FF55FF', '#5555FF', '#55FFFF', '#00AAAA'],
    fonts: [],
    type: 'Letters built from code page 437 blocks: full and half blocks for the shapes, 50% shade blocks to dither between rows, light shade for the drop shadow; the sign-off in the VGA face.',
    concept:
      'The welcome screen of a dial-up bulletin board, where artists drew logos one text cell at a time in 16 colors. ' +
      'OgOs belongs to the people who made things on these machines, and this is how they signed their work.',
    logo: (w) => {
      const cw = 12, a = ansiArt(w, 0, 0, cw);
      const x0 = Math.round((480 - a.w) / 2 / 3) * 3, y0 = 63;
      const sign = '-ogos.bbs-';
      return OG.logoSvg(
        `<g transform="translate(${x0} ${y0})">${a.svg}</g>` +
          OG.bitmap(mono(sign, ANSI_SIGN, 8, 16), { '#': '#AAAAAA' }, { x: x0 + a.w - sign.length * 12, y: y0 + 6 * 24 + 4, px: 1.5 }),
        { crisp: true }
      );
    },
    icon: () => {
      const a = ansiArt('O', 0, 0, 7);
      return OG.iconSvg(`<rect width="64" height="64" rx="6" fill="#000"/><g transform="translate(5 -1)">${a.svg}</g>`, { crisp: true });
    },
  });

  /* ----------------------------------------------- 49. Commander ------- */

  const NC = { blue: '#0000AA', frame: '#55FFFF', text: '#55FFFF', bar: '#00AAAA', yellow: '#FFFF55', white: '#FFFFFF' };

  function doubleBox(x, y, w, h, t, gap, color) {
    const r = (a, b, c, d) => `<rect x="${a}" y="${b}" width="${c}" height="${d}" fill="none" stroke="${color}" stroke-width="${t}"/>`;
    return r(x, y, w, h) + r(x + t + gap, y + t + gap, w - 2 * (t + gap), h - 2 * (t + gap));
  }

  OG.add({
    num: 49,
    id: 'commander',
    name: 'Commander',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['DOS & BIOS'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    scene: true,
    bg: NC.blue,
    ink: NC.white,
    palette: [NC.blue, NC.frame, NC.bar, NC.yellow, '#000000'],
    fonts: ['VT323'],
    type: 'The name in the VGA 8x16 face at 5x, black on cyan like an active panel\'s path; the listing and key bar in VT323.',
    concept:
      'A two-pane DOS file manager panel in blue and cyan, with OgOs where the path would be. ' +
      'It is the tool you lived in before there was a desktop, and a fair picture of what OgOs is: a place to keep and run your own programs.',
    logo: (w) => {
      const vt = (t, x, y, fill, extra = {}) => OG.text(t, Object.assign({ x, y, size: 21, family: "'VT323', monospace", fill }, extra));
      const px = 5, tw = w.length * 8 * px, tx = 240 - tw / 2;
      let s = `<rect width="480" height="300" fill="${NC.blue}"/>`;
      s += doubleBox(18, 58, 444, 196, 3, 3, NC.frame);
      // Columns.
      s += `<rect x="312" y="67" width="3" height="152" fill="${NC.frame}"/><rect x="27" y="219" width="426" height="3" fill="${NC.frame}"/>`;
      s += `<rect x="${tx - 14}" y="${58 - 38}" width="${tw + 28}" height="76" fill="${NC.bar}"/>`;
      s += OG.bitmap(mono(w, VGA, 8, 16), { '#': '#000' }, { x: tx, y: 58 - 38 - 2, px });
      s += vt('Name', 150, 112, NC.yellow, { anchor: 'middle' }) + vt('Size', 384, 112, NC.yellow, { anchor: 'middle' });
      const files = [['..', 'UP--DIR'], ['BIN', 'SUB-DIR'], ['SYSUTIL', 'SUB-DIR'], ['KERNEL   BIN', '412160'], ['STARTUP  SH', '1024']];
      files.forEach(([n, z], i) => {
        const y = 136 + i * 19;
        if (i === 3) s += `<rect x="27" y="${y - 15}" width="426" height="19" fill="${NC.bar}"/>`;
        const c = i === 3 ? '#000' : NC.text;
        s += vt(n, 40, y, c) + vt(z, 440, y, c, { anchor: 'end' });
      });
      s += vt('KERNEL.BIN     412160   10-27-26   12:00', 40, 243, NC.text);
      const keys = [['1', 'Help'], ['2', 'Menu'], ['3', 'View'], ['4', 'Edit'], ['5', 'Copy'], ['10', 'Quit']];
      keys.forEach(([n, k], i) => {
        const kx = 18 + i * 75, nw = n.length * 8 + 3;
        s += vt(n, kx, 286, NC.white) + `<rect x="${kx + nw}" y="270" width="${71 - nw}" height="21" fill="${NC.bar}"/>` + vt(k, kx + nw + 3, 286, '#000');
      });
      return OG.logoSvg(s);
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="${NC.blue}"/>` + doubleBox(6, 8, 52, 48, 3, 3, NC.frame) +
          `<rect x="20" y="3" width="24" height="10" fill="${NC.bar}"/><rect x="15" y="31" width="34" height="7" fill="${NC.bar}"/>`,
        { crisp: true }
      ),
  });

  /* --------------------------------------------------- 54. Floppy ------ */

  const FLOPPY_STRIPES = ['#E2231A', '#F7B500', '#2DA84A', '#1E6FD9'];

  // A 3.5" disk, S units square, its top-left corner at the origin.
  function floppy(S, label) {
    const k = S / 100, r = (v) => f2(v * k);
    let s = `<path d="M 0 ${r(3)} Q 0 0 ${r(3)} 0 H ${r(93)} L ${r(100)} ${r(7)} V ${r(97)} Q ${r(100)} ${r(100)} ${r(97)} ${r(100)} H ${r(3)} Q 0 ${r(100)} 0 ${r(97)} Z" fill="url(#fbody)"/>`;
    // The shutter's channel, the shutter and its window.
    s += `<rect x="${r(14)}" y="0" width="${r(72)}" height="${r(37)}" fill="#141519"/>`;
    s += `<rect x="${r(22)}" y="0" width="${r(56)}" height="${r(35.5)}" rx="${r(1)}" fill="url(#fmetal)"/>`;
    s += `<rect x="${r(52)}" y="${r(5)}" width="${r(13)}" height="${r(25)}" rx="${r(1)}" fill="#2A2C31"/>`;
    s += `<rect x="${r(22)}" y="${r(34)}" width="${r(56)}" height="${r(1.5)}" fill="#000" opacity="0.35"/>`;
    // The write-protect and density holes.
    s += `<rect x="${r(3)}" y="${r(88)}" width="${r(6)}" height="${r(7)}" fill="#08090B"/><rect x="${r(91)}" y="${r(88)}" width="${r(6)}" height="${r(7)}" fill="#08090B"/>`;
    s += `<rect x="${r(4)}" y="${r(88)}" width="${r(4)}" height="${r(3.5)}" fill="#3B3D44"/>`;
    // The label.
    s += `<rect x="${r(10)}" y="${r(44)}" width="${r(80)}" height="${r(53)}" rx="${r(1.5)}" fill="#F7F5EE"/>`;
    FLOPPY_STRIPES.forEach((c, i) => { s += `<rect x="${r(10)}" y="${r(46 + i * 2.6)}" width="${r(80)}" height="${r(2)}" fill="${c}"/>`; });
    if (label) s += label;
    return s;
  }
  const FLOPPY_DEFS =
    `<linearGradient id="fbody" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#34363D"/><stop offset="1" stop-color="#18191D"/></linearGradient>` +
    `<linearGradient id="fmetal" x1="0" y1="0" x2="1" y2="0.3"><stop offset="0" stop-color="#E4E7EB"/><stop offset="0.5" stop-color="#B7BDC5"/><stop offset="0.75" stop-color="#D5D9DE"/><stop offset="1" stop-color="#9AA1AA"/></linearGradient>` +
    `<filter id="fsh" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="7"/></filter>`;

  OG.add({
    num: 54,
    id: 'floppy',
    name: 'Floppy',
    caps: 'OgOs',
    flexCase: true,
    family: 'Object',
    sources: ['Print & packaging'],
    paletteTag: 'Primary',
    risk: 'Safe',
    bg: '#5AA6C8',
    ink: '#FFFFFF',
    palette: ['#1E1F24', '#F7F5EE', '#E2231A', '#F7B500', '#2DA84A', '#1E6FD9'],
    fonts: ['Kalam:wght@700'],
    type: 'Kalam Bold, a felt-tip hand, written across the label the way you labeled a boot disk; the disk is drawn.',
    concept:
      'A 3.5-inch boot disk with a striped label and the name written on it in marker. ' +
      'It is how software actually traveled in the Pentium II years, and an OS small enough to boot from one is a point of pride.',
    logo: (w) => {
      const S = 228, k = S / 100;
      const label = `<g transform="rotate(-4 ${f2(50 * k)} ${f2(76 * k)})">` +
        OG.text(w, { x: f2(50 * k), y: f2(86 * k), size: 66, family: "'Kalam', cursive", weight: 700, fill: '#1B2A8C', anchor: 'middle', len: fitLen(w, 66, KALAM_W, 66 * k) }) +
        `</g>`;
      return OG.logoSvg(
        `<defs>${FLOPPY_DEFS}</defs><g transform="translate(240 150) rotate(-7) translate(${-S / 2} ${-S / 2})">` +
          `<rect x="8" y="12" width="${S}" height="${S}" rx="6" fill="#06223A" opacity="0.45" filter="url(#fsh)"/>` +
          floppy(S, label) + `</g>`
      );
    },
    icon: () => OG.iconSvg(`<defs>${FLOPPY_DEFS}</defs><g transform="translate(4 4)">${floppy(56, '')}</g>`),
  });

  /* -------------------------------------------------- 59. Keycaps ------ */

  // A sculpted keycap seen from a little above: the top, the side walls
  // and the taller front wall.
  function keycap(x, y, w, h, legend, dark, opts = {}) {
    const top = dark ? ['#B3AFA7', '#9C9890'] : ['#F2ECDF', '#DED6C4'];
    const side = dark ? '#8E8A82' : '#CFC6B1', front = dark ? '#7A766F' : '#BDB39C';
    const inset = w * 0.11, tb = h * 0.72;
    let s = `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${w * 0.1}" fill="${side}"/>`;
    s += `<path d="M ${f2(x + inset)} ${f2(y + tb - 6)} L ${f2(x + w - inset)} ${f2(y + tb - 6)} L ${f2(x + w - 2)} ${f2(y + h - 8)} Q ${f2(x + w - 3)} ${y + h} ${f2(x + w - 12)} ${y + h} H ${f2(x + 12)} Q ${x + 3} ${y + h} ${x + 2} ${f2(y + h - 8)} Z" fill="${front}"/>`;
    s += `<rect x="${f2(x + inset)}" y="${y + 4}" width="${f2(w - 2 * inset)}" height="${f2(tb - 4)}" rx="${f2(w * 0.08)}" fill="url(#${dark ? 'ktd' : 'ktl'})"/>`;
    s += `<rect x="${f2(x + inset + 4)}" y="${y + 6}" width="${f2(w - 2 * inset - 8)}" height="2" rx="1" fill="#fff" opacity="${dark ? 0.25 : 0.7}"/>`;
    if (legend) {
      const cx = opts.center;
      s += OG.text(legend, {
        x: f2(cx ? x + w / 2 : x + inset + w * 0.07), y: f2(cx ? y + tb * 0.62 + (opts.size || 30) * 0.35 : y + 6 + (opts.size || 30) * 0.86),
        size: opts.size || 30, family: "'IBM Plex Sans', sans-serif", weight: 500, fill: dark ? '#1E1D1B' : '#2E2C28', anchor: cx ? 'middle' : 'start',
      });
    }
    return s;
  }
  const KEY_DEFS =
    `<linearGradient id="ktl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#F7F2E7"/><stop offset="0.5" stop-color="#E7E0D0"/><stop offset="1" stop-color="#EFE9DC"/></linearGradient>` +
    `<linearGradient id="ktd" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#B9B5AD"/><stop offset="0.5" stop-color="#A6A29A"/><stop offset="1" stop-color="#B0ACA4"/></linearGradient>` +
    `<filter id="kblur" x="-20%" y="-50%" width="140%" height="200%"><feGaussianBlur stdDeviation="6"/></filter>`;

  OG.add({
    num: 59,
    id: 'keycaps',
    name: 'Keycaps',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['IBM & OS/2'],
    paletteTag: 'Earthy',
    risk: 'Safe',
    bg: '#2B333C',
    ink: '#EFE9DC',
    palette: ['#EFE9DC', '#CFC6B1', '#A6A29A', '#2B333C', '#2E2C28'],
    fonts: ['IBM Plex Sans:wght@500', 'IBM Plex Mono:wght@500'],
    type: 'IBM Plex Sans Medium, printed in the top-left corner of each keytop the way the legends sat on a buckling-spring board.',
    concept:
      'Four sculpted beige keys from the heaviest keyboard ever made, the last one in modifier grey, spelling the name. ' +
      'OgOs is for typing code on, and the keyboard is the one piece of 1986 hardware people still buy.',
    logo: (w) => {
      const kw = 94, kh = 106, gap = 10, n = w.length, W = n * kw + (n - 1) * gap, x0 = 240 - W / 2, y0 = 74;
      let s = `<defs>${KEY_DEFS}<linearGradient id="kcase" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#E2DAC8"/><stop offset="1" stop-color="#BDB39D"/></linearGradient></defs>`;
      s += `<rect x="${x0 - 8}" y="${y0 + 8}" width="${W + 16}" height="${kh + 22}" rx="16" fill="#000" opacity="0.55" filter="url(#kblur)"/>`;
      s += `<rect x="${x0 - 14}" y="${y0 - 14}" width="${W + 28}" height="${kh + 28}" rx="14" fill="url(#kcase)"/>`;
      s += `<rect x="${x0 - 14}" y="${y0 - 14}" width="${W + 28}" height="${kh + 28}" rx="14" fill="none" stroke="#fff" stroke-opacity="0.45" stroke-width="1.5"/>`;
      s += `<rect x="${x0 - 5}" y="${y0 - 5}" width="${W + 10}" height="${kh + 10}" rx="7" fill="#2A2724"/>`;
      [...w].forEach((ch, i) => { s += keycap(x0 + i * (kw + gap), y0, kw, kh, ch, i === n - 1, { size: 40 }); });
      s += OG.text('PRESS ANY KEY', { x: 240, y: 240, size: 13, family: "'IBM Plex Mono', monospace", weight: 500, fill: '#A9B3BD', anchor: 'middle', ls: 6 });
      return OG.logoSvg(s);
    },
    icon: () => OG.iconSvg(`<defs>${KEY_DEFS}</defs>` + keycap(4, 5, 56, 56, 'O', false, { center: true, size: 30 })),
  });

  /* ----------------------------------------------------- 64. Chip ------ */

  function dip(x, y, w, h, n, pinW, pinL) {
    const pitch = w / n;
    let s = '';
    for (let i = 0; i < n; i++) {
      const cx = x + pitch * (i + 0.5);
      [[y - pinL, 1], [y + h, -1]].forEach(([py, dir]) => {
        s += `<rect x="${f2(cx - pinW / 2)}" y="${f2(py)}" width="${f2(pinW)}" height="${f2(pinL)}" fill="url(#cpin)"/>`;
        const tip = dir > 0 ? py : py + pinL * 0.45;
        s += `<rect x="${f2(cx - pinW / 2)}" y="${f2(tip)}" width="${f2(pinW)}" height="${f2(pinL * 0.55)}" fill="url(#cpin)"/>`;
        s += `<rect x="${f2(cx - pinW * 0.22)}" y="${f2(dir > 0 ? py - pinL * 0.35 : py + pinL)}" width="${f2(pinW * 0.44)}" height="${f2(pinL * 0.35)}" fill="url(#cpin)"/>`;
      });
    }
    s += `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${h * 0.04}" fill="url(#cbody)"/>`;
    s += `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${h * 0.04}" fill="#000" filter="url(#cgrain)" opacity="0.5"/>`;
    s += `<rect x="${x + 1}" y="${y + 1}" width="${w - 2}" height="${h - 2}" rx="${h * 0.04}" fill="none" stroke="#fff" stroke-opacity="0.1" stroke-width="2"/>`;
    // The notch at the pin-1 end, and the pin-1 dimple.
    s += `<path d="M ${x} ${f2(y + h / 2 - h * 0.11)} A ${f2(h * 0.11)} ${f2(h * 0.11)} 0 0 1 ${x} ${f2(y + h / 2 + h * 0.11)} Z" fill="#060607" stroke="#fff" stroke-opacity="0.12"/>`;
    s += `<circle cx="${f2(x + h * 0.2)}" cy="${f2(y + h * 0.78)}" r="${f2(h * 0.055)}" fill="#0A0A0B" stroke="#fff" stroke-opacity="0.15"/>`;
    return s;
  }
  const CHIP_DEFS =
    `<linearGradient id="cpin" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#7D838B"/><stop offset="0.35" stop-color="#EEF1F4"/><stop offset="1" stop-color="#9AA0A8"/></linearGradient>` +
    `<linearGradient id="cbody" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2C2D31"/><stop offset="0.12" stop-color="#1B1C1F"/><stop offset="1" stop-color="#121314"/></linearGradient>` +
    `<filter id="cgrain" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="4"/><feColorMatrix values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 0.12 0"/><feComposite in2="SourceGraphic" operator="in"/></filter>` +
    `<filter id="cdrop2" x="-10%" y="-30%" width="120%" height="160%"><feGaussianBlur stdDeviation="5"/></filter>`;
  const CHIP_INK = '#C9CDD2';

  OG.add({
    num: 64,
    id: 'chip',
    name: 'Chip',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['OgOs internals'],
    paletteTag: 'Metallic',
    risk: 'Bold',
    bg: '#0F5B3C',
    ink: '#E8F2EC',
    palette: ['#141517', '#C9CDD2', '#0F5B3C', '#C88A3A'],
    fonts: ['Share Tech Mono'],
    type: 'Share Tech Mono in laser-etch grey, the part number large and the date code small, as printed on a 1990s chip.',
    concept:
      'A 40-pin DIP chip whose part number is the name: OgOs as a component you could socket into a board. ' +
      'It makes the OS feel like hardware, i686 and all, which is exactly the machine it is written for.',
    logo: (w) => {
      const x = 40, y = 96, cw = 400, ch = 112;
      let s = `<defs>${CHIP_DEFS}</defs>`;
      // A few copper traces running off the pins.
      let tr = '';
      [2, 5, 9, 14, 17].forEach((i) => { const cx = x + (cw / 20) * (i + 0.5); tr += `M ${f2(cx)} ${y - 24} V ${y - 46 - (i % 3) * 8} L ${f2(cx + 20)} ${y - 66 - (i % 3) * 8} V -10 `; });
      [1, 4, 8, 12, 16, 19].forEach((i) => { const cx = x + (cw / 20) * (i + 0.5); tr += `M ${f2(cx)} ${y + ch + 24} V ${y + ch + 44 + (i % 2) * 10} L ${f2(cx - 18)} ${y + ch + 62 + (i % 2) * 10} V 310 `; });
      s += `<path d="${tr}" fill="none" stroke="#C88A3A" stroke-opacity="0.35" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>`;
      s += `<rect x="${x + 8}" y="${y + 12}" width="${cw}" height="${ch}" rx="6" fill="#000" opacity="0.45" filter="url(#cdrop2)"/>`;
      s += dip(x, y, cw, ch, 20, 11, 22);
      const t = (str, tx, ty, size, extra = {}) => OG.text(str, Object.assign({ x: tx, y: ty, size, family: "'Share Tech Mono', monospace", fill: CHIP_INK }, extra));
      s += t(w + '-32', 248, 166, 66, { anchor: 'middle', ls: 2, attrs: 'opacity="0.92"' });
      s += t('i686', 92, 194, 17, { attrs: 'opacity="0.8"' }) + t('© 2026', 248, 194, 17, { anchor: 'middle', attrs: 'opacity="0.8"' }) + t('2644', 410, 194, 17, { anchor: 'end', attrs: 'opacity="0.8"' });
      return OG.logoSvg(s);
    },
    icon: () => OG.iconSvg(`<defs>${CHIP_DEFS}</defs>` + dip(8, 18, 48, 28, 4, 5.5, 9)),
  });

  /* -------------------------------------------------- 69. CD-ROM ------- */

  // The disc: silver, two fans of diffraction rainbow, the clear hub.
  function cdDisc(cx, cy, R, id, arcText) {
    const hole = R * 0.12, hub = R * 0.36;
    const ann = (r1, r2) => `M ${f2(cx + r2)} ${cy} A ${r2} ${r2} 0 1 0 ${f2(cx - r2)} ${cy} A ${r2} ${r2} 0 1 0 ${f2(cx + r2)} ${cy} Z M ${f2(cx + r1)} ${cy} A ${r1} ${r1} 0 1 1 ${f2(cx - r1)} ${cy} A ${r1} ${r1} 0 1 1 ${f2(cx + r1)} ${cy} Z`;
    let s = `<defs><radialGradient id="${id}s" cx="0.5" cy="0.5" r="0.5"><stop offset="0.3" stop-color="#F4F6F9"/><stop offset="0.7" stop-color="#C9D0D9"/><stop offset="1" stop-color="#E9EDF2"/></radialGradient>` +
      `<clipPath id="${id}c"><path d="${ann(hub + 4, R - 2)}" clip-rule="evenodd"/></clipPath>` +
      `<filter id="${id}b" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="${f2(R * 0.035)}"/></filter></defs>`;
    s += `<path d="${ann(hole, R)}" fill="url(#${id}s)" fill-rule="evenodd"/>`;
    // The rainbow: thin wedges, colored where the two fans fall, blurred.
    let wedges = '';
    for (let a = 0; a < 360; a += 3) {
      [[28, 1], [208, 1], [118, 0.35], [298, 0.35]].forEach(([c, amp]) => {
        let d = ((a - c + 540) % 360) - 180;
        if (Math.abs(d) > 46) return;
        const t = (d + 46) / 92, hue = Math.round(330 - t * 300), op = amp * Math.pow(Math.cos((d / 46) * Math.PI / 2), 0.7);
        const p1 = [cx + R * Math.cos(rad(a)), cy + R * Math.sin(rad(a))], p2 = [cx + R * Math.cos(rad(a + 3.4)), cy + R * Math.sin(rad(a + 3.4))];
        wedges += `<path d="M ${cx} ${cy} L ${pt(p1)} L ${pt(p2)} Z" fill="hsl(${hue} 95% 60%)" opacity="${f2(op * 0.8)}"/>`;
      });
    }
    s += `<g clip-path="url(#${id}c)"><g filter="url(#${id}b)">${wedges}</g></g>`;
    // Rings: the data edge, the rim, the hub's stacking ring.
    s += `<circle cx="${cx}" cy="${cy}" r="${f2(R - 1)}" fill="none" stroke="#fff" stroke-width="2" opacity="0.8"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${f2(hub + 4)}" fill="none" stroke="#9AA4B1" stroke-width="1.2"/>`;
    s += `<path d="${ann(hole, hub)}" fill="#DCE3EA" fill-opacity="0.55" fill-rule="evenodd"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${f2(hub * 0.62)}" fill="none" stroke="#B5BEC9" stroke-width="${f2(R * 0.02)}"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${f2(hole)}" fill="none" stroke="#8D97A4" stroke-width="1"/>`;
    // Glare.
    s += `<path d="M ${cx} ${cy} L ${pt([cx + R * Math.cos(rad(-62)), cy + R * Math.sin(rad(-62))])} L ${pt([cx + R * Math.cos(rad(-54)), cy + R * Math.sin(rad(-54))])} Z" fill="#fff" opacity="0.55" clip-path="url(#${id}c)" filter="url(#${id}b)"/>`;
    if (arcText) {
      const r = R * 0.83;
      s += `<path id="${id}t" d="M ${f2(cx - r)} ${cy} A ${f2(r)} ${f2(r)} 0 0 1 ${f2(cx + r)} ${cy}" fill="none"/>`;
      s += `<text font-size="${f2(R * 0.095)}" font-family="'Share Tech Mono', monospace" fill="#3A4656" letter-spacing="${f2(R * 0.03)}" text-anchor="middle"><textPath href="#${id}t" startOffset="50%">${arcText}</textPath></text>`;
    }
    return s;
  }

  OG.add({
    num: 69,
    id: 'cd-rom',
    name: 'CD-ROM',
    caps: 'OgOs',
    flexCase: true,
    family: 'Object',
    sources: ['Print & packaging'],
    paletteTag: 'Metallic',
    risk: 'Bold',
    bg: '#0D1626',
    ink: '#FFFFFF',
    palette: ['#0D1626', '#E9EDF2', '#C9D0D9', '#FF4FA3', '#3FD0FF', '#FFE14F'],
    fonts: ['Russo One', 'Share Tech Mono'],
    type: 'Russo One for the name, square-shouldered like the extended faces on 1995 jewel cases; the disc\'s print in Share Tech Mono on an arc.',
    concept:
      'A silver CD-ROM catching the light in two rainbow fans, with MULTIMEDIA EDITION printed round its edge. ' +
      'It is the late end of retro, when an OS arrived on a shiny disc, and the disc is the O.',
    logo: (w) => {
      const name = OG.text(w, { x: 0, y: 0, size: 76, family: "'Russo One', sans-serif", fill: 'url(#cdn)', len: fitLen(w, 76, RUSSO_W, 196) });
      return OG.logoSvg(
        `<defs><linearGradient id="cdn" x1="0" y1="-60" x2="0" y2="0" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFFFFF"/><stop offset="0.55" stop-color="#DCE5F0"/><stop offset="0.56" stop-color="#9FB0C6"/><stop offset="1" stop-color="#E6EDF5"/></linearGradient>` +
          `<linearGradient id="cdr" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#FF4FA3"/><stop offset="0.33" stop-color="#FFE14F"/><stop offset="0.66" stop-color="#4FFFB0"/><stop offset="1" stop-color="#3FD0FF"/></linearGradient></defs>` +
          cdDisc(134, 150, 106, 'cd', 'MULTIMEDIA EDITION') +
          `<g transform="translate(260 168)">${name}</g>` +
          `<rect x="262" y="186" width="194" height="4" fill="url(#cdr)"/>` +
          OG.text('CD-ROM · 32-BIT', { x: 262, y: 212, size: 15, family: "'Share Tech Mono', monospace", fill: '#9FB0C6', ls: 3.2 })
      );
    },
    icon: () => OG.iconSvg(cdDisc(32, 32, 30, 'ci', '')),
  });
  /* ------------------------------------------------- 74. Cassette ------ */

  const TAPE_STRIPES = ['#F6C343', '#F08A24', '#D94A2A', '#8C2F45'];

  // A tape hub seen through the window: the tape wound on it, a white
  // spool and its six teeth.
  function reel(cx, cy, pack, hub) {
    let s = `<circle cx="${cx}" cy="${cy}" r="${pack}" fill="#3B2416"/><circle cx="${cx}" cy="${cy}" r="${pack}" fill="none" stroke="#6A4630" stroke-width="1" opacity="0.6"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${hub}" fill="#F2F0EA"/><circle cx="${cx}" cy="${cy}" r="${f2(hub * 0.55)}" fill="#202024"/>`;
    for (let k = 0; k < 6; k++) {
      const a = rad(k * 60 + 15);
      s += `<rect x="${f2(cx - hub * 0.12)}" y="${f2(cy - hub * 0.55)}" width="${f2(hub * 0.24)}" height="${f2(hub * 0.24)}" fill="#F2F0EA" transform="rotate(${k * 60 + 15} ${cx} ${cy})"/>`;
      void a;
    }
    return s;
  }

  function cassette(w, icon) {
    if (icon) {
      let s = `<rect x="2" y="12" width="60" height="40" rx="4" fill="#2C2B30"/><rect x="6" y="15" width="52" height="25" rx="2" fill="#F3EBD8"/>`;
      TAPE_STRIPES.forEach((c, i) => { s += `<rect x="6" y="${22 + i * 3}" width="52" height="3" fill="${c}"/>`; });
      s += `<rect x="16" y="23" width="32" height="12" rx="2" fill="#151518"/>` + reel(23, 29, 5, 3.4) + reel(41, 29, 4, 3.4);
      s += `<path d="M 15 52 L 18 44 H 46 L 49 52 Z" fill="#3E3D43"/>`;
      return s;
    }
    let s = `<defs><linearGradient id="tbody" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3A393F"/><stop offset="1" stop-color="#1F1E22"/></linearGradient>` +
      `<linearGradient id="tglass" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.18"/><stop offset="0.45" stop-color="#fff" stop-opacity="0.04"/><stop offset="0.5" stop-color="#fff" stop-opacity="0.16"/><stop offset="0.56" stop-color="#fff" stop-opacity="0.02"/></linearGradient>` +
      `<filter id="tsh" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="7"/></filter></defs>`;
    s += `<rect x="56" y="44" width="380" height="240" rx="14" fill="#000" opacity="0.35" filter="url(#tsh)"/>`;
    s += `<rect x="50" y="32" width="380" height="240" rx="14" fill="url(#tbody)"/><rect x="51" y="33" width="378" height="238" rx="13" fill="none" stroke="#fff" stroke-opacity="0.14" stroke-width="2"/>`;
    // The label.
    s += `<rect x="76" y="44" width="328" height="164" rx="8" fill="#F3EBD8"/>`;
    TAPE_STRIPES.forEach((c, i) => { s += `<rect x="76" y="${110 + i * 11}" width="328" height="11" fill="${c}"/>`; });
    s += OG.text(w, { x: 94, y: 98, size: 54, family: "'Oswald', sans-serif", weight: 700, fill: '#1F1E22' });
    s += OG.text('C-90', { x: 388, y: 96, size: 32, family: "'Oswald', sans-serif", weight: 500, fill: '#1F1E22', anchor: 'end' });
    // The side A box.
    s += `<rect x="94" y="122" width="34" height="34" rx="2" fill="#1F1E22"/>` + OG.text('A', { x: 111, y: 151, size: 30, family: "'Oswald', sans-serif", weight: 700, fill: '#F3EBD8', anchor: 'middle' });
    // The window, the tape and the reels.
    s += `<rect x="148" y="114" width="184" height="66" rx="9" fill="#151518" stroke="#1F1E22" stroke-width="3"/>`;
    s += reel(190, 147, 29, 12) + reel(290, 147, 19, 12);
    s += `<rect x="148" y="114" width="184" height="66" rx="9" fill="url(#tglass)"/>`;
    s += OG.text('SIDE A · 2 × 45 MIN · NORMAL BIAS', { x: 240, y: 199, size: 10.5, family: "'Oswald', sans-serif", weight: 500, fill: '#5A5148', anchor: 'middle', ls: 1.6 });
    // The head opening at the bottom, with its holes.
    s += `<path d="M 116 272 L 132 222 H 348 L 364 272 Z" fill="#333237"/><path d="M 132 222 H 348" stroke="#fff" stroke-opacity="0.12" stroke-width="1.5"/>`;
    s += `<circle cx="166" cy="250" r="8" fill="#0C0C0E"/><circle cx="314" cy="250" r="8" fill="#0C0C0E"/>`;
    s += `<rect x="140" y="256" width="9" height="9" fill="#0C0C0E"/><rect x="331" y="256" width="9" height="9" fill="#0C0C0E"/><rect x="220" y="252" width="40" height="16" rx="2" fill="#0C0C0E"/>`;
    // Screws.
    [[64, 46], [416, 46], [64, 258], [416, 258], [240, 236]].forEach(([x, y]) => {
      s += `<circle cx="${x}" cy="${y}" r="5" fill="#55545A"/><path d="M ${x - 3} ${y} H ${x + 3} M ${x} ${y - 3} V ${y + 3}" stroke="#1A1A1D" stroke-width="1.4"/>`;
    });
    return s;
  }

  OG.add({
    num: 74,
    id: 'cassette',
    name: 'Cassette',
    caps: 'OgOs',
    flexCase: true,
    family: 'Object',
    sources: ['Home computers', 'Print & packaging'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    bg: '#6FA39A',
    ink: '#FFFFFF',
    palette: ['#2C2B30', '#F3EBD8', '#F6C343', '#F08A24', '#D94A2A', '#8C2F45'],
    fonts: ['Oswald:wght@500;700'],
    type: 'Oswald Bold, the condensed print of a tape label, with the C-90 and side marks in the same family; the cassette is drawn.',
    concept:
      'A C-90 compact cassette with a sunset-striped label, OgOs printed where the band name would go. ' +
      'Home computers loaded their programs from tape, screeching for minutes, and this is the warm, analog end of retro computing.',
    logo: (w) => OG.logoSvg(cassette(w, false)),
    icon: () => OG.iconSvg(cassette('', true)),
  });

  /* ----------------------------------------------- 79. Punch Card ------ */

  // A 5x7 face for dot-printed text: keypunch interpretation, dot-matrix
  // print and terminal banners. Caps on rows 0-6, descenders on 7-8.
  const DOT5 = {
    O: ['.###.', '#...#', '#...#', '#...#', '#...#', '#...#', '.###.'],
    G: ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.'],
    S: ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
    o: ['', '', '.###.', '#...#', '#...#', '#...#', '.###.'],
    g: ['', '', '.####', '#...#', '#...#', '#...#', '.####', '....#', '.###.'],
    s: ['', '', '.####', '#....', '.###.', '....#', '####.'],
    '/': ['....#', '....#', '...#.', '..#..', '.#...', '#....', '#....'],
    '-': ['', '', '', '#####'],
    '0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
    '1': ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'],
    '2': ['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'],
    '3': ['####.', '....#', '....#', '.###.', '....#', '....#', '####.'],
  };
  // The Hollerith rows each character punches: 12, 11, 0 to 9.
  const HOLLERITH = { O: [11, 6], G: [12, 7], S: [0, 2], '-': [11], '0': [0], '1': [1], '2': [2], '3': [3] };
  const CARD_BG = '#1E2A33', CARD = '#F1E6C9', CARD_INK = '#6E5B48', CARD_RED = '#B3261E';

  function punchCard(x, y, W, opts = {}) {
    const k = W / 7.375, H = 3.25 * k, pitch = 0.087 * k, left = 0.25 * k, cut = 0.25 * k;
    const colX = (c) => x + left + pitch * (c - 0.5);
    const rowY = (r) => y + (r === 12 ? 0.25 : r === 11 ? 0.5 : 0.75 + r * 0.25) * k;
    const punched = {};
    const text = opts.text || '';
    [...text].forEach((ch, i) => { punched[opts.col + i] = HOLLERITH[ch] || []; });
    [...(opts.seq || '')].forEach((ch, i) => { punched[73 + i] = HOLLERITH[ch] || []; });
    let s = `<path d="M ${f2(x + cut)} ${y} H ${f2(x + W)} V ${f2(y + H)} H ${x} V ${f2(y + cut * 1.6)} Z" fill="${CARD}"/>`;
    // The printed digit rows, one <text> a row, skipping punched spots and
    // the printed name field.
    const field = opts.field || [0, 0];
    for (let r = 0; r <= 9; r++) {
      const xs = [], chs = [];
      for (let c = 1; c <= 80; c++) {
        if ((punched[c] || []).includes(r)) continue;
        if (c >= field[0] && c <= field[1] && r <= 6) continue;
        xs.push(f2(colX(c))); chs.push(r);
      }
      s += `<text x="${xs.join(' ')}" y="${f2(rowY(r) + pitch * 0.6)}" font-size="${f2(pitch * 1.15)}" font-family="'Share Tech Mono', monospace" fill="${CARD_INK}" text-anchor="middle">${chs.join('')}</text>`;
    }
    // Column numbers under rows 0 and 9.
    [0, 9].forEach((r) => {
      const xs = [], chs = [];
      for (let c = 1; c <= 80; c++) {
        if (c >= field[0] && c <= field[1] && r === 0) continue;
        const str = String(c), dw = pitch * 0.36;
        [...str].forEach((d, i) => { xs.push(f2(colX(c) + (i - (str.length - 1) / 2) * dw)); chs.push(d); });
      }
      s += `<text x="${xs.join(' ')}" y="${f2(rowY(r) + pitch * 1.45)}" font-size="${f2(pitch * 0.55)}" font-family="'Share Tech Mono', monospace" fill="${CARD_INK}" opacity="0.8" text-anchor="middle">${chs.join('')}</text>`;
    });
    // The holes, and the characters interpreted along the top edge.
    let holes = '', dots = '';
    Object.keys(punched).forEach((c) => {
      punched[c].forEach((r) => {
        holes += `<rect x="${f2(colX(+c) - pitch * 0.32)}" y="${f2(rowY(r) - pitch * 0.72)}" width="${f2(pitch * 0.64)}" height="${f2(pitch * 1.44)}" fill="${opts.hole || CARD_BG}"/>`;
      });
    });
    const interp = (str, c0) => [...str].forEach((ch, i) => {
      const g = DOT5[ch];
      if (!g) return;
      const d = pitch * 0.17;
      g.forEach((row, j) => [...row].forEach((v, m) => {
        if (v === '#') dots += `<circle cx="${f2(colX(c0 + i) + (m - 2) * d)}" cy="${f2(y + pitch * 0.55 + j * d)}" r="${f2(d * 0.5)}"/>`;
      }));
    });
    interp(text, opts.col);
    interp(opts.seq || '', 73);
    return s + holes + `<g fill="#2B2420">${dots}</g>`;
  }

  OG.add({
    num: 79,
    id: 'punch-card',
    name: 'Punch Card',
    caps: 'OGOS',
    flexCase: false,
    family: 'Experimental',
    sources: ['IBM & OS/2'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    bg: CARD_BG,
    ink: CARD,
    palette: [CARD, CARD_RED, CARD_INK, CARD_BG],
    fonts: ['IBM Plex Sans Condensed:wght@700', 'Share Tech Mono'],
    type: 'IBM Plex Sans Condensed Bold in red for the card\'s printed field; the keypunch\'s own dot letters along the top edge; Share Tech Mono for the digit rows.',
    concept:
      'An 80-column punch card with OGOS-32 punched in real Hollerith code (O is 11-6, G is 12-7, S is 0-2) and interpreted along its top edge, plus a sequence number in columns 73 to 80. ' +
      'Before screens, this is what a program was: the deepest root of the machines OgOs pays homage to.',
    logo: () => {
      const x = 20, y = 54, W = 440;
      return OG.logoSvg(
        `<defs><filter id="pcs" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="6"/></filter></defs>` +
          `<rect x="${x + 6}" y="${y + 10}" width="${W}" height="${f2((W / 7.375) * 3.25)}" fill="#000" opacity="0.4" filter="url(#pcs)"/>` +
          punchCard(x, y, W, { text: 'OGOS-32', col: 1, seq: '00000010', field: [14, 66] }) +
          `<rect x="104" y="73" width="272" height="122" fill="none" stroke="${CARD_RED}" stroke-width="1.5"/>` +
          OG.text('PROGRAM', { x: 112, y: 86, size: 9, family: "'Share Tech Mono', monospace", fill: CARD_RED, ls: 2 }) +
          OG.text('OGOS', { x: 240, y: 180, size: 104, family: "'IBM Plex Sans Condensed', sans-serif", weight: 700, fill: CARD_RED, anchor: 'middle', ls: 1 })
      );
    },
    icon: () => {
      // Columns O G O S, rows 12, 11 and 0 to 7 of a card's left end.
      const cols = [[11, 6], [12, 7], [11, 6], [0, 2]];
      const ry = (r) => (r === 12 ? 9 : r === 11 ? 14.5 : 20 + r * 5);
      let s = `<path d="M 10 2 H 62 V 62 H 2 V 12 Z" fill="${CARD}"/>`;
      for (let c = 0; c < 5; c++) for (let r = 0; r <= 7; r++) s += `<rect x="${13 + c * 10}" y="${ry(r) + 1}" width="2" height="2" fill="${CARD_INK}" opacity="0.45"/>`;
      cols.forEach((rs, c) => rs.forEach((r) => { s += `<rect x="${11 + c * 10}" y="${ry(r) - 1.5}" width="6" height="${r >= 11 ? 6 : 7}" fill="${CARD_BG}"/>`; }));
      return OG.iconSvg(`<rect width="64" height="64" rx="6" fill="${CARD_BG}"/>` + s, { crisp: true });
    },
  });

  /* ---------------------------------------------------- 84. Turbo ------ */

  // Seven-segment digits: each letter lights a set of segments a-g.
  const SEG = { O: 'abcdef', G: 'acdef', S: 'acdfg', '8': 'abcdefg' };
  function sevenSeg(ch, x, y, w, h, t, on, off) {
    const lit = SEG[ch] || '';
    const g = Math.max(1, t * 0.14);
    const hz = (x0, x1, yy) => `M ${f2(x0 + g)} ${f2(yy)} L ${f2(x0 + g + t / 2)} ${f2(yy - t / 2)} L ${f2(x1 - g - t / 2)} ${f2(yy - t / 2)} L ${f2(x1 - g)} ${f2(yy)} L ${f2(x1 - g - t / 2)} ${f2(yy + t / 2)} L ${f2(x0 + g + t / 2)} ${f2(yy + t / 2)} Z`;
    const vt = (xx, y0, y1) => `M ${f2(xx)} ${f2(y0 + g)} L ${f2(xx + t / 2)} ${f2(y0 + g + t / 2)} L ${f2(xx + t / 2)} ${f2(y1 - g - t / 2)} L ${f2(xx)} ${f2(y1 - g)} L ${f2(xx - t / 2)} ${f2(y1 - g - t / 2)} L ${f2(xx - t / 2)} ${f2(y0 + g + t / 2)} Z`;
    const m = y + h / 2, L = x + t / 2, R = x + w - t / 2, T = y + t / 2, B = y + h - t / 2;
    const segs = { a: hz(L, R, T), b: vt(R, T, m), c: vt(R, m, B), d: hz(L, R, B), e: vt(L, m, B), f: vt(L, T, m), g: hz(L, R, m) };
    let a = '', b = '';
    Object.keys(segs).forEach((k) => { if (lit.includes(k)) a += segs[k] + ' '; else b += segs[k] + ' '; });
    return { on: `<path d="${a}" fill="${on}"/>`, off: `<path d="${b}" fill="${off}"/>` };
  }
  const LED_ON = '#FF2B12', LED_OFF = '#3E0B07';

  function readout(chars, x, y, w, h, t, gapIn, gapOut) {
    let on = '', off = '', cx = x;
    chars.forEach((ch, i) => {
      if (ch === ' ') { cx += gapOut - gapIn; return; }
      const d = sevenSeg(ch, cx, y, w, h, t, LED_ON, LED_OFF);
      on += d.on; off += d.off; cx += w + gapIn;
      void i;
    });
    return `<g transform="translate(0 ${y + h}) skewX(-7) translate(0 ${-(y + h)})">${off}<g filter="url(#tglow)">${on}</g></g>`;
  }
  const TURBO_DEFS =
    `<filter id="tglow" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="3.2" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>` +
    `<linearGradient id="tgl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.09"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>` +
    `<radialGradient id="tled" cx="0.4" cy="0.35" r="0.65"><stop offset="0" stop-color="#fff" stop-opacity="0.9"/><stop offset="0.35" stop-color="#fff" stop-opacity="0"/></radialGradient>`;

  OG.add({
    num: 84,
    id: 'turbo',
    name: 'Turbo',
    caps: 'OG OS',
    flexCase: false,
    family: 'Object',
    sources: ['DOS & BIOS'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    bg: '#2E3338',
    ink: '#E3DCC8',
    palette: ['#E3DCC8', '#C9BFA6', LED_ON, '#2BD15A', '#2E3338'],
    fonts: ['Kode Mono:wght@700'],
    type: 'Seven-segment digits drawn as hexagonal bars and slanted, so the S reads as a 5; the name on the bezel in Kode Mono Bold.',
    concept:
      'The front of a beige 1990s tower, where a red LED readout showed the clock speed and a TURBO button doubled it. ' +
      'Here the readout says OG OS, the original OS, and the charm is that the S can only ever be a 5.',
    logo: () => {
      const x = 28, y = 70, W = 424, H = 160;
      let s = `<defs>${TURBO_DEFS}<linearGradient id="tbz" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ECE5D2"/><stop offset="1" stop-color="#CBC1A8"/></linearGradient>` +
        `<filter id="tbsh" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="7"/></filter></defs>`;
      s += `<rect x="${x + 4}" y="${y + 12}" width="${W}" height="${H}" rx="12" fill="#000" opacity="0.45" filter="url(#tbsh)"/>`;
      s += `<rect x="${x}" y="${y}" width="${W}" height="${H}" rx="12" fill="url(#tbz)"/><rect x="${x + 1}" y="${y + 1}" width="${W - 2}" height="${H - 2}" rx="11" fill="none" stroke="#fff" stroke-opacity="0.6" stroke-width="1.5"/>`;
      // Vent grooves along the bottom edge.
      for (let i = 0; i < 9; i++) s += `<rect x="${x + 22 + i * 14}" y="${y + H - 18}" width="8" height="5" rx="2" fill="#A89E86"/>`;
      // The display window.
      s += `<rect x="48" y="96" width="232" height="104" rx="8" fill="#1A0605"/><rect x="48" y="96" width="232" height="104" rx="8" fill="none" stroke="#9E937A" stroke-width="3"/>`;
      s += readout(['O', 'G', ' ', 'O', 'S'], 58, 110, 41, 76, 9.5, 9, 30);
      s += `<rect x="51" y="99" width="226" height="50" rx="6" fill="url(#tgl)"/>`;
      // The name on the bezel, the LEDs and the buttons.
      s += OG.text('OG OS', { x: 300, y: 112, size: 22, family: "'Kode Mono', monospace", weight: 700, fill: '#4A453B', ls: 1 });
      s += OG.text('32', { x: 408, y: 112, size: 13, family: "'Kode Mono', monospace", weight: 700, fill: '#B23A2A' });
      [['POWER', '#2BD15A', 312], ['TURBO', '#FFC21A', 362], ['HDD', '#FF4A2A', 412]].forEach(([l, c, lx]) => {
        s += `<circle cx="${lx}" cy="134" r="6" fill="${c}" filter="url(#tglow)"/><circle cx="${lx}" cy="134" r="6" fill="url(#tled)"/>` +
          OG.text(l, { x: lx, y: 154, size: 9, family: "'Kode Mono', monospace", weight: 700, fill: '#6B6455', anchor: 'middle', ls: 0.5 });
      });
      s += `<rect x="300" y="166" width="92" height="30" rx="4" fill="#B9AF95"/><rect x="302" y="167" width="88" height="25" rx="3" fill="#E6DFCB"/>` +
        OG.text('TURBO', { x: 346, y: 185, size: 13, family: "'Kode Mono', monospace", weight: 700, fill: '#4A453B', anchor: 'middle', ls: 2 });
      s += `<rect x="400" y="166" width="32" height="30" rx="4" fill="#B9AF95"/><rect x="402" y="167" width="28" height="25" rx="3" fill="#DDD5BF"/>` +
        OG.text('RST', { x: 416, y: 184, size: 9, family: "'Kode Mono', monospace", weight: 700, fill: '#6B6455', anchor: 'middle' });
      return OG.logoSvg(s);
    },
    icon: () =>
      OG.iconSvg(
        `<defs>${TURBO_DEFS}</defs><rect x="2" y="6" width="60" height="52" rx="6" fill="#1A0605" stroke="#C9BFA6" stroke-width="3"/>` +
          readout(['O', 'G'], 7, 13, 22, 38, 7, 6, 0)
      ),
  });

  /* ----------------------------------------------- 89. Dot Matrix ------ */

  // Prints bitmap rows as round dots, struck twice a little apart the way
  // emphasized mode did, with a little ink variation.
  function dotPrint(rows, x, y, p, r, seed, ink, single) {
    const rnd = OG.rng(seed);
    let s = '';
    rows.forEach((row, j) => [...row].forEach((v, i) => {
      if (v !== '#') return;
      const cx = x + i * p, cy = y + j * p, o = 0.78 + rnd() * 0.2;
      s += `<circle cx="${f2(cx)}" cy="${f2(cy)}" r="${f2(r)}" opacity="${f2(o)}"/>`;
      if (!single) s += `<circle cx="${f2(cx + p * 0.24)}" cy="${f2(cy + rnd() * 0.4)}" r="${f2(r)}" opacity="${f2(o * 0.55)}"/>`;
    }));
    return `<g fill="${ink}" filter="url(#dmink)">${s}</g>`;
  }
  const DM_DEFS =
    `<filter id="dmink" x="-5%" y="-5%" width="110%" height="110%"><feTurbulence type="fractalNoise" baseFrequency="0.8" numOctaves="1" seed="3" result="n"/><feDisplacementMap in="SourceGraphic" in2="n" scale="1.6"/><feGaussianBlur stdDeviation="0.35"/></filter>` +
    `<filter id="dmsh" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="6"/></filter>`;
  const DM_GREEN = '#D3E8CF', DM_INK = '#232733';

  function tractorPaper(x0, x1, y0, y1, band) {
    let s = `<rect x="${x0}" y="${y0}" width="${x1 - x0}" height="${y1 - y0}" fill="#FBFAF4"/>`;
    for (let y = y0 + band.offset; y < y1; y += band.h * 2) s += `<rect x="${x0 + band.strip}" y="${f2(y)}" width="${x1 - x0 - 2 * band.strip}" height="${band.h}" fill="${DM_GREEN}"/>`;
    [x0 + band.strip, x1 - band.strip].forEach((lx) => { s += `<path d="M ${lx} ${y0} V ${y1}" stroke="#B8B4A6" stroke-width="1" stroke-dasharray="3 3"/>`; });
    for (let y = y0 + band.hole0; y < y1; y += band.pitch) {
      [x0 + band.strip / 2, x1 - band.strip / 2].forEach((hx) => { s += `<circle cx="${hx}" cy="${f2(y)}" r="${band.r}" fill="${band.holeFill}"/>`; });
    }
    return s;
  }

  OG.add({
    num: 89,
    id: 'dot-matrix',
    name: 'Dot Matrix',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['Print & packaging'],
    paletteTag: 'Pastel',
    risk: 'Bold',
    bg: '#353B42',
    ink: '#FBFAF4',
    palette: ['#FBFAF4', DM_GREEN, DM_INK, '#353B42'],
    fonts: [],
    type: 'Letters on a 5x7 grid of round dots, struck twice slightly apart as emphasized print, with a rough ink edge; the report lines are the system font printed the same way.',
    concept:
      'A listing on green-bar tractor-feed paper, the name hammered out by a 9-pin printer. ' +
      'It is the sound and smell of the home office in 1990, and a reminder that OgOs is a system you make things with and print them out.',
    logo: (w) => {
      const band = { strip: 26, h: 30, offset: 14, hole0: 10, pitch: 24, r: 5, holeFill: '#353B42' };
      const rows = prop(w, DOT5, 9, 2, 3), p = 12.5;
      const tw = (rows[0].length - 1) * p, x = 240 - tw / 2;
      const small = (t, sx, sy) => dotPrint(OG.textRows(t, {}), sx, sy, 1.6, 0.85, 7, DM_INK, true);
      return OG.logoSvg(
        `<defs>${DM_DEFS}</defs><rect x="36" y="0" width="420" height="300" fill="#000" opacity="0.35" filter="url(#dmsh)"/>` +
          tractorPaper(30, 450, -10, 310, band) +
          `<path d="M 56 56 H 424" stroke="#B8B4A6" stroke-width="1" stroke-dasharray="2 3"/>` +
          small('OGOS-32 LISTING', 70, 76) + small('PAGE 001', 410 - 8 * 12.8, 76) +
          dotPrint(rows, x, 114, p, 4.5, 11, DM_INK) +
          small('10/27/26  12:00   READY.', 70, 236)
      );
    },
    icon: () => {
      const band = { strip: 13, h: 12, offset: 8, hole0: 8, pitch: 16, r: 3.4, holeFill: '#353B42' };
      return OG.iconSvg(`<defs>${DM_DEFS}</defs>` + tractorPaper(0, 64, 0, 64, band) + dotPrint(DOT5.O, 22, 16, 5, 2.4, 5, DM_INK));
    },
  });

  /* ----------------------------------------------------- 94. 3270 ------ */

  const P3270 = { hi: '#7DFF9E', mid: '#33E06A', lo: '#1D8A42', bg: '#03100A' };

  // Characters set in a monospace face at fixed columns, so spaces never
  // collapse: one <text> a row, each character placed by an x list.
  function monoLine(str, x, y, cw, size, fill, extra = '') {
    const xs = [], cs = [];
    [...str].forEach((ch, i) => { if (ch !== ' ') { xs.push(f2(x + i * cw)); cs.push(OG.esc(ch)); } });
    return xs.length ? `<text x="${xs.join(' ')}" y="${y}" font-size="${size}" font-family="'IBM Plex Mono', monospace" fill="${fill}" ${extra}>${cs.join('')}</text>` : '';
  }

  OG.add({
    num: 94,
    id: 'mainframe-3270',
    name: '3270',
    caps: 'OGOS',
    flexCase: true,
    family: 'Interface',
    sources: ['IBM & OS/2'],
    paletteTag: 'Phosphor',
    risk: 'Safe',
    scene: true,
    bg: P3270.bg,
    ink: P3270.hi,
    palette: [P3270.bg, P3270.hi, P3270.mid, P3270.lo],
    fonts: ['IBM Plex Mono:wght@500;700'],
    type: 'IBM Plex Mono throughout; the big OGOS/32 is a banner made of its own letters, each lit cell of a 5x7 grid printed as the letter it belongs to.',
    concept:
      'A green-phosphor mainframe logon screen: a character banner, input fields waiting with underscores, and the status line along the bottom. ' +
      'Serious, quiet and glowing, the big-iron ancestor of every terminal OgOs draws.',
    logo: (w) => {
      const cw = 7.8, fs = 13, name = w + '/32';
      const rows = prop(name, DOT5, 9, 1, 3);
      const bx = 240 - (rows[0].length * cw) / 2;
      // Which letter each banner column belongs to, so the cell prints it.
      const owner = [];
      [...name].forEach((ch) => {
        const g = DOT5[ch] || [];
        let wdt = 0;
        g.forEach((r) => { wdt = Math.max(wdt, r.length); });
        for (let i = 0; i < wdt; i++) owner.push(ch);
        owner.push(' ');
      });
      let banner = '';
      rows.forEach((r, j) => {
        const line = [...r].map((v, i) => (v === '#' ? owner[i] : ' ')).join('');
        banner += monoLine(line, bx, 70 + j * 12.5, cw, fs, P3270.hi, 'font-weight="700"');
      });
      let s = `<defs><radialGradient id="crt" cx="0.5" cy="0.5" r="0.75"><stop offset="0.55" stop-color="${P3270.bg}"/><stop offset="1" stop-color="#000"/></radialGradient>` +
        `<pattern id="scan" width="4" height="3" patternUnits="userSpaceOnUse"><rect width="4" height="1" fill="#000" opacity="0.45"/></pattern>` +
        `<filter id="glow" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="1.8" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>` +
        `<rect width="480" height="300" fill="url(#crt)"/><g filter="url(#glow)">`;
      s += monoLine('OGOS/32 LOGON', 24, 34, cw, fs, P3270.mid) + monoLine('TERMINAL L042', 456 - 13 * cw, 34, cw, fs, P3270.mid);
      s += banner;
      s += monoLine('USERID   ===>', 72, 196, cw, fs, P3270.mid) + monoLine('________', 72 + 14 * cw, 196, cw, fs, P3270.hi);
      s += monoLine('PASSWORD ===>', 72, 216, cw, fs, P3270.mid);
      s += monoLine('COMMAND  ===>', 72, 236, cw, fs, P3270.mid) + `<rect x="${f2(72 + 14 * cw)}" y="226" width="${cw}" height="13" fill="${P3270.hi}"/>`;
      s += `<rect x="24" y="254" width="432" height="1" fill="${P3270.lo}"/>`;
      s += `<rect x="24" y="261" width="${f2(2 * cw + 4)}" height="15" fill="${P3270.mid}"/>` + monoLine('4A', 26, 273, cw, fs, P3270.bg, 'font-weight="700"');
      s += monoLine('X SYSTEM', 64, 273, cw, fs, P3270.lo) + monoLine('001/012', 456 - 7 * cw, 273, cw, fs, P3270.mid);
      s += `</g><rect width="480" height="300" fill="url(#scan)"/>`;
      return OG.logoSvg(s);
    },
    icon: () =>
      OG.iconSvg(
        `<defs><filter id="ig" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="2" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>` +
          `<rect width="64" height="64" rx="8" fill="${P3270.bg}"/>` +
          OG.text('OG', { x: 32, y: 43, size: 30, family: "'IBM Plex Mono', monospace", weight: 700, fill: P3270.hi, anchor: 'middle', attrs: 'filter="url(#ig)"' }) +
          `<rect x="13" y="49" width="38" height="3" fill="${P3270.mid}"/>`
      ),
  });

  /* ---------------------------------------------------- 99. Nixie ------ */

  // Cathode shapes, as monoline paths in a box 56 wide and 132 tall.
  function nixieGlyph(ch) {
    const E = (cx, cy, rx, ry) => `M ${cx - rx} ${cy} A ${rx} ${ry} 0 1 0 ${cx + rx} ${cy} A ${rx} ${ry} 0 1 0 ${cx - rx} ${cy} Z`;
    const P = (cx, cy, rx, ry, d) => pt([cx + rx * Math.cos(rad(d)), cy + ry * Math.sin(rad(d))]);
    const S = (top, bot, b) => {
      const a = (bot - top) / 4, yt = top + a, yb = bot - a;
      return `M ${P(28, yt, b, a, -32)} A ${b} ${f2(a)} 0 1 0 28 ${f2(yt + a)} A ${b} ${f2(a)} 0 1 1 ${P(28, yb, b, a, 148)}`;
    };
    switch (ch) {
      case 'O': return E(28, 66, 25, 62);
      case 'o': return E(28, 94, 24, 34);
      case 'G': return `M ${P(28, 66, 25, 62, -38)} A 25 62 0 1 0 ${P(28, 66, 25, 62, 6)} H 30`;
      case 'g': return E(26, 84, 21, 28) + ` M 47 56 V 112 Q 47 128 30 128 H 12`;
      case 'S': return S(4, 128, 23);
      case 's': return S(60, 128, 21);
      default: return '';
    }
  }
  // The unlit cathodes stacked behind: an 8, a 0 and a 1.
  const NIXIE_BACK = 'M 28 4 A 21 30 0 1 0 28 64 A 21 30 0 1 0 28 4 Z M 28 64 A 24 32 0 1 0 28 128 A 24 32 0 1 0 28 64 Z M 6 66 A 22 62 0 1 0 50 66 A 22 62 0 1 0 6 66 Z M 28 4 V 128';

  function nixieTube(x, y, ch, id) {
    const W = 84, H = 190;
    let s = '';
    // Glass envelope: a dark interior, the lit cathode, then the glass.
    const body = `M ${x} ${y + 34} A ${W / 2} 34 0 0 1 ${x + W} ${y + 34} V ${y + H} H ${x} Z`;
    s += `<path d="${body}" fill="url(#nxin)"/>`;
    s += `<ellipse cx="${x + W / 2}" cy="${y + 100}" rx="40" ry="70" fill="url(#nxwarm)"/>`;
    s += `<g transform="translate(${x + 14} ${y + 42})" fill="none" stroke-linecap="round" stroke-linejoin="round">` +
      `<path d="${NIXIE_BACK}" stroke="#6B5544" stroke-width="1.6" opacity="0.55" transform="translate(1.5 -1.5) scale(0.98)"/>` +
      `<g filter="url(#nxglow)"><path d="${nixieGlyph(ch)}" stroke="#FF7A1F" stroke-width="5"/><path d="${nixieGlyph(ch)}" stroke="#FFE2B8" stroke-width="1.8"/></g></g>`;
    s += `<path d="${body}" fill="url(#nxmesh)" opacity="0.5"/>`;
    s += `<path d="${body}" fill="url(#nxglass)"/>`;
    s += `<path d="M ${x + 12} ${y + 40} Q ${x + 14} ${y + 16} ${x + 34} ${y + 8}" fill="none" stroke="#fff" stroke-opacity="0.55" stroke-width="3" stroke-linecap="round"/>`;
    s += `<path d="M ${x + 13} ${y + 52} V ${y + H - 12}" stroke="#fff" stroke-opacity="0.18" stroke-width="5" stroke-linecap="round"/>`;
    s += `<path d="${body}" fill="none" stroke="#fff" stroke-opacity="0.3" stroke-width="1.5"/>`;
    s += `<path d="M ${x + W / 2 - 4} ${y + 1} Q ${x + W / 2} ${y - 9} ${x + W / 2 + 4} ${y + 1}" fill="#C9D3DC" opacity="0.6"/>`;
    // The socket.
    s += `<rect x="${x - 4}" y="${y + H - 2}" width="${W + 8}" height="26" rx="5" fill="url(#nxbase)"/><rect x="${x - 4}" y="${y + H - 2}" width="${W + 8}" height="4" fill="#000" opacity="0.5"/>`;
    void id;
    return s;
  }
  const NIXIE_DEFS =
    `<linearGradient id="nxin" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#241A14"/><stop offset="1" stop-color="#120D0A"/></linearGradient>` +
    `<radialGradient id="nxwarm" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#FF6A10" stop-opacity="0.32"/><stop offset="1" stop-color="#FF6A10" stop-opacity="0"/></radialGradient>` +
    `<linearGradient id="nxglass" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#fff" stop-opacity="0.16"/><stop offset="0.25" stop-color="#fff" stop-opacity="0.02"/><stop offset="0.8" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#fff" stop-opacity="0.14"/></linearGradient>` +
    `<linearGradient id="nxbase" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3A3430"/><stop offset="1" stop-color="#16120F"/></linearGradient>` +
    `<pattern id="nxmesh" width="7" height="12" patternUnits="userSpaceOnUse"><path d="M 0 0 L 3.5 2 L 3.5 6 L 0 8 M 3.5 6 L 7 8 M 7 0 L 3.5 2 M 0 8 V 12 M 7 8 V 12" fill="none" stroke="#000" stroke-width="0.8"/></pattern>` +
    `<filter id="nxglow" x="-40%" y="-30%" width="180%" height="160%"><feGaussianBlur stdDeviation="4" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>`;

  OG.add({
    num: 99,
    id: 'nixie',
    name: 'Nixie',
    caps: 'OgOS',
    flexCase: true,
    family: 'Object',
    sources: ['Beyond computing'],
    paletteTag: 'Neon',
    risk: 'Bold',
    bg: '#0E0C0B',
    ink: '#FFB060',
    palette: ['#0E0C0B', '#FF7A1F', '#FFE2B8', '#6B5544', '#3A3430'],
    fonts: [],
    type: 'Cathodes bent as monoline wire letters, drawn as paths and glowing, with the unlit 8, 0 and 1 cathodes faint behind them.',
    concept:
      'Four nixie tubes, the glowing neon readouts of 1960s instruments, each lighting one letter of the name inside its glass. ' +
      'Warm, physical and a little magical, it is the oldest display technology in the set and the softest light.',
    logo: (w) => {
      const n = w.length, W = 84, gap = 14, x0 = 240 - (n * W + (n - 1) * gap) / 2;
      let s = `<defs>${NIXIE_DEFS}<radialGradient id="nxfloor" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#FF6A10" stop-opacity="0.22"/><stop offset="1" stop-color="#FF6A10" stop-opacity="0"/></radialGradient></defs>`;
      s += `<ellipse cx="240" cy="262" rx="230" ry="22" fill="url(#nxfloor)"/>`;
      s += `<rect x="${x0 - 18}" y="252" width="${n * W + (n - 1) * gap + 36}" height="16" rx="4" fill="#211B17"/><rect x="${x0 - 18}" y="252" width="${n * W + (n - 1) * gap + 36}" height="2" fill="#5A4A3E"/>`;
      [...w].forEach((ch, i) => { s += nixieTube(x0 + i * (W + gap), 40, ch, i); });
      return OG.logoSvg(s);
    },
    icon: () => {
      const body = 'M 10 22 A 22 18 0 0 1 54 22 V 56 Q 54 62 48 62 H 16 Q 10 62 10 56 Z';
      return OG.iconSvg(
        `<defs>${NIXIE_DEFS}</defs><path d="${body}" fill="url(#nxin)"/><ellipse cx="32" cy="38" rx="20" ry="22" fill="url(#nxwarm)"/>` +
          `<g fill="none" filter="url(#nxglow)"><ellipse cx="32" cy="38" rx="13" ry="18" stroke="#FF7A1F" stroke-width="4.5"/><ellipse cx="32" cy="38" rx="13" ry="18" stroke="#FFE2B8" stroke-width="1.6"/></g>` +
          `<path d="${body}" fill="url(#nxglass)"/><path d="M 16 24 Q 18 12 30 8" fill="none" stroke="#fff" stroke-opacity="0.6" stroke-width="2.5" stroke-linecap="round"/>` +
          `<path d="${body}" fill="none" stroke="#fff" stroke-opacity="0.35" stroke-width="1.5"/>`
      );
    },
  });
})();
