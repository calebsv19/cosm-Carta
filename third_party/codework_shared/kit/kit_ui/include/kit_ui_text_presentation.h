#ifndef KIT_UI_TEXT_PRESENTATION_H
#define KIT_UI_TEXT_PRESENTATION_H
#include "kit_ui.h"
#include "kit_ui_text_edit.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIT_UI_TEXT_PRESENTATION_BYTES 8192u
#define KIT_UI_TEXT_PRESENTATION_ROWS 256u
/* Measurements are in the same coordinate space as the viewport. No implicit
 * zoom/DPI multiplication. The callback measures a NUL-terminated UTF-8 run. */
typedef CoreResult (*KitUiTextMeasure)(void *, const char *, float *);
typedef struct KitUiTextPresentationOptions {
    KitRenderRect viewport;
    float line_height, caret_width, scroll_y;
    int wrap, active, reveal_caret;
} KitUiTextPresentationOptions;
typedef struct KitUiTextPresentationRow {
    size_t start, end, text_offset;
    KitRenderVec2 origin;
    float width;
    int newline;
    KitRenderRect selection, preedit;
} KitUiTextPresentationRow;
typedef struct KitUiTextPresentation {
    /* Caller-owned: this complete object must outlive queued frame submission.
     * Row text uses offsets, so copying an accepted presentation is safe. */
    char display[KIT_UI_TEXT_PRESENTATION_BYTES];
    char row_text[KIT_UI_TEXT_PRESENTATION_BYTES + KIT_UI_TEXT_PRESENTATION_ROWS];
    KitUiTextPresentationRow rows[KIT_UI_TEXT_PRESENTATION_ROWS];
    size_t count, cursor, replace_start, replace_end, composition_bytes;
    KitUiTextPresentationOptions options;
    KitRenderRect caret;
    float scroll_x, scroll_y, content_height;
} KitUiTextPresentation;
typedef struct KitUiTextPresentationColors {
    KitRenderColor selection, caret, preedit;
} KitUiTextPresentationColors;
typedef struct KitUiTextPainter {
    void *user;
    CoreResult (*rect)(void *, KitRenderRect, KitRenderColor);
    CoreResult (*text)(void *, KitRenderVec2, const char *);
} KitUiTextPainter;
CoreResult kit_ui_text_presentation_build(KitUiTextPresentation *, const KitUiTextEdit *,
    const KitUiTextPresentationOptions *, KitUiTextMeasure, void *);
CoreResult kit_ui_text_presentation_hit(const KitUiTextPresentation *, float, float,
    KitUiTextMeasure, void *, size_t *);
CoreResult kit_ui_text_presentation_paint(const KitUiTextPresentation *,
    const KitUiTextPresentationColors *, const KitUiTextPainter *);
/* Optional queued adapter. Restores the enclosing clip and rolls the command
 * buffer/clip depth back if recording fails. Text is borrowed from presentation. */
CoreResult kit_ui_text_presentation_render(KitUiContext *, KitRenderFrame *,
    const KitUiTextPresentation *, CoreFontRoleId, CoreFontTextSizeTier,
    CoreThemeColorToken, const KitUiTextPresentationColors *);
#ifdef __cplusplus
}
#endif
#endif
