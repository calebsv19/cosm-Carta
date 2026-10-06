#ifndef KIT_UI_TEXT_EDIT_H
#define KIT_UI_TEXT_EDIT_H
#include "core_base.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Caller owns the NUL-terminated UTF-8 buffer. Positions are scalar boundaries,
 * expressed in bytes. No shaping, persistence or application commit policy. */
enum { KIT_UI_TEXT_SINGLE_LINE=1u, KIT_UI_TEXT_DIGITS=2u };
typedef struct KitUiTextEdit {
    char *text; size_t capacity, cursor, anchor;
    unsigned flags;
    char composition[256]; int composition_start, composition_length;
} KitUiTextEdit;
typedef enum KitUiTextCommand { KIT_UI_TEXT_LEFT, KIT_UI_TEXT_RIGHT,
    KIT_UI_TEXT_HOME, KIT_UI_TEXT_END, KIT_UI_TEXT_BACKSPACE, KIT_UI_TEXT_DELETE,
    KIT_UI_TEXT_SELECT_ALL } KitUiTextCommand;
CoreResult kit_ui_text_bind(KitUiTextEdit *edit, char *text, size_t capacity, unsigned flags);
CoreResult kit_ui_text_position(KitUiTextEdit *edit, size_t cursor, size_t anchor);
CoreResult kit_ui_text_insert(KitUiTextEdit *edit, const char *utf8);
CoreResult kit_ui_text_command(KitUiTextEdit *edit, KitUiTextCommand command, int extend_selection);
CoreResult kit_ui_text_selection(const KitUiTextEdit *edit, char *output, size_t capacity);
CoreResult kit_ui_text_compose(KitUiTextEdit *edit, const char *utf8, int start, int length);
void kit_ui_text_cancel_composition(KitUiTextEdit *edit);
#ifdef __cplusplus
}
#endif
#endif
