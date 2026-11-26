# OgOs brand kit

Everything for making things that look like OgOs:
- **The guide**, [GUIDE.md](GUIDE.md): the identity's rules.
- **Pre-rendered assets**, in [`assets/`](assets/).
- **Example compositions**, in [`examples/`](examples/).
- **The tools** that draw all of it, in [`tools/`](tools/).
- **Skills** for agents, in [`.claude/skills/`](.claude/skills/).

Open **[index.html](index.html)** in a browser to see everything at once. To learn the rules, read **[GUIDE.md](GUIDE.md)**.

## What's here

| Path | What's there |
|---|---|
| `GUIDE.md` | The brand guide: the idea, the base, color, type, the sun, the themes, a checklist |
| `FEEDBACK.md` | Every round of the operator's feedback, word for word, and what it decided |
| `index.html` | The gallery: every asset and example, with its files. Generated |
| `assets/` | Pre-rendered SVGs and PNGs: the mark, the wordmark, lockups, the sun, each theme's pieces, and the guide's figures. `assets/manifest.json` lists them, with what each is for; `assets/favicon.ico` is the favicon. Generated |
| `examples/` | Compositions: `*.mjs` is the source, `*.svg` and `*.png` are its renders. Start new work by copying one |
| `tools/lib/` | The drawing library. [tools/README.md](tools/README.md) is its reference |
| `tools/catalog.mjs` | The list of assets the build renders |
| `tools/build.mjs`, `render.mjs`, `make.mjs`, `check.mjs` | Build everything, render one composition, make a one-off piece, check files against the palette and type rules |
| `tools/figures.mjs` | The guide's figures: the palette, the type, the anatomy, the do's and don'ts |
| `fonts/jost/` | Jost, the Mission Patch typeface, with its license |
| `.claude/skills/` | `ogos-brand-create`, `ogos-brand-review` and `ogos-brand-update` |

## Requirements

- **Node 22 or later,** for its built-in WebSocket. The kit was built with Node 24. Nothing to install: the kit has no npm dependencies.
- **Google Chrome or Chromium,** for PNGs only: the tools drive it headless. They find it in `/Applications` on macOS or on the `PATH`; set `CHROME` to point elsewhere. SVGs need no browser.

## Using it

**Find an asset.** Look in the gallery, or search `assets/manifest.json`, which lists each asset's files and what it's for. Prefer the SVG. For PNG, use the size closest to what you need, and don't rescale pixel art by fractions.

**Make a one-off piece** with `tools/make.mjs`:

```sh
node tools/make.mjs badge --line "POWERED BY" --above --out powered.png --scale 2
node tools/make.mjs patch --shape round --mission "Release 1.0" --out release.svg
node tools/make.mjs text "Hello" --face label --px 4 --color ink --bg paper --out hello.png
node tools/make.mjs icon --size 512 --out icon.png
```

**Make something new.** Write a composition: a small module that draws with the library.
1. Copy the closest file in `examples/` (all of them are compositions), and change it.
2. Render it with `node tools/render.mjs my-thing.mjs --png`. The SVG and PNG appear next to the module, or wherever `--out <dir>` says.
3. Check it with `node tools/check.mjs my-thing.svg my-thing.png`.
4. Review it against [GUIDE.md's checklist](GUIDE.md#checklist).

```js
import * as B from '../tools/lib/index.mjs';

export const meta = { title: 'Poster', use: 'What it is for, in one line', theme: 'base' };

export default () => {
  const P = B.PALETTE;
  const lockup = B.lockupH({ u: 4 });
  return B.svgDoc(800, 600,
    B.sun({ x: 0, y: 300, w: 400, h: 300, px: 10, r: 26, cx: 8 }) +
    B.group(lockup.svg, 'transform="translate(340 240)"'),
    { bg: P.blue, crisp: true, title: 'OgOs' });
};
```

**Check anything,** made with the kit or not, with `node tools/check.mjs <files>`. It catches off-palette colors, live text, other typefaces and pixel art at fractional scales. It can't judge composition. Run it without arguments to check the whole kit.

## For agents

The skills in `.claude/skills/` cover the three jobs:

- **`ogos-brand-create`:** make a new asset or artifact that fits the identity.
- **`ogos-brand-review`:** judge an asset against the guide.
- **`ogos-brand-update`:** take the operator's feedback into the guide, the tools and the assets.

Claude Code finds them when it works on files under `brand/`. If they don't show up, read their `SKILL.md` files directly: they're plain instructions.

## Updating the kit

The kit changes through feedback. The `ogos-brand-update` skill walks through these steps:

1. **Log the feedback** in [FEEDBACK.md](FEEDBACK.md), word for word, as a new round.
2. **Decide what it changes.** For each point, decide whether the change belongs in a rule (GUIDE.md), a drawing (`tools/lib/`), the catalog of assets, or the examples, or whether nothing changes. Write a decision row for each point. Ask the operator about anything ambiguous before changing it.
3. **Change the source:**
   - GUIDE.md for rules.
   - `tools/lib/` for how things are drawn.
   - `tools/catalog.mjs` for which assets exist.
   - `examples/*.mjs` for examples.
   - `tools/figures.mjs` for the guide's figures.
4. **Rebuild:** `npm run build` (or `node tools/build.mjs`), then `npm run check`.
5. **Look at the result.** Open `index.html`, or render the changed pieces, and look before calling it done.
6. **Summarize** for the operator: what changed and why, pointing to the FEEDBACK.md round.

**Rules:**
- **Don't edit generated files by hand.** That's everything in `assets/`, `index.html`, and `examples/*.svg` and `*.png`. Change the source and rebuild. `tools/lib/jost-glyphs.mjs` is generated too, by `tools/dev/extract-jost.mjs`, when the fonts change.
- **Keep rules and reasons together.** Every rule in GUIDE.md that came from feedback has a decision in FEEDBACK.md.
- **Retire things explicitly.** When something is dropped, add it to GUIDE.md's "Do and don't", so it isn't reinvented.
- **Rebuilds are deterministic:** without a change in the source, rebuilding changes no SVG. A diff in `assets/` should always be explained by a change in the source.

## Fonts and licenses

- **The OgOs faces** (Display, Label, Micro) were drawn for this kit. Display and Label grew from the RetrOS logo's lettering.
- **RetrOS System** is the kernel's font, from `lib/font8.c`: font8x8, by Daniel Hepper, in the public domain.
- **Jost** is © The Jost Project Authors, under the SIL Open Font License 1.1 (`fonts/jost/OFL.txt`). The kit draws its outlines into SVGs, which the license allows.
