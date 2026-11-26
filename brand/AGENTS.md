# Notes for agents working in brand/

This is the OgOs brand kit. Start with `README.md`, which covers what's here, how to use it and how to update it. `GUIDE.md` has the rules.

- **Making something:** use the `ogos-brand-create` skill (`.claude/skills/ogos-brand-create/SKILL.md`).
- **Judging something:** use `ogos-brand-review`.
- **Taking feedback:** use `ogos-brand-update`. Log it in `FEEDBACK.md` before changing anything.
- **Generated files are never edited by hand:** `assets/`, `index.html`, `examples/*.svg|png` and `tools/lib/jost-glyphs.mjs`. Change the source, then run `npm run build` and `npm run check`.
- **Look before handing back:** render your work and look at the PNG. The checker catches colors and type, not composition.
- **No dependencies:** Node 22+ and Chrome (for PNGs) only. Don't add npm packages.
