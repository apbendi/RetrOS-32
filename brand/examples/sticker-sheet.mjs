// A sheet of stickers: the identity's pieces side by side, 1200 x 800.
import * as B from '../tools/lib/index.mjs';

export const meta = {
  title: 'Sticker sheet',
  use: 'How the themes sit together: both patches, the badge, the app icon, the desk set and the sunrise screen, on paper.',
  theme: 'mixed',
  ground: B.PALETTE.paper,
};

export default () => {
  const P = B.PALETTE, W = 1200, H = 800;
  const at = (piece, x, y, k = 1) => B.group(piece.svg, `transform="translate(${x} ${y})${k !== 1 ? ` scale(${k})` : ''}"`);
  const crisp = (s) => B.group(s, 'shape-rendering="crispEdges"');
  const mp = B.monitorPatch({ shadow: true });
  const rp = B.roundPatch({ shadow: true });
  const desk = B.deskSetRows({ shadow: true });
  const sr = B.sunriseScreen({ u: 6 });
  // Two rows of three, each piece centered in its cell.
  const cell = (piece, col, row, k = 1) => {
    const cw = W / 3, ch = (H - 40) / 2;
    const x = Math.round(col * cw + (cw - piece.w * k) / 2), y = Math.round(20 + row * ch + (ch - piece.h * k) / 2);
    return at(piece, x, y, k);
  };
  const pix = (svg, w, h) => ({ svg: crisp(svg), w, h });
  return B.svgDoc(W, H,
    cell(mp, 0, 0, 1.2) +
    cell(rp, 1, 0, 1.2) +
    cell(pix(B.drawMark(B.tile(32), 0, 0, 7), 224, 224), 2, 0) +
    cell(pix(sr.svg, sr.w, sr.h), 0, 1) +
    cell(pix(B.drawIso(desk, 0, 0, 4, P.paper), desk[0].length * 4, desk.length * 4), 1, 1) +
    cell(pix(B.badge({}, 0, 0, 3), 88 * 3, 31 * 3), 2, 1),
    { bg: P.paper, title: 'OgOs stickers' });
};
