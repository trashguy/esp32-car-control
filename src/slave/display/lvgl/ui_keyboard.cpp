#include "ui_keyboard.h"
#include <Arduino.h>

// =============================================================================
// Internal State (per-keyboard user data)
// =============================================================================

struct UiKeyboardUserData {
    UiKeyboardReadyCallback on_ready;
    UiKeyboardCancelCallback on_cancel;
    lv_obj_t* current_textarea;
};

// =============================================================================
// Event Handler
// =============================================================================

static void keyboard_event_handler(lv_event_t* e) {
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    UiKeyboardUserData* data = (UiKeyboardUserData*)lv_obj_get_user_data(kb);
    if (!data) return;
    
    if (code == LV_EVENT_READY) {
        // Enter/OK pressed
        const char* text = nullptr;
        if (data->current_textarea) {
            text = lv_textarea_get_text(data->current_textarea);
        }
        
        // Hide keyboard
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        
        // Call callback
        if (data->on_ready && text) {
            data->on_ready(text);
        }
    } else if (code == LV_EVENT_CANCEL) {
        // Cancel/close keyboard
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        
        if (data->on_cancel) {
            data->on_cancel();
        }
    }
}

// =============================================================================
// Public Interface
// =============================================================================

lv_obj_t* ui_keyboard_create(lv_obj_t* parent) {
    lv_obj_t* keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(keyboard, 320, 120);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Allocate and attach user data
    UiKeyboardUserData* data = (UiKeyboardUserData*)lv_malloc(sizeof(UiKeyboardUserData));
    if (data) {
        data->on_ready = nullptr;
        data->on_cancel = nullptr;
        data->current_textarea = nullptr;
        lv_obj_set_user_data(keyboard, data);
    }
    
    // Add event handler
    lv_obj_add_event_cb(keyboard, keyboard_event_handler, LV_EVENT_ALL, nullptr);
    
    return keyboard;
}

void ui_keyboard_show(lv_obj_t* keyboard, lv_obj_t* textarea,
                      UiKeyboardReadyCallback on_ready,
                      UiKeyboardCancelCallback on_cancel) {
    if (!keyboard) return;
    
    UiKeyboardUserData* data = (UiKeyboardUserData*)lv_obj_get_user_data(keyboard);
    if (data) {
        data->on_ready = on_ready;
        data->on_cancel = on_cancel;
        data->current_textarea = textarea;
    }
    
    if (textarea) {
        lv_keyboard_set_textarea(keyboard, textarea);
    }
    
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void ui_keyboard_hide(lv_obj_t* keyboard) {
    if (!keyboard) return;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

bool ui_keyboard_is_visible(lv_obj_t* keyboard) {
    if (!keyboard) return false;
    return !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void ui_keyboard_set_callbacks(lv_obj_t* keyboard,
                               UiKeyboardReadyCallback on_ready,
                               UiKeyboardCancelCallback on_cancel) {
    if (!keyboard) return;
    
    UiKeyboardUserData* data = (UiKeyboardUserData*)lv_obj_get_user_data(keyboard);
    if (data) {
        data->on_ready = on_ready;
        data->on_cancel = on_cancel;
    }
}
