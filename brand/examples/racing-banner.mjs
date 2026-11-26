// The Racing Stripes theme as a banner, 1500 x 500.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Racing banner',
  use: 'A wide header in the Racing Stripes theme: the racing lockup, big, on Desktop Blue.',
  theme: 'stripes',
};

export default () => {
  const P = B.PALETTE, W = 1500, H = 500;
  const r = B.racing({ px: 6 });
  const k = 2;
  return B.svgDoc(W, H, B.group(r.svg, `transform="translate(${Math.round((W - r.w * k) / 2)} ${Math.round((H - r.h * k) / 2)}) scale(${k})"`), {
    bg: P.blue, title: 'OgOs',
  });
};
