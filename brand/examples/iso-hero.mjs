// The Isometric theme as a hero image, 1200 x 630 (from #105).
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Isometric hero',
  use: 'A header or card in the Isometric theme: the desk set in the round, and the name in OgOs Display with its shadow.',
  theme: 'isometric',
};

export default () => {
  const P = B.PALETTE, W = 1200, H = 630, px = 6;
  const rows = B.deskSetRows({ shadow: true });
  const art = B.drawIso(rows, 110, (H - rows.length * px) / 2 + 10, px, P.blue);
  const tx = 110 + rows[0].length * px + 64;
  const name = B.pixelText('OgOs', { face: 'display', px: 8, x: tx, y: 222, fill: P.cream, shadow: { dx: 1, dy: 1, fill: P.ink } });
  const line = B.pixelText('A 32-bit hobby OS', { face: 'label', px: 4, x: tx, y: 222 + 14 * 8 + 28, fill: P.sky });
  return B.svgDoc(W, H, art + name.svg + line.svg, { bg: P.blue, crisp: true, title: 'OgOs' });
};
