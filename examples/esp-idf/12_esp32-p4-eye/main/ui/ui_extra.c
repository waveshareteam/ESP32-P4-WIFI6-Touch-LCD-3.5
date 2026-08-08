/*
 * UI Extra Module
 * 
 * This file implements additional UI functionality for the Eye Factory Demo.
 * It handles the UI interactions, page navigation, settings management,
 * and provides interfaces for controlling the camera features.
 * 
 * The main components include:
 * - Page management (camera, interval camera, video, album, settings)
 * - UI element creation and events handling
 * - Settings storage and retrieval
 * - Button event handlers
 * - Camera control interfaces
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "lvgl.h"

#include "bsp/display.h"

#include "ui_extra.h"

#include "app_storage.h"
#include "app_album.h"
#include "app_video_stream.h"
#include "app_isp.h"

/* Constants */
#define IMG_BASE_ZOOM       60
#define BTN_ZOOM_FACTOR     1.8
#define IMG_ZOOM_FACTOR     1.8
#define IMG_ZOOM_OFFSET     0

#define IMG_SCROLL_MAX_ANGLE    0
#define IMG_SCROLL_CENTER_ANGLE 900

#define ARC_RIGHT_HALF      1

#define MIN_INTERVAL_TIME   5
#define MAX_INTERVAL_TIME   120
#define INTERVAL_TIME_STEP  5
#define MAX_MAGNIFICATION_FACTOR 4
#define MIN_MAGNIFICATION_FACTOR 1

#define DEFAULT_MAGNIFICATION_FACTOR 1
#define DEFAULT_INTERVAL_TIME 30
#define DEFAULT_SAVED_PHOTO_COUNT 0

#define UI_BASE_SIZE 240

#define UI_SCALE_X(val) ((lv_coord_t)((int32_t)(val) * (int32_t)BSP_LCD_H_RES / UI_BASE_SIZE))
#define UI_SCALE_Y(val) ((lv_coord_t)((int32_t)(val) * (int32_t)BSP_LCD_V_RES / UI_BASE_SIZE))

static const char * TAG = "ui_extra";

/* Global variables */
// UI state variables
static ui_page_t current_page = UI_PAGE_MAIN;
static bool is_scrolling = false;
static bool is_sd_card_mounted = false;
static bool is_usb_disk_mounted = false;
static bool is_video_recording = false;
static bool is_camera_settings_panel_active = false;
static bool is_ui_init = false;

// AI Detection Mode variables
static ai_detect_mode_t current_ai_detect_mode = AI_DETECT_PEDESTRIAN;
static lv_obj_t *ai_mode_label = NULL;

// UI settings
static uint16_t magnification_factor = DEFAULT_MAGNIFICATION_FACTOR;
static uint16_t interval_time = DEFAULT_INTERVAL_TIME;
static uint16_t saved_photo_count = DEFAULT_SAVED_PHOTO_COUNT;
static uint32_t video_recording_seconds = 0;

static uint32_t contrast_percent = DEFAULT_CONTRAST_PERCENT;
static uint32_t saturation_percent = DEFAULT_SATURATION_PERCENT;
static uint32_t brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;
static uint32_t hue_percent = DEFAULT_HUE_PERCENT;

// UI elements
static lv_obj_t *scroll_cont = NULL;
static lv_obj_t *info_label = NULL;
static lv_obj_t *selected_btn = NULL;
static lv_obj_t *vkey_cont = NULL;
static lv_obj_t *album_vkey_cont = NULL;
static lv_obj_t *album_vkey_root = NULL;
static lv_obj_t *take_photos_popup = NULL;
static lv_obj_t *take_photos_popup_label = NULL;
static uint32_t take_photos_session_count = 0;
static lv_coord_t btn_width = 0;
static lv_coord_t btn_height = 0;

// Timers
static lv_timer_t *lv_popup_timer = NULL;
static lv_timer_t *lv_additional_photo_timer = NULL;
static lv_timer_t *lv_interval_timer = NULL;
static lv_timer_t *lv_video_timer = NULL;
static lv_timer_t *lv_usb_disk_timer = NULL;
static lv_timer_t *lv_take_photos_popup_timer = NULL;

// Settings options
static const char* const gyroscope_options[] = {"Off", "On"};
static const char* const od_options[] = {"Off", "On"};
static const char* const resolution_options[] = {"720P", "1080P", "480P", "960P"};
static const char* const flash_options[] = {"Off", "On"};

// Settings state
static int current_settings_item = 0;
static lv_obj_t* settings_items[5];
static settings_info_t current_settings;

static lv_obj_t *camera_settings_items[5]; 
static int current_camera_settings_item = 0;

static lv_obj_t *camera_settings_slider_touch_area[4] = {0};
static lv_coord_t camera_settings_slider_last_y[4] = {0};
static bool camera_settings_slider_dragging[4] = {0};

typedef struct {
    const char* const* options;
    int option_count;
    int current_option;
    lv_obj_t* label;
} setting_options_t;

static setting_options_t settings_options[5];

static bool s_touch_controls_base_inited = false;
static lv_obj_t *s_touch_controls_album_bound_root = NULL;
static lv_obj_t *s_touch_controls_usb_bound_root = NULL;

typedef enum {
    UI_EXTRA_ALBUM_DELETE_CHOICE_YES = 1,
    UI_EXTRA_ALBUM_DELETE_CHOICE_NO,
} ui_extra_album_delete_choice_t;

typedef enum {
    UI_AXIS_X = 0,
    UI_AXIS_Y = 1,
} ui_axis_t;

static void update_settings_focus(int new_item);
static void update_camera_settings_focus(int item_index);
static void save_camera_settings(void);
static void switch_to_camera_settings_panel(void);
static void take_photos_popup_timer_callback(lv_timer_t * timer);
static void ui_extra_set_magnification_factor_runtime(uint16_t factor);

static void ui_extra_camera_settings_slider_touch_area_cb(lv_event_t *e);
static void ui_extra_setup_camera_settings_slider_touch_areas(void);

static void ui_extra_vkey_rotate_ccw_90(lv_obj_t * img)
{
    if (img == NULL) {
        return;
    }

    lv_coord_t w = lv_obj_get_width(img);
    lv_coord_t h = lv_obj_get_height(img);
    lv_coord_t px = w > 0 ? w / 2 : 0;
    lv_coord_t py = h > 0 ? h / 2 : 0;

    lv_obj_set_style_transform_pivot_x(img, px, 0);
    lv_obj_set_style_transform_pivot_y(img, py, 0);
    lv_obj_set_style_transform_angle(img, 2700, 0);
}

static void ui_extra_obj_rotate_cw_90(lv_obj_t * obj)
{
    while(true) {
        if(obj == NULL) break;

        lv_obj_update_layout(obj);
        lv_obj_set_style_transform_pivot_x(obj, lv_obj_get_width(obj) / 2, 0);
        lv_obj_set_style_transform_pivot_y(obj, lv_obj_get_height(obj) / 2, 0);
        lv_obj_set_style_transform_angle(obj, 900, 0);
        break;
    }
}

static void ui_extra_album_vkey_apply_layout(void)
{
    if (!ui_ScreenAlbum || !ui_ImageScreenAlbum) {
        return;
    }

    if (ui_ScreenAlbum != album_vkey_root) {
        album_vkey_root = ui_ScreenAlbum;
        album_vkey_cont = NULL;
    }

    if (album_vkey_cont == NULL) {
        album_vkey_cont = lv_obj_create(ui_ImageScreenAlbum);
        lv_obj_set_style_bg_opa(album_vkey_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(album_vkey_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(album_vkey_cont, 0, 0);
        lv_obj_set_flex_flow(album_vkey_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(album_vkey_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(album_vkey_cont, UI_SCALE_X(12), 0);
        lv_obj_clear_flag(album_vkey_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(album_vkey_cont, LV_OBJ_FLAG_CLICKABLE);
    }

    if (!album_vkey_cont) {
        return;
    }

    if (ui_ImageAlbumUp && lv_obj_get_parent(ui_ImageAlbumUp) != album_vkey_cont) {
        lv_obj_set_parent(ui_ImageAlbumUp, album_vkey_cont);
    }

    if (ui_ImageAlbumMenu && lv_obj_get_parent(ui_ImageAlbumMenu) != album_vkey_cont) {
        lv_obj_set_parent(ui_ImageAlbumMenu, album_vkey_cont);
    }

    if (ui_ImageAlbumDown && lv_obj_get_parent(ui_ImageAlbumDown) != album_vkey_cont) {
        lv_obj_set_parent(ui_ImageAlbumDown, album_vkey_cont);
    }

    lv_obj_set_size(album_vkey_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(album_vkey_cont, LV_ALIGN_BOTTOM_MID, 0, -UI_SCALE_Y(20));

    ui_extra_vkey_rotate_ccw_90(ui_ImageAlbumUp);
    ui_extra_vkey_rotate_ccw_90(ui_ImageAlbumMenu);
    ui_extra_vkey_rotate_ccw_90(ui_ImageAlbumDown);
}

void ui_extra_key_input(ui_extra_key_t key)
{
    switch (key) {
        case UI_EXTRA_KEY_MENU:
            ui_extra_btn_menu();
            return;
        case UI_EXTRA_KEY_OK:
            ui_extra_btn_encoder();
            return;
        case UI_EXTRA_KEY_UP:
            ui_extra_btn_up();
            if (current_page == UI_PAGE_ALBUM &&
                lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
                app_album_prev_image();
            }
            return;
        case UI_EXTRA_KEY_DOWN:
            ui_extra_btn_down();
            if (current_page == UI_PAGE_ALBUM &&
                lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) &&
                lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
                app_album_next_image();
            }
            return;
        case UI_EXTRA_KEY_MAG_1X:
            while(true) {
                if (current_page != UI_PAGE_CAMERA && current_page != UI_PAGE_VIDEO_MODE) break;
                app_extra_set_magnification_factor(1);
                break;
            }
            return;
        default:
            return;
    }
}

static void ui_extra_vkey_event_cb(lv_event_t *e)
{
    ui_extra_key_input((ui_extra_key_t)(intptr_t)lv_event_get_user_data(e));
}

static void ui_extra_menu_item_clicked_cb(lv_event_t *e)
{
    selected_btn = lv_event_get_target(e);
    ui_extra_btn_menu();
}

static void ui_extra_album_delete_choice_clicked_cb(lv_event_t *e)
{
    const ui_extra_album_delete_choice_t choice = (ui_extra_album_delete_choice_t)(intptr_t)lv_event_get_user_data(e);

    if (!ui_ButtonPanelImageScreenAlbumDeleteYES || !ui_ButtonPanelImageScreenAlbumDeleteNO) {
        return;
    }

    lv_obj_clear_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED);
    lv_obj_clear_state(ui_ButtonPanelImageScreenAlbumDeleteNO, LV_STATE_FOCUSED);

    switch (choice) {
        case UI_EXTRA_ALBUM_DELETE_CHOICE_YES:
            lv_obj_add_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED);
            break;
        case UI_EXTRA_ALBUM_DELETE_CHOICE_NO:
            lv_obj_add_state(ui_ButtonPanelImageScreenAlbumDeleteNO, LV_STATE_FOCUSED);
            break;
        default:
            return;
    }

    ui_extra_btn_menu();
}

static void ui_extra_settings_item_clicked_cb(lv_event_t *e)
{
    const int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (idx < 0 || idx > 3) {
        return;
    }

    is_camera_settings_panel_active = false;
    update_settings_focus(idx);
    ui_extra_btn_menu();
}

static void ui_extra_settings_menu_clicked_cb(lv_event_t *e)
{
    (void)e;

    if (current_page != UI_PAGE_SETTINGS) {
        return;
    }

    if (!is_camera_settings_panel_active) {
        switch_to_camera_settings_panel();
    }

    update_camera_settings_focus(4);
    ui_extra_btn_menu();
}

static void ui_extra_camera_settings_item_clicked_cb(lv_event_t *e)
{
    const int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (!is_camera_settings_panel_active) {
        switch_to_camera_settings_panel();
    }

    switch (idx) {
        case 0:
        case 1:
        case 2:
        case 3:
            update_camera_settings_focus(idx);
            break;
        case 4:
            update_camera_settings_focus(4);
            ui_extra_btn_menu();
            break;
        default:
            break;
    }
}

static void ui_extra_camera_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    const int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const lv_event_code_t code = lv_event_get_code(e);
    const uint32_t value = (uint32_t)lv_slider_get_value(slider);

    switch (idx) {
        case 0:
            contrast_percent = value;
            app_isp_set_contrast(contrast_percent);
            break;
        case 1:
            saturation_percent = value;
            app_isp_set_saturation(saturation_percent);
            break;
        case 2:
            brightness_percent = value;
            app_isp_set_brightness(brightness_percent);
            break;
        case 3:
            hue_percent = value;
            app_isp_set_hue(hue_percent);
            break;
        default:
            return;
    }

    switch (code) {
        case LV_EVENT_RELEASED:
            app_storage_save_camera_settings(contrast_percent, saturation_percent, brightness_percent, hue_percent);
            break;
        default:
            break;
    }
}

static void ui_extra_register_touch_controls_base(void)
{
    if (s_touch_controls_base_inited) {
        return;
    }

    s_touch_controls_base_inited = true;

    lv_obj_add_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageCanvasMenu, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);

    lv_obj_add_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageCanvasUp, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_UP);

    lv_obj_add_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageCanvasDown, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_DOWN);

    lv_obj_add_flag(ui_ImageCanvasSelect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageCanvasSelect, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_LabelCanvas2X, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_LabelCanvas2X, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_UP);

    lv_obj_add_flag(ui_LabelCanvas3X, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_LabelCanvas3X, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_DOWN);

    lv_obj_add_flag(ui_LabelCanvas1X, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_LabelCanvas1X, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MAG_1X);

    lv_obj_add_flag(ui_LabelCanvas5mplus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_LabelCanvas5mplus, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_UP);

    lv_obj_add_flag(ui_LabelCanvas5mSub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_LabelCanvas5mSub, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_DOWN);

    lv_obj_add_flag(ui_PanelPanelSettingsGyroscope, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsGyroscope, ui_extra_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)0);

    lv_obj_add_flag(ui_PanelPanelSettingsLanguage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsLanguage, ui_extra_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)1);

    lv_obj_add_flag(ui_PanelPanelSettingsRes, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsRes, ui_extra_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)2);

    lv_obj_add_flag(ui_PanelPanelSettingsFlash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsFlash, ui_extra_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)3);

    lv_obj_add_flag(ui_PanelSettingsMenu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelSettingsMenu, ui_extra_settings_menu_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupCamera, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupCameraInterval, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupVideoMode, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupSDWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupIntervalTimerWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupIntervalTimerWarningEnd, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupAICamera, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupSDSpaceWarning, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupSDSpaceWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelCanvasPopupCameraSDSpaceWarning, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelCanvasPopupCameraSDSpaceWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);

    lv_obj_add_flag(ui_PanelPanelSettingsContrast, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsContrast, ui_extra_camera_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsContrast, ui_extra_camera_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)0);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsContrast, ui_extra_camera_slider_event_cb, LV_EVENT_RELEASED, (void *)0);

    lv_obj_add_flag(ui_PanelPanelSettingsSaturation, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsSaturation, ui_extra_camera_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsSaturation, ui_extra_camera_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)1);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsSaturation, ui_extra_camera_slider_event_cb, LV_EVENT_RELEASED, (void *)1);

    lv_obj_add_flag(ui_PanelPanelSettingsBrightness, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsBrightness, ui_extra_camera_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)2);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsBrightness, ui_extra_camera_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)2);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsBrightness, ui_extra_camera_slider_event_cb, LV_EVENT_RELEASED, (void *)2);

    lv_obj_add_flag(ui_PanelPanelSettingsHue, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelPanelSettingsHue, ui_extra_camera_settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)3);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsHue, ui_extra_camera_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)3);
    lv_obj_add_event_cb(ui_SliderPanelPanelSettingsHue, ui_extra_camera_slider_event_cb, LV_EVENT_RELEASED, (void *)3);
}

static void ui_extra_register_touch_controls_album(void)
{
    if (!ui_ScreenAlbum || !ui_ImageAlbumMenu || !ui_ImageAlbumUp || !ui_ImageAlbumDown) {
        return;
    }

    if (ui_ScreenAlbum == s_touch_controls_album_bound_root) {
        return;
    }

    s_touch_controls_album_bound_root = ui_ScreenAlbum;

    lv_obj_add_flag(ui_ImageAlbumMenu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageAlbumMenu, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);

    lv_obj_add_flag(ui_ImageAlbumUp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageAlbumUp, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_UP);

    lv_obj_add_flag(ui_ImageAlbumDown, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ImageAlbumDown, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_DOWN);

    if (ui_PanelAlbumPopupSDWarning) {
        lv_obj_add_flag(ui_PanelAlbumPopupSDWarning, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_PanelAlbumPopupSDWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_OK);
    }

    if (ui_ButtonPanelImageScreenAlbumDeleteYES) {
        lv_obj_add_event_cb(ui_ButtonPanelImageScreenAlbumDeleteYES, ui_extra_album_delete_choice_clicked_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_ALBUM_DELETE_CHOICE_YES);
    }

    if (ui_ButtonPanelImageScreenAlbumDeleteNO) {
        lv_obj_add_event_cb(ui_ButtonPanelImageScreenAlbumDeleteNO, ui_extra_album_delete_choice_clicked_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_ALBUM_DELETE_CHOICE_NO);
    }
}

static void ui_extra_register_touch_controls_usb(void)
{
    if (!ui_ScreenUSB) {
        return;
    }

    if (ui_ScreenUSB == s_touch_controls_usb_bound_root) {
        return;
    }

    s_touch_controls_usb_bound_root = ui_ScreenUSB;

    lv_obj_add_flag(ui_ScreenUSB, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ScreenUSB, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);

    if (ui_ImageScreenUSB) {
        lv_obj_add_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_ImageScreenUSB, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);
    }

    if (ui_ImageScreenUSBSuccess) {
        lv_obj_add_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_ImageScreenUSBSuccess, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);
    }

    if (ui_ImageScreenUSBWarning) {
        lv_obj_add_flag(ui_ImageScreenUSBWarning, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_ImageScreenUSBWarning, ui_extra_vkey_event_cb, LV_EVENT_CLICKED, (void *)UI_EXTRA_KEY_MENU);
    }
}

// Page mapping
typedef struct {
    const char *name;
    int page;
} PageMapping;

static const PageMapping page_map[] = {
    {"CAMERA", UI_PAGE_CAMERA},
    {"INTERVAL CAM", UI_PAGE_INTERVAL_CAM},
    {"VIDEO MODE", UI_PAGE_VIDEO_MODE},
    {"AI DETECT", UI_PAGE_AI_DETECT},
    {"ALBUM", UI_PAGE_ALBUM},
    {"USB DISK", UI_PAGE_USB_DISK},
    {"SETTINGS", UI_PAGE_SETTINGS},
    {NULL, -1}
};

/* Forward declarations */
static void ui_extra_redirect_to_main_page(void);
static void ui_extra_redirect_to_camera_page(void);
static void ui_extra_redirect_to_interval_camera_page(void);
static void ui_extra_redirect_to_video_mode_page(void);
static void ui_extra_redirect_to_album_page(void);
static void ui_extra_redirect_to_usb_disk_page(void);
static void ui_extra_redirect_to_settings_page(void);
static void ui_extra_clear_popup_window(void);
static void ui_extra_focus_on_picture_delete(void);
static void ui_extra_update_ai_detect_mode_label(void);
static void ui_extra_change_ai_detect_mode(ai_detect_mode_t mode);

/*-------------------------------------------------*/
/* Settings Management Functions                    */
/*-------------------------------------------------*/

/**
 * @brief Save current settings to storage
 */
static void save_current_settings(void)
{
    settings_info_t *settings = &current_settings;
    uint16_t interval = interval_time;
    uint16_t magnify = magnification_factor;
    
    app_storage_save_settings(settings, interval, magnify);
}

/**
 * @brief Initialize camera settings display
 */
static void init_camera_settings_display(void) {
    // Set initial value of the slider
    lv_slider_set_value(ui_SliderPanelPanelSettingsContrast, contrast_percent, LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderPanelPanelSettingsSaturation, saturation_percent, LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderPanelPanelSettingsBrightness, brightness_percent, LV_ANIM_OFF);
    lv_slider_set_value(ui_SliderPanelPanelSettingsHue, hue_percent, LV_ANIM_OFF);
}

static void ui_extra_camera_settings_slider_touch_area_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (idx < 0 || idx > 3) {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
        return;
    }

    lv_obj_t *slider = NULL;
    switch (idx) {
        case 0: slider = ui_SliderPanelPanelSettingsContrast; break;
        case 1: slider = ui_SliderPanelPanelSettingsSaturation; break;
        case 2: slider = ui_SliderPanelPanelSettingsBrightness; break;
        case 3: slider = ui_SliderPanelPanelSettingsHue; break;
        default: return;
    }

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        camera_settings_slider_dragging[idx] = true;
        camera_settings_slider_last_y[idx] = p.y;
        if (is_camera_settings_panel_active) {
            update_camera_settings_focus(idx);
        }
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (!camera_settings_slider_dragging[idx]) {
            return;
        }

        const lv_coord_t dy = p.y - camera_settings_slider_last_y[idx];
        const int32_t step = (int32_t)(dy / 2);
        if (step == 0) {
            return;
        }

        const int32_t cur = (int32_t)lv_slider_get_value(slider);
        int32_t next = cur + step;
        next = LV_CLAMP(0, next, 100);

        if (next != cur) {
            lv_slider_set_value(slider, (int32_t)next, LV_ANIM_OFF);
            lv_event_send(slider, LV_EVENT_VALUE_CHANGED, NULL);
        }

        camera_settings_slider_last_y[idx] = p.y;
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        camera_settings_slider_dragging[idx] = false;
        lv_event_send(slider, LV_EVENT_RELEASED, NULL);
        return;
    }
}

static void ui_extra_setup_camera_settings_slider_touch_areas(void)
{
    lv_obj_t *sliders[4] = {
        ui_SliderPanelPanelSettingsContrast,
        ui_SliderPanelPanelSettingsSaturation,
        ui_SliderPanelPanelSettingsBrightness,
        ui_SliderPanelPanelSettingsHue,
    };

    lv_obj_t *panels[4] = {
        ui_PanelPanelSettingsContrast,
        ui_PanelPanelSettingsSaturation,
        ui_PanelPanelSettingsBrightness,
        ui_PanelPanelSettingsHue,
    };

    for (int i = 0; i < 4; i++) {
        if (!sliders[i] || !panels[i]) {
            continue;
        }

        lv_slider_set_range(sliders[i], 0, 100);
        lv_obj_clear_flag(sliders[i], LV_OBJ_FLAG_CLICKABLE);

        if (camera_settings_slider_touch_area[i] && lv_obj_is_valid(camera_settings_slider_touch_area[i])) {
            if (lv_obj_get_parent(camera_settings_slider_touch_area[i]) != panels[i]) {
                lv_obj_set_parent(camera_settings_slider_touch_area[i], panels[i]);
            }
        } else {
            camera_settings_slider_touch_area[i] = lv_obj_create(panels[i]);
            lv_obj_set_style_bg_opa(camera_settings_slider_touch_area[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_opa(camera_settings_slider_touch_area[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(camera_settings_slider_touch_area[i], 0, 0);
            lv_obj_clear_flag(camera_settings_slider_touch_area[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(camera_settings_slider_touch_area[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(camera_settings_slider_touch_area[i], LV_OBJ_FLAG_EVENT_BUBBLE);

            lv_obj_add_event_cb(camera_settings_slider_touch_area[i], ui_extra_camera_settings_slider_touch_area_cb, LV_EVENT_PRESSED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(camera_settings_slider_touch_area[i], ui_extra_camera_settings_slider_touch_area_cb, LV_EVENT_PRESSING, (void *)(intptr_t)i);
            lv_obj_add_event_cb(camera_settings_slider_touch_area[i], ui_extra_camera_settings_slider_touch_area_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(camera_settings_slider_touch_area[i], ui_extra_camera_settings_slider_touch_area_cb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)i);
        }

        lv_obj_set_size(camera_settings_slider_touch_area[i], lv_obj_get_width(panels[i]), lv_obj_get_height(panels[i]));
        lv_obj_align(camera_settings_slider_touch_area[i], LV_ALIGN_CENTER, 0, 0);
    }
}

static void ui_extra_set_magnification_factor_runtime(uint16_t factor)
{
    while(true) {
        if (factor > MAX_MAGNIFICATION_FACTOR) {
            factor = MAX_MAGNIFICATION_FACTOR;
        }
        if (factor < MIN_MAGNIFICATION_FACTOR) {
            factor = MIN_MAGNIFICATION_FACTOR;
        }

        magnification_factor = factor;

        if (ui_LabelCanvasFactor == NULL) break;
        lv_label_set_text_fmt(ui_LabelCanvasFactor, "%dX", magnification_factor);
        ui_extra_obj_rotate_cw_90(ui_LabelCanvasFactor);
        break;
    }
}

/**
 * @brief Save camera settings
 */
static void save_camera_settings(void)
{
    app_storage_save_camera_settings(contrast_percent, saturation_percent, 
                                    brightness_percent, hue_percent);
    
    app_isp_set_contrast(contrast_percent);
    app_isp_set_saturation(saturation_percent);
    app_isp_set_brightness(brightness_percent);
    app_isp_set_hue(hue_percent);
}

/**
 * @brief Update camera settings focus
 * @param item_index Index of the item to focus
 */
static void update_camera_settings_focus(int item_index) {
    // Clear focus of all items
    for (int i = 0; i < 5; i++) {
        lv_obj_clear_state(camera_settings_items[i], LV_STATE_FOCUSED);
    }
    
    // Set focus of the current item
    lv_obj_add_state(camera_settings_items[item_index], LV_STATE_FOCUSED);
    current_camera_settings_item = item_index;
}

/**
 * @brief Switch to camera settings panel
 */
static void switch_to_camera_settings_panel(void) {
    // Hide main settings panel
    lv_obj_add_flag(ui_PanelSettings, LV_OBJ_FLAG_HIDDEN);
    // Show camera settings panel
    lv_obj_clear_flag(ui_PanelCameraSettings, LV_OBJ_FLAG_HIDDEN);
    
    // Initialize camera settings items
    camera_settings_items[0] = ui_PanelPanelSettingsContrast;
    camera_settings_items[1] = ui_PanelPanelSettingsSaturation;
    camera_settings_items[2] = ui_PanelPanelSettingsBrightness;
    camera_settings_items[3] = ui_PanelPanelSettingsHue;
    camera_settings_items[4] = ui_PanelSettingsMenu; // "back to menu" item
    
    // Initialize camera settings display
    init_camera_settings_display();

    ui_extra_setup_camera_settings_slider_touch_areas();
    
    // Reset current selected item and focus on the first item
    current_camera_settings_item = 0;
    update_camera_settings_focus(current_camera_settings_item);
    
    // Update flag
    is_camera_settings_panel_active = true;
}

/**
 * @brief Switch to main settings panel
 */
static void switch_to_main_settings_panel(void) {
    // Hide camera settings panel
    lv_obj_add_flag(ui_PanelCameraSettings, LV_OBJ_FLAG_HIDDEN);
    // Show main settings panel
    lv_obj_clear_flag(ui_PanelSettings, LV_OBJ_FLAG_HIDDEN);
    
    // Update flag
    is_camera_settings_panel_active = false;
}

/**
 * @brief Update display for a specific setting
 * @param setting_index Index of the setting to update
 */
static void update_setting_display(int setting_index) {
    if (setting_index < 0 || setting_index >= 5) {
        ESP_LOGW(TAG, "Invalid setting index: %d", setting_index);
        return;
    }
    
    setting_options_t* opt = &settings_options[setting_index];
    const char* current_text = opt->options[opt->current_option];
    
    // Update the label text
    if (opt->label) {
        lv_label_set_text(opt->label, current_text);
    }
    
    // Update the current settings info
    switch (setting_index) {
        case 0:
            current_settings.gyroscope = current_text;
            break;
        case 1:
            current_settings.od = current_text;
            break;
        case 2:
            current_settings.resolution = current_text;
            break;
        case 3:
            current_settings.flash = current_text;
            break;
    }
    
    save_current_settings();

    ESP_LOGD(TAG, "Setting %d updated to: %s", setting_index, current_text);
}

/**
 * @brief Initialize settings options
 */
static void init_settings_options(void) {
    // Gyroscope options
    settings_options[0].options = gyroscope_options;
    settings_options[0].option_count = sizeof(gyroscope_options) / sizeof(gyroscope_options[0]);
    settings_options[0].current_option = 0;    // Set to 0, corresponds to "Off"
    settings_options[0].label = ui_LabelPanelPanelSettingsGyroscopeBody;
    
    // OD options
    settings_options[1].options = od_options;
    settings_options[1].option_count = sizeof(od_options) / sizeof(od_options[0]);
    settings_options[1].current_option = 1;    // Set to 1, corresponds to "On"
    settings_options[1].label = ui_LabelPanelPanelSettingsODBody;
    
    // Resolution options
    settings_options[2].options = resolution_options;
    settings_options[2].option_count = sizeof(resolution_options) / sizeof(resolution_options[0]);
    settings_options[2].current_option = 3;    // Set to 3, corresponds to "960P"
    settings_options[2].label = ui_LabelPanelPanelSettingsResBody;
    
    // Flash options
    settings_options[3].options = flash_options;
    settings_options[3].option_count = sizeof(flash_options) / sizeof(flash_options[0]);
    settings_options[3].current_option = 1;    // Set to 1, corresponds to "On"
    settings_options[3].label = ui_LabelPanelPanelSettingsFlashBody;
    
    // Menu options
    settings_options[4].options = NULL;
    settings_options[4].option_count = 0;
    settings_options[4].current_option = 0;
    settings_options[4].label = ui_LabelPanelSettingsMenu;
    
    // Initialize the current settings info
    current_settings.gyroscope = gyroscope_options[0]; // Set to "Off"
    current_settings.od = od_options[1];           // Set to "On"
    current_settings.resolution = resolution_options[3];  // Set to "960P"
    current_settings.flash = flash_options[1];    // Set to "On"
}

/**
 * @brief Initialize settings display
 */
static void init_settings_display(void) {
    for (int i = 0; i < 4; i++) {  // Only update the first four settings items
        update_setting_display(i);
    }
}

/*-------------------------------------------------*/
/* UI Utility Functions                            */
/*-------------------------------------------------*/

/**
 * @brief Set zoom level for an image object
 * @param obj Image object
 * @param zoom Zoom level
 */
static void app_extra_img_set_zoom(lv_obj_t * obj, uint16_t zoom)
{
    if (!obj) return;
    
    lv_img_t * img = (lv_img_t *)obj;
    if(zoom == img->zoom) return;

    if(zoom == 0) zoom = 1;

    lv_obj_update_layout(obj);

    lv_coord_t w = lv_obj_get_width(obj);
    lv_coord_t h = lv_obj_get_height(obj);
    lv_area_t a;
    _lv_img_buf_get_transformed_area(&a, w, h, img->angle, img->zoom >> 8, &img->pivot);
    a.x1 += obj->coords.x1 - 1;
    a.y1 += obj->coords.y1 - 1;
    a.x2 += obj->coords.x1 + 1;
    a.y2 += obj->coords.y1 + 1;
    lv_obj_invalidate_area(obj, &a);

    img->zoom = zoom;

    /* Disable invalidations because lv_obj_refresh_ext_draw_size would invalidate
     * the whole ext draw area */
    lv_disp_t * disp = lv_obj_get_disp(obj);
    lv_disp_enable_invalidation(disp, false);
    lv_obj_refresh_ext_draw_size(obj);
    lv_disp_enable_invalidation(disp, true);

    _lv_img_buf_get_transformed_area(&a, w, h, img->angle, img->zoom, &img->pivot);
    a.x1 += obj->coords.x1 - 1;
    a.y1 += obj->coords.y1 - 1;
    a.x2 += obj->coords.x1 + 1;
    a.y2 += obj->coords.y1 + 1;
    lv_obj_invalidate_area(obj, &a);
}

/**
 * @brief Create an image button
 * @param parent Parent object
 * @param img_src Image source
 * @param btn_text Button text
 * @return Created button object
 */
static lv_obj_t * create_img_button(lv_obj_t *parent, const void *img_src, const char *btn_text) {
    if (!parent || !img_src || !btn_text) {
        ESP_LOGW(TAG, "Invalid parameters for create_img_button");
        return NULL;
    }
    
    lv_obj_t * btn = lv_btn_create(parent);
    
    lv_obj_set_user_data(btn, (void *)btn_text);
    
    // Set the button style
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_shadow_spread(btn, 0, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_ofs_x(btn, 0, 0);
    lv_obj_set_style_shadow_ofs_y(btn, 0, 0);
    
    lv_coord_t side = LV_MIN(UI_SCALE_X(50), UI_SCALE_Y(50));
    lv_obj_set_size(btn, side, side);

    lv_obj_add_event_cb(btn, ui_extra_menu_item_clicked_cb, LV_EVENT_CLICKED, NULL);

    // Create and configure the icon
    lv_obj_t * img = lv_img_create(btn);
    lv_img_set_src(img, img_src);
    lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_zoom(img, IMG_BASE_ZOOM);
    lv_obj_refr_size(img);
    lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_REAL);
    lv_obj_add_flag(img, LV_OBJ_FLAG_FLOATING);
    
    return btn;
}

/**
 * @brief Update settings focus
 * @param new_item New selected item index
 */
static void update_settings_focus(int new_item)
{
    // Clear the focus of all settings items
    for (int i = 0; i < 5; i++) {
        lv_event_send(settings_items[i], LV_EVENT_DEFOCUSED, NULL);
    }
    
    // Set the focus of the new selected item
    current_settings_item = new_item;
    lv_event_send(settings_items[current_settings_item], LV_EVENT_FOCUSED, NULL);
    ESP_LOGD(TAG, "Settings: selected item %d", current_settings_item);
}

static ui_axis_t ui_axis_other(ui_axis_t axis)
{
    return axis == UI_AXIS_X ? UI_AXIS_Y : UI_AXIS_X;
}

static lv_coord_t ui_scale_by_axis(ui_axis_t axis, lv_coord_t v)
{
    return axis == UI_AXIS_X ? UI_SCALE_X(v) : UI_SCALE_Y(v);
}

static lv_coord_t ui_obj_size_on_axis(lv_obj_t * obj, ui_axis_t axis)
{
    return axis == UI_AXIS_X ? lv_obj_get_width(obj) : lv_obj_get_height(obj);
}

static lv_coord_t ui_area_center_on_axis(const lv_area_t * a, ui_axis_t axis)
{
    return axis == UI_AXIS_X ? (a->x1 + lv_area_get_width(a) / 2) : (a->y1 + lv_area_get_height(a) / 2);
}

static lv_coord_t ui_btn_size_on_axis(ui_axis_t axis)
{
    return axis == UI_AXIS_X ? btn_width : btn_height;
}

static void ui_obj_set_translate_on_axis(lv_obj_t * obj, ui_axis_t axis, lv_coord_t v)
{
    switch(axis)
    {
        case UI_AXIS_X:
            lv_obj_set_style_translate_x(obj, v, 0);
            lv_obj_set_style_translate_y(obj, 0, 0);
            break;
        default:
            lv_obj_set_style_translate_x(obj, 0, 0);
            lv_obj_set_style_translate_y(obj, v, 0);
            break;
    }
}

static ui_axis_t ui_menu_detect_scroll_axis(lv_obj_t * cont)
{
    lv_coord_t range_x = lv_obj_get_scroll_left(cont) + lv_obj_get_scroll_right(cont);
    lv_coord_t range_y = lv_obj_get_scroll_top(cont) + lv_obj_get_scroll_bottom(cont);

    if (range_x == 0 && range_y == 0) {
        return lv_obj_get_width(cont) >= lv_obj_get_height(cont) ? UI_AXIS_X : UI_AXIS_Y;
    }
    return range_y > range_x ? UI_AXIS_Y : UI_AXIS_X;
}

/*-------------------------------------------------*/
/* Event Callbacks                                 */
/*-------------------------------------------------*/

/**
 * @brief Scroll event callback
 * @param e Event data
 */
static void scroll_event_cb(lv_event_t * e)
{
    static uint32_t last_scroll_tick = 0;
    uint32_t now_tick = lv_tick_get();
    if ((uint32_t)(now_tick - last_scroll_tick) < 16) {
        return;
    }
    last_scroll_tick = now_tick;

    is_scrolling = true; 
    lv_obj_t * cont = lv_event_get_target(e);
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);

    ui_axis_t axis_scroll = ui_menu_detect_scroll_axis(cont);
    ui_axis_t axis_translate = ui_axis_other(axis_scroll);

    lv_coord_t cont_center = ui_area_center_on_axis(&cont_a, axis_scroll);

    lv_coord_t r = ui_obj_size_on_axis(cont, axis_scroll) / 2;
    lv_coord_t max_translate = ui_obj_size_on_axis(cont, axis_translate) - (lv_coord_t)((float)ui_btn_size_on_axis(axis_translate) * BTN_ZOOM_FACTOR);
    max_translate = max_translate > 0 ? max_translate : 0;
    lv_coord_t y_range = max_translate > 0 ? max_translate : 1;
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    
    // Find the closest child to center
    lv_obj_t * closest_child = NULL;
    lv_coord_t min_diff = LV_COORD_MAX;
    
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);

        lv_coord_t child_center = ui_area_center_on_axis(&child_a, axis_scroll);
        lv_coord_t diff = LV_ABS(child_center - cont_center);
        
        if(diff < min_diff) {
            min_diff = diff;
            closest_child = child;
        }

        lv_coord_t y = max_translate;
        if (diff < r) {
            lv_sqrt_res_t sqrt_res;
            lv_sqrt((uint32_t)r * (uint32_t)r - (uint32_t)diff * (uint32_t)diff, &sqrt_res, 0x8000);
            y = LV_MIN((lv_coord_t)(r - sqrt_res.i), max_translate);
        }

        lv_coord_t translate = ui_scale_by_axis(axis_translate, 6) + (ARC_RIGHT_HALF ? (max_translate - y) : y);
        ui_obj_set_translate_on_axis(child, axis_translate, translate);
        lv_obj_set_style_opa(child, LV_OPA_COVER - lv_map(y, 0, y_range, 0, LV_OPA_COVER), 0);
        lv_obj_set_size(child, btn_width, btn_height);
    }
    
    // Apply special effects to closest child
    if(closest_child) {
        lv_obj_set_size(closest_child, btn_width * BTN_ZOOM_FACTOR, btn_height * BTN_ZOOM_FACTOR);
    }
}

/**
 * @brief Scroll end event callback
 * @param e Event data
 */
static void scroll_end_event_cb(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);
    
    // Get the center coordinates of the container
    lv_area_t cont_a;
    lv_obj_get_coords(cont, &cont_a);

    ui_axis_t axis_scroll = ui_menu_detect_scroll_axis(cont);
    lv_coord_t cont_center = ui_area_center_on_axis(&cont_a, axis_scroll);
    
    // Find the closest child to the center
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    lv_obj_t * closest_child = NULL;
    lv_coord_t min_diff = LV_COORD_MAX;
    
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);
        
        lv_coord_t child_center = ui_area_center_on_axis(&child_a, axis_scroll);
        lv_coord_t diff = LV_ABS(child_center - cont_center);
        
        if(diff < min_diff) {
            min_diff = diff;
            closest_child = child;
        }
    }
    
    // After scroll end, apply the styles again
    if(closest_child) {
        selected_btn = closest_child;
        const char* btn_text = lv_obj_get_user_data(selected_btn);
        if (btn_text) {
            ESP_LOGD(TAG, "selected: %s", btn_text);

            if (info_label) {
                lv_label_set_text(info_label, btn_text);

                if(current_page == UI_PAGE_MAIN) {
                    lv_obj_clear_flag(info_label, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        // Reset the styles of all children
        for(uint32_t i = 0; i < child_cnt; i++) {
            lv_obj_t * child = lv_obj_get_child(cont, i);
            lv_obj_set_size(child, btn_width, btn_height);

            // Reset the icon position to the default position
            lv_obj_t * img = lv_obj_get_child(child, 0);
            if(img) {
                app_extra_img_set_zoom(img, IMG_BASE_ZOOM);
                lv_img_set_angle(img, IMG_SCROLL_CENTER_ANGLE);
                lv_obj_refr_size(img);
                lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
            }
        }
        
        // Zoom the selected child
        lv_obj_set_size(closest_child, btn_width * BTN_ZOOM_FACTOR, btn_height * BTN_ZOOM_FACTOR);

        // Set the special position for the center button
        lv_obj_t * img = lv_obj_get_child(closest_child, 0);
        if(img) {
            app_extra_img_set_zoom(img, IMG_BASE_ZOOM * IMG_ZOOM_FACTOR);
            lv_img_set_angle(img, IMG_SCROLL_CENTER_ANGLE);
            lv_obj_refr_size(img);

            lv_coord_t img_off_x = axis_scroll == UI_AXIS_X ? IMG_ZOOM_OFFSET : 0;
            lv_coord_t img_off_y = axis_scroll == UI_AXIS_Y ? IMG_ZOOM_OFFSET : 0;
            lv_obj_align(img, LV_ALIGN_CENTER, img_off_x, img_off_y);
        }
        
        // Scroll to the view
        lv_obj_scroll_to_view(closest_child, LV_ANIM_ON);
    }

    is_scrolling = false; 
}

/*-------------------------------------------------*/
/* Timer Callbacks                                 */
/*-------------------------------------------------*/

/**
 * @brief Popup timer callback
 * @param timer Timer object
 */
static void pop_up_timer_callback(lv_timer_t * timer)
{
    if(timer->user_data == ui_PanelCanvasPopupCamera) {
        lv_obj_add_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN);
    
        lv_obj_clear_flag(ui_PanelCanvasMaskCamera, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas1X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas2X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas3X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN);
        
        is_sd_card_mounted ? lv_obj_clear_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);

    } else if(timer->user_data == ui_PanelCanvasPopupCameraInterval) {
        lv_obj_add_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_PanelCanvasMaskCameraInterval, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas5mplus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas5mSub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvasInvervalTime, LV_OBJ_FLAG_HIDDEN);

        is_sd_card_mounted ? lv_obj_clear_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);

    } else if(timer->user_data == ui_PanelCanvasPopupVideoMode) {
        lv_obj_add_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_PanelCanvasMaskVideoMode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas1X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas2X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvas3X, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);

        is_sd_card_mounted ? lv_obj_clear_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);
    } else if(timer->user_data == ui_PanelCanvasPopupAICamera) {
        lv_obj_add_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(ui_PanelCanvasMaskAICamera, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN);
        
        // Show AI mode label after popup disappears
        if (ai_mode_label != NULL) {
            lv_obj_clear_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if(lv_popup_timer){
        lv_timer_del(lv_popup_timer);
        lv_popup_timer = NULL;
    }
}

static void take_photos_popup_timer_callback(lv_timer_t * timer)
{
    (void)timer;

    while(true) {
        if (take_photos_popup == NULL) break;
        lv_obj_add_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    while(true) {
        if (lv_take_photos_popup_timer == NULL) break;
        lv_timer_del(lv_take_photos_popup_timer);
        lv_take_photos_popup_timer = NULL;
        break;
    }
}

/**
 * @brief USB disk timer callback
 * @param timer Timer object
 */
static void usb_disk_timer_callback(lv_timer_t * timer)
{
    lv_obj_add_flag(ui_ImageScreenUSBWarning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_HIDDEN);

    if (lv_usb_disk_timer) {
        lv_timer_del(lv_usb_disk_timer);
        lv_usb_disk_timer = NULL;
    }
}

/**
 * @brief Video timer callback
 * @param timer Timer object
 */
static void video_timer_callback(lv_timer_t * timer)
{
    video_recording_seconds++;
    uint32_t hours = video_recording_seconds / 3600;
    uint32_t minutes = (video_recording_seconds % 3600) / 60;
    uint32_t seconds = video_recording_seconds % 60;
    
    if (hours > 0) {
        lv_label_set_text_fmt(ui_LabelRedDotTime, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    } else {
        lv_label_set_text_fmt(ui_LabelRedDotTime, "%02lu:%02lu", minutes, seconds);
    }
}

/**
 * @brief Interval timer callback
 * @param timer Timer object
 */
static void interval_timer_callback(lv_timer_t * timer)
{
    lv_obj_add_flag(ui_PanelInrervalTimePrompt, LV_OBJ_FLAG_HIDDEN);

    if(lv_interval_timer){
        lv_timer_del(lv_interval_timer);
        lv_interval_timer = NULL;
    }
}

/**
 * @brief Additional photo popup callback
 * @param timer Timer object
 */
static void pop_up_additional_photo_callback(lv_timer_t * timer)
{
    if(timer->user_data == ui_PanelCanvasPopupIntervalTimerWarning) {
        lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN);

        app_video_stream_start_interval_photo(interval_time);
    } else if (timer->user_data == ui_PanelCanvasPopupIntervalTimerWarningEnd) {
        lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clear_flag(ui_PanelCanvasMaskCameraInterval, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_LabelCanvas5mplus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_LabelCanvas5mSub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_LabelCanvasInvervalTime, LV_OBJ_FLAG_HIDDEN);

    is_sd_card_mounted ? lv_obj_clear_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);
    
    if(lv_additional_photo_timer){
        lv_timer_del(lv_additional_photo_timer);
        lv_additional_photo_timer = NULL;
    }
}

/*-------------------------------------------------*/
/* UI Creation Functions                           */
/*-------------------------------------------------*/

/**
 * @brief Create scroll container
 */
static void lv_scroll_create(void)
{
    // Create main container
    scroll_cont = lv_obj_create(ui_PanelCanvas);
    lv_coord_t bar_h = UI_SCALE_Y(90);
    lv_obj_set_size(scroll_cont, BSP_LCD_H_RES, bar_h);
    lv_obj_align(scroll_cont, LV_ALIGN_TOP_MID, 0, UI_SCALE_Y(10));
    
    // Set container properties
    lv_obj_set_style_pad_column(scroll_cont, UI_SCALE_X(10), 0);
    lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_radius(scroll_cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scroll_cont, true, 0);
    
    // Configure scrolling behavior
    lv_obj_set_scroll_dir(scroll_cont, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(scroll_cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_OFF);
    
    // Add scroll event handlers
    lv_obj_add_event_cb(scroll_cont, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(scroll_cont, scroll_end_event_cb, LV_EVENT_SCROLL_END, NULL);

    // Set container visual style
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(scroll_cont, LV_OPA_TRANSP, 0);

    info_label = lv_label_create(ui_PanelCanvas);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(info_label, 120, 20);
    lv_label_set_text(info_label, "");  

    const char* btn_texts[] = {
        "CAMERA", "INTERVAL CAM", "VIDEO MODE", "AI DETECT","ALBUM", 
        "USB DISK", "SETTINGS"
    };

    // Define the image source for each button
    const void* img_srcs[] = {
        &ui_img_camera_big_png,
        &ui_img_interval_big_png,
        &ui_img_video_big_png,
        &ui_img_ai_detect_png,
        &ui_img_album_big_png,
        &ui_img_usb_big_png,
        &ui_img_settings_big_png,
    };
    
    // Use a loop to create all buttons
    lv_obj_t *btn = NULL;
    const int btn_count = (int)(sizeof(btn_texts)/sizeof(btn_texts[0]));
    for (int i = 0; i < btn_count; i++) {
        btn = create_img_button(
            scroll_cont,
            img_srcs[i],
            btn_texts[i]
        );
    }

    lv_obj_update_layout(scroll_cont);
    lv_obj_t * sample_btn = lv_obj_get_child(scroll_cont, 0);
    sample_btn = sample_btn ? sample_btn : btn;
    if (sample_btn) {
        btn_width = lv_obj_get_width(sample_btn);
        btn_height = lv_obj_get_height(sample_btn);
    }

    ui_axis_t axis_scroll = ui_menu_detect_scroll_axis(scroll_cont);
    ui_axis_t axis_translate = ui_axis_other(axis_scroll);

    lv_coord_t max_translate_init = ui_obj_size_on_axis(scroll_cont, axis_translate) - (lv_coord_t)((float)ui_btn_size_on_axis(axis_translate) * BTN_ZOOM_FACTOR);
    max_translate_init = max_translate_init > 0 ? max_translate_init : 0;

    lv_coord_t cont_x = 0;
    lv_coord_t cont_y = UI_SCALE_Y(10);
    lv_coord_t shift = ARC_RIGHT_HALF ? -max_translate_init : 0;
    cont_x = axis_translate == UI_AXIS_X ? shift : cont_x;
    cont_y = axis_translate == UI_AXIS_Y ? (cont_y + shift) : cont_y;
    lv_obj_align(scroll_cont, LV_ALIGN_TOP_MID, cont_x, cont_y);

    lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 150, 95);
    lv_obj_set_style_transform_pivot_x(info_label, 60, 0);
    lv_obj_set_style_transform_pivot_y(info_label, 10, 0);
    lv_obj_set_style_transform_angle(info_label, 900, 0);

    lv_coord_t center_pad = (ui_obj_size_on_axis(scroll_cont, axis_scroll) - ui_btn_size_on_axis(axis_scroll)) / 2;
    center_pad = center_pad > 0 ? center_pad : 0;
    switch(axis_scroll)
    {
        case UI_AXIS_X:
            lv_obj_set_style_pad_left(scroll_cont, center_pad, 0);
            lv_obj_set_style_pad_right(scroll_cont, center_pad, 0);
            lv_obj_set_style_pad_top(scroll_cont, 0, 0);
            lv_obj_set_style_pad_bottom(scroll_cont, 0, 0);
            break;
        default:
            lv_obj_set_style_pad_left(scroll_cont, 0, 0);
            lv_obj_set_style_pad_right(scroll_cont, 0, 0);
            lv_obj_set_style_pad_top(scroll_cont, center_pad, 0);
            lv_obj_set_style_pad_bottom(scroll_cont, center_pad, 0);
            break;
    }
    lv_obj_update_layout(scroll_cont);

    {
        if (vkey_cont == NULL) {
            vkey_cont = lv_obj_create(ui_PanelCanvas);
            lv_obj_set_style_bg_opa(vkey_cont, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_opa(vkey_cont, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(vkey_cont, 0, 0);
            lv_obj_set_flex_flow(vkey_cont, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(vkey_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(vkey_cont, UI_SCALE_X(12), 0);
            lv_obj_clear_flag(vkey_cont, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(vkey_cont, LV_OBJ_FLAG_CLICKABLE);
        }

        lv_obj_set_size(vkey_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    }

    // Initialize scroll position
    lv_event_send(scroll_cont, LV_EVENT_SCROLL, NULL);
    lv_obj_scroll_to_view(lv_obj_get_child(scroll_cont, 0), LV_ANIM_OFF);

    const char* initial_text = lv_obj_get_user_data(lv_obj_get_child(scroll_cont, 0));
    if (initial_text) {
        lv_label_set_text(info_label, initial_text);
    }
}

static void ui_extra_vkey_restore_default_layout(void)
{
    if (!ui_PanelCanvas) {
        return;
    }

    if (ui_ImageCanvasSelect) {
        lv_obj_set_style_opa(ui_ImageCanvasSelect, LV_OPA_COVER, 0);
        lv_obj_clear_state(ui_ImageCanvasSelect, LV_STATE_DISABLED);
    }

    if (ui_ImageCanvasSelect && lv_obj_get_parent(ui_ImageCanvasSelect) != ui_PanelCanvas) {
        lv_obj_set_parent(ui_ImageCanvasSelect, ui_PanelCanvas);
        lv_obj_set_align(ui_ImageCanvasSelect, LV_ALIGN_RIGHT_MID);
        lv_obj_set_pos(ui_ImageCanvasSelect, -UI_SCALE_X(20), -100);
    }

    if (ui_ImageCanvasUp && lv_obj_get_parent(ui_ImageCanvasUp) != ui_PanelCanvas) {
        lv_obj_set_parent(ui_ImageCanvasUp, ui_PanelCanvas);
        lv_obj_set_align(ui_ImageCanvasUp, LV_ALIGN_TOP_RIGHT);
        lv_obj_set_pos(ui_ImageCanvasUp, -UI_SCALE_X(20), UI_SCALE_Y(120));
    }

    if (ui_ImageCanvasDown && lv_obj_get_parent(ui_ImageCanvasDown) != ui_PanelCanvas) {
        lv_obj_set_parent(ui_ImageCanvasDown, ui_PanelCanvas);
        lv_obj_set_align(ui_ImageCanvasDown, LV_ALIGN_TOP_RIGHT);
        lv_obj_set_pos(ui_ImageCanvasDown, -UI_SCALE_X(20), UI_SCALE_Y(175));
    }

    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasUp);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasSelect);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasDown);
}

static void ui_extra_vkey_apply_main_layout(void)
{
    if (!ui_PanelCanvas || !scroll_cont || !vkey_cont) {
        return;
    }

    if (ui_ImageCanvasSelect) {
        lv_obj_set_style_opa(ui_ImageCanvasSelect, LV_OPA_COVER, 0);
        lv_obj_clear_state(ui_ImageCanvasSelect, LV_STATE_DISABLED);
    }

    if (ui_ImageCanvasUp && lv_obj_get_parent(ui_ImageCanvasUp) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasUp, vkey_cont);
    }

    if (ui_ImageCanvasSelect && lv_obj_get_parent(ui_ImageCanvasSelect) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasSelect, vkey_cont);
    }

    if (ui_ImageCanvasDown && lv_obj_get_parent(ui_ImageCanvasDown) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasDown, vkey_cont);
    }

    lv_obj_align(vkey_cont, LV_ALIGN_BOTTOM_MID, 0, -UI_SCALE_Y(20));

    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasUp);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasSelect);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasDown);
}

static void ui_extra_vkey_apply_ai_detect_layout(void)
{
    if (!ui_PanelCanvas) {
        return;
    }

    if (vkey_cont == NULL) {
        vkey_cont = lv_obj_create(ui_PanelCanvas);
        lv_obj_set_style_bg_opa(vkey_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(vkey_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(vkey_cont, 0, 0);
        lv_obj_set_flex_flow(vkey_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vkey_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(vkey_cont, UI_SCALE_X(12), 0);
        lv_obj_clear_flag(vkey_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(vkey_cont, LV_OBJ_FLAG_CLICKABLE);
    }

    if (!vkey_cont) {
        return;
    }

    if (ui_ImageCanvasUp && lv_obj_get_parent(ui_ImageCanvasUp) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasUp, vkey_cont);
    }

    if (ui_ImageCanvasSelect && lv_obj_get_parent(ui_ImageCanvasSelect) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasSelect, vkey_cont);
    }

    if (ui_ImageCanvasDown && lv_obj_get_parent(ui_ImageCanvasDown) != vkey_cont) {
        lv_obj_set_parent(ui_ImageCanvasDown, vkey_cont);
    }

    lv_obj_set_size(vkey_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(vkey_cont, LV_ALIGN_BOTTOM_MID, 0, -UI_SCALE_Y(20));

    if (ui_ImageCanvasSelect) {
        lv_obj_set_style_opa(ui_ImageCanvasSelect, LV_OPA_0, 0);
        lv_obj_add_state(ui_ImageCanvasSelect, LV_STATE_DISABLED);
    }

    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasUp);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasSelect);
    ui_extra_vkey_rotate_ccw_90(ui_ImageCanvasDown);
}

/*-------------------------------------------------*/
/* Page Management Functions                       */
/*-------------------------------------------------*/

/**
 * @brief Clear all UI elements from the current page
 */
void ui_extra_clear_page(void)
{
    lv_obj_add_flag(ui_ImageCanvasSelect, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_ImageCanvasMenu, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_LabelCanvas1X, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvas2X, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvas3X, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvasFactor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasMaskCamera, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasMaskAICamera, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasMaskCameraInterval, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvas5mplus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvas5mSub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_LabelCanvasInvervalTime, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasMaskVideoMode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_flag(ui_LabelRedDotTime, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelSettings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelSettingsMenu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scroll_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);

    while(true) {
        if (take_photos_popup == NULL) break;
        lv_obj_add_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);
        break;
    }
    
    // Hide AI mode label if it exists
    if (ai_mode_label != NULL) {
        lv_obj_add_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Clear popup windows
 */
static void ui_extra_clear_popup_window(void)
{
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN) ||
       (take_photos_popup != NULL && !lv_obj_has_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN))) {

            if(lv_popup_timer) {
                lv_timer_ready(lv_popup_timer);
            }

            if(lv_additional_photo_timer) {
                lv_timer_ready(lv_additional_photo_timer);
            }

            if (lv_take_photos_popup_timer) {
                lv_timer_ready(lv_take_photos_popup_timer);
            }

            lv_obj_add_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN);

            while(true) {
                if (take_photos_popup == NULL) break;
                lv_obj_add_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);
                break;
            }
            
            // Make sure AI mode label is visible when in AI detect page and popup is cleared
            if (current_page == UI_PAGE_AI_DETECT && ai_mode_label != NULL) {
                lv_obj_clear_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
            }
    }
}

/**
 * @brief Redirect to main page
 */
static void ui_extra_redirect_to_main_page(void)
{
    current_page = UI_PAGE_MAIN;

    ui_extra_clear_page();

    lv_obj_clear_flag(scroll_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(info_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasSelect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_HIDDEN);

    _ui_screen_change(&ui_ScreenCamera, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_ScreenCamera_screen_init);

    ui_extra_vkey_apply_main_layout();
}

/**
 * @brief Redirect to camera page
 */
static void ui_extra_redirect_to_camera_page(void)
{
    current_page = UI_PAGE_CAMERA;

    ui_extra_set_magnification_factor_runtime(DEFAULT_MAGNIFICATION_FACTOR);

    take_photos_session_count = 0;

    ui_extra_clear_page();

    lv_obj_clear_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN);
    if(!lv_popup_timer){
        lv_popup_timer = lv_timer_create(pop_up_timer_callback, 5000, ui_PanelCanvasPopupCamera);
    }
}

/**
 * @brief Redirect to interval camera page
 */
static void ui_extra_redirect_to_interval_camera_page(void)
{
    current_page = UI_PAGE_INTERVAL_CAM;

    ui_extra_clear_page();

    lv_obj_clear_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN);
    if(!lv_popup_timer){
        lv_popup_timer = lv_timer_create(pop_up_timer_callback, 5000, ui_PanelCanvasPopupCameraInterval);
    }
}

/**
 * @brief Redirect to video mode page
 */
static void ui_extra_redirect_to_video_mode_page(void)
{
    current_page = UI_PAGE_VIDEO_MODE;

    ui_extra_set_magnification_factor_runtime(DEFAULT_MAGNIFICATION_FACTOR);

    ui_extra_clear_page();
    
    lv_obj_clear_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN);
    if(!lv_popup_timer){
        lv_popup_timer = lv_timer_create(pop_up_timer_callback, 5000, ui_PanelCanvasPopupVideoMode);
    }
}

/**
 * @brief Redirect to album page
 */
static void ui_extra_redirect_to_album_page(void)
{
    current_page = UI_PAGE_ALBUM;

    ui_extra_clear_page();
    
    _ui_screen_change(&ui_ScreenAlbum, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_ScreenAlbum_screen_init);

    ui_extra_register_touch_controls_album();

    ui_extra_album_vkey_apply_layout();

    if(is_sd_card_mounted) {
        lv_obj_add_flag(ui_PanelAlbumPopupSDWarning, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_PanelAlbumPopupSDWarning, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Redirect to USB disk page
 */
static void ui_extra_redirect_to_usb_disk_page(void)
{
    current_page = UI_PAGE_USB_DISK;

    ui_extra_clear_page();
    
    _ui_screen_change(&ui_ScreenUSB, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_ScreenUSB_screen_init);

    ui_extra_register_touch_controls_usb();

    if(is_usb_disk_mounted) {
        lv_obj_add_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ImageScreenUSBWarning, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ImageScreenUSBWarning, LV_OBJ_FLAG_HIDDEN);
    }

    if(is_sd_card_mounted) {
        lv_obj_add_flag(ui_ImageUSBNOSDcard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_ImageUSBNOSDcard, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Redirect to settings page
 */
static void ui_extra_redirect_to_settings_page(void)
{
    current_page = UI_PAGE_SETTINGS;

    ui_extra_clear_page();
    
    lv_obj_clear_flag(ui_PanelSettings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_PanelSettingsMenu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasSelect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_HIDDEN);

    ui_extra_vkey_apply_main_layout();

    // Initialize settings items
    settings_items[0] = ui_PanelPanelSettingsGyroscope;
    settings_items[1] = ui_PanelPanelSettingsLanguage;
    settings_items[2] = ui_PanelPanelSettingsRes;
    settings_items[3] = ui_PanelPanelSettingsFlash;
    settings_items[4] = ui_PanelSettingsMenu;
    
    // Initialize the settings display
    init_settings_display();
    
    // Reset the current selected item and focus the first item
    current_settings_item = 0;
    update_settings_focus(current_settings_item);

    is_camera_settings_panel_active = false;
}

/**
 * @brief Redirect to AI detect page
 */
static void ui_extra_redirect_to_ai_detect_page(void)
{
    current_page = UI_PAGE_AI_DETECT;

    ui_extra_clear_page();

    lv_obj_clear_flag(ui_ImageCanvasUp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasDown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImageCanvasSelect, LV_OBJ_FLAG_HIDDEN);

    ui_extra_vkey_apply_ai_detect_layout();

    // Create AI mode label if it doesn't exist
    if (ai_mode_label == NULL) {
        ai_mode_label = lv_label_create(ui_ScreenCamera);
        lv_obj_set_style_text_font(ai_mode_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(ai_mode_label, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_bg_color(ai_mode_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(ai_mode_label, LV_OPA_70, 0);
        lv_obj_set_style_radius(ai_mode_label, 5, 0);
        lv_obj_set_style_pad_all(ai_mode_label, 5, 0);

        lv_obj_align(ai_mode_label, LV_ALIGN_TOP_LEFT, UI_SCALE_X(10), UI_SCALE_Y(10));

        lv_obj_update_layout(ai_mode_label);
        lv_obj_set_style_transform_pivot_x(ai_mode_label, lv_obj_get_width(ai_mode_label) / 2, 0);
        lv_obj_set_style_transform_pivot_y(ai_mode_label, lv_obj_get_height(ai_mode_label) / 2, 0);
        lv_obj_set_style_transform_angle(ai_mode_label, 900, 0);
    }
    
    // Initially hide AI mode label when popup is shown
    lv_obj_add_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
    
    // Update label text (it will be shown when popup disappears)
    ui_extra_update_ai_detect_mode_label();
    
    // Show popup
    lv_obj_clear_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN);
    if(!lv_popup_timer){
        lv_popup_timer = lv_timer_create(pop_up_timer_callback, 5000, ui_PanelCanvasPopupAICamera);
    }
}

/**
 * @brief Update AI detection mode label
 */
static void ui_extra_update_ai_detect_mode_label(void)
{
    if (ai_mode_label == NULL) {
        return;
    }
    
    if (current_ai_detect_mode == AI_DETECT_PEDESTRIAN) {
        lv_label_set_text(ai_mode_label, "Mode: Pedestrian");
    } else {
        lv_label_set_text(ai_mode_label, "Mode: Face");
    }
}

/**
 * @brief Change AI detection mode
 * @param mode New AI detection mode
 */
static void ui_extra_change_ai_detect_mode(ai_detect_mode_t mode)
{
    if (mode >= AI_DETECT_MODE_MAX) {
        mode = 0;
    }
    
    current_ai_detect_mode = mode;
    
    // Update label text
    ui_extra_update_ai_detect_mode_label();
    
    // Ensure label is visible if popup is not visible
    if (ai_mode_label != NULL && lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Log the mode change (mode value is only used for UI display)
    ESP_LOGI(TAG, "Changed AI detection mode display to %s", 
             (current_ai_detect_mode == AI_DETECT_PEDESTRIAN) ? "pedestrian" : "face");
}

/**
 * @brief Toggle focus between YES and NO in picture delete dialog
 */
static void ui_extra_focus_on_picture_delete(void)
{
    if(lv_obj_has_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED)) {
        lv_obj_add_state(ui_ButtonPanelImageScreenAlbumDeleteNO, LV_STATE_FOCUSED);
        lv_obj_clear_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED);
    } else {
        lv_obj_clear_state(ui_ButtonPanelImageScreenAlbumDeleteNO, LV_STATE_FOCUSED);
        lv_obj_add_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED);
    }
}

/*-------------------------------------------------*/
/* Public API Functions                            */
/*-------------------------------------------------*/

/**
 * @brief Navigate to specified page
 * @param page Target page enum value
 */
void ui_extra_goto_page(ui_page_t page)
{
    ui_page_t previous_page = current_page;

    while(true) {
        if (!(previous_page == UI_PAGE_CAMERA)) break;
        if (page == UI_PAGE_CAMERA) break;

        take_photos_session_count = 0;

        while(true) {
            if (lv_take_photos_popup_timer == NULL) break;
            lv_timer_del(lv_take_photos_popup_timer);
            lv_take_photos_popup_timer = NULL;
            break;
        }

        while(true) {
            if (take_photos_popup == NULL) break;
            lv_obj_add_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        break;
    }

    current_page = page;

    if (page != UI_PAGE_MAIN) {
        ui_extra_vkey_restore_default_layout();
    }
    
    // Redirect to the page
    switch(page) {
        case UI_PAGE_MAIN:
            ui_extra_redirect_to_main_page();
            break;
        case UI_PAGE_CAMERA:
            ui_extra_redirect_to_camera_page();
            break;
        case UI_PAGE_INTERVAL_CAM:
            ui_extra_redirect_to_interval_camera_page();
            break;
        case UI_PAGE_VIDEO_MODE:
            ui_extra_redirect_to_video_mode_page();
            break;
        case UI_PAGE_ALBUM:
            app_album_refresh();
            ui_extra_redirect_to_album_page();
            break;
        case UI_PAGE_USB_DISK:
            ui_extra_redirect_to_usb_disk_page();
            break;
        case UI_PAGE_SETTINGS:
            ui_extra_redirect_to_settings_page();
            break;
        case UI_PAGE_AI_DETECT:
            ui_extra_redirect_to_ai_detect_page();
            break;
        default:
            ui_extra_redirect_to_main_page();
            break;
    }   
}

/**
 * @brief Get current page
 * @return Current page enum value
 */
ui_page_t ui_extra_get_current_page(void)
{
    return current_page;
}

/**
 * @brief Get chosen page from selected button
 * @return Chosen page enum value
 */
ui_page_t ui_extra_get_choosed_page(void)
{
    const char * user_data = lv_obj_get_user_data(selected_btn);

    for (int i = 0; page_map[i].name != NULL; i++) {
        if (strcmp(user_data, page_map[i].name) == 0) {
            return page_map[i].page;
        }
    }

    return UI_PAGE_MAIN;
}

bool ui_extra_is_ui_init(void)
{
    return is_ui_init;
}

/**
 * @brief Get current settings information
 * @return Pointer to settings information structure
 */
settings_info_t* ui_extra_get_settings(void)
{
    return &current_settings;
}

/**
 * @brief Set magnification factor
 * @param factor Magnification factor value
 */
void app_extra_set_magnification_factor(uint16_t factor)
{
    if(factor > MAX_MAGNIFICATION_FACTOR) {
        factor = MAX_MAGNIFICATION_FACTOR;
    } else if(factor < MIN_MAGNIFICATION_FACTOR) {
        factor = MIN_MAGNIFICATION_FACTOR;
    }

    magnification_factor = factor;
    
    lv_label_set_text_fmt(ui_LabelCanvasFactor, "%dX", magnification_factor);
    ui_extra_obj_rotate_cw_90(ui_LabelCanvasFactor);

    save_current_settings();
}

/**
 * @brief Get magnification factor
 * @return Current magnification factor value
 */
uint16_t app_extra_get_magnification_factor(void)
{
    return magnification_factor;
}   

/**
 * @brief Set saved photo count
 * @param count Number of saved photos
 */
void app_extra_set_saved_photo_count(uint16_t count)
{
    saved_photo_count = count;
    app_storage_save_photo_count(saved_photo_count);
}

/**
 * @brief Get saved photo count
 * @return Current saved photo count
 */
uint16_t app_extra_get_saved_photo_count(void)
{
    return saved_photo_count;
}

/**
 * @brief Set interval time
 * @param time Interval time in minutes
 */
void app_extra_set_interval_time(uint16_t time)
{
    // Limit time range
    if(time > MAX_INTERVAL_TIME) {
        time = MIN_INTERVAL_TIME;
    } else if(time < MIN_INTERVAL_TIME) {
        time = MAX_INTERVAL_TIME;
    }

    interval_time = time;

    lv_label_set_text_fmt(ui_LabelCanvasInvervalTime, "%dmin", interval_time);
    lv_label_set_text_fmt(ui_LabelPanelInrervalTimePrompt, "%d", interval_time);

    save_current_settings();
}

/**
 * @brief Get interval time
 * @return Current interval time in minutes
 */
uint16_t app_extra_get_interval_time(void)
{
    return interval_time;
}

/**
 * @brief Set SD card mount status
 * @param mounted Whether SD card is mounted
 */
void ui_extra_set_sd_card_mounted(bool mounted)
{
    is_sd_card_mounted = mounted;
    
    if(current_page != UI_PAGE_CAMERA && current_page != UI_PAGE_INTERVAL_CAM && current_page != UI_PAGE_VIDEO_MODE && current_page != UI_PAGE_ALBUM) {
        return;
    }

    if(is_sd_card_mounted) {
        lv_obj_add_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_ImageCanvasSDcard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ImageCanvasNOSDcard, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Get SD card mount status
 * @return Whether SD card is mounted
 */
bool ui_extra_get_sd_card_mounted(void)
{
    return is_sd_card_mounted;
}

/**
 * @brief Set USB disk mount status
 * @param mounted Whether USB disk is mounted
 */
void ui_extra_set_usb_disk_mounted(bool mounted)
{
    is_usb_disk_mounted = mounted;
    
    ui_extra_goto_page(UI_PAGE_USB_DISK);
}

/**
 * @brief Get USB disk mount status
 * @return Whether USB disk is mounted
 */
bool ui_extra_get_usb_disk_mounted(void)
{
    return is_usb_disk_mounted;
}

/**
 * @brief Get current AI detection mode
 * 
 * @details Returns the current AI detection mode that will be displayed
 *          in the UI. This can be either pedestrian detection (AI_DETECT_PEDESTRIAN)
 *          or face detection (AI_DETECT_FACE).
 * 
 * @return Current AI detection mode value
 */
ai_detect_mode_t ui_extra_get_ai_detect_mode(void)
{
    return current_ai_detect_mode;
}

/**
 * @brief Set AI detection mode
 * @param mode New AI detection mode
 */
void ui_extra_set_ai_detect_mode(ai_detect_mode_t mode)
{
    ui_extra_change_ai_detect_mode(mode);
}

/**
 * @brief Get gyroscope enabled status
 * @return True if gyroscope is enabled, false otherwise
 */
bool ui_extra_get_gyroscope_enabled(void)
{
    return (strcmp(current_settings.gyroscope, "On") == 0);
}

/**
 * @brief Get popup window visible status
 * @return Whether popup window is visible
 */
bool ui_extra_get_popup_window_visible(void)
{
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN) ||
       (take_photos_popup != NULL && !lv_obj_has_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN))) {
        
        return true;
    }

    return false;
}

/**
 * @brief Start interval timer
 */
void ui_extra_start_interval_timer(void)
{
    ui_extra_clear_page();
    ui_extra_obj_rotate_cw_90(ui_PanelCanvasPopupIntervalTimerWarning);
    lv_label_set_text_fmt(ui_LabelPanelCanvasPopupIntervalTimer, "Starting %d min", interval_time);
    lv_obj_clear_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Show interval timer warning popup
 */
void ui_extra_popup_interval_timer_warning(void)
{
    if(!(current_page == UI_PAGE_INTERVAL_CAM)) {
        return;
    }
    app_storage_get_photo_count(&saved_photo_count);
    ui_extra_clear_page();
    ui_extra_obj_rotate_cw_90(ui_PanelCanvasPopupIntervalTimerWarningEnd);
    lv_label_set_text_fmt(ui_LabelPanelCanvasPopupIntervalTimerEnd, "Ended %d min", interval_time);
    lv_label_set_text_fmt(ui_LabelPanelCanvasPopupCameraIntervalTimerWarningEnd, "%d photos saved to \n       SD Card", saved_photo_count);
    lv_obj_clear_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN);

    if(!lv_additional_photo_timer){
        lv_additional_photo_timer = lv_timer_create(pop_up_additional_photo_callback, 5000, ui_PanelCanvasPopupIntervalTimerWarningEnd);
    }
}

void ui_extra_popup_take_photos_count(void)
{
    while(true) {
        if (!(current_page == UI_PAGE_CAMERA)) break;
        if (ui_PanelCanvas == NULL) break;

        while(true) {
            if (take_photos_popup != NULL) break;

            take_photos_popup = lv_obj_create(ui_PanelCanvas);
            lv_obj_set_size(take_photos_popup, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_center(take_photos_popup);

            lv_obj_set_style_bg_color(take_photos_popup, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(take_photos_popup, LV_OPA_70, 0);
            lv_obj_set_style_border_opa(take_photos_popup, LV_OPA_TRANSP, 0);
            lv_obj_set_style_radius(take_photos_popup, UI_SCALE_X(12), 0);
            lv_obj_set_style_pad_all(take_photos_popup, UI_SCALE_X(14), 0);

            lv_obj_clear_flag(take_photos_popup, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(take_photos_popup, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);

            take_photos_popup_label = lv_label_create(take_photos_popup);
            lv_label_set_text(take_photos_popup_label, "");
            lv_obj_set_style_text_color(take_photos_popup_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(take_photos_popup_label, &ui_font_FontKoHoMiniBold24, 0);
            lv_obj_center(take_photos_popup_label);
            break;
        }

        while(true) {
            if (take_photos_popup_label == NULL) break;
            take_photos_session_count++;
            lv_label_set_text_fmt(take_photos_popup_label, "take photos %lu", (unsigned long)take_photos_session_count);
            break;
        }

        lv_obj_center(take_photos_popup);
        ui_extra_obj_rotate_cw_90(take_photos_popup);
        lv_obj_clear_flag(take_photos_popup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(take_photos_popup);

        while(true) {
            if (lv_take_photos_popup_timer == NULL) break;
            lv_timer_del(lv_take_photos_popup_timer);
            lv_take_photos_popup_timer = NULL;
            break;
        }

        lv_take_photos_popup_timer = lv_timer_create(take_photos_popup_timer_callback, 1000, NULL);
        break;
    }
}

/**
 * @brief Show SD space warning popup
 */
void ui_extra_popup_camera_sd_space_warning(void)
{
    ui_extra_clear_page();
    lv_obj_clear_flag(ui_PanelCanvasPopupSDSpaceWarning, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Hide SD space warning popup
 */
void ui_extra_popup_camera_sd_space_warning_end(void)
{
    if(!lv_obj_has_flag(ui_PanelCanvasPopupSDSpaceWarning, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(ui_PanelCanvasPopupSDSpaceWarning, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Handle USB disk page
 * @return Whether USB disk page is handled
 */
bool ui_extra_handle_usb_disk_page(void)
{
    if(ui_extra_get_current_page() == UI_PAGE_USB_DISK) {
        if(!lv_obj_has_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_HIDDEN)) {
            ui_extra_goto_page(UI_PAGE_MAIN);
        } else if (!lv_obj_has_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(ui_ImageScreenUSBWarning, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ImageScreenUSB, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ImageScreenUSBSuccess, LV_OBJ_FLAG_HIDDEN);

            if (!lv_usb_disk_timer) {
                lv_usb_disk_timer = lv_timer_create(usb_disk_timer_callback, 5000, NULL);
            }
        }
        return true;  
    }
    return false;
}

/**
 * @brief Show picture delete warning popup
 */
void ui_extra_popup_picture_delete_warning(void)
{
    ui_extra_obj_rotate_cw_90(ui_PanelImageScreenAlbumDelete);
    lv_obj_clear_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN);
    ui_extra_focus_on_picture_delete();
}

/**
 * @brief Show picture delete success popup
 */
void ui_extra_popup_picture_delete_success(void)
{
    lv_obj_add_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN);
}

/*-------------------------------------------------*/
/* Button Event Handlers                           */
/*-------------------------------------------------*/

/**
 * @brief Up button handler
 */
void ui_extra_btn_up(void)
{
    // Check if there are any popup windows that need to be cleared
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
        
        return;
    }

    switch(current_page) {
        case UI_PAGE_MAIN:
        {
            ui_axis_t axis_scroll = ui_menu_detect_scroll_axis(scroll_cont);
            lv_coord_t step = ui_scale_by_axis(axis_scroll, 40);
            lv_coord_t dx = axis_scroll == UI_AXIS_X ? step : 0;
            lv_coord_t dy = axis_scroll == UI_AXIS_Y ? step : 0;
            lv_obj_scroll_by(scroll_cont, dx, dy, LV_ANIM_ON);
            lv_event_send(scroll_cont, LV_EVENT_SCROLL, NULL);
            lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);
            break;
        }
            
        case UI_PAGE_SETTINGS:
            if (is_camera_settings_panel_active) {
                if(current_camera_settings_item < 4) {
                    lv_obj_t *slider = NULL;
                    uint32_t *value_ptr = NULL;

                    switch(current_camera_settings_item) {
                        case 0:
                            slider = ui_SliderPanelPanelSettingsContrast;
                            value_ptr = &contrast_percent;
                            break;
                        case 1:
                            slider = ui_SliderPanelPanelSettingsSaturation;
                            value_ptr = &saturation_percent;
                            break;
                        case 2:
                            slider = ui_SliderPanelPanelSettingsBrightness;
                            value_ptr = &brightness_percent;
                            break;
                        case 3:
                            slider = ui_SliderPanelPanelSettingsHue;
                            value_ptr = &hue_percent;
                            break;
                    }

                    if(slider && value_ptr) {
                        if(*value_ptr >= 5) {
                            *value_ptr -= 5;
                        } else {
                            *value_ptr = 0;
                        }

                        lv_slider_set_value(slider, *value_ptr, LV_ANIM_ON);

                        switch(current_camera_settings_item) {
                            case 0: app_isp_set_contrast(*value_ptr); break;
                            case 1: app_isp_set_saturation(*value_ptr); break;
                            case 2: app_isp_set_brightness(*value_ptr); break;
                            case 3: app_isp_set_hue(*value_ptr); break;
                        }

                        save_camera_settings();
                    }
                }
            } else {
                // main settings panel is active
                if(current_settings_item == 3 && settings_items[current_settings_item] == ui_PanelPanelSettingsFlash) {
                    // if current selected item is flash setting item, switch to camera settings panel when up button is pressed
                    switch_to_camera_settings_panel();
                } else if(current_settings_item < 4) {
                    update_settings_focus(current_settings_item + 1);
                }
            }
            break;
            
        case UI_PAGE_CAMERA:
        case UI_PAGE_VIDEO_MODE:
            app_extra_set_magnification_factor(2);
            break;
            
        case UI_PAGE_INTERVAL_CAM:
            app_extra_set_interval_time(interval_time + INTERVAL_TIME_STEP);

            if(lv_obj_has_flag(ui_PanelInrervalTimePrompt, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ui_PanelInrervalTimePrompt, LV_OBJ_FLAG_HIDDEN);
                if(!lv_interval_timer) {
                    lv_interval_timer = lv_timer_create(interval_timer_callback, 3000, NULL);
                }
            }

            if (lv_interval_timer) {
                lv_timer_reset(lv_interval_timer);
            }
            break;

        case UI_PAGE_ALBUM:
            if(!lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN)) {
                ui_extra_focus_on_picture_delete();
            }
            break;
            
        case UI_PAGE_AI_DETECT:
            // Switch to previous AI detection mode (cycle back to the last if at first)
            if (current_ai_detect_mode == AI_DETECT_PEDESTRIAN) {
                ui_extra_change_ai_detect_mode(AI_DETECT_FACE);
            } else {
                ui_extra_change_ai_detect_mode(AI_DETECT_PEDESTRIAN);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Down button handler
 */
void ui_extra_btn_down(void)
{
    // Check if there are any popup windows that need to be cleared
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
        
        return;
    }

    switch(current_page) {
        case UI_PAGE_MAIN:
        {
            ui_axis_t axis_scroll = ui_menu_detect_scroll_axis(scroll_cont);
            lv_coord_t step = ui_scale_by_axis(axis_scroll, 40);
            lv_coord_t dx = axis_scroll == UI_AXIS_X ? -step : 0;
            lv_coord_t dy = axis_scroll == UI_AXIS_Y ? -step : 0;
            lv_obj_scroll_by(scroll_cont, dx, dy, LV_ANIM_ON);
            lv_event_send(scroll_cont, LV_EVENT_SCROLL, NULL);
            lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN);
            break;
        }
            
        case UI_PAGE_SETTINGS:
            if (is_camera_settings_panel_active) {
                if(current_camera_settings_item < 4) {
                    lv_obj_t *slider = NULL;
                    uint32_t *value_ptr = NULL;

                    switch(current_camera_settings_item) {
                        case 0:
                            slider = ui_SliderPanelPanelSettingsContrast;
                            value_ptr = &contrast_percent;
                            break;
                        case 1:
                            slider = ui_SliderPanelPanelSettingsSaturation;
                            value_ptr = &saturation_percent;
                            break;
                        case 2:
                            slider = ui_SliderPanelPanelSettingsBrightness;
                            value_ptr = &brightness_percent;
                            break;
                        case 3:
                            slider = ui_SliderPanelPanelSettingsHue;
                            value_ptr = &hue_percent;
                            break;
                    }

                    if(slider && value_ptr) {
                        if(*value_ptr <= 95) {
                            *value_ptr += 5;
                        } else {
                            *value_ptr = 100;
                        }

                        lv_slider_set_value(slider, *value_ptr, LV_ANIM_ON);

                        switch(current_camera_settings_item) {
                            case 0: app_isp_set_contrast(*value_ptr); break;
                            case 1: app_isp_set_saturation(*value_ptr); break;
                            case 2: app_isp_set_brightness(*value_ptr); break;
                            case 3: app_isp_set_hue(*value_ptr); break;
                        }

                        save_camera_settings();
                    }
                }
            } else {
                // main settings panel is active
                if(current_settings_item > 0) {
                    update_settings_focus(current_settings_item - 1);
                }
            }
            break;
            
        case UI_PAGE_CAMERA:
        case UI_PAGE_VIDEO_MODE:
            app_extra_set_magnification_factor(3);
            break;
            
        case UI_PAGE_INTERVAL_CAM:
            app_extra_set_interval_time(interval_time - INTERVAL_TIME_STEP);
            
            if(lv_obj_has_flag(ui_PanelInrervalTimePrompt, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ui_PanelInrervalTimePrompt, LV_OBJ_FLAG_HIDDEN);
                if(!lv_interval_timer) {
                    lv_interval_timer = lv_timer_create(interval_timer_callback, 3000, NULL);
                }
            }

            if(lv_interval_timer) {
                lv_timer_reset(lv_interval_timer);
            }
            break;

        case UI_PAGE_ALBUM:
            if(!lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN)) {
                ui_extra_focus_on_picture_delete();
            }
            break;
            
        case UI_PAGE_AI_DETECT:
            // Switch to next AI detection mode (cycle to the first if at last)
            if (current_ai_detect_mode == AI_DETECT_FACE) {
                ui_extra_change_ai_detect_mode(AI_DETECT_PEDESTRIAN);
            } else {
                ui_extra_change_ai_detect_mode(AI_DETECT_FACE);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Right button handler
 */
void ui_extra_btn_right(void)
{
    switch(current_page) {
        case UI_PAGE_SETTINGS:
            if (is_camera_settings_panel_active) {
                if(current_camera_settings_item < 4) {
                    update_camera_settings_focus(current_camera_settings_item + 1);
                }
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Left button handler
 */
void ui_extra_btn_left(void)
{
    switch(current_page) {
        case UI_PAGE_SETTINGS:
            if (is_camera_settings_panel_active) {
                if(current_camera_settings_item == 0) {
                    switch_to_main_settings_panel();
                    update_settings_focus(3);
                } else if(current_camera_settings_item > 0) {
                    update_camera_settings_focus(current_camera_settings_item - 1);
                }
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Menu button handler
 */
void ui_extra_btn_menu(void)
{
    if(!lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    // Check if there are any popup windows that need to be cleared
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupAICamera, LV_OBJ_FLAG_HIDDEN)) {
        
        ui_extra_clear_popup_window();
        return;
    }

    // Ignore selection during scroll
    if (is_scrolling && current_page == UI_PAGE_MAIN) {
        ESP_LOGI(TAG, "Ignoring selection during scroll");
        return;
    }

    // Perform different actions based on current page
    switch(current_page) {
        case UI_PAGE_MAIN:
            ui_extra_goto_page(ui_extra_get_choosed_page());
            break;
            
        case UI_PAGE_SETTINGS:
            if(is_camera_settings_panel_active && current_camera_settings_item == 4 && camera_settings_items[current_camera_settings_item] == ui_PanelSettingsMenu) {
                // If current setting item is menu item, return to main page
                ui_extra_goto_page(UI_PAGE_MAIN);

                // Apply gyroscope setting
                app_storage_save_gyroscope_setting(strcmp(current_settings.gyroscope, "On") == 0);

                if(strcmp(current_settings.flash, "On") == 0) {
                    app_video_stream_set_flash_light(true);
                } else {
                    app_video_stream_set_flash_light(false);
                }

                app_album_enable_coco_od(strcmp(current_settings.od, "On") == 0);

                lv_obj_add_flag(ui_PanelCameraSettings, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_PanelSettings, LV_OBJ_FLAG_HIDDEN);

                app_video_stream_set_photo_resolution_by_string(current_settings.resolution);
            } else {
                // Otherwise, cycle through options
                setting_options_t* opt = &settings_options[current_settings_item];
                opt->current_option = (opt->current_option + 1) % opt->option_count;
                update_setting_display(current_settings_item);
            }
            break;
            
        case UI_PAGE_ALBUM:
            if(!lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN)) {
                if(lv_obj_has_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED)) {
                    app_album_delete_current_image();
                    ui_extra_popup_picture_delete_success();
                } else {
                    ui_extra_popup_picture_delete_success();
                }
            } else {
                ui_extra_goto_page(UI_PAGE_MAIN);
            }
            break;
        case UI_PAGE_VIDEO_MODE:
            if(lv_obj_has_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN)) {
                ui_extra_goto_page(UI_PAGE_MAIN); 
            } else {
                app_video_stream_stop_take_video();

                lv_obj_add_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_LabelRedDotTime, LV_OBJ_FLAG_HIDDEN);
                
                // Stop the video recording and pause the timer
                is_video_recording = false;
                if (lv_video_timer) {
                    lv_timer_pause(lv_video_timer);
                }
            }
            break;    
        
        default:
            // For other pages, return to main page
            ui_extra_goto_page(UI_PAGE_MAIN);
            ui_extra_popup_camera_sd_space_warning_end();
            if(lv_interval_timer) {
                lv_timer_ready(lv_interval_timer);
            }
            
            // Hide AI mode label when leaving AI detect page
            if (current_page == UI_PAGE_AI_DETECT && ai_mode_label != NULL) {
                lv_obj_add_flag(ai_mode_label, LV_OBJ_FLAG_HIDDEN);
            }
            break;
    }
}

/**
 * @brief Encoder button handler
 */
void ui_extra_btn_encoder(void)
{
    if(!lv_obj_has_flag(ui_PanelCanvasPopupCamera, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupCameraInterval, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupVideoMode, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) ||
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN)) {

        ui_extra_clear_popup_window();
        return;
    }

    if(!lv_obj_has_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN)) {
        switch(current_page) {
            case UI_PAGE_CAMERA:
                ui_extra_goto_page(UI_PAGE_CAMERA);
                break;
            case UI_PAGE_INTERVAL_CAM:
                ui_extra_goto_page(UI_PAGE_INTERVAL_CAM);
                break;
            case UI_PAGE_VIDEO_MODE:
                ui_extra_goto_page(UI_PAGE_VIDEO_MODE);
                break;
            default:
                break;
        }
       
        ui_extra_clear_popup_window();
        return;
    }

    if(!lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarning, LV_OBJ_FLAG_HIDDEN) || 
       !lv_obj_has_flag(ui_PanelCanvasPopupIntervalTimerWarningEnd, LV_OBJ_FLAG_HIDDEN)) {
        
        ui_extra_goto_page(UI_PAGE_INTERVAL_CAM);
        ui_extra_clear_popup_window();
        return;
    }

    if(!is_sd_card_mounted) {
        switch(current_page) {
            case UI_PAGE_CAMERA:
            case UI_PAGE_INTERVAL_CAM:
            case UI_PAGE_VIDEO_MODE:
                ui_extra_clear_page();
                lv_obj_clear_flag(ui_PanelCanvasPopupSDWarning, LV_OBJ_FLAG_HIDDEN);
                break;
            case UI_PAGE_MAIN:
                ui_extra_btn_menu();
                break;
            default:
                break;
        }
        return;
    }

    // Ignore selection during scroll
    if (is_scrolling && current_page == UI_PAGE_MAIN) {
        ESP_LOGI(TAG, "Ignoring selection during scroll");
        return;
    }

    switch(current_page) {
        case UI_PAGE_MAIN:
            ui_extra_btn_menu();
            break;
        case UI_PAGE_SETTINGS:
            ui_extra_btn_menu();
            break;
        case UI_PAGE_INTERVAL_CAM:
            if(lv_interval_timer) {
                lv_timer_ready(lv_interval_timer);
                vTaskDelay(300 / portTICK_PERIOD_MS);
            }
            
            ui_extra_start_interval_timer();
            
            if(!lv_additional_photo_timer){
                lv_additional_photo_timer = lv_timer_create(pop_up_additional_photo_callback, 7000, ui_PanelCanvasPopupIntervalTimerWarning);
            }
            break;
        case UI_PAGE_CAMERA:
            // Check if we can store a new image
            if (!app_album_can_store_new_image()) {
                // Show warning to user that storage is full or low
                ESP_LOGE(TAG, "Cannot store more images");
                ui_extra_popup_camera_sd_space_warning();
                return;
            } else {
                ui_extra_popup_camera_sd_space_warning_end();
            }

            app_video_stream_take_photo();
            break;
        case UI_PAGE_ALBUM:
            if(lv_obj_has_flag(ui_PanelImageScreenAlbumDelete, LV_OBJ_FLAG_HIDDEN)) {
                ui_extra_popup_picture_delete_warning();
                ui_extra_focus_on_picture_delete();
            } else {
                if(lv_obj_has_state(ui_ButtonPanelImageScreenAlbumDeleteYES, LV_STATE_FOCUSED)) {
                    app_album_delete_current_image();
                    ui_extra_popup_picture_delete_success();
                } else {
                    ui_extra_popup_picture_delete_success();
                }
            }
            break;
        case UI_PAGE_VIDEO_MODE:
            if(lv_obj_has_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN)) {

                // Check if we can store a new image
                if (!app_video_stream_can_store_new_mp4(100)) {
                    // Show warning to user that storage is full or low
                    ui_extra_popup_camera_sd_space_warning();
                    ESP_LOGE(TAG, "Cannot store more mp4 videos");
                    return;
                } else {
                    ui_extra_popup_camera_sd_space_warning_end();
                }

                app_video_stream_take_video();

                lv_obj_clear_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_LabelRedDotTime, LV_OBJ_FLAG_HIDDEN);
                
                // Reset the video recording seconds and start the timer
                video_recording_seconds = 0;
                lv_label_set_text(ui_LabelRedDotTime, "00:00");
                is_video_recording = true;
                
                if (!lv_video_timer) {
                    lv_video_timer = lv_timer_create(video_timer_callback, 1000, NULL);
                } else {
                    lv_timer_resume(lv_video_timer);
                }
            } else {
                app_video_stream_stop_take_video();

                lv_obj_add_flag(ui_ImageRedDot, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_LabelRedDotTime, LV_OBJ_FLAG_HIDDEN);
                
                // Stop the video recording and pause the timer
                is_video_recording = false;
                if (lv_video_timer) {
                    lv_timer_pause(lv_video_timer);
                }
            }
            break;

        default:
            break;
    }
}

/**
 * @brief Initialize the UI extra module
 */
void ui_extra_init(void)
{
    ui_init();

    init_settings_options();

    lv_scroll_create();

    ui_extra_register_touch_controls_base();

    // reset flag
    is_camera_settings_panel_active = false;

    // Initialize AI detection mode
    current_ai_detect_mode = AI_DETECT_PEDESTRIAN;
    ai_mode_label = NULL;

    // load settings
    settings_info_t settings;
    uint16_t loaded_interval_time;
    uint16_t loaded_magnification;
    uint32_t loaded_contrast;
    uint32_t loaded_saturation;
    uint32_t loaded_brightness;
    uint32_t loaded_hue;
    
    // Set default values
    settings.gyroscope = gyroscope_options[0];     // Set to "Off"
    settings.od = od_options[1];           // Set to "On"
    settings.resolution = resolution_options[3];  // Set to "960P"
    settings.flash = flash_options[0];     // Set to "On"

    loaded_interval_time = DEFAULT_INTERVAL_TIME;
    loaded_magnification = DEFAULT_MAGNIFICATION_FACTOR;

    loaded_contrast = DEFAULT_CONTRAST_PERCENT;
    loaded_saturation = DEFAULT_SATURATION_PERCENT;
    loaded_brightness = DEFAULT_BRIGHTNESS_PERCENT;
    loaded_hue = DEFAULT_HUE_PERCENT;
    
    // Load settings from NVS
    esp_err_t err = app_storage_load_settings(&settings, &loaded_interval_time, &loaded_magnification);
    ESP_LOGD(TAG, "loaded_interval_time: %d, loaded_magnification: %d", loaded_interval_time, loaded_magnification);
    if (err == ESP_OK) {
        // Apply loaded settings
        // Update gyroscope settings
        for (int i = 0; i < settings_options[0].option_count; i++) {
            if (strcmp(settings.gyroscope, settings_options[0].options[i]) == 0) {
                settings_options[0].current_option = i;
                break;
            }
        }
        
        // Update od settings
        for (int i = 0; i < settings_options[1].option_count; i++) {
            if (strcmp(settings.od, settings_options[1].options[i]) == 0) {
                settings_options[1].current_option = i;
                break;
            }
        }
        
        // Update resolution settings
        for (int i = 0; i < settings_options[2].option_count; i++) {
            if (strcmp(settings.resolution, settings_options[2].options[i]) == 0) {
                settings_options[2].current_option = i;
                break;
            }
        }
        
        // Update flash settings
        for (int i = 0; i < settings_options[3].option_count; i++) {
            if (strcmp(settings.flash, settings_options[3].options[i]) == 0) {
                settings_options[3].current_option = i;
                break;
            }
        }
        
        // Update current settings
        current_settings.gyroscope = settings.gyroscope;
        current_settings.od = settings.od;
        current_settings.resolution = settings.resolution;
        current_settings.flash = settings.flash;

        if(strcmp(current_settings.flash, "On") == 0) {
            app_video_stream_set_flash_light(true);
        } else {
            app_video_stream_set_flash_light(false);
        }

        app_album_enable_coco_od(strcmp(current_settings.od, "On") == 0);

        app_video_stream_set_photo_resolution_by_string(current_settings.resolution);
        ESP_LOGI(TAG, "Power-on resolution: %s", current_settings.resolution);
        
        // Update interval time and magnification
        interval_time = loaded_interval_time;
        magnification_factor = loaded_magnification;
        
        is_video_recording = false;
        video_recording_seconds = 0;

        // Update the display
        lv_label_set_text_fmt(ui_LabelCanvasFactor, "%dX", magnification_factor);
        lv_label_set_text_fmt(ui_LabelCanvasInvervalTime, "%dmin", interval_time);

        // Update display
        init_settings_display();
    } else {
        // First time boot or NVS error, use default values
        ESP_LOGW(TAG, "Failed to load settings from NVS (%s), using default values", esp_err_to_name(err));
        
        // Update current settings with default values
        current_settings.gyroscope = gyroscope_options[0];     // Set to "Off"
        current_settings.od = od_options[1];           // Set to "On"
        current_settings.resolution = resolution_options[3];  // Set to "960P"
        current_settings.flash = flash_options[1];     // Set to "On"

        if(strcmp(current_settings.flash, "On") == 0) {
            app_video_stream_set_flash_light(true);
        } else {
            app_video_stream_set_flash_light(false);
        }

        app_album_enable_coco_od(strcmp(current_settings.od, "On") == 0);

        app_video_stream_set_photo_resolution_by_string(current_settings.resolution);
        ESP_LOGI(TAG, "Power-on resolution: %s", current_settings.resolution);
        
        // Use default interval time and magnification
        interval_time = loaded_interval_time;
        magnification_factor = loaded_magnification;
        
        is_video_recording = false;
        video_recording_seconds = 0;

        // Update the display
        lv_label_set_text_fmt(ui_LabelCanvasFactor, "%dX", magnification_factor);
        lv_label_set_text_fmt(ui_LabelCanvasInvervalTime, "%dmin", interval_time);

        // Update display
        init_settings_display();
        
        // Save default settings to NVS for next boot
        save_current_settings();
    }

    // Load camera settings
    err = app_storage_load_camera_settings(&loaded_contrast, &loaded_saturation, 
                                          &loaded_brightness, &loaded_hue);
    if (err == ESP_OK) {
        // Update camera settings
        contrast_percent = loaded_contrast;
        saturation_percent = loaded_saturation;
        brightness_percent = loaded_brightness;
        hue_percent = loaded_hue;
    }
    init_camera_settings_display();

    // Redirect to the main page
    ui_extra_goto_page(UI_PAGE_MAIN);

    is_ui_init = true;
}
