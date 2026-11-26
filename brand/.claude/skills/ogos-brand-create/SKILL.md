---
name: ogos-brand-create
description: Use when asked to make anything that should look like OgOs (the OS formerly RetrOS-32): a logo use, icon, banner, social card, slide, poster, sticker, badge, patch, wallpaper, splash screen, web page graphic, or a new riff or theme on the identity. Uses the brand kit in brand/ to draw it, render it and check it.
---

# Making an OgOs asset

The kit in `brand/` has the rules (`GUIDE.md`), finished assets (`assets/`, listed in `assets/manifest.json`), example compositions (`examples/`) and the tools that draw them all (`tools/`). Work from the kit rather than from scratch. Its pieces already follow the rules, and a piece drawn freehand rarely does.

Paths below are relative to `brand/`.

## 1. Understand the ask

Work out:
- what the piece is for, where it will appear, and its size in pixels;
- what it has to say;
- whether a theme was asked for.

If the size or the place is unknown and it changes the design (a 16 px favicon and a poster are different jobs), ask. Otherwise pick sensible defaults and say what you picked.

## 2. Read the guide, at least these parts

- **"The idea" and "The base":** the base, the mark on Desktop Blue, is the default.
- **"Color" and "Type":** the palette, the contrast table, which face for what, and OgOs capitalization.
- **"The pixel grid":** whole-number scales, one base pixel.
- **The theme section,** if one fits:
  - Isometric for a showpiece object.
  - Mission Patch for an occasion, merch or a release.
  - Web Badge for a small link button.
  - Racing Stripes for speed or energy.
  - "Making a new theme" for anything new.
- **"Do and don't".**

## 3. Reuse before you draw

1. **Look for an existing asset.** Search `assets/manifest.json` for one that already does the job. If one does, use it, at a size it's rendered at or a whole multiple.
2. **For one-offs,** use `node tools/make.mjs badge|patch|text|icon ...` (options are at the top of `tools/make.mjs`).
3. **Otherwise, write a composition.** Copy the closest `examples/*.mjs` to a working location: the operator's chosen path, or `examples/` if it's meant to join the kit. Change it, using the library documented in `tools/README.md`. Keep to:
   - pieces the library already draws (`lockupH`, `sun`, `frame`, `badge`, `roundPatch`, `deskSetRows` ...), placed and combined;
   - `PALETTE` colors only. Text goes through `pixelText()`, or `jostText()` and `jostArc()` for patches, never `<text>`;
   - one base pixel size, with everything else a whole multiple of it;
   - the light at the bottom left, one sun at most, and the name once.

## 4. Render, check, look

```sh
node tools/render.mjs path/to/thing.mjs --png        # thing.svg and thing.png
node tools/check.mjs path/to/thing.svg path/to/thing.png
```

1. **Fix every FAIL** from `check.mjs`. Read each WARN and either fix it or say why it's fine. Anti-aliased edges on vector art, like patches, are expected.
2. **Open the PNG** (with the Read tool) and look at it critically:
   - Is anything clipped, crowded or off-center?
   - Does everything read at the size it'll be used? Render it at that size too, if it'll be shown small.
3. **Go through GUIDE.md's "Checklist",** point by point. For an independent read, use the `ogos-brand-review` skill.

Iterate until it passes. Look at every render before showing it to anyone.

## 5. Hand it over

Give:
- the files' paths;
- what you made, and which theme or motifs it uses;
- any defaults you chose;
- anything the checklist flagged that you kept on purpose, and why.

If the piece belongs in the kit (the operator says so, or asked for a kit addition), add it:
- a composition goes in `examples/`;
- a new drawing goes in `tools/lib/`, with a catalog entry in `tools/catalog.mjs`;
- then run `npm run build` and `npm run check`.

Don't edit generated files (`assets/`, `index.html`, rendered examples) by hand.

## New themes and riffs

The guide invites new themes, so riff when asked. Keep the four anchors ("Making a new theme"): the monitor or a homage to it, the palette, the light at the bottom left, and the kit's type. Make two or three options, show them, and let the operator choose before anything joins the kit. Their choice is feedback: record it with `ogos-brand-update`.
