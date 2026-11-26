/*
 * The sun: the LED grown into a striped sun rising in the bottom-left
 * corner. Orange above, red below, cut by blank bands that get closer
 * together toward the horizon. GUIDE.md, "The sun".
 */
import { PALETTE as P, bitmap } from './core.mjs';

/*
 * The rows (counted up from the bottom, 0 first) left blank in a sun of
 * radius r: 1, 2, 4, 7, 11, ..., the gaps between them growing by one, up
 * to `reach` of the radius.
 */
export function sunGaps(r, reach = 0.6) {
  const out = [];
  for (let k = 1, step = 1; k < r * reach; k += step, step++) out.push(k);
  return out;
}

/*
 * A sun on a cols x rows grid, sitting on the bottom edge with its center
 * cx cells in from the left edge. Rows below `split` of the radius are
 * red, the rest orange. Keys: 'o' orange, 'r' red, '.' empty.
 */
export function sunRows(cols, rows, r, gaps = sunGaps(r), cx = 0, split = 0.5) {
  const out = [];
  for (let j = 0; j < rows; j++) {
    const k = rows - 1 - j;
    let s = '';
    for (let i = 0; i < cols; i++) {
      if (Math.hypot(i + 0.5 - cx, k + 0.5) > r || gaps.includes(k)) s += '.';
      else s += k + 0.5 >= r * split ? 'o' : 'r';
    }
    out.push(s);
  }
  return out;
}

export const SUN_MAP = { o: P.orange, r: P.red };

/*
 * Draws a sun rising from the bottom edge of a box. opts: x, y, w, h (the
 * box, in output units), px (the pixel size), r (radius in pixels), cx
 * (center from the box's left, in pixels; 0 is the corner), gaps, split,
 * colors: { o, r }.
 */
export function sun(opts) {
  const px = opts.px || 8;
  const cols = Math.ceil(opts.w / px), rows = Math.ceil(opts.h / px);
  const r = opts.r ?? rows * 0.8;
  const grid = sunRows(cols, rows, r, opts.gaps || sunGaps(r), opts.cx ?? 0, opts.split ?? 0.5);
  return bitmap(grid, { ...SUN_MAP, ...opts.colors }, { x: opts.x || 0, y: (opts.y || 0) + opts.h - rows * px, px });
}
