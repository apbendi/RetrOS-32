// A patch for a release: the round patch with its own mission line.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Release patch',
  use: 'How a patch marks an occasion: the round patch with a mission line of its own, shown as an object on charcoal.',
  theme: 'patch',
  ground: B.PALETTE.charcoal,
};

export default () => {
  const P = B.PALETTE, W = 600, H = 600;
  const p = B.roundPatch({ mission: 'Release 1.0', shadow: true });
  const k = 1.8;
  return B.svgDoc(W, H, B.group(p.svg, `transform="translate(${(W - p.w * k) / 2} ${(H - p.h * k) / 2 - 6}) scale(${k})"`), {
    bg: P.charcoal, title: 'OGOS mission patch: Release 1.0',
  });
};
