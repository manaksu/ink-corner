# InkCorner

A minimal Pebble watchface for Pebble Time Steel (Basalt).

- 24hr time bottom-left, seconds top-right
- Configurable font: Space Grotesk Regular/Medium × 24/28px
- Configurable background: ePaper Cream / Black / White
- Settings persist across reboots via `persist_write/read`

## CloudPebble Import

1. [cloudpebble.net](https://cloudpebble.net) → **Create Project → Import → Import from ZIP**
2. Upload `inkcorner.zip`
3. In **Settings → App Message Keys** add:
   - `FONT_CHOICE` = `0`
   - `BG_CHOICE` = `1`

## Settings page

Host `resources/index.html` on any static server (GitHub Pages, Vercel, etc.)  
Then update the `Pebble.openURL(...)` line in `src/js/index.js` with your URL.

## Message Keys

| Key | Integer | Values |
|-----|---------|--------|
| `FONT_CHOICE` | `0` | 0=Regular24, 1=Regular28, 2=Medium24, 3=Medium28 |
| `BG_CHOICE`   | `1` | 0=Cream, 1=Black, 2=White |

## Project structure

```
src/
  main.c              — watchface C code
  js/
    index.js          — PebbleKit JS (config bridge)
resources/
  fonts/
    SpaceGrotesk-Regular.ttf
    SpaceGrotesk-Medium.ttf
  images/
    bg_cream.png
    bg_black.png
    bg_white.png
  index.html          — settings UI (host separately)
appinfo.json
```
