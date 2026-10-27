# OgOs design explorations

100 logo and brand directions for renaming RetrOS-32 to OgOs, in a dashboard you open in a browser.

Open `index.html`. It loads its fonts from Google Fonts, so it needs a network connection; without one, the pixel-drawn logos still render and the rest fall back to system fonts.

## Using the dashboard

- **Case:** redraws every flexible design as OgOs, OgOS, OGOS or ogos, so you can compare capitalizations across all of them. "As designed" shows each one in the case it was made for.
- **Filters:** family, inspiration, palette, risk (safe, bold, wild), the case it was designed in, and whether it redraws in other cases. Within a group the choices are alternatives; across groups they combine.
- **Ground:** each logo on its own background, or all on light or all on dark.
- **Shortlist:** star directions, then "Copy shortlist" copies them as text, with your notes. Stars, notes and settings are kept in this browser's local storage.
- **Detail sheet:** click a logo. It shows the four cases, the icon down to 16 px, the logo on light and dark, and rough mockups: a boot screen, a desktop with an About box, a floppy label and stickers. `index.html#n23` opens direction 23.

## Files

| Path | What's there |
|---|---|
| `index.html` | The dashboard page |
| `assets/ogos.js` | The registry (`OG.add`), the tag vocabularies, and drawing helpers: the VGA palettes, RGB332, pixel bitmaps, and the kernel's own 8x8 font from `lib/font8.c` |
| `assets/dashboard.js`, `assets/dashboard.css` | The dashboard |
| `directions/core.js` | Directions 1 and 2 |
| `directions/a-desktops.js` | Desktops and interfaces: Windows, classic Mac OS, BeOS, Unix workstations, OS/2 |
| `directions/b-machines.js` | Machines and media: home computers, DOS and BIOS screens, disks, keycaps, chips, tapes |
| `directions/c-games.js` | Games, the demoscene and the early web |
| `directions/d-internals.js` | The OS's own internals, and type- and geometry-led marks |
| `directions/e-beyond.js` | From outside computing: print, signage, heraldry, craft, mascots |

Each direction is an `OG.add({...})` call with its tags, palette and notes, and two functions returning SVG: `logo(word)`, the lockup on a 480×300 canvas (VGA's 320×200 shape), and `icon()`, the mark on 64×64. `logo` is called with the capitalization to draw.
