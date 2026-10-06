#include "app/app_pin_panel_internal.h"
#include "kit_ui_text_edit.h"
#include <string.h>
// Legacy cursor facade uses shared scalar boundaries and atomic capacity refusal.
static bool bind(KitUiTextEdit *edit,char *buffer,size_t cap,int *cursor) {
    if(!buffer || !cursor || kit_ui_text_bind(edit,buffer,cap,KIT_UI_TEXT_SINGLE_LINE).code!=CORE_OK)return false;
    size_t p=*cursor<0?0:(size_t)*cursor;
    return kit_ui_text_position(edit,p,p).code==CORE_OK;
}
bool app_pin_name_edit_insert_text(char *buffer,size_t cap,int *cursor,const char *text) {
    KitUiTextEdit edit={0};if(!text || !*text || !bind(&edit,buffer,cap,cursor))return false;
    size_t before=strlen(buffer);CoreResult r=kit_ui_text_insert(&edit,text);*cursor=(int)edit.cursor;
    return r.code==CORE_OK && strlen(buffer)!=before;
}
bool app_pin_name_edit_backspace(char *buffer,int *cursor) {
    if(!buffer || !cursor)return false;
    KitUiTextEdit edit={0};size_t cap=strnlen(buffer,MAPFORGE_PIN_NAME_CAPACITY)+1;
    if(!bind(&edit,buffer,cap,cursor))return false;
    size_t before=strlen(buffer);CoreResult r=kit_ui_text_command(&edit,KIT_UI_TEXT_BACKSPACE,0);*cursor=(int)edit.cursor;
    return r.code==CORE_OK && before!=strlen(buffer);
}
static bool move(const char *buffer,int *cursor,int direction) {
    if(!buffer || !cursor)return false;
    KitUiTextEdit edit={0};if(!bind(&edit,(char *)buffer,strnlen(buffer,MAPFORGE_PIN_NAME_CAPACITY)+1,cursor))return false;
    size_t before=edit.cursor;CoreResult r=kit_ui_text_command(&edit,direction<0?KIT_UI_TEXT_LEFT:KIT_UI_TEXT_RIGHT,0);*cursor=(int)edit.cursor;
    return r.code==CORE_OK && before!=edit.cursor;
}
bool app_pin_name_edit_move_left(const char *buffer,int *cursor){return move(buffer,cursor,-1);}
bool app_pin_name_edit_move_right(const char *buffer,int *cursor){return move(buffer,cursor,1);}
