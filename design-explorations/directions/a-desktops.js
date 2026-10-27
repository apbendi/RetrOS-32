/* Batch A: Desktops & interfaces. Directions 3, 8, 13 ... 98. */
(function () {
  /* ------------------------------------------------------------ helpers */

  // Advance widths at 100 px, measured in Chrome, so text can be sized and
  // fitted before the web font has loaded.
  const ADV = {
    nunito: { O: 80.7, G: 74.7, S: 65.1, o: 60.2, g: 62.6, s: 49.6 },
    chakra: { O: 70, G: 67.6, S: 63.7, o: 59.2, g: 60.2, s: 53.6 },
    hanken: { O: 74.7, G: 73.7, S: 57.9, o: 57.1, g: 52.5, s: 47.2 },
    corm: { O: 65.5, G: 63.3, S: 47, o: 40.8, g: 38.8, s: 31.6 },
    comfortaa: { O: 92.1, G: 77.7, S: 68.8, o: 67.5, g: 67.5, s: 54.3 },
    intertight: { O: 76, G: 73.8, S: 63.2, o: 59.1, g: 61.2, s: 54.7 },
    krona: { O: 100, G: 100, S: 100, o: 82.8, g: 83.3, s: 75 },
    saira: { O: 69.9, G: 66.4, S: 61.2, o: 57.2, g: 58.4, s: 50.1 },
    bai: { O: 72.4, G: 71.2, S: 65.6, o: 59.4, g: 60.6, s: 54.9 },
    jost: { O: 79, G: 77.5, S: 54.8, o: 54.6, g: 57, s: 40.3 },
    michroma: { O: 104.7, G: 106.3, S: 104.6, o: 78.2, g: 78.7, s: 77.6 },
    franklin: { O: 77.7, G: 75.7, S: 69, o: 60, g: 64.3, s: 53.5 },
    tiltwarp: { O: 73.6, G: 67.6, S: 53.8, o: 60, g: 59.6, s: 47.5 },
  };
  const tw = (font, str, size) => ([...str].reduce((a, c) => a + (ADV[font][c] || 60), 0) * size) / 100;
  // The font size at which str is at most maxW wide, but no bigger than size.
  const fit = (font, str, size, maxW) => Math.min(size, (maxW / tw(font, str, 100)) * 100);

  const rect = (x, y, w, h, fill, extra = '') =>
    `<rect x="${+x.toFixed(2)}" y="${+y.toFixed(2)}" width="${+w.toFixed(2)}" height="${+h.toFixed(2)}" fill="${fill}"${extra ? ' ' + extra : ''}/>`;

  // A bitmap alphabet: each glyph is an array of rows ('#' set), all `h` tall.
  // Returns the word's rows, glyphs joined by `gap` blank columns.
  function wordRows(font, word, gap = 1) {
    const rows = Array.from({ length: font.h }, () => '');
    [...word].forEach((ch, n) => {
      const g = font.g[ch] || font.g[ch.toUpperCase()] || font.g[ch.toLowerCase()];
      const wdt = g ? g[0].length : font.space || 4;
      for (let r = 0; r < font.h; r++) rows[r] += (n ? '.'.repeat(gap) : '') + (g ? g[r] : '.'.repeat(wdt));
    });
    return rows;
  }

  // Grows a bitmap by one cell all round and marks the cells next to set
  // cells with `mark` (8-neighbour outline). `set` is the set character(s).
  function outlineRows(rows, mark = 'k', set = '#', diag = true) {
    const H = rows.length + 2, W = rows[0].length + 2;
    const get = (i, j) => (j >= 1 && j <= rows.length && i >= 1 && i <= rows[0].length ? rows[j - 1][i - 1] : '.');
    const out = [];
    for (let j = 0; j < H; j++) {
      let s = '';
      for (let i = 0; i < W; i++) {
        const c = get(i, j);
        if (set.includes(c)) { s += c; continue; }
        let near = false;
        for (let dj = -1; dj <= 1 && !near; dj++)
          for (let di = -1; di <= 1; di++) {
            if (!diag && di && dj) continue;
            if (set.includes(get(i + di, j + dj))) { near = true; break; }
          }
        s += near ? mark : '.';
      }
      out.push(s);
    }
    return out;
  }

  // Like outlineRows, but only outside the letters: counters stay open.
  function outlineOuter(rows, mark = 'k', set = '#') {
    const H = rows.length + 2, W = rows[0].length + 2;
    const g = Array.from({ length: H }, (_, j) => Array.from({ length: W }, (_, i) =>
      j >= 1 && j <= rows.length && i >= 1 && i <= rows[0].length ? rows[j - 1][i - 1] : '.'));
    const ext = Array.from({ length: H }, () => Array(W).fill(false));
    const stack = [[0, 0]];
    while (stack.length) {
      const [i, j] = stack.pop();
      if (i < 0 || j < 0 || i >= W || j >= H || ext[j][i] || set.includes(g[j][i])) continue;
      ext[j][i] = true;
      stack.push([i + 1, j], [i - 1, j], [i, j + 1], [i, j - 1]);
    }
    return g.map((row, j) => row.map((c, i) => {
      if (set.includes(c) || !ext[j][i]) return c;
      for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) if (set.includes((g[j + dj] || [])[i + di] || '.')) return mark;
      return c;
    }).join(''));
  }

  // Classic two-step 3D bevel (Windows 9x): outer and inner light/dark edges.
  function bevel(x, y, w, h, u, lt, lt2, dk2, dk, face, pressed) {
    if (pressed) [lt, lt2, dk2, dk] = [dk, dk2, lt2, lt];
    let s = face ? rect(x, y, w, h, face) : '';
    s += rect(x, y, w, h, dk); // outer dark, then cover with the rest
    s += rect(x, y, w - u, h - u, lt);
    s += rect(x + u, y + u, w - 2 * u, h - 2 * u, dk2);
    s += rect(x + u, y + u, w - 3 * u, h - 3 * u, lt2);
    s += rect(x + 2 * u, y + 2 * u, w - 4 * u, h - 4 * u, face || '#C0C0C0');
    return s;
  }

  // Joins rows of a bitmap drawn with OG.bitmap. Lets art be written with a
  // template string: one row per line, leading spaces stripped.
  const art = (s) => s.trim().split('\n').map((r) => r.trim());

  /* ------------------------------------------------- bitmap alphabets */

  // A bold Windows-9x UI face: 9-pixel caps, 2-pixel stems.
  const W95 = {
    h: 11,
    g: {
      O: art(`
        .######.
        ##....##
        ##....##
        ##....##
        ##....##
        ##....##
        ##....##
        ##....##
        .######.
        ........
        ........`),
      G: art(`
        .######.
        ##....##
        ##......
        ##......
        ##..####
        ##....##
        ##....##
        ##....##
        .#######
        ........
        ........`),
      S: art(`
        .#####.
        ##...##
        ##.....
        ##.....
        .#####.
        .....##
        .....##
        ##...##
        .#####.
        .......
        .......`),
      o: art(`
        .......
        .......
        .......
        .#####.
        ##...##
        ##...##
        ##...##
        ##...##
        .#####.
        .......
        .......`),
      g: art(`
        .......
        .......
        .......
        .######
        ##...##
        ##...##
        ##...##
        ##...##
        .######
        .....##
        ######.`),
      s: art(`
        ......
        ......
        ......
        .#####
        ##....
        ###...
        .####.
        ...###
        #####.
        ......
        ......`),
    },
  };

  // A Chicago-like 1-bit system face: 10-pixel caps, 7-pixel x-height.
  const CHI = {
    h: 13,
    space: 4,
    g: {
      O: art(`
        ..####..
        .##..##.
        ##....##
        ##....##
        ##....##
        ##....##
        ##....##
        ##....##
        .##..##.
        ..####..
        ........
        ........
        ........`),
      G: art(`
        ..####..
        .##..##.
        ##....##
        ##......
        ##......
        ##..####
        ##....##
        ##....##
        .##..###
        ..###.##
        ........
        ........
        ........`),
      S: art(`
        .#####.
        ##...##
        ##.....
        ###....
        .####..
        ...###.
        ....###
        .....##
        ##...##
        .#####.
        .......
        .......
        .......`),
      R: art(`
        ######.
        ##...##
        ##...##
        ##...##
        ######.
        ####...
        ##.##..
        ##..##.
        ##...##
        ##...##
        .......
        .......
        .......`),
      o: art(`
        .......
        .......
        .......
        .#####.
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .#####.
        .......
        .......
        .......`),
      g: art(`
        .......
        .......
        .......
        .######
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .######
        .....##
        ##...##
        .#####.`),
      s: art(`
        .......
        .......
        .......
        .#####.
        ##...##
        ###....
        .#####.
        ....###
        ##...##
        .#####.
        .......
        .......
        .......`),
      a: art(`
        .......
        .......
        .......
        .#####.
        .....##
        .######
        ##...##
        ##...##
        ##..###
        .###.##
        .......
        .......
        .......`),
      c: art(`
        ......
        ......
        ......
        .####.
        ##..##
        ##....
        ##....
        ##....
        ##..##
        .####.
        ......
        ......
        ......`),
      d: art(`
        .....##
        .....##
        .....##
        .######
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .######
        .......
        .......
        .......`),
      e: art(`
        .......
        .......
        .......
        .#####.
        ##...##
        ##...##
        #######
        ##.....
        ##...##
        .#####.
        .......
        .......
        .......`),
      h: art(`
        ##.....
        ##.....
        ##.....
        ##.###.
        ###.###
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .......
        .......
        .......`),
      i: art(`
        ##
        ##
        ..
        ##
        ##
        ##
        ##
        ##
        ##
        ##
        ..
        ..
        ..`),
      l: art(`
        ##
        ##
        ##
        ##
        ##
        ##
        ##
        ##
        ##
        ##
        ..
        ..
        ..`),
      n: art(`
        .......
        .......
        .......
        ##.###.
        ###.###
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .......
        .......
        .......`),
      p: art(`
        .......
        .......
        .......
        ######.
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        ######.
        ##.....
        ##.....
        ##.....`),
      r: art(`
        ......
        ......
        ......
        ##.###
        ###...
        ##....
        ##....
        ##....
        ##....
        ##....
        ......
        ......
        ......`),
      t: art(`
        .....
        .##..
        .##..
        #####
        .##..
        .##..
        .##..
        .##..
        .##..
        ..###
        .....
        .....
        .....`),
      u: art(`
        .......
        .......
        .......
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        ##..###
        .###.##
        .......
        .......
        .......`),
      x: art(`
        .......
        .......
        .......
        ##...##
        ##...##
        .##.##.
        ..###..
        .##.##.
        ##...##
        ##...##
        .......
        .......
        .......`),
      y: art(`
        .......
        .......
        .......
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        ##...##
        .######
        .....##
        ##...##
        .#####.`),
      '.': art(`
        ..
        ..
        ..
        ..
        ..
        ..
        ..
        ..
        ##
        ##
        ..
        ..
        ..`),
    },
  };

  /* 3. Start Here ------------------------------------------------------- */

  const CRT = art(`
    ................
    .kkkkkkkkkkkkkk.
    .kwwwwwwwwwwwdk.
    .kwkkkkkkkkkkdk.
    .kwkbbyyyybbkdk.
    .kwkbybbbbybkdk.
    .kwkbybbbbybkdk.
    .kwkbybbbbybkdk.
    .kwkbybbbbybkdk.
    .kwkbbyyyybbkdk.
    .kwkkkkkkkkkkdk.
    .kwsssssssgssdk.
    .kddddddddddddk.
    .kkkkkkkkkkkkkk.
    ....kwssssdk....
    ..kkkkkkkkkkkk..`);
  const CRT_MAP = { k: '#000000', w: '#FFFFFF', s: '#C0C0C0', d: '#808080', b: '#000080', y: '#FFFF55', g: '#55FF55' };

  OG.add({
    num: 3,
    id: 'start-here',
    name: 'Start Here',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Windows 9x'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    scene: false,
    bg: '#008080',
    ink: '#FFFFFF',
    palette: ['#C0C0C0', '#008080', '#000080', '#FFFF55', '#000000'],
    fonts: [],
    type: 'A bold 9x-style UI bitmap face drawn for this logo: 9-pixel caps, 2-pixel stems, set at 5 screen pixels per pixel.',
    concept:
      'The name is a taskbar button, the first thing you press on a 9x desktop, with its two-step bevel and a dotted focus ring. ' +
      'Its 16x16 icon is a tiny CRT showing an O, so the mark and the button arrive as one familiar object that says "click here to begin".',
    logo: (w) => {
      const u = 5;
      const rows = wordRows(W95, w);
      const T = rows[0].length;
      // Bevel 2, padding 2, focus ring 1, padding 2, icon 16, gap 5, label, and back out.
      const bwU = 7 + 16 + 5 + T + 7, bhU = 30;
      const bw = bwU * u, bh = bhU * u;
      const sh = (bhU + 6) * u, sy = Math.round((300 - sh) / 2 / u) * u;
      const bx = Math.round((480 - bw) / 2 / u) * u, by = sy + 3 * u;
      let s = '';
      // The taskbar: light top edges, then the strip, bleeding off both sides.
      s += rect(0, sy, 480, sh, '#C0C0C0') + rect(0, sy, 480, u, '#DFDFDF') + rect(0, sy + u, 480, u, '#FFFFFF');
      s += rect(0, sy + sh - u, 480, u, '#808080') + rect(0, sy + sh, 480, u, '#000000');
      // A grip groove either side of the button, as on a toolbar.
      [bx - 5 * u, bx + bw + 3 * u].forEach((gx) => {
        s += rect(gx, by + 2 * u, u, bh - 4 * u, '#808080') + rect(gx + u, by + 2 * u, u, bh - 4 * u, '#FFFFFF');
      });
      s += bevel(bx, by, bw, bh, u, '#FFFFFF', '#DFDFDF', '#808080', '#000000', '#C0C0C0');
      const ix = bx + 7 * u, iy = by + 7 * u;
      s += OG.bitmap(CRT, CRT_MAP, { x: ix, y: iy, px: u });
      const tx = ix + 21 * u, ty = iy + 4 * u;
      s += OG.bitmap(rows, { '#': '#000000' }, { x: tx, y: ty, px: u });
      // The dotted focus rectangle of a focused button.
      const fx = bx + 4 * u, fy = by + 4 * u, fw = bw - 8 * u, fh = bh - 8 * u;
      let dots = '';
      for (let i = 0; i < fw / u; i += 2) dots += rect(fx + i * u, fy, u, u, '#000') + rect(fx + i * u, fy + fh - u, u, u, '#000');
      for (let j = 2; j < fh / u - 1; j += 2) dots += rect(fx, fy + j * u, u, u, '#000') + rect(fx + fw - u, fy + j * u, u, u, '#000');
      s += dots;
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(CRT, CRT_MAP, { x: 0, y: 0, px: 4 }), { crisp: true }),
  });

  /* 8. Cloud Nine ------------------------------------------------------- */

  // A chunky pixel cloud: a union of circles on a grid, white on top and a
  // blue-gray underside, with a checkerboard dither between the two.
  function cloudRows(cols, rowsN, blobs, depth = 2) {
    const inside = (i, j) => blobs.some(([cx, cy, r]) => (i + 0.5 - cx) ** 2 + (j + 0.5 - cy) ** 2 <= r * r);
    const rows = [];
    for (let j = 0; j < rowsN; j++) {
      let s = '';
      for (let i = 0; i < cols; i++) {
        if (!inside(i, j)) { s += '.'; continue; }
        // How far is the cloud's underside below this cell, in this column?
        let k = 0;
        while (j + k + 1 < rowsN && inside(i, j + k + 1)) k++;
        // Two tones, and a third, lighter step where the shade meets the white.
        s += k < depth ? 's' : k === depth ? 'm' : 'w';
      }
      rows.push(s);
    }
    return rows;
  }

  const SKY = ['#1B4FA8', '#2259B4', '#2B65BF', '#3572C9', '#4280D2', '#518EDA', '#629DE1', '#76ADE7', '#8BBDEC', '#A2CCF0'];

  function sky(x, y, w, h, bands, p, id) {
    const bh = h / bands.length;
    let defs = '', s = '';
    bands.forEach((c, i) => {
      s += rect(x, y + i * bh, w, bh + 0.5, c);
      if (i < bands.length - 1) {
        defs += `<pattern id="${id}${i}" width="${2 * p}" height="${2 * p}" patternUnits="userSpaceOnUse" x="${x}" y="0">${rect(0, 0, p, p, bands[i + 1])}${rect(p, p, p, p, bands[i + 1])}</pattern>`;
        s += rect(x, y + (i + 1) * bh - p, w, p, `url(#${id}${i})`);
      }
    });
    return `<defs>${defs}</defs>` + s;
  }

  const CLOUD_A = cloudRows(30, 12, [[7, 8, 4.2], [13, 6, 5.5], [20, 7, 4.6], [25, 9, 3.2], [16, 9.5, 3.2], [4, 10, 2.4]]);
  const CLOUD_B = cloudRows(22, 9, [[5, 6, 3.2], [10, 4.5, 4.2], [16, 5.5, 3.6], [19.5, 7, 2.4]]);
  const CLOUD_C = cloudRows(13, 6, [[3.5, 4, 2.2], [7, 3, 3], [10, 4, 2.4]], 1);

  const RAINBOW = ['#FF5555', '#FF9933', '#FFFF55', '#55FF55', '#55FFFF', '#5577FF', '#AA55FF'];

  OG.add({
    num: 8,
    id: 'cloud-nine',
    name: 'Cloud Nine',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Windows 9x'],
    paletteTag: 'Pastel',
    risk: 'Bold',
    scene: true,
    bg: '#4280D2',
    ink: '#FFFFFF',
    palette: ['#3572C9', '#A2CCF0', '#FFFFFF', '#0B2A66', '#FFFF55', '#FF5555'],
    fonts: ['Nunito:ital,wght@1,900'],
    type: 'Nunito Black Italic in deep navy, with a soft white lift, over hand-built pixel clouds shaded from their own undersides.',
    concept:
      'A boot splash for a calm morning: a banded, dithered sky, chunky two-tone pixel clouds and a rainbow strip that keeps sliding while the system starts. ' +
      'It is the moment between power-on and desktop, and it promises the wait will be pleasant.',
    logo: (w) => {
      const size = fit('nunito', w, 92, 340);
      const p = 6;
      let s = sky(0, 0, 480, 300, SKY, p, 'sk');
      const cm = { w: '#FFFFFF', m: '#E2EBF7', s: '#B9CCE8' };
      s += `<g opacity="0.9">${OG.bitmap(CLOUD_C, cm, { x: 360, y: 34, px: p })}</g>`;
      s += OG.bitmap(CLOUD_A, cm, { x: 18, y: 26, px: p });
      s += OG.bitmap(CLOUD_B, cm, { x: 336, y: 222, px: p });
      const ty = 168;
      s += OG.text(w, { x: 243, y: ty + 3, size, family: "'Nunito', sans-serif", weight: 900, italic: true, fill: '#FFFFFF', anchor: 'middle', attrs: 'opacity="0.85"' });
      s += OG.text(w, { x: 240, y: ty, size, family: "'Nunito', sans-serif", weight: 900, italic: true, fill: '#0B2A66', anchor: 'middle' });
      // The progress strip: pixel segments of the rainbow, sliding left.
      const sw = 228, sx = 240 - sw / 2, sy2 = 206, seg = 12;
      let band = '';
      for (let i = 0; i < (sw / seg) * 2 + RAINBOW.length; i++) band += rect(sx + i * seg, sy2, seg, 6, RAINBOW[i % RAINBOW.length]);
      s += `<defs><clipPath id="strip">${rect(sx, sy2, sw, 6, '#000')}</clipPath></defs>`;
      s += rect(sx - 3, sy2 - 3, sw + 6, 12, '#0B2A66', 'opacity="0.55"');
      s += `<g clip-path="url(#strip)"><g>${band}<animateTransform attributeName="transform" type="translate" values="0 0;${-seg * RAINBOW.length} 0" dur="1.4s" calcMode="discrete" keyTimes="0;1" repeatCount="indefinite"/></g></g>`;
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => {
      // 16x16: a sky, a sun ring peeking out from behind a cloud.
      const rows = [];
      for (let j = 0; j < 16; j++) {
        let r = '';
        for (let i = 0; i < 16; i++) {
          const x = i + 0.5, y = j + 0.5;
          const cl = [[5, 11.5, 3.3], [9.5, 10, 3.8], [13, 12, 2.7], [3, 13, 2.2]].some(([cx, cy, rr]) => (x - cx) ** 2 + (y - cy) ** 2 <= rr * rr) && y < 15;
          const d = Math.hypot(x - 10.5, y - 5.5);
          if (cl) r += y > 12.6 ? 's' : 'w';
          else if (d <= 4.6 && d >= 2.1) r += d > 3.6 ? 'o' : 'y';
          else r += j < 5 ? 'a' : j < 10 ? 'b' : 'c';
        }
        rows.push(r);
      }
      return OG.iconSvg(OG.bitmap(rows, { a: '#2B65BF', b: '#4280D2', c: '#629DE1', w: '#FFFFFF', s: '#B9CCE8', y: '#FFFF55', o: '#FFAA00' }, { px: 4 }), { crisp: true });
    },
  });

  /* 13. Hourglass ------------------------------------------------------- */

  const HOURGLASS = art(`
    kkkkkkkkkkkkkkk
    knnnnnnnnnnnnnk
    khhhhhhhhhhhhhk
    kkkkkkkkkkkkkkk
    .kwwwwwwwwwwwk.
    .kwwwwwwwwwwwk.
    .kwyyyyyyyyywk.
    ..kyyyyyyyyyk..
    ...kbyyyyybk...
    ....kbyyybk....
    .....kbybk.....
    ......kyk......
    ......kwk......
    .....kwwwk.....
    ....kwwwwwk....
    ...kwwwwwwwk...
    ..kwwwwwwwwwk..
    .kwwwwwyywwwwk.
    .kwwwyyyyyywwk.
    .kwyyyyyyyyybk.
    kkkkkkkkkkkkkkk
    knnnnnnnnnnnnnk
    khhhhhhhhhhhhhk
    kkkkkkkkkkkkkkk`);
  const HG_MAP = { k: '#000000', w: '#FFFFFF', y: '#FFFF55', b: '#AA5500', n: '#AA5500', h: '#FFAA55' };

  OG.add({
    num: 13,
    id: 'hourglass',
    name: 'Hourglass',
    caps: 'OgOS',
    flexCase: true,
    family: 'Interface',
    sources: ['Windows 9x'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: '#E9E5DA',
    ink: '#111111',
    palette: ['#000000', '#FFFF55', '#AA5500', '#FFFFFF', '#E9E5DA'],
    fonts: ['Chakra Petch:wght@500;700'],
    type: 'Chakra Petch Bold for the name, its clipped corners a match for the pixel steps; Chakra Petch Medium for the caption.',
    concept:
      'The wait cursor, owned outright: a pixel hourglass with VGA-yellow sand and single grains dropping through the neck. ' +
      'An OS for slow, happy afternoons can laugh at itself, so the caption admits it is rewinding to 1998.',
    logo: (w) => {
      const px = 8, hx = 52, hy = 54;
      let s = `<g opacity="0.18">${OG.bitmap(HOURGLASS, { k: '#000', n: '#000', h: '#000', w: '#000', y: '#000', b: '#000' }, { x: hx + px, y: hy + px, px })}</g>`;
      s += OG.bitmap(HOURGLASS, HG_MAP, { x: hx, y: hy, px });
      // Grains falling through the lower bulb, two rows a step.
      const grain = (r) => rect(hx + 7 * px, hy + r * px, px, px, '#FFFF55');
      s += `<g>${grain(12) + grain(14) + grain(16)}<animateTransform attributeName="transform" type="translate" values="0 0;0 ${px}" dur="0.5s" calcMode="discrete" repeatCount="indefinite"/></g>`;
      const tx = 200, size = fit('chakra', w, 78, 252);
      s += OG.text(w, { x: tx, y: 168, size, family: "'Chakra Petch', sans-serif", weight: 700, fill: '#111111' });
      s += OG.text('please wait · rewinding to 1998', { x: tx + 2, y: 202, size: 15.5, family: "'Chakra Petch', sans-serif", weight: 500, fill: '#6B6658', ls: 0.4 });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(HOURGLASS, HG_MAP, { x: 13.25, y: 2, px: 2.5 }), { crisp: true }),
  });

  /* 18. Pinstripe ------------------------------------------------------- */

  // A Platinum-style title bar: grooves either side of a centred title.
  function platinumBar(x, y, w, h, titleW, opts = {}) {
    let s = rect(x, y, w, h, '#DDDDDD');
    const box = opts.box, gx0 = x + opts.inset, gx1 = x + w - opts.inset;
    const tx0 = x + w / 2 - titleW / 2, tx1 = x + w / 2 + titleW / 2;
    const n = opts.grooves, period = opts.period, gy = y + (h - (n - 1) * period - opts.line * 2) / 2;
    for (let i = 0; i < n; i++) {
      const yy = gy + i * period;
      [[gx0, tx0], [tx1, gx1]].forEach(([a, b]) => {
        s += rect(a, yy, b - a, opts.line, '#8E8E8E') + rect(a, yy + opts.line, b - a, opts.line, '#FFFFFF');
      });
    }
    return s;
  }

  // A Platinum close or collapse box: a sunken well holding a raised square.
  function platinumBox(x, y, n, u, kind) {
    let s = rect(x, y, n, n, '#8E8E8E') + rect(x + u, y + u, n - u, n - u, '#FFFFFF');
    s += rect(x + u, y + u, n - 2 * u, n - 2 * u, '#000000');
    s += rect(x + 2 * u, y + 2 * u, n - 4 * u, n - 4 * u, '#FFFFFF');
    s += rect(x + 3 * u, y + 3 * u, n - 5 * u, n - 5 * u, '#BBBBBB');
    s += rect(x + 3 * u, y + 3 * u, n - 6 * u, n - 6 * u, '#E4E4E4');
    if (kind === 'collapse') s += rect(x + 2 * u, y + n / 2 - u / 2, n - 4 * u, u, '#000000');
    return s;
  }

  OG.add({
    num: 18,
    id: 'pinstripe',
    name: 'Pinstripe',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Mac OS Classic'],
    paletteTag: 'Mono',
    risk: 'Safe',
    scene: false,
    bg: '#6F7A99',
    ink: '#FFFFFF',
    palette: ['#DDDDDD', '#8E8E8E', '#FFFFFF', '#000000', '#9C99E0'],
    fonts: ['Hanken Grotesk:wght@800'],
    type: 'Hanken Grotesk ExtraBold, a sturdy grotesque in the spirit of a late-90s system title face.',
    concept:
      'The name is the title of a window: a light-gray title bar cut with embossed grooves, a close box, a collapse box and the top of a window beneath. ' +
      'The only color is a lavender scroll thumb, the one splash that Platinum allowed, so the whole mark feels quiet, precise and well made.',
    logo: (w) => {
      const size = fit('hanken', w, 54, 190);
      const titleW = tw('hanken', w, size) + 36;
      const u = 4, x = 24, y = 58, W = 432, bar = 72, body = 112;
      let s = rect(x + 6, y + 6, W, bar + body, '#000000', 'opacity="0.25"');
      s += rect(x, y, W, bar + body, '#000000');
      s += rect(x + 2, y + 2, W - 4, bar + body - 4, '#9A9A9A') + rect(x + 2, y + 2, W - 6, bar + body - 6, '#FFFFFF');
      s += rect(x + 4, y + 4, W - 8, bar + body - 8, '#DDDDDD');
      s += platinumBar(x + 2 * u, y + 2 * u, W - 4 * u, bar - 3 * u, titleW, { inset: 56, grooves: 6, period: 8, line: 2 });
      s += platinumBox(x + 18, y + 2 * u + (bar - 3 * u - 28) / 2, 28, 3, 'close');
      s += platinumBox(x + W - 18 - 28, y + 2 * u + (bar - 3 * u - 28) / 2, 28, 3, 'collapse');
      s += OG.text(w, { x: 240, y: y + bar / 2 + size * 0.34, size, family: "'Hanken Grotesk', sans-serif", weight: 800, fill: '#111111', anchor: 'middle' });
      // The window: a sunken white content area and a Platinum scroll bar.
      const cy = y + bar, cx = x + 3 * u, cw = W - 6 * u, ch = body - 4 * u;
      s += rect(cx - 2, cy - 2, cw + 4, ch + 4, '#8E8E8E') + rect(cx, cy, cw + 2, ch + 2, '#FFFFFF');
      s += rect(cx, cy, cw, ch, '#000000') + rect(cx + 2, cy + 2, cw - 4, ch - 4, '#FFFFFF');
      const sbw = 30, sbx = cx + cw - 2 - sbw;
      s += rect(sbx, cy + 2, 2, ch - 4, '#000000') + rect(sbx + 2, cy + 2, sbw - 2, ch - 4, '#CCCCCC');
      s += rect(sbx + 2, cy + 2, sbw - 2, 28, '#E6E6E6') + rect(sbx + 2, cy + 30, sbw - 2, 2, '#000000');
      s += `<path d="M${sbx + 9} ${cy + 22} h14 l-7 -8 z" fill="#000"/>`;
      const th = 38, ty = cy + 40;
      s += rect(sbx + 4, ty, sbw - 6, th, '#5B57B8') + rect(sbx + 4, ty, sbw - 8, th - 2, '#C9C6F5') + rect(sbx + 6, ty + 2, sbw - 10, th - 4, '#9C99E0');
      for (let i = 0; i < 4; i++) s += rect(sbx + 10, ty + 11 + i * 5, sbw - 18, 2, '#5B57B8') + rect(sbx + 10, ty + 13 + i * 5, sbw - 18, 1, '#E3E1FB');
      // A text cursor and a few lines of gray "text" in the window.
      [[0, 210], [1, 260], [2, 150]].forEach(([i, lw]) => (s += rect(cx + 22, cy + 24 + i * 22, lw, 6, '#D6D6D6')));
      s += rect(cx + 22 + 150 + 6, cy + 24 + 2 * 22 - 7, 2, 20, '#000000');
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => {
      let s = rect(4, 6, 56, 52, '#000') + rect(6, 8, 52, 48, '#DDDDDD') + rect(6, 8, 52, 2, '#FFF');
      for (let i = 0; i < 4; i++) s += rect(18, 12 + i * 3, 38, 1, '#8E8E8E') + rect(18, 13 + i * 3, 38, 1, '#FFF');
      s += rect(9, 12, 7, 7, '#000') + rect(10, 13, 5, 5, '#E4E4E4');
      s += rect(6, 24, 52, 2, '#000') + rect(8, 26, 48, 28, '#FFFFFF');
      s += `<circle cx="32" cy="40" r="9" fill="none" stroke="#111" stroke-width="5"/>`;
      return OG.iconSvg(s);
    },
  });

  /* 23. Six Bands ------------------------------------------------------- */

  const BANDS6 = ['#5EBD3E', '#FFB900', '#F78200', '#E23838', '#973999', '#009CDF'];

  // A pixel lowercase o: a round ring whose counter leans like a pen-drawn
  // old-style o, filled with six horizontal bands.
  function bandedO(n, ri = 0.36, rj = 0.56, tilt = -24) {
    const rows = [];
    const c = n / 2, a = (tilt * Math.PI) / 180;
    for (let j = 0; j < n; j++) {
      let s = '';
      for (let i = 0; i < n; i++) {
        const x = i + 0.5 - c, y = j + 0.5 - c;
        const xr = x * Math.cos(a) + y * Math.sin(a), yr = -x * Math.sin(a) + y * Math.cos(a);
        const out = (x * x) / (c * 0.96) ** 2 + (y * y) / (c * c) <= 1;
        const inn = (xr * xr) / (c * ri) ** 2 + (yr * yr) / (c * rj) ** 2 <= 1;
        s += out && !inn ? String(Math.min(5, Math.floor((j * 6) / n))) : '.';
      }
      rows.push(s);
    }
    return rows;
  }

  OG.add({
    num: 23,
    id: 'six-bands',
    name: 'Six Bands',
    caps: 'ogos',
    flexCase: true,
    family: 'Geometric',
    sources: ['Mac OS Classic'],
    paletteTag: 'Rainbow',
    risk: 'Safe',
    scene: false,
    bg: '#FBF9F3',
    ink: '#1C1A17',
    palette: BANDS6.slice(),
    fonts: ['Cormorant Garamond:ital,wght@1,700'],
    type: 'Cormorant Garamond Bold Italic, condensed to 86% and set tight, a bookish old-style voice.',
    concept:
      'A pixel-stepped lowercase o, its counter tilted like a pen-drawn old-style letter, filled with six bands of color from green to blue. ' +
      'It recalls the friendliness of 1984 desktop computing, when a computer could be a creative tool for anyone, without borrowing anyone’s mark.',
    logo: (w) => {
      const rows = bandedO(24);
      const px = 6, mw = rows[0].length * px;
      const size = 132;
      let tw0 = tw('corm', w, size) * 0.86;
      const maxT = 432 - mw - 26;
      const len = Math.min(tw0, maxT);
      const total = mw + 26 + len;
      const mx = Math.round(240 - total / 2), my = 150 - (24 * px) / 2 - 4;
      const map = {}; BANDS6.forEach((c, i) => (map[i] = c));
      let s = OG.bitmap(rows, map, { x: mx, y: my, px });
      s += OG.text(w, { x: mx + mw + 26, y: my + 24 * px - 6, size, family: "'Cormorant Garamond', serif", weight: 700, italic: true, fill: '#1C1A17', len: len.toFixed(1) });
      return OG.logoSvg(s);
    },
    icon: () => {
      const rows = bandedO(18, 0.4, 0.58);
      const map = {}; BANDS6.forEach((c, i) => (map[i] = c));
      return OG.iconSvg(OG.bitmap(rows, map, { x: 5, y: 5, px: 3 }), { crisp: true });
    },
  });

  /* 28. Happy Machine --------------------------------------------------- */

  // A compact all-in-one, 26 x 32 pixels plus a one-pixel drop shadow.
  function machineRows() {
    const W = 26, H = 32;
    const g = Array.from({ length: H + 1 }, () => Array(W + 1).fill('.'));
    const set = (i, j) => { if (g[j] && i >= 0 && i <= W) g[j][i] = '#'; };
    const hline = (j, a, b) => { for (let i = a; i <= b; i++) set(i, j); };
    const vline = (i, a, b) => { for (let j = a; j <= b; j++) set(i, j); };
    // The case, with rounded top corners, and its shadow.
    hline(0, 2, W - 3); set(1, 1); set(W - 2, 1); vline(0, 2, H - 1); vline(W - 1, 2, H - 1); hline(H - 1, 0, W - 1);
    vline(W, 2, H); hline(H, 2, W);
    // The screen bezel, corners cut.
    hline(3, 4, W - 5); hline(19, 4, W - 5); vline(3, 4, 18); vline(W - 4, 4, 18);
    // The face: two little o's and a smile.
    [[8, 8], [14, 8]].forEach(([x, y]) => { hline(y, x + 1, x + 2); hline(y + 3, x + 1, x + 2); vline(x, y + 1, y + 2); vline(x + 3, y + 1, y + 2); });
    set(8, 13); set(17, 13); hline(14, 9, 10); hline(14, 15, 16); hline(15, 11, 14);
    // A floppy slot, a power light, and the chin line.
    hline(24, 13, 21); set(13, 25); set(21, 25);
    hline(24, 4, 5); hline(25, 4, 5);
    hline(H - 5, 1, W - 2);
    // Fill the inside of the case white.
    for (let j = 1; j <= H - 2; j++) for (let i = j === 1 ? 2 : 1; i <= (j === 1 ? W - 3 : W - 2); i++) if (g[j][i] === '.') g[j][i] = 'w';
    return g.map((r) => r.join(''));
  }
  const MACHINE_1BIT = machineRows();

  OG.add({
    num: 28,
    id: 'happy-machine',
    name: 'Happy Machine',
    caps: 'OgOs',
    flexCase: true,
    family: 'Mascot',
    sources: ['Mac OS Classic'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: '#E8E8E8',
    ink: '#000000',
    palette: ['#000000', '#E8E8E8', '#FFFFFF'],
    fonts: [],
    type: 'A 1-bit bitmap bold drawn for OgOs in the manner of a classic system face: 2-pixel stems, round shoulders, a big x-height.',
    concept:
      'A compact all-in-one computer, drawn pixel by pixel in 1-bit, whose screen smiles back with two little o’s for eyes. ' +
      'It is the mascot that greets you at startup and the promise of a machine that is glad to see you.',
    logo: (w) => {
      const px = 5, mw = 26 * px;
      const rows = wordRows(CHI, w);
      const nw = rows[0].length * px;
      const mx = Math.round((480 - mw) / 2 / px) * px, my = 20;
      let s = OG.bitmap(MACHINE_1BIT, { '#': '#000000', w: '#FFFFFF' }, { x: mx, y: my, px });
      s += OG.bitmap(rows, { '#': '#000000' }, { x: Math.round((480 - nw) / 2 / px) * px, y: my + 33 * px + 18, px });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(MACHINE_1BIT, { '#': '#000000', w: '#FFFFFF' }, { x: 6, y: 0, px: 64 / 33 }), { crisp: true }),
  });

  /* 33. Bondi ----------------------------------------------------------- */

  function bondiRing(cx, cy, R, r, id) {
    const m = (R + r) / 2;
    let s = `<defs>
      <radialGradient id="${id}b" cx="0.42" cy="0.38" r="0.7">
        <stop offset="0" stop-color="#7FE6EE"/><stop offset="0.45" stop-color="#16B4C8"/><stop offset="0.85" stop-color="#00889C"/><stop offset="1" stop-color="#005E70"/>
      </radialGradient>
      <radialGradient id="${id}sh" cx="0.5" cy="0.5" r="0.5">
        <stop offset="0" stop-color="#00A2B6" stop-opacity="0.55"/><stop offset="0.6" stop-color="#00A2B6" stop-opacity="0.22"/><stop offset="1" stop-color="#00A2B6" stop-opacity="0"/>
      </radialGradient>
      <radialGradient id="${id}ca" cx="0.5" cy="0.5" r="0.5">
        <stop offset="0" stop-color="#E8FFFF" stop-opacity="0.95"/><stop offset="1" stop-color="#7FE6EE" stop-opacity="0"/>
      </radialGradient>
      <linearGradient id="${id}hl" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.95"/><stop offset="1" stop-color="#FFFFFF" stop-opacity="0.05"/>
      </linearGradient>
      <pattern id="${id}rib" width="5" height="5" patternUnits="userSpaceOnUse"><rect width="1.6" height="5" fill="#FFFFFF" opacity="0.16"/></pattern>
      <mask id="${id}m"><circle cx="${cx}" cy="${cy}" r="${R}" fill="#fff"/><circle cx="${cx}" cy="${cy}" r="${r}" fill="#000"/></mask>
      <filter id="${id}bl" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="3"/></filter>
      <filter id="${id}bl2" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="7"/></filter>
    </defs>`;
    // A colored shadow with a bright caustic, as light passes through.
    s += `<ellipse cx="${cx + 8}" cy="${cy + R + 6}" rx="${R * 1.02}" ry="${R * 0.17}" fill="url(#${id}sh)"/>`;
    s += `<ellipse cx="${cx + 4}" cy="${cy + R + 5}" rx="${R * 0.36}" ry="${R * 0.06}" fill="url(#${id}ca)"/>`;
    s += `<g mask="url(#${id}m)">`;
    s += `<circle cx="${cx}" cy="${cy}" r="${R}" fill="url(#${id}b)" opacity="0.93"/>`;
    s += `<rect x="${cx - R}" y="${cy - R}" width="${2 * R}" height="${2 * R}" fill="url(#${id}rib)"/>`;
    // Inner shadow at the hole's upper rim, and a frosted glow along its lower rim.
    s += `<circle cx="${cx}" cy="${cy - 4}" r="${r + 7}" fill="none" stroke="#004454" stroke-width="10" opacity="0.4" filter="url(#${id}bl)"/>`;
    s += `<circle cx="${cx + 3}" cy="${cy + 6}" r="${r + 5}" fill="none" stroke="#E6FFFF" stroke-width="5" opacity="0.6" filter="url(#${id}bl)"/>`;
    s += `<circle cx="${cx}" cy="${cy}" r="${R - 3}" fill="none" stroke="#004454" stroke-width="7" opacity="0.35" filter="url(#${id}bl)"/>`;
    // Bright rim light at the bottom right of the outer edge.
    s += `<path d="M ${cx + R * 0.86} ${cy + R * 0.2} A ${R - 6} ${R - 6} 0 0 1 ${cx + R * 0.1} ${cy + R - 6}" fill="none" stroke="#C9FBFF" stroke-width="5" stroke-linecap="round" opacity="0.8" filter="url(#${id}bl)"/>`;
    s += `</g>`;
    // The big glossy highlight across the top.
    s += `<path d="M ${cx - R * 0.78} ${cy - R * 0.2} A ${R * 0.84} ${R * 0.84} 0 0 1 ${cx + R * 0.62} ${cy - R * 0.6} Q ${cx + R * 0.1} ${cy - R * 0.62} ${cx - R * 0.26} ${cy - R * 0.38} Q ${cx - R * 0.56} ${cy - R * 0.22} ${cx - R * 0.78} ${cy - R * 0.2} Z" fill="url(#${id}hl)" opacity="0.92"/>`;
    s += `<ellipse cx="${cx + R * 0.52}" cy="${cy + R * 0.5}" rx="${R * 0.09}" ry="${R * 0.05}" transform="rotate(-40 ${cx + R * 0.52} ${cy + R * 0.5})" fill="#FFFFFF" opacity="0.7"/>`;
    return s;
  }

  OG.add({
    num: 33,
    id: 'bondi',
    name: 'Bondi',
    caps: 'ogos',
    flexCase: true,
    family: 'Object',
    sources: ['Windows 9x', 'Mac OS Classic'],
    paletteTag: 'Pastel',
    risk: 'Bold',
    scene: false,
    bg: '#EEF3F4',
    ink: '#0C4D5A',
    palette: ['#00A2B6', '#7FE6EE', '#005E70', '#FFFFFF', '#0C4D5A'],
    fonts: ['Comfortaa:wght@700'],
    type: 'Comfortaa Bold, a round geometric sans of the dot-com years, in deep teal.',
    concept:
      'An o made of translucent candy plastic: Bondi-blue, glossy, ribbed inside, casting a tinted shadow with a bright caustic where light passes through. ' +
      'This is the 1998 end of retro, when computers became friendly objects you wanted on the desk.',
    logo: (w) => {
      const R = 74, r = 30, cx = 34 + R, cy = 138;
      const size = fit('comfortaa', w, 96, 432 - 2 * R - 30);
      let s = bondiRing(cx, cy, R, r, 'bd');
      s += OG.text(w, { x: cx + R + 28, y: cy + size * 0.27, size, family: "'Comfortaa', sans-serif", weight: 700, fill: '#0C4D5A' });
      return OG.logoSvg(s);
    },
    icon: () => OG.iconSvg(bondiRing(32, 30, 27, 11, 'bi').replace(/<ellipse cx="[^"]*" cy="[^"]*" rx="[^"]*" ry="[^"]*" fill="url\(#bish\)"\/>/, '')),
  });

  /* 38. Be Tab ---------------------------------------------------------- */

  // A small beveled square, the close box on a yellow tab.
  function beBox(x, y, n) {
    return rect(x, y, n, n, '#8A6A00') + rect(x, y, n - 2, n - 2, '#FFF6C8') + rect(x + 2, y + 2, n - 4, n - 4, '#C99A00') +
      `<rect x="${x + 2}" y="${y + 2}" width="${n - 6}" height="${n - 6}" fill="url(#bebox)"/>`;
  }

  function beWindow(w, size, x, y, tabH, winW, winH) {
    const pad = 22, boxN = Math.round(size * 0.42), gap = 16;
    const textW = tw('intertight', w, size);
    const tabW = Math.round(pad + boxN + gap + textW + pad + 6);
    let s = `<defs><linearGradient id="bebox" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#FFE680"/><stop offset="1" stop-color="#E0A800"/></linearGradient>
      <linearGradient id="betab" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFD83A"/><stop offset="1" stop-color="#FFC400"/></linearGradient></defs>`;
    // A soft shadow under the window and tab.
    s += `<g opacity="0.28">${rect(x + 8, y + 8, tabW, tabH, '#000')}${rect(x + 8, y + tabH + 8, winW, winH, '#000')}</g>`;
    // The tab: a dark edge, a light top-left edge, a darker bottom-right edge.
    s += rect(x, y, tabW, tabH + 2, '#5C4600');
    s += rect(x + 2, y + 2, tabW - 4, tabH - 2, '#C99600');
    s += rect(x + 2, y + 2, tabW - 6, tabH - 4, '#FFF0A0');
    s += rect(x + 4, y + 4, tabW - 8, tabH - 6, 'url(#betab)');
    s += beBox(x + pad, y + (tabH - boxN) / 2, boxN);
    s += OG.text(w, { x: x + pad + boxN + gap, y: y + tabH / 2 + size * 0.36, size, family: "'Inter Tight', sans-serif", weight: 800, fill: '#141414', len: textW.toFixed(1) });
    // The window frame and its content, with a resize knob in the corner.
    const fy = y + tabH;
    s += rect(x, fy, winW, winH, '#4A4A4A');
    s += rect(x + 2, fy + 2, winW - 4, winH - 4, '#E8E8E8') + rect(x + 4, fy + 4, winW - 6, winH - 6, '#9A9A9A') + rect(x + 4, fy + 4, winW - 8, winH - 8, '#D8D8D8');
    s += rect(x + 9, fy + 9, winW - 18, winH - 18, '#7A7A7A') + rect(x + 10, fy + 10, winW - 20, winH - 20, '#FFFFFF');
    return { s, tabW };
  }

  OG.add({
    num: 38,
    id: 'be-tab',
    name: 'Be Tab',
    caps: 'OgOS',
    flexCase: true,
    family: 'Interface',
    sources: ['BeOS'],
    paletteTag: 'Primary',
    risk: 'Safe',
    scene: false,
    bg: '#336698',
    ink: '#FFFFFF',
    palette: ['#FFCB00', '#336698', '#D8D8D8', '#141414', '#FFF0A0'],
    fonts: ['Inter Tight:wght@500;800'],
    type: 'Inter Tight ExtraBold, a compact grotesque, set exactly as wide as the tab allows.',
    concept:
      'A warm yellow window tab, only as wide as its title, sitting on the corner of a gray window, with a little close box. ' +
      'It is the friendliest piece of 90s interface design, and it turns the name into something you could grab and drag.',
    logo: (w) => {
      const size = fit('intertight', w, 56, 250);
      const winW = 380, x = 50, y = 62, tabH = 76, winH = 120;
      const { s } = beWindow(w, size, x, y, tabH, winW, winH);
      let k = '';
      // A menu bar along the top of the window's content.
      const my = y + tabH + 10;
      k += rect(x + 10, my, winW - 20, 26, '#E6E6E6') + rect(x + 10, my + 25, winW - 20, 1, '#9A9A9A') + rect(x + 10, my, winW - 20, 1, '#FFFFFF');
      ['File', 'Edit', 'View', 'Window'].forEach((m, i) => (k += OG.text(m, { x: x + 24 + i * 58, y: my + 18, size: 14, family: "'Inter Tight', sans-serif", weight: 500, fill: '#222222' })));
      // The resize knob: a few diagonal ridges in the window's corner.
      const kx = x + winW - 12, ky = y + tabH + winH - 12;
      for (let i = 0; i < 3; i++) k += `<path d="M${kx - 8 - i * 7} ${ky} L${kx} ${ky - 8 - i * 7}" stroke="#9A9A9A" stroke-width="2"/><path d="M${kx - 6 - i * 7} ${ky} L${kx} ${ky - 6 - i * 7}" stroke="#FFFFFF" stroke-width="1.5"/>`;
      return OG.logoSvg(s + k);
    },
    icon: () => {
      let s = `<defs><linearGradient id="it" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFD83A"/><stop offset="1" stop-color="#FFC400"/></linearGradient></defs>`;
      s += rect(2, 8, 60, 40, '#5C4600') + rect(4, 10, 56, 36, '#FFF0A0') + rect(6, 12, 54, 34, 'url(#it)');
      s += rect(4, 48, 58, 10, '#4A4A4A') + rect(6, 48, 54, 8, '#D8D8D8');
      s += OG.text('Og', { x: 32, y: 39, size: 30, family: "'Inter Tight', sans-serif", weight: 800, fill: '#141414', anchor: 'middle', ls: -1 });
      return OG.iconSvg(s);
    },
  });

  /* 43. Blue & Red ------------------------------------------------------ */

  // The name with bouncing baselines: first half blue, second half red.
  function bounceText(w, x, y, size, fills, attrs = '') {
    const ch = [...w], half = Math.ceil(ch.length / 2);
    const off = [0, -8, 2, -6, 4, -6];
    const rot = [-4, 3, -1, 5, -3, 2];
    let prev = 0, spans = '';
    ch.forEach((c, i) => {
      const dy = off[i % off.length] - prev;
      prev = off[i % off.length];
      spans += `<tspan dy="${dy}" rotate="${rot[i % rot.length]}" fill="${i < half ? fills[0] : fills[1]}">${OG.esc(c)}</tspan>`;
    });
    return `<text x="${x}" y="${y}" font-size="${size}" font-family="'Krona One', sans-serif" text-anchor="middle"${attrs ? ' ' + attrs : ''}>${spans}</text>`;
  }

  OG.add({
    num: 43,
    id: 'blue-and-red',
    name: 'Blue & Red',
    caps: 'OgOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['BeOS'],
    paletteTag: 'Primary',
    risk: 'Safe',
    scene: false,
    bg: '#F3F1EC',
    ink: '#14161F',
    palette: ['#1F3FD6', '#E3251B', '#0B1A6E', '#7A0B08', '#F3F1EC'],
    fonts: ['Krona One'],
    type: 'Krona One, a wide and sturdy geometric, each letter turned and bounced a little, extruded and glossed.',
    concept:
      'Og in deep blue, OS in red, letters jostling on a springy baseline with a hard extruded shadow and a glossy top. ' +
      'It has the cheerful confidence of mid-90s tech, a split name that says this is a new system with its own personality.',
    logo: (w) => {
      const size = fit('krona', w, 92, 380);
      const x = 240, y = 186;
      let defs = `<defs>
        <linearGradient id="bl" gradientUnits="userSpaceOnUse" x1="0" y1="${y - size * 0.8}" x2="0" y2="${y + 10}"><stop offset="0" stop-color="#4C6BFF"/><stop offset="1" stop-color="#1630B8"/></linearGradient>
        <linearGradient id="rd" gradientUnits="userSpaceOnUse" x1="0" y1="${y - size * 0.8}" x2="0" y2="${y + 10}"><stop offset="0" stop-color="#FF5A47"/><stop offset="1" stop-color="#C8160E"/></linearGradient>
        <linearGradient id="gl" gradientUnits="userSpaceOnUse" x1="0" y1="${y - size * 0.86}" x2="0" y2="${y - size * 0.3}"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0.85"/><stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/></linearGradient>
        <clipPath id="tc">${bounceText(w, x, y, size, ['#000', '#000'])}</clipPath>
      </defs>`;
      let s = defs;
      for (let i = 9; i >= 1; i--) s += bounceText(w, x + i * 0.9, y + i * 0.9, size, ['#0B1A6E', '#7A0B08']);
      s += bounceText(w, x, y, size, ['url(#bl)', 'url(#rd)']);
      s += `<rect x="0" y="${y - size}" width="480" height="${size * 0.52}" fill="url(#gl)" clip-path="url(#tc)"/>`;
      return OG.logoSvg(s);
    },
    icon: () => {
      let s = `<defs><linearGradient id="ib" x1="0" y1="0" x2="1" y2="0"><stop offset="0.5" stop-color="#1F3FD6"/><stop offset="0.5" stop-color="#E3251B"/></linearGradient>
        <linearGradient id="ig" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0.75"/><stop offset="0.5" stop-color="#FFFFFF" stop-opacity="0"/></linearGradient></defs>`;
      s += `<circle cx="34" cy="34" r="22" fill="none" stroke="#0B1A6E" stroke-width="13"/>`;
      s += `<circle cx="31" cy="31" r="22" fill="none" stroke="url(#ib)" stroke-width="13"/>`;
      s += `<circle cx="31" cy="31" r="22" fill="none" stroke="url(#ig)" stroke-width="13"/>`;
      return OG.iconSvg(s);
    },
  });

  /* 48. Blinkenlights --------------------------------------------------- */

  // A 5x7 dot-matrix alphabet.
  const DOT5 = {
    h: 7,
    g: {
      O: art(`.###.
        #...#
        #...#
        #...#
        #...#
        #...#
        .###.`),
      G: art(`.###.
        #...#
        #....
        #.###
        #...#
        #...#
        .####`),
      S: art(`.####
        #....
        #....
        .###.
        ....#
        ....#
        ####.`),
      o: art(`.....
        .....
        .###.
        #...#
        #...#
        #...#
        .###.`),
      g: art(`.....
        .####
        #...#
        #...#
        .####
        ....#
        .###.`),
      s: art(`.....
        .....
        .####
        #....
        .###.
        ....#
        ####.`),
    },
  };

  const LED_ON = (i, n) => (i >= n - 3 ? '#FF4033' : i >= n - 6 ? '#FFD23A' : '#3DFF6E');
  const LED_OFF = (i, n) => (i >= n - 3 ? '#3A1512' : i >= n - 6 ? '#3A3212' : '#103A1C');

  function ledColumn(x, y, n, lw, lh, gap, levels, dur) {
    let s = '';
    for (let i = 0; i < n; i++) {
      const ly = y + (n - 1 - i) * (lh + gap);
      s += `<rect x="${x}" y="${ly}" width="${lw}" height="${lh}" rx="${lh / 2.6}" fill="${LED_OFF(i, n)}"/>`;
      const vals = levels.map((h) => (i < h ? 1 : 0));
      const on = `<rect x="${x}" y="${ly}" width="${lw}" height="${lh}" rx="${lh / 2.6}" fill="${LED_ON(i, n)}"/>` +
        `<rect x="${x + lw * 0.18}" y="${ly + lh * 0.18}" width="${lw * 0.5}" height="${lh * 0.22}" rx="${lh * 0.11}" fill="#FFFFFF" opacity="0.55"/>`;
      if (dur) s += `<g opacity="${vals[0]}" filter="url(#ledglow)">${on}<animate attributeName="opacity" values="${vals.join(';')}" dur="${dur}s" calcMode="discrete" repeatCount="indefinite"/></g>`;
      else if (vals[0]) s += `<g filter="url(#ledglow)">${on}</g>`;
    }
    return s;
  }

  OG.add({
    num: 48,
    id: 'blinkenlights',
    name: 'Blinkenlights',
    caps: 'OGOS',
    flexCase: true,
    family: 'Object',
    sources: ['BeOS'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: true,
    bg: '#0A0E1A',
    ink: '#FFB21E',
    palette: ['#0A0E1A', '#3DFF6E', '#FFD23A', '#FF4033', '#FFB21E'],
    fonts: [],
    type: 'A 5x7 dot-matrix alphabet of round LEDs, every unlit dot left faintly visible.',
    concept:
      'Two columns of load-meter LEDs, one per CPU, climbing from green to yellow to red on a dark bezel, and the name glowing in amber dot matrix beside them. ' +
      'It celebrates a machine that shows you how hard it is working, the most honest status display there is.',
    logo: (w) => {
      const rows = wordRows(DOT5, w);
      const p = 12, cols = rows[0].length;
      const nx = 150 + (300 - cols * p) / 2, ny = 108;
      let s = `<defs>
        <filter id="ledglow" x="-60%" y="-120%" width="220%" height="340%"><feGaussianBlur stdDeviation="3.2" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
        <filter id="dotglow" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="4" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
        <linearGradient id="bz" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#151C33"/><stop offset="1" stop-color="#080B15"/></linearGradient>
      </defs>`;
      s += rect(0, 0, 480, 300, 'url(#bz)');
      // The bezel: a recessed window for the meters.
      s += `<rect x="30" y="38" width="94" height="224" rx="10" fill="#04060C" stroke="#232C4A" stroke-width="2"/>`;
      s += ledColumn(44, 52, 14, 28, 10.5, 4.6, [10, 7, 12, 8, 14, 9, 11, 6], 2.4);
      s += ledColumn(82, 52, 14, 28, 10.5, 4.6, [6, 11, 8, 13, 7, 10, 5, 12], 2.4);
      // The dot-matrix name.
      let off = '', on = '';
      rows.forEach((r, j) => [...r].forEach((c, i) => {
        const cx = (nx + i * p + p / 2).toFixed(1), cy = (ny + j * p + p / 2).toFixed(1);
        if (c === '#') on += `<circle cx="${cx}" cy="${cy}" r="${p * 0.4}" fill="#FFB21E"/>`;
        else off += `<circle cx="${cx}" cy="${cy}" r="${p * 0.36}" fill="#2A1E0A"/>`;
      }));
      s += off + `<g filter="url(#dotglow)">${on}</g>`;
      s += OG.pixText('2 CPU - 1000 HZ - 32 BIT', { x: nx, y: ny + 7 * p + 24, px: 1.5, fill: '#46527C' });
      return OG.logoSvg(s);
    },
    icon: () => {
      let s = `<defs><filter id="ledglow" x="-60%" y="-120%" width="220%" height="340%"><feGaussianBlur stdDeviation="1.2" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>`;
      s += `<rect x="2" y="2" width="60" height="60" rx="10" fill="#0A0E1A"/>`;
      s += ledColumn(11, 9, 6, 18, 6, 2, [4], 0);
      s += ledColumn(35, 9, 6, 18, 6, 2, [6], 0);
      return OG.iconSvg(s);
    },
  });

  /* 53. Bright Isometric ------------------------------------------------ */

  // Rasterizes an isometric ring standing on a little tile, BeOS-icon style:
  // bright tops, darker sides, a thick dark outline.
  function isoRing(W, H, cx, cy, R, r, h) {
    const ry = R / 2, iry = r / 2;
    const inE = (x, y, ex, ey, a, b) => ((x - ex) / a) ** 2 + ((y - ey) / b) ** 2 <= 1;
    const grid = [];
    for (let j = 0; j < H; j++) {
      const row = [];
      for (let i = 0; i < W; i++) {
        const x = i + 0.5, y = j + 0.5;
        let c = '.';
        // The tile: a flat diamond with two visible sides.
        const tx = Math.abs(x - cx) / (R * 1.42) + Math.abs(y - (cy + h + 1)) / (R * 0.71);
        if (tx <= 1) c = 't';
        else if (Math.abs(x - cx) <= R * 1.42 && y > cy + h + 1 && y <= cy + h + 1 + 4 + (R * 0.71) * (1 - Math.abs(x - cx) / (R * 1.42))) c = x < cx ? 'L' : 'D';
        // Its shadow, to the lower right.
        if (c === 't' && inE(x, y, cx + 3, cy + h + 3, R * 1.02, ry * 1.02)) c = 'T';
        const u = (x - cx) / R;
        if (Math.abs(u) <= 1) {
          const e = ry * Math.sqrt(1 - u * u);
          if (y >= cy - e && y <= cy + h + e) c = u < -0.45 ? 'a' : u < 0.25 ? 'b' : u < 0.7 ? 'c' : 'd';
        }
        if (inE(x, y, cx, cy, R, ry)) {
          c = inE(x, y, cx - R * 0.25, cy - ry * 0.3, R * 0.55, ry * 0.45) ? 'H' : 'y';
          if (inE(x, y, cx, cy, r, iry)) c = inE(x, y, cx, cy + h, r, iry) ? 'T' : (x - cx) / r < -0.2 ? 'n' : 'm';
        }
        row.push(c);
      }
      grid.push(row);
    }
    // Outline: object cells that touch empty cells, and the rim of the hole.
    const solid = (c) => c !== '.';
    const out = grid.map((row, j) => row.map((c, i) => {
      if (!solid(c)) return c;
      const nb = [[1, 0], [-1, 0], [0, 1], [0, -1]].map(([di, dj]) => (grid[j + dj] || [])[i + di] || '.');
      if (nb.some((n) => !solid(n))) return 'k';
      const isRing = (q) => 'abcdyHnm'.includes(q);
      if ('yH'.includes(c) && nb.some((n) => 'nmT'.includes(n))) return 'k';
      if ('yH'.includes(c) && nb.some((n) => 'abcd'.includes(n)) && j > cy) return 'o';
      if (isRing(c) && nb.some((n) => 'tTLD'.includes(n))) return 'k';
      if ('tT'.includes(c) && nb.some((n) => 'LD'.includes(n))) return 'K';
      return c;
    }).join(''));
    return out;
  }
  const ISO_MAP = {
    k: '#1A1030', K: '#1F4E8C', o: '#C2410C',
    y: '#FFD23F', H: '#FFF3A6', a: '#FF9A3C', b: '#FF6B2C', c: '#E0451F', d: '#A52A18',
    n: '#7A1F12', m: '#B83A1C', t: '#5FD3F3', T: '#3AA6D6', L: '#2E86C1', D: '#1F5F9E',
  };

  OG.add({
    num: 53,
    id: 'bright-isometric',
    name: 'Bright Isometric',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['BeOS'],
    paletteTag: 'Primary',
    risk: 'Bold',
    scene: false,
    bg: '#F2F0EA',
    ink: '#111111',
    palette: ['#FFD23F', '#FF6B2C', '#5FD3F3', '#1F5F9E', '#1A1030'],
    fonts: ['Saira:wght@800'],
    type: 'Saira ExtraBold in black, square-shouldered and plain, to let the icon carry the color.',
    concept:
      'An isometric O: a chunky ring of warm orange plastic standing on a cyan tile, drawn in pixels with a thick dark outline, bright tops and shaded sides. ' +
      'Icons like this made a 90s desktop feel like a box of toys; this one would sit happily next to them.',
    logo: (w) => {
      const rows = isoRing(40, 38, 20, 14, 15.5, 7, 7);
      const px = 4.5, mx = 30, my = 66;
      let s = OG.bitmap(rows, ISO_MAP, { x: mx, y: my, px });
      const size = fit('saira', w, 84, 432 - 40 * px - 18);
      s += OG.text(w, { x: mx + 40 * px + 14, y: 178, size, family: "'Saira', sans-serif", weight: 800, fill: '#111111' });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(OG.bitmap(isoRing(32, 32, 16, 11.5, 12.5, 5.5, 5.5), ISO_MAP, { px: 2 }), { crisp: true }),
  });

  /* 58. Front Panel ----------------------------------------------------- */

  const CDE = { tan: '#AE9E88', tanL: '#D9CDBA', tanD: '#6E6352', teal: '#6E8C9C', mauve: '#A36C7A', sand: '#D2BF96', sage: '#8E9A72' };
  const lighten = (hex, f) => {
    const n = parseInt(hex.slice(1), 16);
    const ch = [n >> 16, (n >> 8) & 255, n & 255].map((v) => Math.round(f > 0 ? v + (255 - v) * f : v * (1 + f)));
    return '#' + ch.map((v) => v.toString(16).padStart(2, '0')).join('');
  };
  // A Motif bevel: two-pixel light and dark edges.
  function motif(x, y, w, h, face, b = 3, pressed = false) {
    let lt = lighten(face, 0.45), dk = lighten(face, -0.45);
    if (pressed) [lt, dk] = [dk, lt];
    return rect(x, y, w, h, dk) + `<path d="M${x} ${y}h${w}l${-b} ${b}h${-(w - 2 * b)}v${h - 2 * b}l${-b} ${b}z" fill="${lt}"/>` + rect(x + b, y + b, w - 2 * b, h - 2 * b, pressed ? lighten(face, -0.12) : face);
  }

  OG.add({
    num: 58,
    id: 'front-panel',
    name: 'Front Panel',
    caps: 'OgOS',
    flexCase: true,
    family: 'Interface',
    sources: ['Unix workstations'],
    paletteTag: 'Earthy',
    risk: 'Bold',
    scene: true,
    bg: '#56677A',
    ink: '#FFFFFF',
    palette: [CDE.tan, CDE.teal, CDE.mauve, CDE.sand, '#56677A'],
    fonts: ['Bai Jamjuree:wght@700'],
    type: 'Bai Jamjuree Bold, one letter to a workspace button, sized to fill each key.',
    concept:
      'A workstation front panel in dusty tans, slate and mauve, its four workspace buttons spelling the name, the second one pressed in. ' +
      'It nods to the Unix workstations of the 90s, the serious machines with the softest colors, and to OgOs having four workspaces of its own.',
    logo: (w) => {
      let s = `<defs><pattern id="cdp" width="6" height="6" patternUnits="userSpaceOnUse"><rect width="6" height="6" fill="#56677A"/><rect width="3" height="3" fill="#5B6C80"/><rect x="3" y="3" width="3" height="3" fill="#5B6C80"/></pattern></defs>`;
      s += rect(0, 0, 480, 300, 'url(#cdp)');
      const x = 18, y = 104, W = 444, H = 112;
      // The subpanel arrows above the clock and the mail.
      [[x + 20 + 6, 64], [x + W - 20 - 6 - 64, 64]].forEach(([ax]) => {
        s += motif(ax, y - 22, 64, 22, CDE.tan, 2);
        s += `<path d="M${ax + 24} ${y - 7} l8 -8 l8 8 z" fill="${CDE.tanD}"/>`;
      });
      s += motif(x, y, W, H, CDE.tan, 3);
      // Handles at both ends: stippled grips.
      [x + 6, x + W - 20].forEach((hx) => {
        s += motif(hx, y + 8, 14, H - 16, CDE.tan, 2, true);
        for (let j = 0; j < 9; j++) s += rect(hx + 4, y + 16 + j * 10, 2, 2, CDE.tanD) + rect(hx + 8, y + 21 + j * 10, 2, 2, CDE.tanL);
      });
      // The clock.
      const ccx = x + 20 + 6 + 32, ccy = y + H / 2;
      s += motif(ccx - 32, y + 10, 64, H - 20, CDE.tan, 2, true);
      s += `<circle cx="${ccx}" cy="${ccy}" r="24" fill="#F2EBDD" stroke="#3B342A" stroke-width="2.5"/>`;
      for (let k = 0; k < 12; k++) {
        const a = (k * Math.PI) / 6;
        s += `<rect x="${(ccx + Math.sin(a) * 19 - 1.2).toFixed(1)}" y="${(ccy - Math.cos(a) * 19 - 1.2).toFixed(1)}" width="2.4" height="2.4" fill="#3B342A"/>`;
      }
      s += `<path d="M${ccx} ${ccy} L${ccx + 9} ${ccy - 7}" stroke="#3B342A" stroke-width="3" stroke-linecap="round"/><path d="M${ccx} ${ccy} L${ccx - 4} ${ccy - 17}" stroke="#3B342A" stroke-width="2" stroke-linecap="round"/><circle cx="${ccx}" cy="${ccy}" r="2.5" fill="${CDE.mauve}"/>`;
      // The mail: an envelope in a slot.
      const mx = x + W - 20 - 6 - 64;
      s += motif(mx, y + 10, 64, H - 20, CDE.tan, 2, true);
      s += `<g transform="translate(${mx + 10} ${y + 36})"><rect width="44" height="30" fill="#F2EBDD" stroke="#3B342A" stroke-width="2.5"/><path d="M0 0 L22 17 L44 0" fill="none" stroke="#3B342A" stroke-width="2.5"/><rect x="30" y="-8" width="5" height="14" fill="${CDE.mauve}"/></g>`;
      // The four workspace buttons.
      const faces = [CDE.teal, CDE.mauve, CDE.sand, CDE.sage];
      const bw = 58, gap = 6, bx0 = 240 - (4 * bw + 3 * gap) / 2;
      const ch = [...w];
      s += motif(bx0 - 6, y + 8, 4 * bw + 3 * gap + 12, H - 16, CDE.tan, 2, true);
      ch.forEach((c, i) => {
        const bx = bx0 + i * (bw + gap), pressed = i === 1;
        s += motif(bx, y + 14, bw, H - 28, faces[i], 3, pressed);
        const d = pressed ? 2 : 0;
        const ink = i === 2 ? '#3B342A' : '#FBF6EC';
        s += OG.text(c, { x: bx + bw / 2 + d, y: y + H / 2 + 19 + d, size: 54, family: "'Bai Jamjuree', sans-serif", weight: 700, fill: ink, anchor: 'middle' });
      });
      return OG.logoSvg(s);
    },
    icon: () => {
      let s = motif(4, 4, 56, 56, CDE.teal, 4);
      s += OG.text('Og', { x: 32, y: 42, size: 28, family: "'Bai Jamjuree', sans-serif", weight: 700, fill: '#FBF6EC', anchor: 'middle' });
      return OG.iconSvg(s);
    },
  });

  /* 63. Tilted Cube ----------------------------------------------------- */

  // Thin geometric letters in a 50x100 cell, as stroked paths.
  function geoLetter(c, x0) {
    const cx = x0 + 25;
    const caps = { y0: 20, y1: 80 }, low = { y0: 38, y1: 80 };
    const ell = (y0, y1, rx) => `<ellipse cx="${cx}" cy="${(y0 + y1) / 2}" rx="${rx}" ry="${(y1 - y0) / 2}"/>`;
    const S = (y0, y1, w) => {
      const h = y1 - y0, a = w / 2, b = h / 4;
      const p = (ex, ey, t) => [ex + a * Math.cos(t), ey + b * Math.sin(t)].map((v) => v.toFixed(2)).join(' ');
      const t1 = (-35 * Math.PI) / 180, t2 = (145 * Math.PI) / 180;
      return `<path d="M${p(cx, y0 + b, t1)} A${a} ${b} 0 1 0 ${cx} ${y0 + 2 * b} A${a} ${b} 0 1 1 ${p(cx, y0 + 3 * b, t2)}"/>`;
    };
    switch (c) {
      case 'O': return ell(caps.y0, caps.y1, 19);
      case 'o': return ell(low.y0, low.y1, 15);
      case 'G': {
        const ry = 30, rx = 19, cy = 50, t = (-40 * Math.PI) / 180;
        return `<path d="M${(cx + rx * Math.cos(t)).toFixed(2)} ${(cy + ry * Math.sin(t)).toFixed(2)} A${rx} ${ry} 0 1 0 ${cx + rx} ${cy} H${cx + 2}"/>`;
      }
      case 'g': return ell(low.y0, low.y1, 15) + `<path d="M${cx + 15} ${low.y0} V${low.y1 + 6} A15 12 0 0 1 ${cx - 13} ${low.y1 + 12}"/>`;
      case 'S': return S(caps.y0, caps.y1, 34);
      case 's': return S(low.y0, low.y1, 26);
      default: return '';
    }
  }

  function cube(cx, cy, size, w, opts = {}) {
    const yaw = ((opts.yaw ?? 38) * Math.PI) / 180, pitch = ((opts.pitch ?? 24) * Math.PI) / 180, roll = ((opts.roll ?? -28) * Math.PI) / 180;
    const P = ([x, y, z]) => {
      // yaw about the vertical axis, then pitch toward the viewer, then roll in the picture plane.
      let X = x * Math.cos(yaw) - z * Math.sin(yaw), Z = x * Math.sin(yaw) + z * Math.cos(yaw), Y = y;
      const Y2 = Y * Math.cos(pitch) - Z * Math.sin(pitch);
      const sx = X, sy = -Y2;
      return [cx + size * (sx * Math.cos(roll) - sy * Math.sin(roll)), cy + size * (sx * Math.sin(roll) + sy * Math.cos(roll))];
    };
    const face = (o, u, v) => {
      const O = P(o), U = P(u), V = P(v);
      return { O, U: [U[0] - O[0], U[1] - O[1]], V: [V[0] - O[0], V[1] - O[1]] };
    };
    // Front-left face (x from -1 to 1 at z = -1), right face (z from -1 to 1 at x = 1), top (y = 1).
    const L = face([-1, 1, 1], [1, 1, 1], [-1, -1, 1]);
    const R = face([1, 1, 1], [1, 1, -1], [1, -1, 1]);
    const T = face([-1, 1, -1], [1, 1, -1], [-1, 1, 1]);
    const poly = (f) => {
      const { O, U, V } = f;
      const pts = [O, [O[0] + U[0], O[1] + U[1]], [O[0] + U[0] + V[0], O[1] + U[1] + V[1]], [O[0] + V[0], O[1] + V[1]]];
      return pts.map((p) => p.map((v) => v.toFixed(2)).join(',')).join(' ');
    };
    const mat = (f) => `matrix(${(f.U[0] / 100).toFixed(4)} ${(f.U[1] / 100).toFixed(4)} ${(f.V[0] / 100).toFixed(4)} ${(f.V[1] / 100).toFixed(4)} ${f.O[0].toFixed(2)} ${f.O[1].toFixed(2)})`;
    const cols = opts.colors || ['#FF5555', '#FFFF55', '#55FF55', '#55FFFF'];
    const ch = [...w];
    let s = `<defs><linearGradient id="ct" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3A3A3A"/><stop offset="1" stop-color="#1A1A1A"/></linearGradient>
      <linearGradient id="cl" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#1E1E1E"/><stop offset="1" stop-color="#0C0C0C"/></linearGradient>
      <linearGradient id="cr" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#121212"/><stop offset="1" stop-color="#030303"/></linearGradient></defs>`;
    s += `<polygon points="${poly(T)}" fill="url(#ct)"/><polygon points="${poly(L)}" fill="url(#cl)"/><polygon points="${poly(R)}" fill="url(#cr)"/>`;
    const sw = opts.stroke || 4.2;
    if (!opts.noLetters) {
      const inset = 'translate(6 5) scale(0.88)';
      s += `<g transform="${mat(L)}"><g transform="${inset}" fill="none" stroke-width="${sw}" stroke-linecap="round">` +
        `<g stroke="${cols[0]}">${geoLetter(ch[0], 0)}</g><g stroke="${cols[1]}">${geoLetter(ch[1], 50)}</g></g></g>`;
      s += `<g transform="${mat(R)}"><g transform="${inset}" fill="none" stroke-width="${sw}" stroke-linecap="round">` +
        `<g stroke="${cols[2]}">${geoLetter(ch[2], 0)}</g><g stroke="${cols[3]}">${geoLetter(ch[3], 50)}</g></g></g>`;
    }
    // Edge highlights where faces meet.
    const e = (a, b) => `<line x1="${a[0].toFixed(2)}" y1="${a[1].toFixed(2)}" x2="${b[0].toFixed(2)}" y2="${b[1].toFixed(2)}"/>`;
    s += `<g stroke="#5A5A5A" stroke-width="${opts.edge || 1.2}" stroke-linecap="round">${e(P([-1, 1, 1]), P([1, 1, 1]))}${e(P([1, 1, 1]), P([1, 1, -1]))}${e(P([1, 1, 1]), P([1, -1, 1]))}</g>`;
    return s;
  }

  OG.add({
    num: 63,
    id: 'tilted-cube',
    name: 'Tilted Cube',
    caps: 'OgOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Unix workstations'],
    paletteTag: 'VGA 16',
    risk: 'Bold',
    scene: false,
    bg: '#EFEEEA',
    ink: '#111111',
    palette: ['#111111', '#FF5555', '#FFFF55', '#55FF55', '#55FFFF'],
    fonts: ['Jost:wght@400'],
    type: 'Thin geometric letters drawn as single strokes on the cube’s faces; Jost Regular, tracked wide, for the name beneath.',
    concept:
      'A black cube tipped up on its corner, Og drawn on one face and OS on the other, each letter in its own VGA color and following the face’s perspective. ' +
      'It has the sober confidence of a 1980s workstation brand, but the colors give it away as a machine for play.',
    logo: (w) => {
      let s = cube(240, 124, 74, w);
      s += OG.text(w.split('').join(''), { x: 240 + 9, y: 268, size: 26, family: "'Jost', sans-serif", weight: 400, fill: '#111111', anchor: 'middle', ls: 18 });
      return OG.logoSvg(s);
    },
    icon: () => OG.iconSvg(cube(32, 33, 20, 'OgOS', { stroke: 7, edge: 0.6 })),
  });

  /* 68. Tube Ring ------------------------------------------------------- */

  function tubeRing(cx, cy, S, t, id) {
    const h = S / 2, a = cx - h, b = cy - h, o = t * 0.42;
    // Four tubes with rounded ends, each running the full side and a little past.
    const tubes = [
      { x: a - o, y: b, w: S + 2 * o, h: t, dir: 'h', g: 'p' }, // top
      { x: a + S - t, y: b - o, w: t, h: S + 2 * o, dir: 'v', g: 'b' }, // right
      { x: a - o, y: b + S - t, w: S + 2 * o, h: t, dir: 'h', g: 'p' }, // bottom
      { x: a, y: b - o, w: t, h: S + 2 * o, dir: 'v', g: 'b' }, // left
    ];
    const stops = {
      p: ['#1A0B45', '#4B2AB8', '#7C5CF0', '#D9CCFF', '#8E6CF7', '#4A27B0', '#1C0C4F'],
      b: ['#071A4D', '#1C4FC9', '#3D7BFF', '#CFE0FF', '#4E8BFF', '#1B48B8', '#081B52'],
    };
    const offs = [0, 0.16, 0.3, 0.42, 0.56, 0.8, 1];
    let defs = '';
    ['p', 'b'].forEach((k) => ['h', 'v'].forEach((d) => {
      defs += `<linearGradient id="${id}${k}${d}" x1="0" y1="0" x2="${d === 'h' ? 0 : 1}" y2="${d === 'h' ? 1 : 0}">${stops[k].map((c, i) => `<stop offset="${offs[i]}" stop-color="${c}"/>`).join('')}</linearGradient>`;
    }));
    defs += `<filter id="${id}sh" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="${t * 0.12}"/></filter>`;
    const tube = (T) => `<rect x="${T.x}" y="${T.y}" width="${T.w}" height="${T.h}" rx="${t / 2}" fill="url(#${id}${T.g}${T.dir})"/>`;
    const corners = [
      // [over, under, corner x, corner y]
      [0, 3, a, b],
      [1, 0, a + S - t, b],
      [2, 1, a + S - t, b + S - t],
      [3, 2, a, b + S - t],
    ];
    let s = `<defs>${defs}`;
    corners.forEach(([ov], k) => {
      const T = tubes[ov];
      s += `<clipPath id="${id}c${k}">${rect(corners[k][2] - 1, corners[k][3] - 1, t + 2, t + 2, '#000')}</clipPath>`;
    });
    s += `</defs>`;
    // A soft glow under the whole ring.
    s += `<rect x="${a}" y="${b}" width="${S}" height="${S}" rx="${t / 2}" fill="none" stroke="#5B3FD9" stroke-width="${t}" opacity="0.25" filter="url(#${id}sh)" transform="translate(0 ${t * 0.3})"/>`;
    tubes.forEach((T) => (s += tube(T)));
    // At each corner, redraw the tube that passes over, with a shadow on the one beneath.
    corners.forEach(([ov], k) => {
      const T = tubes[ov];
      const sh = T.dir === 'h'
        ? `<rect x="${T.x}" y="${T.y + t * 0.18}" width="${T.w}" height="${T.h}" rx="${t / 2}" fill="#05020F" opacity="0.6" filter="url(#${id}sh)"/>`
        : `<rect x="${T.x + t * 0.18}" y="${T.y}" width="${T.w}" height="${T.h}" rx="${t / 2}" fill="#05020F" opacity="0.6" filter="url(#${id}sh)"/>`;
      s += `<g clip-path="url(#${id}c${k})">${sh}${tube(T)}</g>`;
    });
    return s;
  }

  OG.add({
    num: 68,
    id: 'tube-ring',
    name: 'Tube Ring',
    caps: 'OGOS',
    flexCase: true,
    family: 'Geometric',
    sources: ['Unix workstations'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: false,
    bg: '#100C22',
    ink: '#D9D0FF',
    palette: ['#7C5CF0', '#3D7BFF', '#100C22', '#D9CCFF', '#1A0B45'],
    fonts: ['Michroma'],
    type: 'Michroma, wide and machined, set small with very open tracking.',
    concept:
      'A square ring of glossy purple and blue tubes, each passing over the next all the way round, a loop that cannot exist. ' +
      'It borrows the graphics-workstation love of rendered geometry and Escher puzzles, a fitting mark for a system that draws its own pixels.',
    logo: (w) => {
      let s = tubeRing(240, 122, 156, 40, 'tr');
      const size = fit('michroma', w, 24, 300);
      s += OG.text(w, { x: 240 + 7, y: 266, size, family: "'Michroma', sans-serif", fill: '#D9D0FF', anchor: 'middle', ls: 14 });
      return OG.logoSvg(s);
    },
    icon: () => OG.iconSvg(tubeRing(32, 32, 46, 14, 'ti')),
  });

  /* 73. Four Rotations -------------------------------------------------- */

  // One arm of the tile: "ogos" in monoline letters built from arcs and
  // bars, 1 unit a letter, x-height 1, with a half-unit descender zone.
  function rotArm(u, t) {
    const S = (i) => {
      const x0 = i + 0.1, x1 = i + 0.9, y0 = 0.1, y1 = 0.9, a = (x1 - x0) / 2, b = (y1 - y0) / 4, cx = i + 0.5;
      const p = (ey, ang) => [cx + a * Math.cos(ang), ey + b * Math.sin(ang)].map((v) => (v * u).toFixed(2)).join(' ');
      const t1 = (-30 * Math.PI) / 180, t2 = (150 * Math.PI) / 180;
      return `<path d="M${p(y0 + b, t1)} A${a * u} ${b * u} 0 1 0 ${cx * u} ${(y0 + 2 * b) * u} A${a * u} ${b * u} 0 1 1 ${p(y0 + 3 * b, t2)}"/>`;
    };
    const O = (i) => `<circle cx="${(i + 0.5) * u}" cy="${0.5 * u}" r="${0.4 * u}"/>`;
    const G = (i) => O(i) + `<path d="M${(i + 0.9) * u} ${0.5 * u} V${1.1 * u} A${0.3 * u} ${0.3 * u} 0 0 1 ${(i + 0.6) * u} ${1.4 * u} H${(i + 0.22) * u}"/>`;
    return `<g fill="none" stroke-width="${t * u}" stroke-linecap="butt">${O(0)}${G(1)}${O(2)}${S(3)}</g>`;
  }

  function rotTile(cx, cy, u, ink, accent, opts = {}) {
    const T = 1.5, N = 5.5, t = opts.t || 0.19;
    const ox = cx - (N * u) / 2, oy = cy - (N * u) / 2, c = (N * u) / 2;
    let arms = '';
    for (let k = 0; k < 4; k++) arms += `<g transform="rotate(${90 * k} ${c} ${c})"><g transform="translate(${T * u} 0)">${rotArm(u, t)}</g></g>`;
    const d = opts.d || 1.04;
    const dia = `<path d="M${c} ${c - d * u} L${c + d * u} ${c} L${c} ${c + d * u} L${c - d * u} ${c} Z" fill="${accent}"/>`;
    return `<g transform="translate(${ox} ${oy})${opts.rot ? ` rotate(${opts.rot} ${c} ${c})` : ''}"><g stroke="${ink}">${arms}</g>${dia}</g>`;
  }

  OG.add({
    num: 73,
    id: 'four-rotations',
    name: 'Four Rotations',
    caps: 'ogos',
    flexCase: false,
    family: 'Geometric',
    sources: ['Unix workstations'],
    paletteTag: 'Corporate',
    risk: 'Wild',
    scene: false,
    bg: '#F1EDE4',
    ink: '#1B2A6B',
    palette: ['#1B2A6B', '#E8452C', '#F1EDE4'],
    fonts: [],
    type: 'Monoline lowercase built only from circles, arcs and straight bars, with one stroke weight throughout.',
    concept:
      'The word ogos set four times around a square, each copy turned 90 degrees, so the name reads from every side of the table and the four words lock into one tile around a red diamond. ' +
      'It takes the typographic puzzle-logos of 80s workstation makers and makes it about OgOs. The concept depends on these exact lowercase letterforms, so it does not change case.',
    logo: () => OG.logoSvg(rotTile(240, 150, 44, '#1B2A6B', '#E8452C')),
    icon: () => OG.iconSvg(`<rect width="64" height="64" rx="8" fill="#F1EDE4"/>` + rotTile(32, 32, 10.6, '#1B2A6B', '#E8452C', { t: 0.27, d: 1.04 })),
  });

  /* 78. Professional ---------------------------------------------------- */

  const PRO_SQ = ['#E2483D', '#F2B400', '#38A169', '#3B82F6'];

  OG.add({
    num: 78,
    id: 'professional',
    name: 'Professional',
    caps: 'OgOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['Windows 9x'],
    paletteTag: 'Corporate',
    risk: 'Safe',
    scene: false,
    bg: '#D4D0C8',
    ink: '#111111',
    palette: ['#2E4A73', '#5D6E82', '#D4D0C8', '#FFFFFF'].concat(PRO_SQ.slice(0, 2)),
    fonts: ['Libre Franklin:wght@500;800'],
    type: 'Libre Franklin ExtraBold for the name, a Franklin Gothic voice; Libre Franklin Medium for the descriptor.',
    concept:
      'Turn-of-the-millennium corporate calm: a steel-blue band with fine ribbing, the name in a confident gothic, a descriptor line and four small colored squares as the only splash. ' +
      'It is the grown-up OgOs, the edition you would install at the office and then secretly use for games.',
    logo: (w) => {
      const size = fit('franklin', w, 86, 330);
      const wW = tw('franklin', w, size);
      const lx = 240 - wW / 2;
      let s = `<defs><linearGradient id="pb" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1F3A63"/><stop offset="0.55" stop-color="#3A5C8C"/><stop offset="1" stop-color="#66788E"/></linearGradient>
        <linearGradient id="ps" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0.16"/><stop offset="0.5" stop-color="#FFFFFF" stop-opacity="0"/><stop offset="1" stop-color="#000000" stop-opacity="0.18"/></linearGradient>
        <pattern id="pr" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="4" height="1" fill="#FFFFFF" opacity="0.07"/></pattern></defs>`;
      const by = 68, bh = 164, base = by + 112;
      s += rect(0, by, 480, bh, 'url(#pb)') + rect(0, by, 480, bh, 'url(#pr)') + rect(0, by, 480, bh, 'url(#ps)');
      s += rect(0, by, 480, 2, '#A9BEDA') + rect(0, by + bh - 2, 480, 2, '#14243D');
      PRO_SQ.forEach((c, i) => (s += rect(lx + 4 + i * 17, by + 24, 13, 13, c) + rect(lx + 4 + i * 17, by + 24, 13, 2, '#FFFFFF', 'opacity="0.35"')));
      s += OG.text(w, { x: lx + 2, y: base + 2, size, family: "'Libre Franklin', sans-serif", weight: 800, fill: '#000000', attrs: 'opacity="0.25"' });
      s += OG.text(w, { x: lx, y: base, size, family: "'Libre Franklin', sans-serif", weight: 800, fill: '#FFFFFF' });
      s += OG.text('Retro Edition', { x: lx + wW, y: by + bh - 22, size: 20, family: "'Libre Franklin', sans-serif", weight: 500, fill: '#DCE6F2', anchor: 'end', ls: 0.6 });
      return OG.logoSvg(s);
    },
    icon: () => {
      let s = `<defs><linearGradient id="ib" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3A5C8C"/><stop offset="1" stop-color="#1F3A63"/></linearGradient></defs>`;
      s += `<rect x="2" y="2" width="60" height="60" rx="12" fill="url(#ib)"/>`;
      s += OG.text('Og', { x: 32, y: 36, size: 30, family: "'Libre Franklin', sans-serif", weight: 800, fill: '#FFFFFF', anchor: 'middle', ls: -1 });
      PRO_SQ.forEach((c, i) => (s += rect(10 + i * 11.5, 44, 9, 9, c)));
      return OG.iconSvg(s);
    },
  });

  /* 83. Error Bomb ------------------------------------------------------ */

  function bombRows() {
    const rows = [];
    for (let j = 0; j < 32; j++) {
      let r = '';
      for (let i = 0; i < 32; i++) {
        const x = i + 0.5, y = j + 0.5;
        const d = Math.hypot(x - 14, y - 19);
        let c = '.';
        if (d <= 11.6) {
          c = '#';
          const g = Math.hypot(x - 14, y - 19);
          const ang = Math.atan2(y - 19, x - 14);
          if (g >= 7 && g <= 8.6 && ang > -2.75 && ang < -1.75) c = 'w';
          if (Math.hypot(x - 8.5, y - 13.5) < 1.2) c = 'w';
        }
        r += c;
      }
      rows.push(r.split(''));
    }
    // The cap and the fuse.
    const cap = [[20, 9], [21, 9], [22, 9], [19, 10], [20, 10], [21, 10], [22, 10], [23, 10], [20, 11], [21, 11], [22, 11], [23, 11], [24, 11], [21, 12], [22, 12], [23, 12]];
    cap.forEach(([i, j]) => (rows[j][i] = '#'));
    [[22, 8], [23, 7], [23, 6], [24, 5], [25, 5]].forEach(([i, j]) => (rows[j][i] = '#'));
    // The spark: a little starburst at the end of the fuse.
    [[27, 3], [26, 1], [29, 1], [29, 4], [26, 5], [27, 0], [30, 3], [28, 2], [28, 4], [26, 3]].forEach(([i, j]) => (rows[j][i] = '#'));
    return rows.map((r) => r.join(''));
  }
  const BOMB = bombRows();

  // A 1-bit push button with the thick ring of the default button.
  function macButton(x, y, w, h, label, px) {
    const rows = wordRows(CHI, label);
    const lw = rows[0].length * px;
    let s = `<rect x="${x - 7}" y="${y - 7}" width="${w + 14}" height="${h + 14}" rx="${h / 2 + 6}" fill="none" stroke="#000" stroke-width="5"/>`;
    s += `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${h / 2.6}" fill="#FFF" stroke="#000" stroke-width="2.5"/>`;
    s += OG.bitmap(rows, { '#': '#000' }, { x: Math.round(x + (w - lw) / 2), y: Math.round(y + (h - 10 * px) / 2), px });
    return s;
  }

  OG.add({
    num: 83,
    id: 'error-bomb',
    name: 'Error Bomb',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Mac OS Classic'],
    paletteTag: 'Mono',
    risk: 'Wild',
    scene: false,
    bg: '#FFFFFF',
    ink: '#000000',
    palette: ['#000000', '#FFFFFF', '#808080'],
    fonts: [],
    type: 'The name and message in a Chicago-like 1-bit system face drawn for OgOs, at two sizes.',
    concept:
      'The dreaded system error, turned inside out: a 1-bit dialog with a sparking cartoon bomb, a Restart button, and the news that the system has unexpectedly delighted you. ' +
      'A logo with a sense of humor, for a system that remembers crashes fondly.',
    logo: (w) => {
      const x = 26, y = 40, W = 428, H = 218;
      let s = `<defs><pattern id="dth" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="2" height="2" fill="#000"/><rect x="2" y="2" width="2" height="2" fill="#000"/></pattern></defs>`;
      s += `<rect x="${x + 8}" y="${y + 8}" width="${W}" height="${H}" rx="10" fill="url(#dth)"/>`;
      s += `<rect x="${x}" y="${y}" width="${W}" height="${H}" rx="10" fill="#FFF" stroke="#000" stroke-width="2.5"/>`;
      s += `<rect x="${x + 7}" y="${y + 7}" width="${W - 14}" height="${H - 14}" rx="5" fill="none" stroke="#000" stroke-width="5"/>`;
      s += OG.bitmap(BOMB, { '#': '#000', w: '#FFF' }, { x: x + 30, y: y + 32, px: 3 });
      const nrows = wordRows(CHI, w);
      const npx = Math.min(5, Math.floor(240 / nrows[0].length));
      const tx = x + 148;
      s += OG.bitmap(nrows, { '#': '#000' }, { x: tx, y: y + 28, px: npx });
      s += OG.bitmap(wordRows(CHI, 'has unexpectedly'), { '#': '#000' }, { x: tx, y: y + 28 + 10 * npx + 14, px: 2 });
      s += OG.bitmap(wordRows(CHI, 'delighted you.'), { '#': '#000' }, { x: tx, y: y + 28 + 10 * npx + 14 + 26, px: 2 });
      s += macButton(x + W - 28 - 118, y + H - 26 - 32, 118, 32, 'Restart', 2);
      return OG.logoSvg(s, { crisp: false });
    },
    icon: () => OG.iconSvg(OG.bitmap(outlineOuter(BOMB.map((r) => r.replace(/w/g, 'W')), 'e', '#W').slice(0, 33).map((r) => r.slice(0, 33)), { '#': '#000', W: '#FFF', e: '#FFF' }, { x: -1, y: 0, px: 64 / 33 }), { crisp: true }),
  });

  /* 88. Card Stack ------------------------------------------------------ */

  // A bold 1-bit serif in the manner of a classic Mac bitmap serif face.
  const NYB = {
    h: 18,
    g: {
      O: art(`
        ....#####....
        ..###...###..
        .###.....###.
        ###.......###
        ###.......###
        ###.......###
        ###.......###
        ###.......###
        ###.......###
        ###.......###
        .###.....###.
        ..###...###..
        ....#####....
        .............
        .............
        .............
        .............
        .............`),
      G: art(`
        ....#####.#..
        ..###...####.
        .###......##.
        ###........#.
        ###..........
        ###..........
        ###...#######
        ###.....###..
        ###.....###..
        ###.....###..
        .###....###..
        ..###..####..
        ....####..#..
        .............
        .............
        .............
        .............
        .............`),
      S: art(`
        ..#####.#.
        .##...###.
        ###....##.
        ####....#.
        .######...
        ..#######.
        ....######
        .......###
        #.......##
        ##......##
        ###....##.
        #.#####...
        ..........
        ..........
        ..........
        ..........
        ..........
        ..........`).map((r) => r),
      o: art(`
        ..........
        ..........
        ..........
        ..........
        ...####...
        .###..###.
        ###....###
        ###....###
        ###....###
        ###....###
        ###....###
        .###..###.
        ...####...
        ..........
        ..........
        ..........
        ..........
        ..........`),
      g: art(`
        ...........
        ...........
        ...........
        ...........
        ...####.###
        .###..####.
        ###....###.
        ###....###.
        ###....###.
        ###....###.
        ###....###.
        .###..####.
        ...####.##.
        ........##.
        .##....###.
        .########..
        ..######...
        ...........`),
      s: art(`
        .........
        .........
        .........
        .........
        .#####.#.
        ##....##.
        ###.....#
        .#####...
        ...#####.
        #.....###
        ##.....##
        ###...##.
        #.#####..
        .........
        .........
        .........
        .........
        .........`),
    },
  };
  // Fix S to the face's 18 rows (the art above gives 18 after the template trim).

  const HAND = art(`
    .....##.........
    ....#..#........
    ....#..#........
    ....#..#........
    ....#..###......
    ....#..#..##....
    ..###..#..#.##..
    .#..#..#..#..#..
    .#..#........#..
    ..#..........#..
    ..#..........#..
    ...#........#...
    ...#........#...
    ....#.......#...
    ....#.......#...
    ....#########...`);
  // Fills the inside of the hand white.
  function handRows() {
    return HAND.map((r) => {
      const a = r.indexOf('#'), b = r.lastIndexOf('#');
      return [...r].map((c, i) => (c === '#' ? '#' : i > a && i < b ? 'w' : '.')).join('');
    });
  }

  function cardStack(x, y, W, H, n, off, unit) {
    let s = `<defs><pattern id="cs" width="${2 * unit}" height="${2 * unit}" patternUnits="userSpaceOnUse"><rect width="${unit}" height="${unit}" fill="#000"/><rect x="${unit}" y="${unit}" width="${unit}" height="${unit}" fill="#000"/></pattern></defs>`;
    s += rect(x + n * off + 2 * unit + off, y + n * off + 2 * unit + off, W, H, 'url(#cs)');
    for (let k = n; k >= 0; k--) {
      const cx = x + k * off, cy = y + k * off;
      s += rect(cx, cy, W, H, '#000') + rect(cx + unit, cy + unit, W - 2 * unit, H - 2 * unit, '#FFF');
    }
    return s;
  }

  OG.add({
    num: 88,
    id: 'card-stack',
    name: 'Card Stack',
    caps: 'OgOs',
    flexCase: true,
    family: 'Interface',
    sources: ['Mac OS Classic'],
    paletteTag: 'Mono',
    risk: 'Bold',
    scene: false,
    bg: '#E4E1DA',
    ink: '#000000',
    palette: ['#000000', '#FFFFFF', '#E4E1DA'],
    fonts: [],
    type: 'A bold 1-bit bitmap serif drawn for OgOs, with heavy verticals and hairline-thin curves, in the manner of a classic system serif.',
    concept:
      'A stack of 1-bit cards with a dithered shadow, the name printed on the top card and the pointing-hand browse cursor reaching for the arrow to the next one. ' +
      'It recalls software that let anyone build things, the spirit OgOs wants for retro coding and making games.',
    logo: (w) => {
      const u = 3, W = 336, H = 198, x = 52, y = 30;
      let s = cardStack(x, y, W, H, 2, 9, u);
      const rows = wordRows(NYB, w, 2);
      const px = Math.min(5, Math.floor(276 / rows[0].length));
      const tw0 = rows[0].length * px;
      s += OG.bitmap(rows, { '#': '#000' }, { x: Math.round(x + (W - tw0) / 2), y: y + 40, px });
      // A rule under the name and the card's navigation arrows.
      s += rect(x + 36, y + 40 + 17 * px + 8, W - 72, u, '#000');
      const arrow = (ax, ay, dir) => {
        const pts = [[0, -6], [13, -6], [13, -14], [28, 0], [13, 14], [13, 6], [0, 6]].map(([px0, py0]) => `${ax + dir * px0},${ay + py0}`).join(' ');
        return `<polygon points="${pts}" fill="#FFF" stroke="#000" stroke-width="3" stroke-linejoin="miter"/>`;
      };
      const ay = y + H - 32;
      s += arrow(x + 26 + 28, ay, -1) + arrow(x + W - 26 - 28, ay, 1);
      // The browse cursor, its fingertip on the right-hand arrow.
      s += OG.bitmap(handRows(), { '#': '#000', w: '#FFF' }, { x: x + W - 26 - 28 + 16 - 6 * 4, y: ay - 2, px: 4 });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => {
      let s = cardStack(4, 8, 44, 32, 2, 4, 2);
      s += OG.bitmap(handRows(), { '#': '#000', w: '#FFF' }, { x: 26, y: 26, px: 2 });
      return OG.iconSvg(s, { crisp: true });
    },
  });

  /* 93. Warp ------------------------------------------------------------ */

  const WARP_COLS = ['#5CF2FF', '#FF4FD8', '#FFE14F', '#FF8A3D', '#7CFF6B', '#6F8CFF', '#FFFFFF'];

  function warpField(W, H, vx, vy, n, seed, len, id) {
    const rnd = OG.rng(seed);
    let defs = '', s = '';
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2 + rnd() * 0.3;
      const r0 = 18 + rnd() * 40, r1 = r0 + len * (0.5 + rnd() * 0.9);
      const x0 = vx + Math.cos(a) * r0, y0 = vy + Math.sin(a) * r0 * 0.62;
      const x1 = vx + Math.cos(a) * r1, y1 = vy + Math.sin(a) * r1 * 0.62;
      const c = WARP_COLS[i % WARP_COLS.length];
      defs += `<linearGradient id="${id}${i}" gradientUnits="userSpaceOnUse" x1="${x0.toFixed(1)}" y1="${y0.toFixed(1)}" x2="${x1.toFixed(1)}" y2="${y1.toFixed(1)}"><stop offset="0" stop-color="${c}" stop-opacity="0"/><stop offset="0.7" stop-color="${c}" stop-opacity="0.9"/><stop offset="1" stop-color="${c}" stop-opacity="0"/></linearGradient>`;
      s += `<line x1="${x0.toFixed(1)}" y1="${y0.toFixed(1)}" x2="${x1.toFixed(1)}" y2="${y1.toFixed(1)}" stroke="url(#${id}${i})" stroke-width="${(1 + rnd() * 2.6).toFixed(2)}" stroke-linecap="round"/>`;
    }
    return { defs, s };
  }

  OG.add({
    num: 93,
    id: 'warp',
    name: 'Warp',
    caps: 'OgOS',
    flexCase: true,
    family: 'Wordmark',
    sources: ['IBM & OS/2'],
    paletteTag: 'Neon',
    risk: 'Bold',
    scene: true,
    bg: '#070A24',
    ink: '#FFFFFF',
    palette: ['#070A24', '#5CF2FF', '#FF4FD8', '#FFE14F', '#FFFFFF'],
    fonts: ['Tilt Warp'],
    type: 'Tilt Warp, pushed further into a forward lean, white with a cyan and magenta edge, trailing speed lines.',
    concept:
      'Deep space, a starfield and streaks of colored light converging on the name as it drops out of warp. ' +
      'It has the bravado of mid-90s operating-system box art, when a new OS was sold like a starship.',
    logo: (w) => {
      const rnd = OG.rng(93);
      let stars = '';
      for (let i = 0; i < 130; i++) {
        const r = rnd() < 0.08 ? 1.6 : 0.5 + rnd() * 0.8;
        stars += `<circle cx="${(rnd() * 480).toFixed(1)}" cy="${(rnd() * 300).toFixed(1)}" r="${r.toFixed(2)}" fill="#FFFFFF" opacity="${(0.35 + rnd() * 0.65).toFixed(2)}"/>`;
      }
      const vx = 240, vy = 140;
      const f = warpField(480, 300, vx, vy, 56, 7, 260, 'wf');
      let s = `<defs>
        <radialGradient id="sp" cx="0.5" cy="0.47" r="0.75"><stop offset="0" stop-color="#2A2B8A"/><stop offset="0.45" stop-color="#11145073"/><stop offset="1" stop-color="#05061A"/></radialGradient>
        <radialGradient id="core" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#FFFFFF"/><stop offset="0.25" stop-color="#BFF9FF" stop-opacity="0.8"/><stop offset="1" stop-color="#5CF2FF" stop-opacity="0"/></radialGradient>
        <filter id="tg" x="-20%" y="-40%" width="140%" height="180%"><feGaussianBlur stdDeviation="6"/></filter>
        ${f.defs}</defs>`;
      s += rect(0, 0, 480, 300, '#070A24') + rect(0, 0, 480, 300, 'url(#sp)') + stars + f.s;
      s += `<ellipse cx="${vx}" cy="${vy}" rx="120" ry="60" fill="url(#core)" opacity="0.55"/>`;
      const size = fit('tiltwarp', w, 100, 330);
      const tW = tw('tiltwarp', w, size);
      const ty = vy + size * 0.36;
      // Speed lines trailing off to the left of the name.
      const lx = 240 - tW / 2;
      let lines = '';
      [[-0.62, 120, 4], [-0.42, 170, 3], [-0.2, 90, 5], [0.02, 150, 3], [-0.52, 60, 2]].forEach(([fy, len, sw]) => {
        const yy = ty + fy * size;
        lines += `<line x1="${lx - 18 - len}" y1="${yy.toFixed(1)}" x2="${lx - 14}" y2="${yy.toFixed(1)}" stroke="url(#sl)" stroke-width="${sw}" stroke-linecap="round"/>`;
      });
      s += `<defs><linearGradient id="sl" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0"/><stop offset="1" stop-color="#FFFFFF" stop-opacity="0.9"/></linearGradient></defs>` + lines;
      const tt = (fill, dx, dy, extra = '') => `<text x="${240 + dx}" y="${(ty + dy).toFixed(1)}" font-size="${size.toFixed(1)}" font-family="'Tilt Warp', sans-serif" text-anchor="middle" fill="${fill}" transform="skewX(-10)" ${extra}>${OG.esc(w)}</text>`;
      const sk = Math.tan((10 * Math.PI) / 180) * ty;
      s += `<g transform="translate(${sk.toFixed(1)} 0)">`;
      s += tt('#5CF2FF', 0, 0, 'filter="url(#tg)" opacity="0.8"');
      s += tt('#FF4FD8', 4, 3) + tt('#5CF2FF', -3, -2) + tt('#FFFFFF', 0, 0);
      s += `</g>`;
      return OG.logoSvg(s);
    },
    icon: () => {
      const f = warpField(64, 64, 32, 32, 22, 3, 30, 'wi');
      let s = `<defs><linearGradient id="ir" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5CF2FF"/><stop offset="1" stop-color="#FF4FD8"/></linearGradient>${f.defs}</defs>`;
      s += `<rect x="1" y="1" width="62" height="62" rx="12" fill="#070A24"/>` + f.s.replace(/r0/g, '');
      s += `<circle cx="32" cy="32" r="12" fill="none" stroke="url(#ir)" stroke-width="6"/>`;
      return OG.iconSvg(s);
    },
  });

  /* 98. Pointer --------------------------------------------------------- */

  // A heavy outlined pixel face to match the cursor: 3-pixel stems and
  // open, 4-pixel counters.
  const PTR = {
    h: 15,
    g: {
      O: art(`
        ..#######..
        .###...###.
        ###.....###
        ###.....###
        ###.....###
        ###.....###
        ###.....###
        ###.....###
        ###.....###
        .###...###.
        ..#######..
        ...........
        ...........
        ...........
        ...........`),
      G: art(`
        ..#######..
        .###...###.
        ###........
        ###........
        ###........
        ###...#####
        ###.....###
        ###.....###
        ###.....###
        .###...####
        ..#####.###
        ...........
        ...........
        ...........
        ...........`),
      S: art(`
        .########.
        ###....###
        ###.......
        ###.......
        .########.
        .......###
        .......###
        .......###
        ###....###
        ###....###
        .########.
        ..........
        ..........
        ..........
        ..........`),
      o: art(`
        ..........
        ..........
        ..........
        ..######..
        .###..###.
        ###....###
        ###....###
        ###....###
        ###....###
        .###..###.
        ..######..
        ..........
        ..........
        ..........
        ..........`),
      g: art(`
        .........
        .........
        .........
        .########
        ###...###
        ###...###
        ###...###
        ###...###
        ###...###
        ###...###
        .########
        ......###
        ......###
        ###...###
        .#######.`),
      s: art(`
        ........
        ........
        ........
        .#######
        ###.....
        ###.....
        .######.
        .....###
        .....###
        .....###
        #######.
        ........
        ........
        ........
        ........`),
    },
  };

  // The arrow: black outline, white body, and an O cut through to the ground.
  const ARROW = art(`
    k...............
    kk..............
    kwk.............
    kwwk............
    kwwwk...........
    kwwwwk..........
    kwwwwwk.........
    kwwwwwwk........
    kwwwwwwwk.......
    kwwoooowwk......
    kwowwwwowwk.....
    kwowwwwowwwk....
    kwowwwwowwwwk...
    kwowwwwowwwwwk..
    kwwoooowwkkkkkk.
    kwwwwkwwwk......
    kwwwk.kwwwk.....
    kwwk..kwwwk.....
    kwk....kwwwk....
    kk.....kwwwk....
    k.......kwwwk...
    ........kwwwk...
    .........kkk....`);

  OG.add({
    num: 98,
    id: 'pointer',
    name: 'Pointer',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Windows 9x', 'Mac OS Classic'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    scene: false,
    bg: '#55FFFF',
    ink: '#000000',
    palette: ['#55FFFF', '#FFFFFF', '#000000'],
    fonts: [],
    type: 'Heavy black pixel letters drawn for OgOs, with 3-pixel stems and stepped round corners, as chunky as the cursor’s own outline.',
    concept:
      'The arrow pointer, pixel-perfect, with an O cut clean through its body so the ground shows through. ' +
      'Everything on a desktop starts with this arrow, and the name beside it is drawn on the same pixel grid, as plain and certain as the pointer itself.',
    logo: (w) => {
      const rows = wordRows(PTR, w);
      const px = rows[0].length > 40 ? 5.5 : 6, ap = 8, aw = 16 * ap, nw = rows[0].length * px;
      const gap = 24, total = aw + gap + nw;
      const ax = Math.round(240 - total / 2), ay = 58;
      let s = OG.bitmap(ARROW, { k: '#000', w: '#FFF' }, { x: ax, y: ay, px: ap });
      const nx = ax + aw + gap, ny = Math.round(ay + 23 * ap / 2 - 6.5 * px);
      s += OG.bitmap(rows, { '#': '#000' }, { x: nx, y: ny, px });
      return OG.logoSvg(s, { crisp: true });
    },
    icon: () => OG.iconSvg(`<rect x="0" y="0" width="64" height="64" rx="10" fill="#55FFFF"/>` + OG.bitmap(ARROW, { k: '#000', w: '#FFF' }, { x: 13, y: 3, px: 2.5 }), { crisp: true }),
  });
})();
