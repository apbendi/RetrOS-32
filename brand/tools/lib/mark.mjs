/*
 * The mark: the RetrOS monitor, front on. A dark outline, a cream case, a
 * dark screen with the orange LED in its bottom-left corner, a slot on the
 * chin, and a steel foot. GUIDE.md, "The mark", has its anatomy.
 *
 * It comes as pixel masters drawn at fixed sizes (tile16, tile32,
 * computer), and as frame(), the same construction at any size for
 * lockups and themes.
 */
import { PALETTE as P, RAMPS, Grid, bitmap, rect, f2 } from './core.mjs';
import { sunRows } from './sun.mjs';

// The keys pixel masters use.
export const MARK_MAP = {
  k: P.ink, c: P.cream, w: P.white, d: RAMPS.cream[5],
  s: P.screen, q: RAMPS.night[3],
  o: P.orange, r: P.red, p: P.peach,
  t: P.steel, T: P.steelDark,
  b: P.blue, L: P.blueLight, D: P.blueDeep,
};

/*
 * The flat monitor as pixel art: w by h pixels with a one-pixel outline.
 *   screenH: the screen's height; the screen starts 3 pixels in
 *   led: [w, h] of the LED at the screen's bottom left
 *   slot: the slot's width on the chin (0 for none)
 *   foot: the foot's height, its top row darker
 *   inset: the case's width around the screen (default 2)
 */
export function monitorPixels(w, h, o = {}) {
  const inset = o.inset ?? 2, foot = o.foot ?? 4, screenH = o.screenH ?? Math.round(h * 0.45);
  const [lw, lh] = o.led || [4, 2];
  const g = new Grid(w, h);
  g.fill(0, 0, w - 1, h - 1, 'k');
  g.fill(1, 1, w - 2, h - 2, 'c');
  const sx = 1 + inset, sy = 1 + inset, sw = w - 2 - 2 * inset;
  g.rect(sx, sy, sw, screenH, 's');
  g.rect(sx, sy + screenH - lh, lw, lh, 'o');
  if (foot) {
    g.rect(1, h - 1 - foot, w - 2, foot, 't');
    g.rect(1, h - 1 - foot, w - 2, 1, 'T');
  }
  if (o.slot) g.rect(w - 1 - inset - o.slot, h - 1 - foot - 2, o.slot, 1, 'k');
  return g;
}

/*
 * The app icon: the monitor on a beveled tile of Desktop Blue, with an ink
 * edge. Drawn at 16 and 32 pixels; scale the 32 by whole numbers for
 * bigger sizes. The 16 drops the edge and bevels to keep the monitor big.
 */
export function tile(size = 32) {
  const g = new Grid(size, size);
  if (size >= 32) {
    g.fill(0, 0, size - 1, size - 1, 'k');
    g.fill(1, 1, size - 2, size - 2, 'b');
    g.fill(1, 1, size - 2, 1, 'L').fill(1, 1, 1, size - 2, 'L');
    g.fill(2, size - 2, size - 2, size - 2, 'D').fill(size - 2, 2, size - 2, size - 2, 'D');
    g.stamp(monitorPixels(20, 22, { screenH: 10, led: [4, 2], slot: 6, foot: 4 }).rows(), 6, 5);
  } else {
    // At 16 the tile goes edge to edge, without its outline or bevels.
    g.fill(0, 0, size - 1, size - 1, 'b');
    g.stamp(monitorPixels(12, 12, { inset: 1, screenH: 6, led: [3, 2], slot: 0, foot: 2 }).rows(), 2, 2);
  }
  return g.rows();
}

/*
 * The desktop icon: the monitor at 32 by 32, 9x style, with beveled cream
 * plastic and a floppy slot on the chin. screen: 'sun' (the sunrise) or
 * 'led' (the base mark's LED).
 */
export function computer(screen = 'sun') {
  const g = new Grid(32, 32);
  const fill = (x0, y0, x1, y1, k) => g.fill(x0, y0, x1, y1, k);
  fill(2, 1, 29, 29, 'k');
  fill(3, 2, 28, 28, 'c');
  fill(3, 2, 27, 2, 'w'); fill(3, 2, 3, 27, 'w');
  fill(28, 3, 28, 28, 'd'); fill(4, 28, 28, 28, 'd');
  fill(5, 4, 26, 18, 'k');
  fill(6, 5, 26, 18, 's');
  fill(6, 5, 26, 5, 'q'); fill(6, 5, 6, 18, 'q');
  if (screen === 'sun') {
    // The sun in the bottom-left corner of the screen.
    for (let j = 6; j <= 18; j++) {
      for (let i = 7; i <= 26; i++) {
        const k = 18 - j, d = Math.hypot(i - 10.5, k + 0.5);
        if (d <= 7.6 && k !== 1 && k !== 3) g.set(i, j, k >= 4 ? 'o' : 'r');
      }
    }
  } else {
    fill(7, 17, 10, 18, 'o');
    fill(7, 17, 7, 17, 'p');
  }
  fill(5, 19, 26, 19, 'w');
  fill(18, 22, 25, 22, 'k'); fill(18, 23, 25, 23, 'w');
  fill(3, 25, 27, 27, 't'); fill(3, 25, 27, 25, 'T');
  fill(28, 25, 28, 28, 'T');
  return g.rows();
}

// Draws a pixel master at (x, y), px output pixels a pixel.
export const drawMark = (rows, x = 0, y = 0, px = 1) => bitmap(rows, MARK_MAP, { x, y, px });

/*
 * The monitor at any size, built from rectangles: for lockups and themes.
 * Returns { svg, screen: {x, y, w, h}, chin: {y0, y1} }.
 *   line: the outline's width (6)
 *   inset: from the outer edge to the screen (18)
 *   screenH: the screen's height
 *   foot: the foot's height (22; 0 for none)
 *   led: [w, h] of the LED at the screen's bottom left, or null
 *   slot: [w, h] of the slot at the chin's right, or null
 *   colors: { line, frame, screen, foot, footTop, led }
 */
export function frame(x, y, W, H, o = {}) {
  const line = o.line ?? 6, inset = o.inset ?? 18, foot = o.foot ?? 22;
  const c = { line: P.ink, frame: P.cream, screen: P.screen, foot: P.steel, footTop: P.steelDark, led: P.orange, ...o.colors };
  const sx = x + inset, sy = y + inset, sw = W - 2 * inset, sh = o.screenH ?? Math.round((H - 2 * inset) * 0.6);
  let s = line ? rect(x, y, W, H, c.line) : '';
  s += rect(x + line, y + line, W - 2 * line, H - 2 * line, c.frame);
  if (foot) {
    s += rect(x + line, y + H - line - foot, W - 2 * line, foot, c.foot);
    s += rect(x + line, y + H - line - foot, W - 2 * line, Math.max(1, Math.round(foot / 5)), c.footTop);
  }
  s += rect(sx, sy, sw, sh, c.screen);
  if (o.led) s += rect(sx, sy + sh - o.led[1], o.led[0], o.led[1], c.led);
  const chin = { y0: sy + sh, y1: y + H - line - foot };
  if (o.slot) {
    const [w, h] = o.slot;
    s += rect(x + W - inset - w, f2((chin.y0 + chin.y1) / 2 - h / 2), w, h, c.line);
  }
  return { svg: s, screen: { x: sx, y: sy, w: sw, h: sh }, chin };
}

/*
 * The sunrise on a frame()'s screen: a striped sun at the bottom left, on
 * a grid of px pixels. r and cx are in grid cells. Returns markup.
 */
export function screenSun(screen, px, r, cx, gaps) {
  const cols = Math.floor(screen.w / px), rows = Math.floor(screen.h / px);
  return bitmap(sunRows(cols, rows, r, gaps, cx), { o: P.orange, r: P.red }, { x: screen.x, y: screen.y + screen.h - rows * px, px });
}
