// A boot screen, 640 x 480: what you see while OgOs starts.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Boot splash',
  use: 'Shown while the OS starts: the screen\'s own dark, the desktop icon with its sunrise, the name and a progress bar.',
  theme: 'base',
};

export default () => {
  const P = B.PALETTE, W = 640, H = 480, u = 4;
  const icon = B.drawMark(B.computer('sun'), (W - 32 * u) / 2, 96, u);
  const name = B.pixelText('OgOs', { face: 'display', px: 4, x: W / 2, y: 96 + 32 * u + 24, fill: P.cream, anchor: 'middle' });
  // Ten segments, four lit: the bar is pixel art too.
  let bar = '';
  const segW = 16, gap = 4, bx = (W - (10 * segW + 9 * gap)) / 2, by = 390;
  for (let i = 0; i < 10; i++) bar += B.rect(bx + i * (segW + gap), by, segW, 8, i < 4 ? P.orange : P.blueDeep);
  const status = B.pixelText('Starting up', { face: 'system', px: 2, x: W / 2, y: by + 24, fill: P.steel, anchor: 'middle' });
  return B.svgDoc(W, H, icon + name.svg + bar + status.svg, { bg: P.screen, crisp: true, title: 'OgOs is starting' });
};
