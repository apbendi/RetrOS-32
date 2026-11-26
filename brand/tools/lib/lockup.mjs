/*
 * Lockups: the mark and the name together. GUIDE.md, "Lockups".
 *
 * Every lockup is drawn on one pixel grid of `u` output pixels: the mark,
 * the name and the spaces between are whole numbers of u, and the name's
 * pixels are u or 2u. Each returns { svg, w, h } drawn at the origin.
 */
import { PALETTE as P, bitmap, bbox } from './core.mjs';
import { textRows, pixelText } from './type.mjs';
import { monitorPixels, computer, MARK_MAP, frame } from './mark.mjs';
import { sunRows, sunGaps, SUN_MAP } from './sun.mjs';

// The name's color on each ground.
export const NAME_ON = { blue: P.cream, paper: P.ink, ink: P.cream, cream: P.ink };

// The monitor the lockups use: 20 x 22, the app icon's.
export const lockupMonitor = () => monitorPixels(20, 22, { screenH: 10, led: [4, 2], slot: 6, foot: 4 }).rows();

/*
 * The wordmark alone: the name in OgOs Display. opts: text ('OgOs'), u
 * (pixel size, 4), fill, shadow.
 */
export function wordmark(o = {}) {
  const t = pixelText(o.text || 'OgOs', { face: 'display', px: o.u || 4, fill: o.fill || P.cream, shadow: o.shadow });
  return { svg: t.svg, w: t.w + (o.shadow ? (o.shadow.dx ?? 1) * (o.u || 4) : 0), h: t.h };
}

/*
 * The horizontal lockup: the monitor at u, the name at 2u beside it. The
 * name's capitals stand exactly as tall as the monitor (11 rows at 2u = 22
 * at u). opts: text, u (4), ground ('blue' | 'paper' | 'ink' | 'cream').
 */
export function lockupH(o = {}) {
  const u = o.u || 4, text = o.text || 'OgOs';
  const mon = lockupMonitor();
  const name = textRows(text, 'display');
  const gap = 7;
  const svg = bitmap(mon, MARK_MAP, { x: 0, y: 0, px: u }) +
    bitmap(name, { '#': NAME_ON[o.ground || 'blue'] }, { x: (20 + gap) * u, y: 0, px: 2 * u });
  return { svg, w: (20 + gap + 2 * name[0].length) * u, h: 28 * u };
}

/*
 * The stacked lockup: the monitor over the name, both at 2u. opts: text,
 * u (4), ground.
 */
export function lockupV(o = {}) {
  const u = o.u || 4, text = o.text || 'OgOs';
  const mon = lockupMonitor();
  const name = textRows(text, 'display');
  const nw = name[0].length, w = Math.max(nw, 20) * 2;
  const svg = bitmap(mon, MARK_MAP, { x: ((w - 40) / 2) * u, y: 0, px: 2 * u }) +
    bitmap(name, { '#': NAME_ON[o.ground || 'blue'] }, { x: ((w - 2 * nw) / 2) * u, y: (44 + 10) * u, px: 2 * u });
  return { svg, w: w * u, h: (44 + 10 + 28) * u };
}

/*
 * The sunrise screen (from #101): a tall monitor whose screen is the
 * sunrise, with the name once, on the chin, in OgOs Label. 39u x 45u.
 * opts: text, u (6).
 */
export function sunriseScreen(o = {}) {
  const u = o.u || 6, text = o.text || 'OgOs';
  const W = 39 * u, H = 45 * u;
  const m = frame(0, 0, W, H, { line: u, inset: 3 * u, screenH: 23 * u, foot: 4 * u });
  const sun = bitmap(sunRows(33, 23, 13, sunGaps(13), 9.5), SUN_MAP, { x: m.screen.x, y: m.screen.y, px: u });
  const rows = textRows(text, 'label'), b = bbox(rows);
  // The chin is rows 26-39: the name's ink starts 2 rows down.
  const y = (28 - b.y0) * u;
  const x = (Math.floor((39 - b.w) / 2) - b.x0) * u;
  return { svg: m.svg + sun + bitmap(rows, { '#': P.ink }, { x, y, px: u }), w: W, h: H };
}

/*
 * The widescreen (from #102): the monitor stretched wide, so the name fits
 * on the screen in OgOs Display, with the LED at the screen's bottom left.
 * For wide spaces: banners, headers. opts: text, u (8).
 */
export function widescreen(o = {}) {
  const u = o.u || 8, text = o.text || 'OgOs';
  const rows = textRows(text, 'display'), b = bbox(rows);
  const sw = Math.max(50, b.w + 12), W = sw + 6, H = 27;
  const m = frame(0, 0, W * u, H * u, { line: u, inset: 3 * u, screenH: 19 * u, foot: 2 * u, led: [4 * u, 2 * u] });
  const x = m.screen.x + (Math.floor((sw - b.w) / 2) - b.x0) * u;
  const y = m.screen.y + 4 * u;
  return { svg: m.svg + bitmap(rows, { '#': P.cream }, { x, y, px: u }), w: W * u, h: H * u };
}

/*
 * The desktop icon (from #108): the 32 x 32 computer with the sunrise, and
 * the name under it in RetrOS System, as a desktop labels an icon, with no
 * selection box. opts: text, u (4), screen ('sun' | 'led'), ground.
 */
export function desktopIcon(o = {}) {
  const u = o.u || 4, text = o.text || 'OgOs';
  const rows = textRows(text, 'system');
  const tw = rows[0].length, w = Math.max(32, tw);
  const svg = bitmap(computer(o.screen || 'sun'), MARK_MAP, { x: ((w - 32) / 2) * u, y: 0, px: u }) +
    bitmap(rows, { '#': o.ground === 'paper' ? P.ink : P.white }, { x: ((w - tw) / 2) * u, y: 35 * u, px: u });
  return { svg, w: w * u, h: (35 + 8) * u };
}
