# Feedback log

Every round of the operator's feedback on OgOs's look, word for word, and what was decided because of it. [GUIDE.md](GUIDE.md) holds the rules; this file holds the reasons. Newest round first.

To add a round, copy the template at the bottom. The `ogos-brand-update` skill does this.

---

## Round 3: feedback on directions 101–110 (2026-11-25)

Directions 101 to 110 are in `../design-explorations/directions/f-round-two.js`. This round turned them into the kit.

### What the operator said

> Most of these are great! In particular, here are some thoughts on each of them:
>
> * 101 - Very cool, and I like the intro of the sunrise motif
> * 102 - Simple, and I like the Font in this one. The aspect ratio is not my favorite and wouldn't be my default choice, but it could be appropriate in cases where a wide stylized icon is appropriate. I am generally *not* a fan of the monitor with _just_ the O in the middle though.
> * 103 - My least favorite. Don't like the square colors around the letters. But the font itself looks nice!
> * 104 & 102- Both really cool! Love the old school computers on a tilt. Not sure about the blue platform the monitor is on in 104. Not the biggest fan of the fonts though.
> * 106 and 107 - These are both so great!! Love love love the space theme, the design quality is so good. The fonts are nice and fit perfectly with the motif. I love the "Mission 32" idea. The computer render of 107 is simple, but really clean and nice. A big fan of both of these.
> * 108 - Also really nice! In particular, I love the coputer icon itself. It's clean but adds nice touches with the pixelized shading and the inclusion of the sun. I do _not_ like the inclusion of the pointer, nor am I a particularly big fan of the way the name is rendered in the box with the striped border etc...
> * 109 - Love this one too! Computer icon looks great. Old school web badge motif is cool. I like it.
> * 110 - Also really fun! I like the tilted take on the computer icon and the trailing stripes, though the way some of the dots are "missing" is a bit odd. I do _not_ like the font here,t hough.
>
> So, here's what I want to do. Rather than "picking" one of these ten, I want us to use everything we've learned here to create a comprehensive brand package in a new subdirectory. [...]
>
> The way I see it, the computer logo + the blue background sort of represents the base of the brand, i.e. the default starting point for the brand identity. This is best represented by the left side of 109. Then from that base, there are various ways we can play with the logo that can be mixed and matched together to do interesting things within the same identity. This includes introducing the concept of the sun (like 101, 108, 105 & the patches), playing with it's shape (i.e. tilting it in 110, making it isometric in 104 & 105, stretching it in 102), putting it in different contexts (on a patch in 107, on a web button in 109), or using it as the inspiration for other elements that are not explicitly a computer (like the patch in 106, whose shape is clearly a homage to the computer itself).
>
> I see isometric and mission patch as explicit major theme options that can be called out in the branding kit. I think the web badge style and the tilted stripe style are good examples of minor themes that can be used, but also which give a sort of permission structure for generally riffing on the identity in the future (in other words, the guide should not constrain future creators to only these themes). In the case of the mission patch style, the guide should recommend the font style used in those examples, along with the all caps style. In other cases, we should prefer font styles like the ones used in 101, 102, 103 (not the color block), 108 (not the box just the font), and 109. The default capitalization when using these pixel-esque fonts should be OgOs. We should dispense with the other font styles used in 104, 105, and 110.

"104 & 102" in the list reads as 104 and 105, the two isometric computers.

### Decisions

| Feedback | Decision | Where |
|---|---|---|
| The computer and the blue are the base; 109's left half shows it | The base identity is the mark on Desktop Blue. The app icon is 109's well, at 16 and 32. | GUIDE "The base"; `tile()` |
| The sun (101, 105, 108, the patches) | The sun is the identity's one big motif: it rises from the bottom left, once per piece. | GUIDE "The sun"; `sun()` |
| Play with the shape: tilt, isometric, stretch | Stretch, tilt and turn are named transformations; uneven scaling isn't one. | GUIDE "Playing with the shape" |
| Isometric and Mission Patch are major themes | Two fully worked themes, with their own pieces and examples. | GUIDE "Themes" |
| Web badge and stripes are minor themes, and a permission structure | Two minor themes, plus "Making a new theme": four anchors, everything else free. | GUIDE "Making a new theme" |
| Patch type: 106/107's font, all caps | Jost Bold and Medium, capitals, tracked wide, only in patches. | GUIDE "Type"; `jostText()`, `jostArc()` |
| Other type like 101, 102, 103, 108, 109; OgOs by default | The pixel faces: OgOs Display (102, 103), OgOs Label (101, 109), OgOs Micro (109) and RetrOS System (108). All four now have full character sets. The name is OgOs in pixel faces. | GUIDE "Type"; `tools/lib/faces/` |
| Drop 104/105/110's fonts | Saira and Archivo are retired. Isometric and Racing set the name in OgOs Display. | GUIDE "Type", "Do and don't" |
| 102: wide is fine for wide spaces; no monitor with just an O | The widescreen lockup is for banners. The O icon is gone, and it's on the don't list. | GUIDE "Lockups" |
| 103: no colored squares; the font is good | No tiles, which are on the don't list. Its face is OgOs Display. | GUIDE "Do and don't" |
| 104: not sure about the blue platform | Isometric objects stand on a flat floor shadow, with no platform. | `isoMonitorRows({ shadow })` |
| 106/107: the space theme, MISSION 32, 107's clean computer | Both patches are kept as they were. MISSION 32 is the canonical line, and the mission line is a parameter. | `monitorPatch()`, `roundPatch()` |
| 108: love the icon; no pointer; no boxed label | The 32 × 32 computer is a master. The desktop icon lockup labels it plainly, in RetrOS System. Pointers and selection boxes are on the don't list. | `computer()`, `desktopIcon()` |
| 109: love it | The badge is a theme, and its left half is the base. | `badgeRows()` |
| 110: missing dots are odd; no font | The trails are regular: every stripe has every square, with the gaps widening evenly. The name is in OgOs Display, leaning with the monitor. | `stripes()`, `racing()` |

---

## Round 2: feedback on directions 1–100 (2026-11-24)

Directions 1 to 100 are in `../design-explorations/directions/`. This round produced 101 to 110.

### What the operator said

> * I like the idea of riffing off the existing RetrOS logo, but #46 is too dead on. We can explore it more.
>     * What if we changed the color palette? I like #1F5F9E  as a desktop background. I like #E0452B and #FF6B2C as possible bright accent colors
>     * What if we moved the bright element from the top right corner to the bottome left
>     * I don't love how it says OG in the middle of the screen but also OgOs right underneath. Maybe fill that space with something geometric, or change the aspect ratio so there's no so much space there in the first place? Just idea.
> * I'd like to see more riffs on the geometric BeOS style logo like #53, but maybe make the geometric logo a computer or something more relevant. I don't understand why it's a random ring! Maybe combine it with a riff on the existing RetrOS inspired logo
> * I like the retro feel of #03, it's just a bit uninspired
> * I like the mission patch vibe of 52 and the computer icon inside it but it's a bit awkwardly implemented; it looks a bit childish as-is
> * Number 1 has potential but it's too obviously a rip of the win95 logo
> * Number 60 is promising in principle but doesn't land
> * The four color squares in 78 are nice but otherwise uninspired

### Decisions

- **The palette:** Desktop Blue `#1F5F9E`, with Signal Red `#E0452B` and Sunrise Orange `#FF6B2C` as the accents, plus cream and ink.
- **The light moves to the bottom left.** The old logo's LED was actually at the top left of the screen.
- **The name appears once.** The screen stops spelling it: 101 filled the screen with the sun, and 102 made the screen wide enough to hold the whole name.
- **#53's isometric style draws a computer** (104, 105).
- **#52's patch is redrawn soberly** (106, 107).
- **#03, #60, #1 and #78 became 108, 109, 110 and 103.**

---

## Round 1

The first hundred directions (commit `febcf78`, 2026-10-27). Feedback that shaped them wasn't recorded. Round 2's answer, directions 101 to 110, is commit `59f69f1`.

---

## Template

```markdown
## Round N: feedback on <what was shown> (<date>)

<What was shown, and where it is.>

### What the operator said

> <their words, unedited>

### Decisions

| Feedback | Decision | Where |
|---|---|---|
| <a point, briefly> | <what changes, or why nothing does> | <guide section, function, asset> |
```
