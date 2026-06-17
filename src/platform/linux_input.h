#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace platform {

constexpr size_t K_NAV_KEY_COUNT = 5;
using KeyListener                = void (*)(uint32_t key, const char* key_name, void* user_data);
using GlobalKeyListener          = void (*)(uint32_t    key,
                                            const char* key_name,
                                            bool        long_pressed,
                                            void*       user_data);

enum class NavTriggerMode {
  CLICK,
  LONG_PRESS,
};

void        init_key_input(lv_display_t* display);
void        attach_key_router(lv_indev_t* indev);
void        register_nav_button(size_t index, lv_obj_t* button);
void        unregister_nav_button(size_t index, lv_obj_t* button);
void        set_key_listener(KeyListener listener, void* user_data);
void        clear_key_listener(KeyListener listener, void* user_data);
void        set_global_key_listener(GlobalKeyListener listener, void* user_data);
void        clear_global_key_listener(GlobalKeyListener listener, void* user_data);
void        set_nav_trigger_mode(NavTriggerMode mode);
const char* describe_key(uint32_t key);

}  // namespace platform
