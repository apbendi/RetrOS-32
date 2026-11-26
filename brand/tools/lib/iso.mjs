/*
 * The Isometric theme's drawing: a tiny rasterizer for pixel-art isometric
 * scenes, and the objects drawn with it: the monitor and the desk set.
 * GUIDE.md, "Isometric".
 *
 * World axes: X runs right and down the screen, Y left and down, Z up, so
 * a point projects to (X - Y, (X + Y) / 2 - Z): the 2:1 pixel isometric of
 * 90s icons. Light comes from the upper left: tops lightest, fronts (facing
 * lower left) next, sides (lower right) darkest.
 */
import { PALETTE as P, RAMPS, bitmap, crop } from './core.mjs';

/*
 * Rasterizes faces into a W x H grid of color keys. (ox, oy) is where the
 * world origin lands. Each face is a rectangle on a plane:
 *   { p: 'x' | 'y' | 'z', at, u: [lo, hi], v: [lo, hi], c }
 * where (u, v) is (Y, Z) on an x plane, (X, Z) on a y plane and (X, Y) on a
 * z plane. c is a key, or a function of (u, v) relative to the face's low
 * corner returning a key (or nothing, to leave the cell). Faces paint in
 * order, so list them back to front.
 */
export function isoScene(W, H, ox, oy, faces) {
  const g = Array.from({ length: H }, () => Array(W).fill('.'));
  for (let j = 0; j < H; j++) {
    for (let i = 0; i < W; i++) {
      const sx = i + 0.5 - ox, sy = j + 0.5 - oy;
      for (const f of faces) {
        let u, v;
        if (f.p === 'z') {
          const s = 2 * (sy + f.at);
          u = (sx + s) / 2;
          v = (s - sx) / 2;
        } else if (f.p === 'y') {
          u = sx + f.at;
          v = (u + f.at) / 2 - sy;
        } else {
          u = f.at - sx;
          v = (f.at + u) / 2 - sy;
        }
        if (u >= f.u[0] && u < f.u[1] && v >= f.v[0] && v < f.v[1]) {
          const k = typeof f.c === 'function' ? f.c(u - f.u[0], v - f.v[0]) : f.c;
          if (k) g[j][i] = k;
        }
      }
    }
  }
  return g;
}

// The three faces of a box you can see: top, front (the y plane at y1)
// and side (the x plane at x1).
export const box = (x0, x1, y0, y1, z0, z1, c) => [
  { p: 'z', at: z1, u: [x0, x1], v: [y0, y1], c: c.top },
  { p: 'y', at: y1, u: [x0, x1], v: [z0, z1], c: c.front },
  { p: 'x', at: x1, u: [y0, y1], v: [z0, z1], c: c.side },
];

// A flat shadow on the floor, under everything else.
export const floorShadow = (x0, x1, y0, y1) => ({ p: 'z', at: 0, u: [x0, x1], v: [y0, y1], c: 'h' });

/*
 * Outlines a rasterized scene in ink ('k'): every empty cell beside a
 * filled one. Shadow cells ('h') are neither outlined nor outline others.
 */
export function outlineScene(g) {
  const H = g.length, W = g[0].length;
  const solid = (i, j) => j >= 0 && j < H && i >= 0 && i < W && !'.kh'.includes(g[j][i]);
  return g.map((row, j) => row.map((c, i) => {
    if (c !== '.' && c !== 'h') return c;
    return [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([di, dj]) => solid(i + di, j + dj)) ? 'k' : c;
  }).join(''));
}

/*
 * The colors. Cream plastic: T top, F front, R side, with E and Q for the
 * tube's housing behind. N is the lit front edge of a top.
 */
export const ISO_MAP = {
  k: P.ink, T: RAMPS.cream[1], F: P.cream, E: RAMPS.cream[3], R: RAMPS.cream[4], Q: RAMPS.cream[5],
  N: P.white, K: RAMPS.cream[3],
  S: P.screen, s: RAMPS.night[2], L: P.orange, l: P.peach, r: P.red,
  B: P.steel, b: P.steelDark, d: RAMPS.steel[2],
  U: P.blueLight, V: P.blue, W: P.blueDeep, Z: RAMPS.blue[2],
};

// The color map with the floor shadow for a ground: Desktop Blue or paper.
export const isoMap = (ground = P.blue) => ({ ...ISO_MAP, h: ground === P.blue ? P.blueDeep : RAMPS.cream[3] });

const HOUSING = { top: 'F', front: 'E', side: 'Q' };

/*
 * The monitor in the round: a cream slab with the screen, the LED at its
 * bottom left and a steel foot, and the tube's housing behind. (x, y) is
 * the slab's left front corner on the floor, z its base height.
 *   w, h, d: the slab; bd: the housing's depth
 *   sun: the sunrise on the screen instead of the LED
 *   slot: a slot on the chin
 */
export function isoMonitor(x, y, z, o = {}) {
  const w = o.w || 24, h = o.h || 27, d = o.d || 7, bd = o.bd || 9, sun = !!o.sun;
  const yf = y, yb = y - d;
  const sx0 = 2, sx1 = w - 2, sz0 = Math.round(h * 0.4), sz1 = h - 2;
  const front = (u, v) => {
    if (v < 2.5) return v >= 2 ? 'b' : 'B'; // the foot, its top edge darker
    if (u >= sx0 && u < sx1 && v >= sz0 && v < sz1) {
      const su = u - sx0, sv = v - sz0;
      if (sun) {
        if (Math.hypot(su - 2, sv) < 6) return sv >= 2 ? 'L' : 'r';
      } else if (su < 4 && sv < 2) return sv >= 1.2 ? 'l' : 'L';
      if (sv > sz1 - sz0 - 1 || su < 1) return 's'; // the inner shadow along the top and left
      return 'S';
    }
    if (o.slot && v >= sz0 - 4 && v < sz0 - 3 && u >= w - 11 && u < w - 3) return 'd';
    return 'F';
  };
  return [
    ...box(x + 4, x + w - 4, yb - bd, yb, z + Math.round(h * 0.2), z + h - 8, HOUSING),
    { p: 'z', at: z + h, u: [x, x + w], v: [yb, yf], c: (u, v) => (v > d - 1 ? 'N' : 'T') },
    { p: 'y', at: yf, u: [x, x + w], v: [z, z + h], c: front },
    { p: 'x', at: x + w, u: [yb, yf], v: [z, z + h], c: (u, v) => (v < 2.5 ? 'd' : 'R') },
  ];
}

/*
 * The monitor on its own, as pixel rows cropped to the drawing. opts: sun,
 * shadow (a shadow on the floor under it, falling to the lower right).
 */
export function isoMonitorRows(o = {}) {
  const faces = [
    ...(o.shadow ? [floorShadow(1, 27, 5, 22)] : []),
    ...isoMonitor(0, 20, 0, { slot: !o.sun, sun: o.sun }),
  ];
  return crop(outlineScene(isoScene(100, 100, 50, 60, faces)));
}

/*
 * The desk set: a desktop case with a floppy slot and the power light at
 * its bottom left, the monitor on it with the sunrise, and a keyboard in
 * front, as pixel rows cropped to the drawing. opts: shadow, screen ('sun'
 * | 'led').
 */
export function deskSetRows(o = {}) {
  // Rows of keys, with darker grooves between them.
  const keys = (u, v) => (u < 1 || u > 26 || v < 1 || v > 7 ? 'T' : (v - 1) % 2 < 1 ? 'K' : 'N');
  const caseFront = (u, v) => {
    if (u >= 2 && u < 6 && v >= 1.5 && v < 4) return v >= 3.2 ? 'l' : 'L'; // the power light, bottom left
    if (u >= 17 && u < 27 && v >= 4 && v < 5) return 'd'; // the floppy slot
    if (u >= 24 && u < 26 && v >= 2 && v < 3) return 'b'; // its eject button
    return 'F';
  };
  const faces = [
    ...(o.shadow ? [floorShadow(2, 35, 2, 38)] : []),
    ...box(0, 32, 0, 24, 0, 8, { top: 'T', front: caseFront, side: 'R' }),
    ...isoMonitor(4, 18, 8, { w: 24, h: 25, d: 6, bd: 8, sun: (o.screen || 'sun') === 'sun' }),
    ...box(2, 29, 27, 35, 0, 2, { top: keys, front: 'F', side: 'R' }),
  ];
  return crop(outlineScene(isoScene(110, 110, 55, 70, faces)));
}

export const drawIso = (rows, x, y, px, ground) => bitmap(rows, isoMap(ground), { x, y, px });
