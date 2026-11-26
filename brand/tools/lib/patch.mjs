/*
 * The Mission Patch theme: embroidered patches in the brand's colors.
 * GUIDE.md, "Mission Patch".
 *
 * Patches are flat vector art (no pixels), with a cream merrowed edge, a
 * blue field, a night sky, and Jost Bold capitals tracked wide. The light
 * is a planet or sun rising at the lower left.
 *
 * Each function draws at the origin and returns { svg, w, h }.
 */
import { PALETTE as P, RAMPS, rect, circle, f2, uid } from './core.mjs';
import { jostText, jostArc, jostWidth, jostCap } from './type.mjs';
import { frame } from './mark.mjs';

const STITCH = RAMPS.cream[5];

// A drop shadow, for showing a patch as an object. Off in the masters.
function shadowDefs(id) {
  return `<filter id="${id}" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="3"/></filter>`;
}

// The merrowed edge's stitches: short dashes around a shape's border.
const merrow = (t) => `fill="none" stroke="${STITCH}" stroke-width="${f2(t)}" stroke-dasharray="1.4 2.2" opacity="0.5"`;

/*
 * The monitor patch: cut to the RetrOS frame's shape. The screen is a
 * window on space with a red planet rising from its bottom-left corner,
 * one orbit and a cream satellite; the name is on the chin and the mission
 * on the foot. 212 x 272.
 *   name ('OGOS'), mission ('MISSION 32'), shadow (false)
 */
export function monitorPatch(o = {}) {
  const name = (o.name || 'OGOS').toUpperCase(), mission = (o.mission || 'MISSION 32').toUpperCase();
  const W = 212, H = 272, R = 24, b = 11;
  const ix = b, iy = b, iw = W - 2 * b, ih = H - 2 * b;
  const sx = ix + 14, sy = iy + 14, sw = iw - 28, sh = 140;
  const footH = 30, footY = iy + ih - footH;
  const track = 0.2;
  const size = Math.min(40, (iw - 40) / jostWidth(name, { size: 1, tracking: track }));
  const pcx = sx + 6, pcy = sy + sh + 46, pr = 104;
  const stars = [[0.62, 0.14, 1.6], [0.86, 0.3, 1.1], [0.4, 0.22, 1], [0.72, 0.52, 1.2], [0.9, 0.08, 1], [0.24, 0.1, 1.3], [0.52, 0.4, 0.9]];
  const [cf, cs, sd] = [uid('pf'), uid('ps'), uid('pd')];
  const svg =
    `<defs><clipPath id="${cf}"><rect x="${ix}" y="${iy}" width="${iw}" height="${ih}" rx="${R - b}"/></clipPath>` +
    `<clipPath id="${cs}"><rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6"/></clipPath>` +
    (o.shadow ? shadowDefs(sd) : '') + `</defs>` +
    (o.shadow ? `<rect x="2" y="5" width="${W}" height="${H}" rx="${R}" fill="${P.ink}" opacity="0.35" filter="url(#${sd})"/>` : '') +
    `<rect x="0" y="0" width="${W}" height="${H}" rx="${R}" fill="${P.cream}"/>` +
    `<rect x="${f2(1.5 + (b - 3) / 2)}" y="${f2(1.5 + (b - 3) / 2)}" width="${f2(W - 3 - (b - 3))}" height="${f2(H - 3 - (b - 3))}" rx="${f2(R - 1.5 - (b - 3) / 2)}" ${merrow(b - 3)}/>` +
    `<g clip-path="url(#${cf})">` + rect(ix, iy, iw, ih, P.blue) + rect(ix, footY, iw, footH, P.ink) + `</g>` +
    `<rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6" fill="${P.space}"/>` +
    `<g clip-path="url(#${cs})">` +
    stars.map(([fx, fy, r]) => circle(sx + fx * sw, sy + fy * sh, r, P.cream, 'opacity="0.85"')).join('') +
    `<ellipse cx="${pcx}" cy="${pcy}" rx="${pr + 44}" ry="${pr - 30}" fill="none" stroke="${P.cream}" stroke-width="1.6" opacity="0.9" transform="rotate(-28 ${pcx} ${pcy})"/>` +
    circle(pcx, pcy, pr, P.red) +
    `<circle cx="${pcx}" cy="${pcy}" r="${pr - 3}" fill="none" stroke="${P.orange}" stroke-width="6"/>` +
    `</g>` +
    rect(sx + sw - 52, sy + 26, 7, 7, P.cream) +
    `<rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="6" fill="none" stroke="${P.cream}" stroke-width="2"/>` +
    jostText(name, { x: W / 2, y: f2((sy + sh + footY) / 2 + jostCap(size) / 2), size, weight: 700, tracking: track, fill: P.cream, anchor: 'middle' }) +
    jostText(mission, { x: W / 2, y: f2(footY + footH / 2 + jostCap(12, 500) / 2), size: 12, weight: 500, tracking: 0.33, fill: P.steel, anchor: 'middle' });
  return { svg, w: W, h: H };
}

/*
 * The round patch: a cream edge, the name and mission around a blue band
 * with an orange dot at each side, and in the middle the RetrOS monitor
 * over a planet's limb, with the sun coming up at the lower left. 272
 * across.
 *   name ('OGOS'), mission ('MISSION 32'), shadow (false)
 */
export function roundPatch(o = {}) {
  const name = (o.name || 'OGOS').toUpperCase(), mission = (o.mission || 'MISSION 32').toUpperCase();
  const R = 136, cx = R, cy = R, band = 36, fr = R - 12 - band;
  const mid = R - 12 - band / 2;
  const track = 0.24;
  const size = Math.min(27, (Math.PI * mid * 0.62) / jostWidth(name, { size: 1, tracking: track }));
  const rt = mid - jostCap(size) / 2, rb = mid + jostCap(12, 500) / 2;
  const mw = 84, mh = 92, mx = cx - mw / 2, my = cy - mh / 2 - 12;
  const m = frame(mx, my, mw, mh, { line: 3, inset: 11, screenH: 46, foot: 9, led: [12, 6], colors: { screen: RAMPS.night[2] } });
  const lcx = cx + 30, lcy = cy + 240, lr = 196;
  const [cc, gr, sd] = [uid('rc'), uid('rg'), uid('rd')];
  const stars = [[-46, -58, 1.3], [52, -52, 1], [-66, -10, 1], [64, -6, 1.2], [20, -70, 0.9], [-24, -72, 1.1]];
  const svg =
    `<defs><clipPath id="${cc}"><circle cx="${cx}" cy="${cy}" r="${fr}"/></clipPath>` +
    `<linearGradient id="${gr}" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="${P.orange}"/><stop offset="0.55" stop-color="${P.orange}" stop-opacity="0"/></linearGradient>` +
    (o.shadow ? shadowDefs(sd) : '') + `</defs>` +
    (o.shadow ? circle(cx + 2, cy + 5, R, P.ink, `opacity="0.35" filter="url(#${sd})"`) : '') +
    circle(cx, cy, R, P.cream) +
    `<circle cx="${cx}" cy="${cy}" r="${R - 5}" ${merrow(7)}/>` +
    circle(cx, cy, R - 12, P.blue) +
    circle(cx, cy, fr, P.space) +
    `<g clip-path="url(#${cc})">` +
    stars.map(([dx, dy, r]) => circle(cx + dx, cy + dy, r, P.cream, 'opacity="0.8"')).join('') +
    circle(cx - 52, cy + 52, 16, P.orange) +
    circle(lcx, lcy, lr, P.blueDeep) +
    `<circle cx="${lcx}" cy="${lcy}" r="${lr - 1.5}" fill="none" stroke="url(#${gr})" stroke-width="3"/>` +
    `</g>` +
    `<circle cx="${cx}" cy="${cy}" r="${fr}" fill="none" stroke="${P.cream}" stroke-width="2"/>` +
    m.svg +
    jostArc(name, { cx, cy, r: rt, side: 'top', size, weight: 700, tracking: track, fill: P.cream }) +
    jostArc(mission, { cx, cy, r: rb, side: 'bottom', size: 12, weight: 500, tracking: 0.33, fill: P.sky }) +
    circle(cx - mid, cy, 4, P.orange) + circle(cx + mid, cy, 4, P.orange);
  return { svg, w: 2 * R, h: 2 * R };
}

/*
 * The patch as an icon, 64 x 64: the round patch without its lettering.
 */
export function roundPatchIcon() {
  const m = frame(21, 18, 22, 24, { line: 0, inset: 3, screenH: 12, foot: 3, led: [4, 2], colors: { screen: RAMPS.night[2] } });
  const c = uid('ri');
  const svg =
    `<defs><clipPath id="${c}"><circle cx="32" cy="32" r="20"/></clipPath></defs>` +
    circle(32, 32, 31, P.cream) + circle(32, 32, 28, P.blue) + circle(32, 32, 20, P.space) +
    `<g clip-path="url(#${c})">` + circle(20, 45, 5, P.orange) + circle(38, 88, 44, P.blueDeep) + `</g>` +
    `<circle cx="32" cy="32" r="20" fill="none" stroke="${P.cream}" stroke-width="1"/>` +
    m.svg + circle(8, 32, 1.8, P.orange) + circle(56, 32, 1.8, P.orange);
  return { svg, w: 64, h: 64 };
}
