// A banner for the top of the README, 1280 x 320.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'README banner',
  use: 'The top of the repository README: wide and short, so the widescreen lockup carries the name.',
  theme: 'base',
};

export default () => {
  const P = B.PALETTE, W = 1280, H = 320;
  const ws = B.widescreen({ u: 8 });
  const x = Math.round((W - ws.w) / 2), y = Math.round((H - ws.h) / 2);
  const sun = B.sun({ x: 0, y: H - 160, w: 280, h: 160, px: 8, r: 18, cx: 6 });
  return B.svgDoc(W, H, sun + B.group(ws.svg, `transform="translate(${x} ${y})"`), { bg: P.blue, crisp: true, title: 'OgOs' });
};
