/*
 * The Web Badge theme: 88 x 31 buttons, as the early web linked to things.
 * GUIDE.md, "Web Badge".
 *
 * A badge is pixel art at its real size: the monitor on Desktop Blue in a
 * beveled well on the left, two stripes (red, orange), and a cream panel
 * with the name in OgOs Label and a line of small print in OgOs Micro.
 * Show it at 1x where it's used, or at whole multiples to show it off.
 */
import { PALETTE as P, RAMPS, Grid, bitmap, bbox } from './core.mjs';
import { textRows } from './type.mjs';
import { monitorPixels, MARK_MAP } from './mark.mjs';
import { sunRows } from './sun.mjs';

export const BADGE_W = 88, BADGE_H = 31;

const MAP = { ...MARK_MAP, e: RAMPS.cream[4], n: P.ink, m: P.red };

/*
 * The badge's pixel rows. opts:
 *   name: the big line ('OgOs'), in OgOs Label
 *   line: the small print ('32-BIT OS'), in OgOs Micro; '' for none
 *   above: the small print above the name instead of below
 *   screen: 'led' (the base mark) or 'sun'
 * Text that doesn't fit the panel (51 pixels) throws: shorten it.
 */
export function badgeRows(o = {}) {
  const name = o.name ?? 'OgOs', line = o.line ?? '32-BIT OS';
  const g = new Grid(BADGE_W, BADGE_H);
  g.fill(0, 0, 87, 30, 'k');
  // The well: Desktop Blue, beveled.
  g.fill(1, 1, 29, 29, 'b').fill(1, 1, 29, 1, 'L').fill(1, 1, 1, 29, 'L').fill(1, 29, 29, 29, 'D').fill(29, 1, 29, 29, 'D');
  const mon = monitorPixels(19, 21, { screenH: 10, led: [4, 2], slot: 6, foot: 3 });
  // The sunrise in place of the LED: the screen is 13 x 10 at (3, 3).
  if (o.screen === 'sun') mon.rect(3, 3, 13, 10, 's').stamp(sunRows(13, 10, 6.5, [1, 3], 2.5), 3, 3);
  g.stamp(mon.rows(), 6, 5);
  // The stripes.
  g.fill(30, 1, 30, 29, 'r').fill(31, 1, 31, 29, 'o');
  // The panel: cream, beveled.
  g.fill(32, 1, 86, 29, 'c').fill(32, 1, 86, 1, 'w').fill(32, 1, 32, 29, 'w').fill(32, 29, 86, 29, 'e').fill(86, 1, 86, 29, 'e');
  // The name's ink centered in its space on the panel, and the small print.
  const nameRows = textRows(name, 'label'), nb = bbox(nameRows);
  const micro = line ? textRows(line, 'micro') : null;
  const [top, bottom] = !micro ? [2, 28] : o.above ? [12, 27] : [3, 19];
  if (nb.w > 51) throw new Error(`badge: "${name}" is ${nb.w} pixels wide in OgOs Label; the panel fits 51`);
  if (micro && micro[0].length > 51) throw new Error(`badge: "${line}" is ${micro[0].length} pixels wide in OgOs Micro; the panel fits 51`);
  const center = (w) => 33 + Math.floor((53 - w) / 2);
  const ny = top + Math.floor((bottom - top + 1 - nb.h) / 2);
  g.stamp(nameRows.slice(nb.y0, nb.y1).map((r) => r.slice(nb.x0, nb.x1)), center(nb.w), ny, { '#': 'n' });
  if (micro) g.stamp(micro, center(micro[0].length), o.above ? 5 : 21, { '#': 'm' });
  return g.rows();
}

// Draws a badge at (x, y), px screen pixels a badge pixel.
export const badge = (o = {}, x = 0, y = 0, px = 1) => bitmap(badgeRows(o), MAP, { x, y, px });
