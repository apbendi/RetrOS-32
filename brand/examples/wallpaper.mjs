// A desktop wallpaper at 1024 x 768, one of the resolutions OgOs runs at.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Wallpaper',
  use: 'The OS\'s own desktop: Desktop Blue with the sun coming up at the lower left, and the name small in the corner.',
  theme: 'base',
};

export default () => {
  const P = B.PALETTE, W = 1024, H = 768;
  const sun = B.sun({ x: 0, y: H - 320, w: 560, h: 320, px: 8, r: 36, cx: 18 });
  const name = B.pixelText('OgOs', { face: 'display', px: 3, x: W - 40, y: H - 40 - 14 * 3, fill: P.blueLight, anchor: 'end' });
  return B.svgDoc(W, H, sun + name.svg, { bg: P.blue, crisp: true, title: 'OgOs wallpaper' });
};
