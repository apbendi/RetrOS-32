// A social card (Open Graph / Twitter), 1200 x 630: the base identity at its plainest.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Social card',
  use: 'The link preview for the site and repository: Desktop Blue, the sun in the corner, the horizontal lockup and one line.',
  theme: 'base',
};

export default () => {
  const P = B.PALETTE, W = 1200, H = 630;
  // The sun rises from the bottom-left corner, on a 10-pixel grid.
  const sun = B.sun({ x: 0, y: H - 300, w: 520, h: 300, px: 10, r: 27, cx: 9 });
  const lock = B.lockupH({ u: 4 });
  const lx = 640, ly = 214;
  const line = B.pixelText('A 32-bit hobby OS', { face: 'label', px: 4, x: lx, y: ly + lock.h + 40, fill: P.sky });
  return B.svgDoc(W, H, sun + B.group(lock.svg, `transform="translate(${lx} ${ly})"`) + line.svg, {
    bg: P.blue, crisp: true, title: 'OgOs: a 32-bit hobby OS',
  });
};
