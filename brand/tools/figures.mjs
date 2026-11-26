/*
 * The reference figures GUIDE.md shows: the palette, the type, the mark's
 * anatomy, and the do's and don'ts. Each returns an SVG document; the
 * catalog renders them into assets/reference/.
 *
 * Captions are set in RetrOS System, the kernel's font, so the figures
 * are on brand themselves.
 */
import * as B from './lib/index.mjs';

const P = B.PALETTE;
const sys = (text, x, y, fill = P.ink, px = 2, anchor) => B.pixelText(text, { face: 'system', px, x, y, fill, anchor }).svg;

/* ---------------------------------------------------------- the palette */

const ROLES = {
  blue: 'Desktop Blue', ink: 'Ink', cream: 'Case Cream', paper: 'Paper', orange: 'Sunrise Orange', red: 'Signal Red', screen: 'Screen',
  blueDeep: 'Deep Blue', blueLight: 'Light Blue', sky: 'Sky', steel: 'Steel', steelDark: 'Dark Steel', white: 'White', peach: 'Peach',
  space: 'Space', charcoal: 'Charcoal',
};
const CORE = ['blue', 'ink', 'cream', 'paper', 'orange', 'red', 'screen'];

export function paletteFigure() {
  const W = 1200, m = 32, colW = 284, sw = 64;
  let s = '', y = m;
  const group = (keys, title) => {
    s += sys(title, m, y, P.steelDark);
    y += 30;
    keys.forEach((k, n) => {
      const x = m + (n % 4) * colW, yy = y + Math.floor(n / 4) * (sw + 20);
      s += B.rect(x, yy, sw, sw, P[k]);
      if (['paper', 'white', 'cream'].includes(k)) s += `<rect x="${x + 0.5}" y="${yy + 0.5}" width="${sw - 1}" height="${sw - 1}" fill="none" stroke="${B.RAMPS.cream[4]}"/>`;
      s += sys(ROLES[k], x + sw + 14, yy + 12) + sys(P[k], x + sw + 14, yy + 34, P.steelDark);
    });
    y += Math.ceil(keys.length / 4) * (sw + 20) + 24;
  };
  group(CORE, 'THE CORE');
  group(['blueDeep', 'blueLight', 'sky', 'steel', 'steelDark', 'white', 'peach'], 'SUPPORT');
  group(['space', 'charcoal'], 'FOR PATCHES, AND FOR SHOWING OBJECTS ON');
  s += sys('SHADING RAMPS, FOR PIXEL ART ONLY', m, y, P.steelDark);
  y += 30;
  Object.entries(B.RAMPS).forEach(([k, ramp], n) => {
    const x = m + (n % 2) * 528, yy = y + Math.floor(n / 2) * 60;
    s += sys(k, x, yy + 12);
    ramp.forEach((c, i) => (s += B.rect(x + 90 + i * 64, yy, 64, 40, c)));
  });
  y += 120 + m;
  return B.svgDoc(W, y, s, { bg: P.paper, crisp: true, title: 'The OgOs palette' });
}

/* ------------------------------------------------------------- the type */

export function typeFigure() {
  const W = 1120, m = 32;
  let s = '', y = m;
  const head = (name, note) => {
    s += sys(name, m, y, P.ink) + sys(note, m, y + 22, P.steelDark);
    y += 56;
  };
  const line = (text, face, px, fill = P.ink) => {
    s += B.pixelText(text, { face, px, x: m, y, fill }).svg;
    y += B.FACES[face].height * px + 14;
  };
  head('OGOS DISPLAY', 'The wordmark and headlines. 2-pixel strokes; 14 rows.');
  line('OgOs', 'display', 8);
  line('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'display', 3);
  line('abcdefghijklmnopqrstuvwxyz', 'display', 3);
  line('0123456789 .,:;!?&()', 'display', 3);
  y += 16;
  head('OGOS LABEL', 'Subheads, taglines, badges. 2-pixel uprights, 1-pixel bars; 11 rows.');
  line('A 32-bit hobby OS', 'label', 4);
  line('ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789', 'label', 2);
  y += 16;
  head('OGOS MICRO', 'Small print, capitals only. 3 x 5.');
  line('32-BIT OS  MISSION 32  POWERED BY OGOS', 'micro', 4, P.red);
  y += 16;
  head('RETROS SYSTEM', 'The kernel\'s own 8 x 8 font: labels, captions, UI text.');
  line('Starting up. The quick brown fox jumps.', 'system', 3);
  y += 16;
  head('JOST', 'Mission Patch theme only. Bold and Medium capitals, tracked wide.');
  y += B.jostCap(48);
  s += B.jostText('OGOS', { x: m, y, size: 48, weight: 700, tracking: 0.2, fill: P.ink });
  s += B.jostText('MISSION 32', { x: m + 260, y, size: 22, weight: 500, tracking: 0.33, fill: P.steelDark });
  y += 32;
  return B.svgDoc(W, y, s, { bg: P.paper, title: 'The OgOs typefaces' });
}

/* --------------------------------------------------------- the anatomy */

export function anatomyFigure() {
  const W = 1180, H = 420, px = 14, mx = 80, my = 56;
  let s = B.rect(40, 24, 360, H - 48, P.blue);
  s += B.drawMark(B.lockupMonitor(), mx, my, px);
  // Callouts: a point on the drawing (in the mark's pixels), a label, a note.
  const notes = [
    [[19.5, 1], 'Outline', 'Ink, one pixel all round.'],
    [[17.5, 4.5], 'Case', 'Case Cream.'],
    [[12, 8], 'Screen', 'Dark, and never spells the name.'],
    [[4, 12], 'LED', 'Sunrise Orange, at the screen\'s bottom left.'],
    [[15, 15.5], 'Slot', 'An ink line on the chin, at the right.'],
    [[10, 19], 'Foot', 'Steel, its top row Dark Steel.'],
  ];
  const lx = 440;
  notes.forEach(([[px0, py0], label, note]) => {
    const x = mx + px0 * px, y = my + py0 * px;
    s += B.rect(x - 4, y - 4, 8, 8, P.ink) + B.rect(x, y - 1, lx - 12 - x, 2, P.ink);
    s += sys(label, lx, y - 8, P.ink) + sys(note, lx + 120, y - 8, P.steelDark);
  });
  return B.svgDoc(W, H, s, { bg: P.paper, crisp: true, title: 'The mark\'s anatomy' });
}

/* ----------------------------------------------------- do's and don'ts */

export function dontsFigure() {
  const cw = 300, ch = 210, gx = 24, gy = 96, m = 32, cols = 4;
  const panels = [];
  const mon = B.lockupMonitor();
  const panel = (ok, caption, art, ground = P.blue) => panels.push({ ok, caption, art, ground });

  panel(true, ['The lockup on blue,', 'the LED bottom left.'], (x, y) => {
    const l = B.lockupH({ u: 2 });
    return B.group(l.svg, `transform="translate(${x + (cw - l.w) / 2} ${y + (ch - l.h) / 2}) "`);
  });
  panel(false, ['The light anywhere', 'but the bottom left.'], (x, y) => {
    const g = new B.Grid(20, 22);
    g.stamp(mon, 0, 0).rect(3, 3, 14, 10, 's').rect(13, 3, 4, 2, 'o');
    return B.drawMark(g.rows(), x + (cw - 80) / 2, y + (ch - 88) / 2, 4);
  });
  panel(false, ['The name on screen', 'and again below.'], (x, y) => {
    const f = B.frame(x + 100, y + 20, 100, 120, { line: 4, inset: 12, screenH: 60, foot: 12 });
    const on = B.pixelText('Og', { face: 'display', px: 3, x: f.screen.x + f.screen.w / 2, y: f.screen.y + 9, fill: P.cream, anchor: 'middle' });
    const under = B.pixelText('OgOs', { face: 'label', px: 3, x: x + cw / 2, y: y + 150, fill: P.cream, anchor: 'middle' });
    return f.svg + on.svg + under.svg;
  });
  panel(false, ['Letters on tiles.'], (x, y) => {
    let s = '';
    const cols4 = [P.blue, P.orange, P.red, P.ink];
    [...'OgOs'].forEach((c, n) => {
      const bx = x + 30 + n * 62;
      s += B.rect(bx, y + 60, 56, 90, cols4[n]) + B.pixelText(c, { face: 'display', px: 4, x: bx + 12, y: y + 70, fill: P.paper }).svg;
    });
    return s;
  }, P.paper);
  panel(false, ['A pointer or a', 'selection box.'], (x, y) => {
    const ptr = ['k.......', 'kk......', 'kwk.....', 'kwwk....', 'kwwwk...', 'kwwwwk..', 'kwwwwwk.', 'kwwkkkkk', 'kwk.....', 'kk......'];
    const t = B.pixelText('OgOs', { face: 'system', px: 3, x: x + cw / 2, y: y + 160, fill: P.white, anchor: 'middle' });
    let ring = B.rect(t.x - 9, y + 151, t.w + 18, 42, P.red);
    for (let i = 0; i < t.w + 18; i += 6) ring += B.rect(t.x - 9 + i, y + 151, 3, 3, P.white) + B.rect(t.x - 9 + i, y + 190, 3, 3, P.white);
    return B.drawMark(B.computer('sun'), x + cw / 2 - 64, y + 10, 4) + ring + t.svg + B.bitmap(ptr, { k: P.ink, w: P.white }, { x: x + cw / 2 + 30, y: y + 90, px: 4 });
  });
  panel(false, ['Gaps in a pattern', 'that should repeat.'], (x, y) => {
    let s = '';
    B.STRIPE_COLORS.forEach((c, n) => {
      const sy = y + 50 + n * 28, start = x + 150;
      s += B.rect(start, sy, 120, 18, c);
      [1, 2, 3, 4].forEach((k) => { if (!((n + k) % 3 === 0)) s += B.rect(start - k * 30, sy, 18, 18, c); });
    });
    return s;
  });
  panel(false, ['The mark stretched', 'or squashed.'], (x, y) =>
    B.bitmap(mon, B.MARK_MAP, { x: x + (cw - 160) / 2, y: y + (ch - 66) / 2, px: 8, py: 3 }));
  panel(false, ['Pixels of uneven', 'sizes.'], (x, y) =>
    B.group(B.drawMark(mon, 0, 0, 1), `transform="translate(${x + (cw - 20 * 4.6) / 2} ${y + (ch - 22 * 4.6) / 2}) scale(4.6)" data-brand-check="ignore"`));

  const rows = Math.ceil(panels.length / cols);
  const W = 2 * m + cols * cw + (cols - 1) * gx, H = 2 * m + rows * (ch + gy) - gy + 84;
  let s = '', defs = '';
  const clip = (x, y) => {
    const id = B.uid('dc');
    defs += `<clipPath id="${id}"><rect x="${x}" y="${y}" width="${cw}" height="${ch}"/></clipPath>`;
    return id;
  };
  panels.forEach((p, n) => {
    const x = m + (n % cols) * (cw + gx), y = m + Math.floor(n / cols) * (ch + gy);
    s += B.rect(x, y, cw, ch, p.ground) + B.group(p.art(x, y), `clip-path="url(#${clip(x, y)})"`);
    s += B.rect(x, y + ch, cw, 6, p.ok ? P.blue : P.red);
    s += sys(p.ok ? 'DO' : 'DON\'T', x, y + ch + 16, p.ok ? P.blue : P.red);
    p.caption.forEach((c, i) => (s += sys(c, x, y + ch + 40 + i * 20, P.ink)));
  });
  return B.svgDoc(W, H, `<defs>${defs}</defs>` + s, { bg: P.paper, crisp: true, title: 'Do and don\'t' });
}
