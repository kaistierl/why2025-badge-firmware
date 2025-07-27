/**
 * @file lvgl.h
 * Include all LVGL related headers
 */

#ifndef LVGL_H
#define LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************
 * CURRENT VERSION OF LVGL
 ***************************/
#include "lv_version.h"

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lv_init.h"

#include "lvgl/stdlib/lv_mem.h"
#include "lvgl/stdlib/lv_string.h"
#include "lvgl/stdlib/lv_sprintf.h"

#include "lvgl/misc/lv_log.h"
#include "lvgl/misc/lv_timer.h"
#include "lvgl/misc/lv_math.h"
#include "lvgl/misc/lv_array.h"
#include "lvgl/misc/lv_async.h"
#include "lvgl/misc/lv_anim_timeline.h"
#include "lvgl/misc/lv_profiler_builtin.h"
#include "lvgl/misc/lv_rb.h"
#include "lvgl/misc/lv_utils.h"
#include "lvgl/misc/lv_iter.h"
#include "lvgl/misc/lv_circle_buf.h"
#include "lvgl/misc/lv_tree.h"
#include "lvgl/misc/cache/lv_cache.h"

#include "lvgl/tick/lv_tick.h"

#include "lvgl/core/lv_obj.h"
#include "lvgl/core/lv_group.h"
#include "lvgl/indev/lv_indev.h"
#include "lvgl/indev/lv_indev_gesture.h"
#include "lvgl/core/lv_refr.h"
#include "lvgl/display/lv_display.h"

#include "lvgl/font/lv_font.h"
#include "lvgl/font/lv_binfont_loader.h"
#include "lvgl/font/lv_font_fmt_txt.h"

#include "lvgl/widgets/animimage/lv_animimage.h"
#include "lvgl/widgets/arc/lv_arc.h"
#include "lvgl/widgets/bar/lv_bar.h"
#include "lvgl/widgets/button/lv_button.h"
#include "lvgl/widgets/buttonmatrix/lv_buttonmatrix.h"
#include "lvgl/widgets/calendar/lv_calendar.h"
#include "lvgl/widgets/canvas/lv_canvas.h"
#include "lvgl/widgets/chart/lv_chart.h"
#include "lvgl/widgets/checkbox/lv_checkbox.h"
#include "lvgl/widgets/dropdown/lv_dropdown.h"
#include "lvgl/widgets/image/lv_image.h"
#include "lvgl/widgets/imagebutton/lv_imagebutton.h"
#include "lvgl/widgets/keyboard/lv_keyboard.h"
#include "lvgl/widgets/label/lv_label.h"
#include "lvgl/widgets/led/lv_led.h"
#include "lvgl/widgets/line/lv_line.h"
#include "lvgl/widgets/list/lv_list.h"
#include "lvgl/widgets/lottie/lv_lottie.h"
#include "lvgl/widgets/menu/lv_menu.h"
#include "lvgl/widgets/msgbox/lv_msgbox.h"
#include "lvgl/widgets/roller/lv_roller.h"
#include "lvgl/widgets/scale/lv_scale.h"
#include "lvgl/widgets/slider/lv_slider.h"
#include "lvgl/widgets/span/lv_span.h"
#include "lvgl/widgets/spinbox/lv_spinbox.h"
#include "lvgl/widgets/spinner/lv_spinner.h"
#include "lvgl/widgets/switch/lv_switch.h"
#include "lvgl/widgets/table/lv_table.h"
#include "lvgl/widgets/tabview/lv_tabview.h"
#include "lvgl/widgets/textarea/lv_textarea.h"
#include "lvgl/widgets/tileview/lv_tileview.h"
#include "lvgl/widgets/win/lv_win.h"
#include "lvgl/widgets/3dtexture/lv_3dtexture.h"

#include "lvgl/others/snapshot/lv_snapshot.h"
#include "lvgl/others/sysmon/lv_sysmon.h"
#include "lvgl/others/monkey/lv_monkey.h"
#include "lvgl/others/gridnav/lv_gridnav.h"
#include "lvgl/others/fragment/lv_fragment.h"
#include "lvgl/others/imgfont/lv_imgfont.h"
#include "lvgl/others/observer/lv_observer.h"
#include "lvgl/others/ime/lv_ime_pinyin.h"
#include "lvgl/others/file_explorer/lv_file_explorer.h"
#include "lvgl/others/font_manager/lv_font_manager.h"
#include "lvgl/others/xml/lv_xml.h"
#include "lvgl/others/xml/lv_xml_component.h"
#include "lvgl/others/test/lv_test.h"

#include "lvgl/libs/barcode/lv_barcode.h"
#include "lvgl/libs/bin_decoder/lv_bin_decoder.h"
#include "lvgl/libs/bmp/lv_bmp.h"
#include "lvgl/libs/rle/lv_rle.h"
#include "lvgl/libs/fsdrv/lv_fsdrv.h"
#include "lvgl/libs/lodepng/lv_lodepng.h"
#include "lvgl/libs/libpng/lv_libpng.h"
#include "lvgl/libs/gif/lv_gif.h"
#include "lvgl/libs/qrcode/lv_qrcode.h"
#include "lvgl/libs/tjpgd/lv_tjpgd.h"
#include "lvgl/libs/libjpeg_turbo/lv_libjpeg_turbo.h"
#include "lvgl/libs/freetype/lv_freetype.h"
#include "lvgl/libs/rlottie/lv_rlottie.h"
#include "lvgl/libs/ffmpeg/lv_ffmpeg.h"
#include "lvgl/libs/tiny_ttf/lv_tiny_ttf.h"
#include "lvgl/libs/svg/lv_svg.h"
#include "lvgl/libs/svg/lv_svg_render.h"

#include "lvgl/layouts/lv_layout.h"

#include "lvgl/draw/lv_draw_buf.h"
#include "lvgl/draw/lv_draw_vector.h"
#include "lvgl/draw/sw/lv_draw_sw_utils.h"

#include "lvgl/themes/lv_theme.h"

#include "lvgl/drivers/lv_drivers.h"

#include "lvgl/lv_api_map_v8.h"
#include "lvgl/lv_api_map_v9_0.h"
#include "lvgl/lv_api_map_v9_1.h"

#if LV_USE_PRIVATE_API
#include "lvgl/lvgl_private.h"
#endif


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

/** Gives 1 if the x.y.z version is supported in the current version
 * Usage:
 *
 * - Require v6
 * #if LV_VERSION_CHECK(6,0,0)
 *   new_func_in_v6();
 * #endif
 *
 *
 * - Require at least v5.3
 * #if LV_VERSION_CHECK(5,3,0)
 *   new_feature_from_v5_3();
 * #endif
 *
 *
 * - Require v5.3.2 bugfixes
 * #if LV_VERSION_CHECK(5,3,2)
 *   bugfix_in_v5_3_2();
 * #endif
 *
 */
#define LV_VERSION_CHECK(x,y,z) (x == LVGL_VERSION_MAJOR && (y < LVGL_VERSION_MINOR || (y == LVGL_VERSION_MINOR && z <= LVGL_VERSION_PATCH)))

/**
 * Wrapper functions for VERSION macros
 */

static inline int lv_version_major(void)
{
    return LVGL_VERSION_MAJOR;
}

static inline int lv_version_minor(void)
{
    return LVGL_VERSION_MINOR;
}

static inline int lv_version_patch(void)
{
    return LVGL_VERSION_PATCH;
}

static inline const char * lv_version_info(void)
{
    return LVGL_VERSION_INFO;
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LVGL_H*/
