# TimerHUD Renderer

Draws the on-screen heads-up display.

| File | Responsibility |
| --- | --- |
| `hud_renderer.h/c` | Lays out timer blocks, text labels, and background geometry. Delegates glyph drawing to `TextRender/`. |

## Subdirectories

- [`TextRender/`](TextRender/README.md) – SDL_ttf wrapper used to render text in the HUD.
