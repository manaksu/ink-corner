# InkCorner

A minimal Pebble watchface for Pebble Time Steel (Basalt).

- 24hr time in bottom-left corner
- Seconds in top-right corner
- Configurable font (Space Grotesk Regular/Medium × 24/28px)
- Configurable background (ePaper Cream / Black / White)
- Settings persist across reboots

## CloudPebble Import

1. Go to [cloudpebble.net](https://cloudpebble.net)
2. **Create Project → Import → Import from ZIP**
3. Upload `inkcorner.zip`

## Settings page

Host `resources/index.html` on any static server (GitHub Pages, Vercel, etc.)  
Then add to `appinfo.json`:

```json
"configurationUrl": "https://your-url/index.html"
```

## Resources

```
resources/
  fonts/
    SpaceGrotesk-Regular.ttf   → FONT_SG_REGULAR_24 / FONT_SG_REGULAR_28
    SpaceGrotesk-Medium.ttf    → FONT_SG_MEDIUM_24  / FONT_SG_MEDIUM_28
  images/
    bg_cream.png               → BG_CREAM
    bg_black.png               → BG_BLACK
    bg_white.png               → BG_WHITE
  index.html                   → settings page
```

## Message Keys

| Key | Values |
|-----|--------|
| `FONT_CHOICE` | 0=Regular24, 1=Regular28, 2=Medium24, 3=Medium28 |
| `BG_CHOICE`   | 0=Cream, 1=Black, 2=White |
