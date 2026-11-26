---
name: ogos-brand-review
description: Use when asked whether something fits the OgOs brand, or to review, critique or approve an OgOs asset, artifact, page or design (an SVG, PNG, HTML page or composition), including ones made outside the brand kit. Checks it against brand/GUIDE.md with brand/tools/check.mjs and the guide's checklist, and reports concrete findings.
---

# Reviewing an OgOs asset

Judge the piece against `brand/GUIDE.md`, not against taste. Every finding names the rule it breaks (a guide section, or a decision in `brand/FEEDBACK.md`) and what to do about it. Paths below are relative to `brand/`.

## 1. Get something you can see and check

- **SVG:** check it as it is, and render it so you can look at it: `node tools/render.mjs thing.svg --png` writes `thing.png` beside it (`--out <dir>` to put it elsewhere). For a composition, render the `.mjs`.
- **PNG:** look at it with the Read tool, and check it.
- **HTML page:** screenshot it with headless Chrome:

  ```sh
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --hide-scrollbars \
    --window-size=1400,2000 --screenshot=out.png file:///...
  ```

  Check its images and inline SVGs, and read its CSS colors and fonts against the palette and type rules.
- **Something you can only describe** (a mockup in words): review it against the rules; skip the tool.

## 2. Run the machine check

```sh
node tools/check.mjs <files>
```

It reports:
- off-palette colors, with the nearest palette color;
- live `<text>` and fonts other than Jost;
- pixel art scaled by fractions (SVG);
- the share of palette colors (PNG).

Treat FAILs as findings. Weigh WARNs: anti-aliasing on vector art is fine, but on pixel art it means a bad scale.

## 3. Walk the checklist

Go through GUIDE.md's "Checklist" point by point, looking at the image each time:

1. **The mark:** present and intact, or a clear homage; not stretched.
2. **The light:** at the bottom left; one sun at most.
3. **The name:** once, spelled OgOs (OGOS only in patches or OgOs Micro).
4. **Color:** palette only; Desktop Blue (or the theme's ground) dominant; orange and red only as light. Check text contrast against the guide's table.
5. **Type:** the kit's faces; no live text; Jost only in patches.
6. **The grid:** whole-number scales; one base pixel.
7. **The theme:** one theme, or the base, consistently.
8. **The don'ts:** nothing from "Do and don't": pointers, selection boxes, letter tiles, gaps in patterns, retired typefaces, platforms under isometric objects, the light elsewhere, the name on the screen.
9. **Legibility:** everything reads at the size it'll be used. Render it at that size and look.
10. **Clear space** around the mark and lockups.

New themes and riffs are allowed. Judge them by the four anchors in "Making a new theme", not by whether they match an existing theme.

## 4. Report

Lead with the verdict:
- **On brand**;
- **On brand with fixes** (list them);
- **Off brand** (say what's fundamentally wrong).

Then list findings, most serious first. Each one gives:
- what's wrong, and where in the image;
- the rule, with its GUIDE.md section;
- the fix: concretely, and with the kit function or asset that does it right where there is one.

Note what works too, briefly, so it isn't "fixed" away. Say what you couldn't check, e.g. how it looks printed or at a size you didn't render.

Don't fix the piece yourself unless you were asked to. If you were, fix it with `ogos-brand-create`'s workflow and review it again.
