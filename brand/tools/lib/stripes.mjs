/*
 * The Racing Stripes theme: the monitor leaning into speed, trailing four
 * stripes that break into evenly spaced squares, like a late-80s livery.
 * GUIDE.md, "Racing Stripes".
 */
import { PALETTE as P, rect, f2 } from './core.mjs';
import { frame } from './mark.mjs';
import { pixelText } from './type.mjs';

export const STRIPE_COLORS = [P.cream, P.orange, P.red, P.sky];
export const LEAN = -12; // degrees of skew: the theme's one angle

/*
 * Four stripes running in from the left to x1, each breaking at its tail
 * into `trail` squares the stripe's height, the gaps between them widening
 * evenly. Every stripe has every square. Starts step in by `stagger`.
 *   x0: where the first stripe's solid part starts; y: the first's top;
 *   h: a stripe's height; gap: between stripes
 */
export function stripes({ x0, x1, y, h, gap, stagger = 18, trail = 3, colors = STRIPE_COLORS }) {
  let s = '';
  colors.forEach((c, n) => {
    const sy = y + n * (h + gap), start = x0 + n * stagger;
    s += rect(start, sy, x1 - start, h, c);
    let x = start;
    for (let k = 1; k <= trail; k++) {
      x -= h * (0.4 + 0.3 * k) + h; // gap widens by 0.3 squares each step
      s += rect(x, sy, h, h, c);
    }
  });
  return s;
}

// Skews markup by the theme's lean about the horizontal line at y.
export const leanAbout = (inner, y, deg = LEAN) =>
  `<g transform="translate(0 ${f2(y)}) skewX(${deg}) translate(0 ${f2(-y)})">${inner}</g>`;

/*
 * The racing lockup: stripes, the leaning monitor and the name, leaning
 * together. Returns { svg, w, h }, drawn at the origin.
 *   text ('OgOs'), px (the name's pixel size, 6), leanType (lean the name
 *   with the monitor, true)
 */
export function racing(o = {}) {
  const text = o.text || 'OgOs', px = o.px || 6;
  const mw = 104, mh = 116, h = 14, gap = 6, trail = 3;
  // The tails reach this far left of where the first stripe's solid part starts.
  const tail = Array.from({ length: trail }, (_, k) => h * (0.4 + 0.3 * (k + 1)) + h).reduce((a, b) => a + b, 0);
  const lean = Math.tan((-LEAN * Math.PI) / 180) * (mh / 2); // how far the top leans right
  const x0 = tail + 24, mx = x0 + 96, my = 24;
  const m = frame(mx, my, mw, mh, { line: 5, inset: 13, screenH: 58, foot: 13, led: [16, 8] });
  const mark = leanAbout(stripes({ x0, x1: mx + 10, y: my + 16, h, gap }) + m.svg, my + mh / 2);
  const t = pixelText(text, { face: 'display', px, x: mx + mw + lean + 12, y: 0 });
  const ty = Math.round(my + mh / 2 - (t.face.cap * px) / 2);
  const name = pixelText(text, { face: 'display', px, x: t.x, y: ty, fill: P.cream, shadow: { dx: 1, dy: 1, fill: P.ink } }).svg;
  const w = Math.ceil(t.x + t.w + px + 24), hh = my + mh + 24;
  return { svg: mark + (o.leanType === false ? name : leanAbout(name, my + mh / 2)), w, h: hh };
}

// The racing icon, 64 x 64.
export function racingIcon() {
  const m = frame(20, 12, 36, 40, { line: 2.5, inset: 6, screenH: 18, foot: 5, led: [7, 4] });
  let s = '';
  STRIPE_COLORS.forEach((c, n) => (s += rect(2 + n * 3, 18 + n * 6, 22 - n * 3, 4, c)));
  return { svg: leanAbout(s + m.svg, 32), w: 64, h: 64 };
}
