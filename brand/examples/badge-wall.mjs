// A wall of 88 x 31 badges at 2x, as a links page would show them.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Badge wall',
  use: 'The Web Badge theme\'s range: the same 88 x 31 frame with different lines. Make new ones with tools/make.mjs badge.',
  theme: 'badge',
  ground: B.PALETTE.paper,
  png: { scales: [1, 2] },
};

const BADGES = [
  {},
  { screen: 'sun' },
  { name: 'OgOs', line: 'POWERED BY', above: true },
  { name: 'OgOs', line: 'BUILT ON', above: true },
  { name: 'OgOs', line: 'MISSION 32' },
  { name: 'OgOs', line: 'POWERED BY', above: true, screen: 'sun' },
];

export default () => {
  const px = 2, cols = 3, gx = 24, gy = 24, m = 32;
  const W = 2 * m + cols * 88 * px + (cols - 1) * gx, H = 2 * m + 2 * 31 * px + gy;
  const s = BADGES.map((o, n) => B.badge(o, m + (n % cols) * (88 * px + gx), m + Math.floor(n / cols) * (31 * px + gy), px)).join('');
  return B.svgDoc(W, H, s, { bg: B.PALETTE.paper, crisp: true, title: 'OgOs badges' });
};
