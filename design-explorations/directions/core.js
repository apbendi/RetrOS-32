/* Directions 1 and 2: the first two directions, and the examples the rest follow. */
(function () {
  const { C } = OG;

  /* 1. Four Panes ------------------------------------------------------- */

  // A round window of 17x17 pixels, split into four panes by one-pixel bars.
  function porthole(panes, opts) {
    const rows = [];
    for (let j = 0; j < 17; j++) {
      let s = '';
      for (let i = 0; i < 17; i++) {
        const dx = i - 8, dy = j - 8;
        if (dx * dx + dy * dy > 72) s += '.';
        else if (i === 8 || j === 8) s += '.';
        else s += j < 8 ? (i < 8 ? 'a' : 'b') : i < 8 ? 'c' : 'd';
      }
      rows.push(s);
    }
    return OG.bitmap(rows, panes, opts);
  }

  // Loose pixels trailing off the porthole's left side, as it flies.
  function trail(x, y, px, panes) {
    let out = '';
    [[-2, 3, 'a', 0.8], [-4, 5, 'a', 0.5], [-7, 2, 'a', 0.25],
     [-2, 12, 'c', 0.8], [-5, 10, 'c', 0.5], [-8, 13, 'c', 0.25]].forEach(([i, j, k, o]) => {
      out += `<rect x="${x + i * px}" y="${y + j * px}" width="${px}" height="${px}" fill="${panes[k]}" opacity="${o}"/>`;
    });
    return out;
  }

  const PANES = { a: C.lightred, b: C.lightgreen, c: C.lightblue, d: C.yellow };

  OG.add({
    num: 1,
    id: 'four-panes',
    name: 'Four Panes',
    caps: 'OgOs',
    flexCase: true,
    family: 'Pixel',
    sources: ['Windows 9x'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    bg: C.teal,
    ink: C.white,
    palette: [C.teal, C.lightred, C.lightgreen, C.lightblue, C.yellow],
    fonts: ['Archivo:ital,wght@1,900'],
    type: 'Archivo at its black weight, italic, set tight, with a hard one-step drop shadow.',
    concept:
      'The O is a round window split into four VGA panes, flying in on a trail of loose pixels. ' +
      'It borrows the energy of the 9x-era flag without the flag: a porthole you look through into the machine.',
    logo: (w) =>
      OG.logoSvg(
        trail(52, 96, 6, PANES) +
          `<g transform="translate(6 6)" opacity="0.35">${porthole({ a: C.black, b: C.black, c: C.black, d: C.black }, { x: 52, y: 96, px: 6 })}</g>` +
          porthole(PANES, { x: 52, y: 96, px: 6 }) +
          OG.text(w, { x: 331, y: 182, size: 86, family: 'Archivo, sans-serif', weight: 900, italic: true, fill: C.black, anchor: 'middle', ls: -2 }) +
          OG.text(w, { x: 327, y: 178, size: 86, family: 'Archivo, sans-serif', weight: 900, italic: true, fill: C.white, anchor: 'middle', ls: -2 }),
        { crisp: false }
      ),
    icon: () => OG.iconSvg(porthole(PANES, { x: 1.5, y: 1.5, px: 3.6 }), { crisp: true }),
  });

  /* 2. System Font ------------------------------------------------------ */

  // Faint lines on the 8x8 cell grid behind text set at `px` per pixel.
  function cellGrid(x, y, cols, px, color) {
    let out = '';
    for (let i = 0; i <= cols; i++) {
      const major = i % 8 === 0;
      out += `<rect x="${x + i * px}" y="${y}" width="1" height="${8 * px}" fill="${color}" opacity="${major ? 0.7 : 0.25}"/>`;
    }
    for (let j = 0; j <= 8; j++) {
      out += `<rect x="${x}" y="${y + j * px}" width="${cols * px}" height="1" fill="${color}" opacity="${j % 8 === 0 ? 0.7 : 0.25}"/>`;
    }
    return out;
  }

  OG.add({
    num: 2,
    id: 'system-font',
    name: 'System Font',
    caps: 'OgOs',
    flexCase: true,
    family: 'Wordmark',
    sources: ['OgOs internals'],
    paletteTag: 'VGA 16',
    risk: 'Safe',
    bg: C.blue,
    ink: C.white,
    palette: [C.blue, C.white, C.lightblue, C.yellow],
    fonts: [],
    type: "The kernel's own font8x8_basic (lib/font8.c), drawn at 11 screen pixels per font pixel, nothing redrawn.",
    concept:
      'The name set in the exact glyphs the kernel draws its terminal with, on the cell grid they live in. ' +
      'Honest and unpolished on purpose: the logo is what the OS can already render, so it looks the same on a boot screen as on a website.',
    logo: (w) => {
      // The word and one more cell for the cursor.
      const cols = w.length * 8, px = 11;
      const x = Math.round((480 - (cols + 8) * px) / 2), y = 96;
      const codes = [...w].map((ch) => 'U+' + ch.charCodeAt(0).toString(16).toUpperCase().padStart(4, '0')).join(' ');
      return OG.logoSvg(
        cellGrid(x, y, cols + 8, px, C.lightblue) +
          OG.pixText(w, { x, y, px, fill: C.white }) +
          `<rect x="${x + cols * px}" y="${y + 6 * px}" width="${px * 7}" height="${px * 2}" fill="${C.yellow}"><animate attributeName="opacity" values="1;1;0;0" dur="1.06s" repeatCount="indefinite"/></rect>` +
          OG.pixText(codes, { x, y: y + 8 * px + 22, px: 2, fill: C.lightblue }),
        { crisp: true }
      );
    },
    icon: () =>
      OG.iconSvg(
        `<rect width="64" height="64" fill="${C.blue}"/>` + OG.pixText('Og', { x: 2, y: 16, px: 4, fill: C.white, prop: true }),
        { crisp: true }
      ),
  });
})();
