#include "kit_ui_text_edit.h"
#include "kit_ui_focus_scope.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    char text[32]="aé😀z";KitUiTextEdit e={0};char copy[32];
    assert(kit_ui_text_bind(&e,text,sizeof(text),1).code==CORE_OK);
    assert(e.cursor==8);kit_ui_text_command(&e,KIT_UI_TEXT_LEFT,0);assert(e.cursor==7);
    kit_ui_text_command(&e,KIT_UI_TEXT_BACKSPACE,0);assert(strcmp(text,"aéz")==0 && e.cursor==3);
    kit_ui_text_command(&e,KIT_UI_TEXT_LEFT,1);assert(e.cursor==1 && e.anchor==3);
    kit_ui_text_selection(&e,copy,sizeof(copy));assert(strcmp(copy,"é")==0);
    assert(kit_ui_text_insert(&e,"中").code==CORE_OK && strcmp(text,"a中z")==0);
    kit_ui_text_position(&e,2,2);assert(e.cursor==1);kit_ui_text_command(&e,KIT_UI_TEXT_DELETE,0);assert(strcmp(text,"az")==0);
    assert(kit_ui_text_insert(&e,"\xc0\xaf").code!=CORE_OK && strcmp(text,"az")==0);
    kit_ui_text_command(&e,KIT_UI_TEXT_SELECT_ALL,0);assert(kit_ui_text_insert(&e,"12345678901234567890123456789012").code!=CORE_OK && strcmp(text,"az")==0 && e.anchor==0);
    assert(kit_ui_text_compose(&e,"候補",0,2).code==CORE_OK && strcmp(text,"az")==0);
    assert(kit_ui_text_insert(&e,"東京").code==CORE_OK && !e.composition[0] && strcmp(text,"東京")==0);
    e.flags=KIT_UI_TEXT_DIGITS;assert(kit_ui_text_insert(&e,"x").code!=CORE_OK);
    char bad[3]={'a','b','c'};assert(kit_ui_text_bind(&e,bad,sizeof(bad),0).code!=CORE_OK && e.text==text);
    KitUiSurface s={0};KitUiFocusScope f={0};KitUiInteractionControl c;KitRenderRect rect={0,0,20,20};
    assert(kit_ui_focus_scope_sync(&f,&s,1,0).code==CORE_OK);kit_ui_surface_begin(&s,1);
    kit_ui_surface_register(&s,(KitUiSurfaceKey){1,999},rect,NULL,1,&c);kit_ui_surface_end(&s);s.interaction.focused_id=c.id;
    assert(kit_ui_focus_scope_sync(&f,&s,2,1).code==CORE_OK && !s.interaction.focused_id);
    assert(kit_ui_focus_scope_sync(&f,&s,3,1).code!=CORE_OK && f.scope==2);
    kit_ui_focus_scope_sync(&f,&s,1,0);kit_ui_surface_begin(&s,1);kit_ui_surface_register(&s,(KitUiSurfaceKey){1,999},rect,NULL,1,&c);kit_ui_surface_end(&s);
    kit_ui_focus_scope_restore(&f,&s);assert(s.interaction.focused_id==c.id);
    kit_ui_focus_scope_sync(&f,&s,2,1);kit_ui_focus_scope_sync(&f,&s,1,0);kit_ui_surface_begin(&s,1);kit_ui_surface_end(&s);kit_ui_focus_scope_restore(&f,&s);assert(!s.interaction.focused_id);
    kit_ui_focus_scope_cancel(&f,&s);assert(!f.initialized);
    puts("UTF8 scalar edits, transactional capacity, selection, composition and modal semantic focus pass");
}
