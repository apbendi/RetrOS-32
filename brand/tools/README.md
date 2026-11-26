# The drawing library

`tools/lib/` draws the whole kit. Every function returns SVG markup as a string, or pixel art as rows of characters. A composition adds markup together and wraps it in `svgDoc()`. Import it all from one place:

```js
import * as B from '<path to brand>/tools/lib/index.mjs';
```

Pieces that draw "at the origin" return `{ svg, w, h }`. To place one, wrap it in `B.group(piece.svg, 'transform="translate(x y)"')`. Scale pixel art only by whole numbers.

## core.mjs: colors, documents, pixel art

| Export | What it does |
|---|---|
| `PALETTE` | The named colors, e.g. `PALETTE.blue`. GUIDE.md "Color" gives their roles |
| `RAMPS` | Shading ramps (`cream`, `blue`, `steel`, `night`), light to dark, for pixel art only |
| `ALL_COLORS`, `colorName(hex)` | Every allowed color; a color's name |
| `svgDoc(w, h, inner, { crisp, bg, title })` | A whole SVG document. `crisp` for pixel art; `bg` fills the canvas |
| `rect`, `circle`, `group`, `esc`, `f2`, `uid` | Markup helpers. `uid()` makes ids for clip paths and filters; `svgDoc` renumbers them |
| `embed(svg, x, y, scale)` | Puts one whole document inside another |
| `docSize(svg)` | `{ w, h }` of a document |
| `bitmap(rows, map, { x, y, px })` | Draws pixel art: `rows` are strings, `map` gives each character's color, `.` is empty |
| `Grid` | A canvas of keys: `fill(x0, y0, x1, y1, k)`, `rect(x, y, w, h, k)`, `stamp(rows, x, y, recolor)`, `rows()` |
| `bbox`, `crop`, `pad`, `outline`, `solid`, `lean` | Work on rows. `lean(rows, step)` slants pixel art the pixel-art way |
| `rng(seed)` | Seeded random numbers, so art draws the same every time |

## type.mjs: the faces

| Export | What it does |
|---|---|
| `FACES` | `display`, `label`, `micro`, `system`: the pixel faces' data (`faces/*.mjs`) |
| `pixelText(text, { face, x, y, px, fill, anchor, tracking, shadow })` | Sets one line. `y` is the top of the cell (the cap line). `shadow: { dx, dy, fill }` in font pixels. Returns `{ svg, w, h, x, y, rows }` |
| `textRows(text, face)` | The line as rows of `#` and `.`, to recolor or reshape yourself |
| `pixelWidth`, `fitPx(text, opts, maxW, maxH)`, `inkBox` | Measuring: the width; the largest whole pixel size that fits; where the ink is |
| `jostText(text, { x, y, size, weight, tracking, fill, anchor })` | Jost as outlines, on a baseline at `y`. `weight` is 400, 500 or 700; `tracking` is in em |
| `jostArc(text, { cx, cy, r, side, ... })` | Jost around a circle, on its top or bottom, reading left to right |
| `jostWidth`, `jostFit`, `jostCap` | Measuring Jost |

A face is data: `height`, `baseline` (rows above it), `cap`, `xHeight`, `tracking`, `space`, and `glyphs`, a map from each character to its rows. To add a glyph, add its rows to the face's file, with as many rows as the face's `height`.

## mark.mjs: the monitor

| Export | What it does |
|---|---|
| `tile(32 \| 16)` | The app icon's rows |
| `lockupMonitor()` | The 20 × 22 monitor's rows |
| `computer('sun' \| 'led')` | The 32 × 32 desktop icon's rows |
| `monitorPixels(w, h, { screenH, led, slot, foot, inset })` | The flat monitor at another pixel size. Returns a `Grid` |
| `MARK_MAP`, `drawMark(rows, x, y, px)` | The colors for the rows above, and a shortcut to draw them |
| `frame(x, y, W, H, { line, inset, screenH, foot, led, slot, colors })` | The monitor at any size and proportion, from rectangles. Returns `{ svg, screen, chin }`, so you can draw on the screen |
| `screenSun(screen, px, r, cx, gaps)` | The sunrise on a `frame()`'s screen |

## sun.mjs

| Export | What it does |
|---|---|
| `sun({ x, y, w, h, px, r, cx, gaps, split })` | The striped sun rising from the bottom of a box. `r` and `cx` are in pixels of size `px` |
| `sunRows(cols, rows, r, gaps, cx, split)`, `sunGaps(r)`, `SUN_MAP` | The same as rows, the bands' rule, and the colors |

## lockup.mjs

All take `u`, the pixel size, and `text` (default `'OgOs'`), and return `{ svg, w, h }`.

| Export | What it does |
|---|---|
| `wordmark({ u, fill, shadow })` | The name in OgOs Display |
| `lockupH({ u, ground })`, `lockupV({ u, ground })` | Horizontal and stacked. `ground` (`'blue'`, `'paper'`, `'ink'` or `'cream'`) picks the name's color |
| `sunriseScreen({ u })` | The sunrise screen, 39u × 45u |
| `widescreen({ u })` | The widescreen; it widens for longer text |
| `desktopIcon({ u, screen, ground })` | The computer with its label |

## Themes

| Module | Exports |
|---|---|
| `iso.mjs` | `isoMonitorRows({ sun, shadow })` and `deskSetRows({ shadow, screen })` return cropped rows. `drawIso(rows, x, y, px, ground)` draws them, with the shadow colored for `ground`. To draw new objects in the style: `isoScene(W, H, ox, oy, faces)`, `box(...)`, `floorShadow(...)`, `outlineScene(grid)`, `isoMonitor(...)`, `ISO_MAP` and `isoMap(ground)` |
| `patch.mjs` | `monitorPatch({ name, mission, shadow })` (212 × 272), `roundPatch({ name, mission, shadow })` (272 × 272) and `roundPatchIcon()` (64 × 64) |
| `badge.mjs` | `badgeRows({ name, line, above, screen })` returns the 88 × 31 rows, and throws if the text doesn't fit. `badge(opts, x, y, px)` draws them |
| `stripes.mjs` | `racing({ text, px, leanType })`, `racingIcon()`, `stripes({ x0, x1, y, h, gap, stagger, trail, colors })`, `leanAbout(markup, y)`, `LEAN` and `STRIPE_COLORS` |

## raster.mjs

| Export | What it does |
|---|---|
| `openRasterizer()` | Starts headless Chrome. `await r.png(svg, w, h)` returns a PNG Buffer; `await r.close()` when done |
| `ico([{ size, png }])` | Packs PNGs into a `.ico` |
