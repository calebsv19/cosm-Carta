#include "kit_ui_focus_order.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    KitUiFocusItem items[]={{{1,101},KIT_UI_FOCUS_FIELD,1},{{1,102},KIT_UI_FOCUS_BUTTON,0},{{2,103},KIT_UI_FOCUS_BUTTON,1},{{1,104},KIT_UI_FOCUS_FIELD,1}};
    KitUiFocusOrder f={0};assert(kit_ui_focus_order_step(&f,7,items,4,0).code==CORE_OK&&f.key.value==101);
    assert(kit_ui_focus_order_step(&f,7,items,4,0).code==CORE_OK&&f.key.value==103&&f.kind==KIT_UI_FOCUS_BUTTON);
    assert(kit_ui_focus_order_step(&f,7,items,4,1).code==CORE_OK&&f.key.value==101);
    assert(kit_ui_focus_order_step(&f,7,items,4,1).code==CORE_OK&&f.key.value==104);
    KitUiFocusItem swapped[]={items[3],items[2],items[0]};assert(kit_ui_focus_order_sync(&f,7,swapped,3).code==CORE_OK&&f.key.value==104);
    assert(kit_ui_focus_order_sync(&f,7,swapped+1,2).code==CORE_OK&&!f.active);
    assert(kit_ui_focus_order_step(&f,8,items,4,1).code==CORE_OK&&f.key.value==104&&f.scope==8);
    KitUiFocusOrder before=f;items[1].key=items[0].key;
    assert(kit_ui_focus_order_step(&f,9,items,4,0).code!=CORE_OK&&!memcmp(&f,&before,sizeof(f)));
    assert(kit_ui_focus_order_sync(&f,8,NULL,0).code==CORE_OK&&!f.active);
    puts("kit_ui mixed focus order: wrap, reverse, disabled, scope, reorder, disappearance, duplicate rejection passed");return 0;
}
