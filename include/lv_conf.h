/**
 * @file lv_conf.h
 * LVGL configuration for ESP32-S3 with ILI9341 320x240 display
 * 
 * Based on LVGL 9.x configuration template
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565) for ILI9341 */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color for ESP32 SPI */
#define LV_COLOR_16_SWAP 1

/*====================
   MEMORY SETTINGS
 *====================*/

/* Size of the memory available for `lv_malloc()` in bytes (>= 2kB) */
#define LV_MEM_SIZE (48 * 1024U)

/* Use stdlib malloc/free - better for ESP32 with PSRAM */
#define LV_MEM_CUSTOM 0

/* Stack size for LVGL drawing thread (not used in single-thread mode) */
#define LV_DRAW_THREAD_STACK_SIZE (8 * 1024U)

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in ms */
#define LV_DEF_REFR_PERIOD 16

/* Default input device read period in ms */
#define LV_DEF_INDEV_READ_PERIOD 30

/* Use a custom tick source - we'll use esp_timer */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*====================
   OPERATING SYSTEM
 *====================*/

/* Use FreeRTOS */
#define LV_USE_OS LV_OS_FREERTOS

/*====================
   FEATURE CONFIGURATION
 *====================*/

/* Enable float for animations and other calculations */
#define LV_USE_FLOAT 1

/*====================
   DRAWING
 *====================*/

/* Disable ARM assembly optimizations (not for Xtensa ESP32) */
/* Note: Also set via -DLV_USE_DRAW_SW_ASM=0 in platformio.ini build_flags */
#ifndef LV_USE_DRAW_SW_ASM
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#endif

/* Enable complex draw operations */
#define LV_DRAW_SW_COMPLEX 1

/* Shadow drawing - essential for modern UI */
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0

/* Enable layer blending (for transparency effects) */
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (24 * 1024)

/*====================
   GPU CONFIGURATION
 *====================*/

/* No hardware GPU on ESP32 */
#define LV_USE_DRAW_ARM2D 0
#define LV_USE_DRAW_SDL 0
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_PXP 0
#define LV_USE_DRAW_DAVE2D 0

/*====================
   LOGGING
 *====================*/

/* Enable logging for debugging (disable in production) */
#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF 1
#endif

/*====================
   ASSERTS
 *====================*/

/* Enable asserts for debugging */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/*====================
   FONT USAGE
 *====================*/

/* Montserrat fonts - anti-aliased */
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

/* Special fonts */
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable subpixel rendering for smoother fonts */
#define LV_USE_FONT_SUBPX 0
#define LV_FONT_SUBPX_BGR 0

/* Enable font compression - saves flash */
#define LV_USE_FONT_COMPRESSED 1

/* Enable drawing placeholders for missing glyphs */
#define LV_USE_FONT_PLACEHOLDER 1

/*====================
   TEXT SETTINGS
 *====================*/

/* String character encoding - UTF-8 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Line break characters */
#define LV_TXT_BREAK_CHARS " ,.;:-_)]}"

/*====================
   WIDGETS
 *====================*/

/* Base widgets (always enabled) */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 0

/* Extra widgets */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 1
#define LV_USE_LED 1
#define LV_USE_LIST 1
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 1
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 1
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/*====================
   THEMES
 *====================*/

/* Enable default theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* Dark theme by default */
    #define LV_THEME_DEFAULT_DARK 1
    /* Enable grow on press effect */
    #define LV_THEME_DEFAULT_GROW 1
    /* Transition time in ms */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* Simple theme (smaller code) */
#define LV_USE_THEME_SIMPLE 0

/* Monochrome theme */
#define LV_USE_THEME_MONO 0

/*====================
   LAYOUTS
 *====================*/

/* Flexbox layout */
#define LV_USE_FLEX 1

/* Grid layout */
#define LV_USE_GRID 1

/*====================
   IMAGE DECODERS
 *====================*/

/* PNG decoder - useful for icons */
#define LV_USE_PNG 0

/* BMP decoder */
#define LV_USE_BMP 0

/* JPG + split JPG decoder */
#define LV_USE_SJPG 0

/* GIF decoder */
#define LV_USE_GIF 0

/* QR code */
#define LV_USE_QRCODE 0

/* Barcode */
#define LV_USE_BARCODE 0

/*====================
   ANIMATION
 *====================*/

/* Enable screen transition animations */
#define LV_USE_OBJ_SCROLL_MOMENTUM 1

/*====================
   FILE SYSTEM
 *====================*/

/* Disable LVGL file system - we use SD_MMC directly */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_LITTLEFS 0
#define LV_USE_FS_MEMFS 0

/*====================
   OTHERS
 *====================*/

/* Snapshot - take screen captures */
#define LV_USE_SNAPSHOT 0

/* Monkey - random input testing */
#define LV_USE_MONKEY 0

/* Profiler */
#define LV_USE_PROFILER 0

/* Sysmon - display CPU and memory usage */
#define LV_USE_SYSMON 0

/* Enable performance monitor overlay (for debugging) */
#define LV_USE_PERF_MONITOR 0

/* Enable memory monitor overlay (for debugging) */
#define LV_USE_MEM_MONITOR 0

/*====================
   COMPILER SETTINGS
 *====================*/

/* Big endian system */
#define LV_BIG_ENDIAN_SYSTEM 0

/* Custom attribute for large const arrays */
#define LV_ATTRIBUTE_LARGE_CONST

/* Custom attribute for large RAM arrays */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/* Place performance critical functions into a faster memory */
#define LV_ATTRIBUTE_FAST_MEM

/* Export integer constant to binding */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/* Prefix variables for use in external libraries */
#define LV_GLOBAL_PREFIX

/* Default image cache size */
#define LV_CACHE_DEF_SIZE 0

/* Default image header cache count */
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 0

/* Layer buffer allocation */
#define LV_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (3 * 1024)

#endif /* LV_CONF_H */
