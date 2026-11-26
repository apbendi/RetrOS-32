/*
 * The catalog: every pre-rendered asset in assets/, how it's drawn, and
 * which PNGs to make of it. tools/build.mjs renders it, together with the
 * compositions in examples/, and writes the gallery (index.html) and
 * assets/manifest.json from it.
 *
 * To add an asset, add an entry here and run `npm run build`. Fields:
 *   id       unique, kebab-case
 *   section  a key of SECTIONS
 *   path     where it goes, under brand/, without an extension
 *   title    a few words
 *   use      when to reach for it, one sentence
 *   svg      () => a whole SVG document (svgDoc)
 *   png      { scales: [1, 2, ...] } -> name.png, name@2x.png, ...
 *            { sizes: [16, 32, ...] } -> name-16.png, ... (widths in pixels)
 *   ground   the color to show it on in the gallery, if it's transparent
 */
import * as B from './lib/index.mjs';
import { paletteFigure, typeFigure, anatomyFigure, dontsFigure } from './figures.mjs';

const P = B.PALETTE;

export const SECTIONS = {
  mark: 'The mark',
  wordmark: 'The wordmark',
  lockup: 'Lockups',
  sun: 'The sun',
  isometric: 'Theme: Isometric',
  patch: 'Theme: Mission Patch',
  badge: 'Theme: Web Badge',
  stripes: 'Theme: Racing Stripes',
  reference: 'Reference figures',
};

// Pixel art as a document, one unit a pixel.
const pix = (rows, map, title, bg) =>
  B.svgDoc(rows[0].length, rows.length, B.bitmap(rows, map), { crisp: true, title, bg });

// A drawn piece ({ svg, w, h }) as a document, with an optional margin and ground.
const doc = (piece, title, o = {}) => {
  const m = o.margin || 0;
  return B.svgDoc(piece.w + 2 * m, piece.h + 2 * m, m ? B.group(piece.svg, `transform="translate(${m} ${m})"`) : piece.svg, {
    crisp: o.crisp, title, bg: o.bg,
  });
};

const ICON_SIZES = { sizes: [32, 64, 128, 256, 512, 1024] };
const BADGE_MAP = { ...B.MARK_MAP, e: B.RAMPS.cream[4], n: P.ink, m: P.red };

export const CATALOG = [
  /* ---------------------------------------------------------- the mark */
  {
    id: 'app-icon', section: 'mark', path: 'assets/mark/app-icon',
    title: 'App icon',
    use: 'The brand in one square: avatars, app icons, anywhere a square mark goes. Drawn at 32 pixels; scale by whole numbers.',
    svg: () => pix(B.tile(32), B.MARK_MAP, 'OgOs app icon'), png: ICON_SIZES, ground: P.paper,
  },
  {
    id: 'app-icon-small', section: 'mark', path: 'assets/mark/app-icon-small',
    title: 'App icon, small',
    use: 'Favicons and other spots 16 pixels across (or 32 and 48 on dense screens), where the 32 would lose detail. Drawn at 16; do not scale the 32 down.',
    svg: () => pix(B.tile(16), B.MARK_MAP, 'OgOs app icon, 16 px'), png: { sizes: [16, 32, 48] }, ground: P.paper,
  },
  {
    id: 'monitor', section: 'mark', path: 'assets/mark/monitor',
    title: 'Monitor',
    use: 'The mark on its own, for placing on Desktop Blue (or a light ground) in your own layout. 20 x 22 pixels.',
    svg: () => pix(B.lockupMonitor(), B.MARK_MAP, 'OgOs monitor'), png: { scales: [4, 8, 16] },
  },
  {
    id: 'computer-sun', section: 'mark', path: 'assets/mark/computer-sun',
    title: 'Desktop icon, sunrise',
    use: 'The mark at desktop-icon fidelity, beveled, with the sunrise on its screen. 32 x 32.',
    svg: () => pix(B.computer('sun'), B.MARK_MAP, 'OgOs computer with the sunrise'), png: ICON_SIZES,
  },
  {
    id: 'computer-led', section: 'mark', path: 'assets/mark/computer-led',
    title: 'Desktop icon, LED',
    use: 'The same icon with the base mark\'s LED: when the sun would be too much.',
    svg: () => pix(B.computer('led'), B.MARK_MAP, 'OgOs computer'), png: ICON_SIZES,
  },

  /* ------------------------------------------------------ the wordmark */
  ...['cream', 'ink'].map((c) => ({
    id: `wordmark-${c}`, section: 'wordmark', path: `assets/wordmark/wordmark-${c}`,
    title: `Wordmark, ${c}`,
    use: c === 'cream' ? 'The name on Desktop Blue or ink.' : 'The name on paper and other light grounds.',
    svg: () => doc(B.wordmark({ u: 1, fill: P[c] }), 'OgOs', { crisp: true }),
    png: { scales: [4, 8, 16] },
    ground: c === 'cream' ? P.blue : P.paper,
  })),
  {
    id: 'wordmark-shadow', section: 'wordmark', path: 'assets/wordmark/wordmark-shadow',
    title: 'Wordmark with shadow',
    use: 'Cream with a one-pixel ink shadow, for busy grounds: over isometric scenes, stripes or the sun.',
    svg: () => doc(B.wordmark({ u: 1, fill: P.cream, shadow: { dx: 1, dy: 1, fill: P.ink } }), 'OgOs', { crisp: true }),
    png: { scales: [4, 8, 16] }, ground: P.blue,
  },

  /* ------------------------------------------------------- the lockups */
  {
    id: 'lockup-horizontal-blue', section: 'lockup', path: 'assets/lockup/horizontal-on-blue',
    title: 'Horizontal, on blue',
    use: 'The primary lockup, on its ground: headers, title slides, the top of a page.',
    svg: () => doc(B.lockupH({ u: 1 }), 'OgOs', { crisp: true, margin: 8, bg: P.blue }), png: { scales: [2, 4, 8] },
  },
  ...['cream', 'ink'].map((c) => ({
    id: `lockup-horizontal-${c}`, section: 'lockup', path: `assets/lockup/horizontal-${c}`,
    title: `Horizontal, ${c} name`,
    use: c === 'cream' ? 'Transparent, for placing on blue or ink yourself.' : 'Transparent, for placing on paper or other light grounds.',
    svg: () => doc(B.lockupH({ u: 1, ground: c === 'cream' ? 'blue' : 'paper' }), 'OgOs', { crisp: true }),
    png: { scales: [2, 4, 8] }, ground: c === 'cream' ? P.blue : P.paper,
  })),
  ...['cream', 'ink'].map((c) => ({
    id: `lockup-stacked-${c}`, section: 'lockup', path: `assets/lockup/stacked-${c}`,
    title: `Stacked, ${c} name`,
    use: 'For square and tall spaces: a splash, a poster, a sidebar.',
    svg: () => doc(B.lockupV({ u: 1, ground: c === 'cream' ? 'blue' : 'paper' }), 'OgOs', { crisp: true }),
    png: { scales: [2, 4, 8] }, ground: c === 'cream' ? P.blue : P.paper,
  })),
  {
    id: 'sunrise-screen', section: 'lockup', path: 'assets/lockup/sunrise-screen',
    title: 'Sunrise screen',
    use: 'The hero lockup (from #101): the monitor with the sunrise on its screen and the name on its chin. Splash screens, covers, stickers.',
    svg: () => doc(B.sunriseScreen({ u: 1 }), 'OgOs', { crisp: true }), png: { scales: [4, 6, 12] },
  },
  {
    id: 'widescreen', section: 'lockup', path: 'assets/lockup/widescreen',
    title: 'Widescreen',
    use: 'For wide, short spaces (from #102): banners and headers. The name is on the screen, so nothing else says it.',
    svg: () => doc(B.widescreen({ u: 1 }), 'OgOs', { crisp: true }), png: { scales: [4, 8, 16] },
  },
  {
    id: 'desktop-icon', section: 'lockup', path: 'assets/lockup/desktop-icon',
    title: 'Desktop icon',
    use: 'The computer labeled as a desktop labels it (from #108), in RetrOS System. On Desktop Blue only.',
    svg: () => doc(B.desktopIcon({ u: 1 }), 'OgOs', { crisp: true }), png: { scales: [2, 4, 8] },
  },

  /* ----------------------------------------------------------- the sun */
  {
    id: 'sun-corner', section: 'sun', path: 'assets/sun/sun-corner',
    title: 'Sun, corner',
    use: 'The sun rising in a bottom-left corner, centered on the corner. 48 x 32 pixels; scale by whole numbers and set it in the corner of a layout.',
    svg: () => B.svgDoc(48, 32, B.sun({ x: 0, y: 0, w: 48, h: 32, px: 1, r: 30, cx: 0 }), { crisp: true, title: 'The sun, rising in a corner' }),
    png: { scales: [4, 8, 16] },
  },
  {
    id: 'sun-dome', section: 'sun', path: 'assets/sun/sun-dome',
    title: 'Sun, dome',
    use: 'The whole half-sun, as on #101\'s screen: set it on a horizon, toward the left.',
    svg: () => B.svgDoc(40, 20, B.sun({ x: 0, y: 0, w: 40, h: 20, px: 1, r: 19.5, cx: 20 }), { crisp: true, title: 'The sun, rising' }),
    png: { scales: [4, 8, 16] },
  },

  /* ---------------------------------------------------- the isometric */
  ...[['blue', P.blue], ['paper', P.paper]].flatMap(([g, ground]) => [
    {
      id: `iso-monitor-${g}`, section: 'isometric', path: `assets/isometric/monitor-on-${g}`,
      title: `Monitor, for ${g}`,
      use: `The monitor in the round, its shadow colored for a ${g} ground.`,
      svg: () => pix(B.isoMonitorRows({ shadow: true }), B.isoMap(ground), 'OgOs monitor, isometric'), png: { scales: [4, 8] }, ground,
    },
    {
      id: `iso-desk-set-${g}`, section: 'isometric', path: `assets/isometric/desk-set-on-${g}`,
      title: `Desk set, for ${g}`,
      use: `The whole computer, case, monitor and keyboard, with the sunrise on the screen and its shadow colored for a ${g} ground.`,
      svg: () => pix(B.deskSetRows({ shadow: true }), B.isoMap(ground), 'OgOs desk set, isometric'), png: { scales: [4, 8] }, ground,
    },
  ]),
  {
    id: 'iso-monitor-sun', section: 'isometric', path: 'assets/isometric/monitor-sun',
    title: 'Monitor with the sunrise, no shadow',
    use: 'For scenes you build yourself, where you draw the floor.',
    svg: () => pix(B.isoMonitorRows({ sun: true }), B.isoMap(), 'OgOs monitor with the sunrise, isometric'), png: { scales: [4, 8] },
  },

  /* -------------------------------------------------- the mission patch */
  {
    id: 'patch-monitor', section: 'patch', path: 'assets/patch/monitor-patch',
    title: 'Monitor patch',
    use: 'The patch cut to the monitor\'s shape (from #106). Flat, for print and embroidery; add a shadow only when showing it as an object.',
    svg: () => doc(B.monitorPatch(), 'OGOS mission patch'), png: { scales: [1, 2, 4] }, ground: P.charcoal,
  },
  {
    id: 'patch-round', section: 'patch', path: 'assets/patch/round-patch',
    title: 'Round patch',
    use: 'The round crew patch (from #107).',
    svg: () => doc(B.roundPatch(), 'OGOS mission patch'), png: { scales: [1, 2, 4] }, ground: P.charcoal,
  },
  {
    id: 'patch-round-icon', section: 'patch', path: 'assets/patch/round-patch-icon',
    title: 'Round patch, icon',
    use: 'The round patch without its lettering, for 64 pixels and below.',
    svg: () => doc(B.roundPatchIcon(), 'OgOs patch icon'), png: { sizes: [64, 128, 256] }, ground: P.charcoal,
  },

  /* ------------------------------------------------------ the web badge */
  ...[
    ['badge', 'OgOs badge', {}],
    ['badge-sun', 'OgOs badge, sunrise', { screen: 'sun' }],
    ['badge-powered-by', 'Powered by OgOs', { line: 'POWERED BY', above: true }],
  ].map(([name, title, o]) => ({
    id: name, section: 'badge', path: `assets/badge/${name}`,
    title, use: 'An 88 x 31 button. Use it at 1x, where it is crisp, or at whole multiples.',
    svg: () => pix(B.badgeRows(o), BADGE_MAP, title), png: { scales: [1, 2, 4] }, ground: P.paper,
  })),

  /* --------------------------------------------------- the racing stripes */
  {
    id: 'racing', section: 'stripes', path: 'assets/stripes/racing',
    title: 'Racing lockup',
    use: 'The monitor leaning into speed with its stripes, and the name leaning with it (from #110). On Desktop Blue.',
    svg: () => doc(B.racing(), 'OgOs'), png: { scales: [1, 2, 4] },
  },
  {
    id: 'racing-icon', section: 'stripes', path: 'assets/stripes/racing-icon',
    title: 'Racing icon',
    use: 'The racing mark in a square, for 64 pixels and up.',
    svg: () => doc(B.racingIcon(), 'OgOs'), png: { sizes: [64, 128, 256] },
  },

  /* ------------------------------------------------ reference figures */
  ...[
    ['palette', 'The palette', 'Every color the kit uses, with its name and hex value.', paletteFigure],
    ['type', 'The typefaces', 'The four pixel faces and Jost, with what each is for.', typeFigure],
    ['anatomy', 'The mark\'s anatomy', 'The parts of the monitor and their colors.', anatomyFigure],
    ['donts', 'Do and don\'t', 'The mistakes the feedback rounds ruled out.', dontsFigure],
  ].map(([name, title, use, fig]) => ({
    id: `figure-${name}`, section: 'reference', path: `assets/reference/${name}`,
    title, use, svg: fig, png: { scales: [1] }, ground: P.paper, figure: true,
  })),
];
