#include <stdio.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include <lvgl.h>
#include <lvgl/drivers/sdl/lv_sdl_window.h>
#include <lvgl/drivers/sdl/lv_sdl_mouse.h>
#include <lvgl/drivers/sdl/lv_sdl_mousewheel.h>
#include <lvgl/drivers/sdl/lv_sdl_keyboard.h>

#define LV_USE_DEMO_WIDGETS 1

extern void lv_demo_widgets(void);
void lv_demo_music(void);

static lv_display_t *lvDisplay;
static lv_indev_t *lvMouse;
static lv_indev_t *lvMouseWheel;
static lv_indev_t *lvKeyboard;

#if LV_USE_LOG != 0
static void lv_log_print_g_cb(lv_log_level_t level, const char * buf)
{
    printf("LVGL_LOG: %s\n", buf);
    LV_UNUSED(level);
}
#endif

int main()
{
    /* initialize lvgl */
    lv_init();

    /* Register the log print callback */
    #if LV_USE_LOG != 0
    lv_log_register_print_cb(lv_log_print_g_cb);
    #endif

    /* Add a display
    * Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/

    lvDisplay = lv_sdl_window_create(640, 480);
    if (!lvDisplay) {
        printf("Failed to create lvDisplay\n");
        return 1;
    } else {
        printf("Created lvDisplay\n");
    }
    lvKeyboard = lv_sdl_keyboard_create();
    if (!lvKeyboard) {
        printf("Failed to create lvKeyboard\n");
        return 1;
    } else {
        printf("Created lvKeyboard\n");
    }
    lvMouse = lv_sdl_mouse_create();
    if (!lvMouse) {
        printf("Failed to create lvMouse\n");
    }
    lvMouseWheel = lv_sdl_mousewheel_create();
    if (!lvMouseWheel) {
        printf("Failed to create lvMouse\n");
    }

    printf("lv_display_get_color_format(disp) :%u\n", lv_display_get_color_format(lvDisplay));
    printf("lv_color_format_get_size(lv_display_get_color_format(disp)); :%u\n", lv_color_format_get_size(lv_display_get_color_format(lvDisplay)));

    /* create Widgets on the screen */
    // lv_demo_widgets();
    lv_demo_music();

    Uint32 lastTick = SDL_GetTicks();
    while(1) {
        SDL_Delay(5);
        Uint32 current = SDL_GetTicks();
        lv_tick_inc(current - lastTick); // Update the tick timer. Tick is new for LVGL 9
        lastTick = current;
        lv_timer_handler(); // Update the UI-
    }

    return 0;
}
