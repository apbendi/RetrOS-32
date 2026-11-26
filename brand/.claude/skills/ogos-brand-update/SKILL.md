---
name: ogos-brand-update
description: Use when the operator gives feedback on OgOs brand work (the guide, assets, examples, explorations or a new design), or asks to change the brand's rules, colors, type, themes or tools. Records the feedback in brand/FEEDBACK.md, turns it into decisions, updates brand/GUIDE.md, the tools and the assets, rebuilds and verifies.
---

# Updating the OgOs brand kit

The kit changes only through the operator's decisions, and every change is traceable: a rule in `GUIDE.md` has a reason in `FEEDBACK.md`. Paths below are relative to `brand/`.

## 1. Log the feedback first

Add a round at the top of `FEEDBACK.md`, from its template:
- what was shown;
- the operator's words, verbatim, in a quote block;
- an empty decisions table.

Fix nothing in the quote. If a reference is ambiguous (like "104 & 102" when they meant 104 and 105), add a line under the quote saying how you read it.

## 2. Turn each point into a decision

For every point the operator made, write a row:
- the feedback, in brief;
- the decision;
- where it lands.

A point lands in one of these:
- **A rule:** a GUIDE.md section. A new rule, a changed one, or something retired onto "Do and don't".
- **A drawing:** a function in `tools/lib/`. A theme's pieces, a face's glyphs, the palette in `core.mjs`.
- **An asset:** an entry in `tools/catalog.mjs`, added, changed or removed.
- **An example:** `examples/*.mjs`.
- **A figure:** `tools/figures.mjs` (palette, type, anatomy, do's and don'ts).
- **Nothing:** say why.

Ask the operator before acting on anything ambiguous, or anything that contradicts an earlier decision in FEEDBACK.md. Name the earlier round when you ask. Liking something doesn't make it a rule. Disliking something usually earns a line on "Do and don't".

## 3. Make the changes

- **Change the source, never the output.** `assets/`, `index.html`, `examples/*.svg` and `*.png`, and `tools/lib/jost-glyphs.mjs` are generated.
- **Keep the guide honest.** If a function's behavior changes, check every GUIDE.md and `tools/README.md` passage that describes it.
- **New colors** go in `PALETTE` (or `RAMPS`) in `tools/lib/core.mjs`, with a role. Then update the guide's color table and contrast table, and `figures.mjs`'s role names.
- **New glyphs** go in `tools/lib/faces/*.mjs`, with exactly the face's `height` rows. Render a specimen and look at it.
- **A new theme** gets:
  - its drawing functions in `tools/lib/<theme>.mjs`, exported from `index.mjs`;
  - catalog entries with their own section, plus a `SECTIONS` entry;
  - at least one example;
  - a section in GUIDE.md's "Themes" with pieces, construction, type, grounds and what to avoid;
  - a mention in the create and review skills if it needs special handling.

## 4. Rebuild and verify

```sh
npm run build    # node tools/build.mjs: SVGs, PNGs, favicon, manifest, index.html
npm run check    # node tools/check.mjs: the whole kit, palette and type rules
```

Both must finish without errors, and check must report 0 failed.

Then look at every piece you changed: render it, or screenshot `index.html`, and read the PNGs. Compare them with what the operator asked for.

Check `git status`, too: every changed generated file should follow from a source change. Rebuilds are deterministic, so unexplained diffs mean something drifted.

## 5. Report

Tell the operator:
- what changed, point by point, with the FEEDBACK.md round as the record;
- where to look (gallery sections, file paths);
- anything you didn't do, and the questions you need answered.

If a point couldn't be settled, add it to GUIDE.md's "Open questions". Don't commit unless asked; when asked, show a draft message first.
