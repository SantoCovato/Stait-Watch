/**
 * @file ui.c
 * @brief UI Smartwatch Completa a 5 Assi con Centro Notifiche Scorrevole e Riquadri Infiniti
 */

/*********************
 *      INCLUDES
 *********************/
#include "ui.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <time.h>
#endif

/**********************
 *      DEFINES
 **********************/
#define WHITE_COLOR   0xFFFFFF
#define GRAY_TEXT     0x999999
#define GRAY_DARK     0x1A1A1A
#define BLUE_NEON     0x00E5FF

/**********************
 *  IMAGE DECLARATION
 **********************/
LV_IMAGE_DECLARE(Staitwatchface);

LV_FONT_DECLARE(Montserrat_custom_12);
LV_FONT_DECLARE(Montserrat_custom_14);
LV_FONT_DECLARE(Montserrat_custom_16);
LV_FONT_DECLARE(Montserrat_custom_48);

/**********************
 *  STATIC VARIABLES
 **********************/
/* Schermate applicazione */
static lv_obj_t * watchface_scr;
static lv_obj_t * settings_scr;
static lv_obj_t * player_scr;
static lv_obj_t * weather_scr;
static lv_obj_t * notify_scr;

/* Widget Watchface */
static lv_obj_t * ui_hour_label;
static lv_obj_t * ui_min_label;
static lv_obj_t * ui_ampm_label;
static lv_obj_t * ui_date_label;
static lv_obj_t * ui_bat_label;
static lv_obj_t * ui_weather_label;
static lv_obj_t * ui_notify_screen_title;
static lv_obj_t * ui_weather_screen_title;
static lv_obj_t * ui_player_screen_title;
static lv_obj_t * ui_settings_screen_title;
static lv_obj_t * ui_call_screen_title;
static lv_obj_t * ui_brightness_label;
static lv_obj_t * ui_timeout_label;
static lv_obj_t * ui_time_format_label;
static lv_obj_t * ui_notifications_label;
static lv_obj_t * ui_unpair_label;
static lv_obj_t * ui_info_name_label;
static lv_obj_t * ui_call_reject_label;
static lv_obj_t * ui_popup_title_label;
static bool ui_english = true;
static bool ui_time_12h = false;

/* Widget Schermata Meteo Dinamici */
static lv_obj_t * ui_scr_w_icon_main;
static lv_obj_t * ui_scr_w_temp_main;
static lv_obj_t * ui_scr_w_cond_label;
static lv_obj_t * ui_scr_w_forecast_labels[3];

/* Widget Impostazioni Dinamici */
static lv_obj_t * ui_settings_brightness_slider;
static lv_obj_t * ui_settings_timeout_dropdown;
static lv_obj_t * ui_settings_time_format_dropdown;
static lv_obj_t * ui_settings_notify_switch;
static lv_obj_t * ui_settings_info_mac_label;
static lv_obj_t * ui_settings_info_bat_label;
static void (*settings_brightness_cb)(uint8_t level);
static void (*settings_timeout_cb)(uint8_t index);
static void (*settings_notifications_cb)(bool on);
static void (*settings_unpair_cb)(void);
static void (*settings_time_format_cb)(bool twelve_hour);

/* Widget Media Player Dinamici */
static lv_obj_t * ui_track_title_label;
static lv_obj_t * ui_track_title_viewport;
static lv_obj_t * ui_track_artist_label;
static lv_obj_t * ui_btn_play_icon;
static lv_obj_t * ui_track_progress_bar;
static lv_timer_t * ui_track_title_timer;
static int32_t ui_track_title_offset;
static int8_t ui_track_title_direction;
static uint8_t ui_track_title_phase;
static uint32_t ui_track_title_phase_tick;

/* Widget Watchface dinamici (aggiornati dal firmware) */
static lv_obj_t * ui_popup_cont;
static lv_obj_t * ui_popup_title;
static lv_obj_t * ui_popup_body;
static lv_timer_t * ui_popup_timer;
static uint32_t ui_popup_shown_tick = 0;

/* Lista notifiche dinamica */
#define MAX_NOTIFIES 5
static lv_obj_t * ui_notify_list;
static lv_obj_t * ui_notify_cards[MAX_NOTIFIES] = { NULL };

/* Schermata Chiamata */
static lv_obj_t * call_scr;
static lv_obj_t * call_name_label;
static lv_obj_t * call_reject_btn;

/* Dati simulati di sistema (aggiornati dal firmware tramite ui_set_*) */
static int8_t mock_battery = 45;
static int8_t mock_temp = 20;
static int8_t mock_wday = 2;
static int ui_current_hour = 10;
static const char * ui_weather_icon_str = "\u2600";
static const char * ui_weather_cond_str  = "SUN";
static const char * mock_mac_address = "AA:BB:CC:DD:EE:FF";
static const char * mock_days_str[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char * mock_days_it_str[] = { "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab" };
static const char * mock_months_str[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char * mock_months_it_str[] = {
    "Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"
};

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void ui_clock_update_cb(lv_timer_t * timer)
{
    (void)timer;
    int hour = 10, min = 34, day = 12, month = 10, wday = 2;

#ifdef _MSC_VER
    SYSTEMTIME st;
    GetLocalTime(&st);
    hour  = st.wHour;
    min   = st.wMinute;
    day   = st.wDay;
    month = st.wMonth;
    wday  = st.wDayOfWeek;
#else
    time_t now = time(NULL);
    struct tm * t = localtime(&now);
    if(t) {
        hour  = t->tm_hour;
        min   = t->tm_min;
        day   = t->tm_mday;
        month = t->tm_mon + 1;
        wday  = t->tm_wday;
    }
#endif

    mock_wday = wday;
    ui_current_hour = hour;
    char buf[32];

    int display_hour = hour;
    if(ui_time_12h) {
        display_hour = hour % 12;
        if(display_hour == 0) display_hour = 12;
        if(ui_ampm_label) {
            lv_label_set_text(ui_ampm_label, hour >= 12 ? "PM" : "AM");
            lv_obj_clear_flag(ui_ampm_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else if(ui_ampm_label) {
        lv_obj_add_flag(ui_ampm_label, LV_OBJ_FLAG_HIDDEN);
    }

    snprintf(buf, sizeof(buf), "%02d", display_hour);
    lv_label_set_text(ui_hour_label, buf);

    snprintf(buf, sizeof(buf), "%02d", min);
    lv_label_set_text(ui_min_label, buf);

    const char * battery_icon = LV_SYMBOL_BATTERY_FULL;
    if(mock_battery < 15) battery_icon = LV_SYMBOL_BATTERY_EMPTY;
    else if(mock_battery < 35) battery_icon = LV_SYMBOL_BATTERY_1;
    else if(mock_battery < 60) battery_icon = LV_SYMBOL_BATTERY_2;
    else if(mock_battery < 85) battery_icon = LV_SYMBOL_BATTERY_3;
    snprintf(buf, sizeof(buf), "%s %d%%", battery_icon, mock_battery);
    lv_label_set_text(ui_bat_label, buf);

    bool is_night = hour >= 19 || hour < 7;
    const char * display_weather_icon = (is_night && strcmp(ui_weather_icon_str, "\u2600") == 0)
                                        ? "\U0001F319" : ui_weather_icon_str;
    snprintf(buf, sizeof(buf), "%s %d°", display_weather_icon, mock_temp);
    lv_label_set_text(ui_weather_label, buf);

    const char * d_str = (wday >= 0 && wday < 7) ? (ui_english ? mock_days_str[wday] : mock_days_it_str[wday]) : "Day";
    const char * m_str = (month >= 1 && month <= 12) ? (ui_english ? mock_months_str[month - 1] : mock_months_it_str[month - 1]) : "Mth";
    snprintf(buf, sizeof(buf), "%s %d %s", d_str, day, m_str);
    lv_label_set_text(ui_date_label, buf);

    snprintf(buf, sizeof(buf), "%d°C", mock_temp);
    lv_label_set_text(ui_scr_w_temp_main, buf);
    lv_label_set_text(ui_scr_w_cond_label, ui_weather_cond_str);

    if(ui_scr_w_icon_main) {
        bool is_night = hour >= 19 || hour < 7;
        const char * display_icon = (is_night && strcmp(ui_weather_icon_str, "\u2600") == 0)
                                    ? "\U0001F319" : ui_weather_icon_str;
        lv_label_set_text(ui_scr_w_icon_main, display_icon);
    }

    for(int i = 0; i < 3; i++) {
        int next_wday = (mock_wday + 1 + i) % 7;
        int next_temp = mock_temp + (i == 0 ? 1 : (i == 1 ? -1 : 2));
        const char * sym = (i == 1) ? "\u2601" : "\u2600";
        
        snprintf(buf, sizeof(buf), "%s       %s       %d°",
                 ui_english ? mock_days_str[next_wday] : mock_days_it_str[next_wday],
                 sym, next_temp);
        lv_label_set_text(ui_scr_w_forecast_labels[i], buf);
    }
}

/**
 * @brief Gestore dei gesti (Swipe multidirezionale a 5 assi)
 */
static void ui_gesture_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);

    if(code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

        /* --- Navigazione dalla Watchface principale --- */
        if(target == watchface_scr) {
            if(dir == LV_DIR_LEFT) {        
                lv_screen_load_anim(settings_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
            }
            else if(dir == LV_DIR_BOTTOM) { 
                lv_screen_load_anim(player_scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
            }
            else if(dir == LV_DIR_TOP) {    
                lv_screen_load_anim(weather_scr, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
            }
            else if(dir == LV_DIR_RIGHT) {  
                lv_screen_load_anim(notify_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            }
        }
        /* --- Ritorno dalle sotto-pagine alla Watchface --- */
        else if(target == settings_scr && dir == LV_DIR_RIGHT) {
            lv_screen_load_anim(watchface_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        }
        else if(target == player_scr && dir == LV_DIR_TOP) {
            lv_screen_load_anim(watchface_scr, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
        }
        else if(target == weather_scr && dir == LV_DIR_BOTTOM) {
            lv_screen_load_anim(watchface_scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, false);
        }
        else if(target == notify_scr && dir == LV_DIR_LEFT) {
            lv_screen_load_anim(watchface_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        }
    }
}

static void ui_settings_brightness_event_cb(lv_event_t * e)
{
    if(settings_brightness_cb) {
        int32_t value = lv_slider_get_value(lv_event_get_target(e));
        uint8_t level = (uint8_t)((value + 12) / 25) + 1;
        if(level < 1) level = 1;
        if(level > 5) level = 5;
        settings_brightness_cb(level);
    }
}

static void ui_settings_timeout_event_cb(lv_event_t * e)
{
    if(settings_timeout_cb) {
        settings_timeout_cb((uint8_t)lv_dropdown_get_selected(lv_event_get_target(e)));
    }
}

static void ui_settings_time_format_event_cb(lv_event_t * e)
{
    bool twelve_hour = lv_dropdown_get_selected(lv_event_get_target(e)) == 1;
    ui_set_time_format(twelve_hour);
    if(settings_time_format_cb) {
        settings_time_format_cb(twelve_hour);
    }
}

static void ui_settings_notifications_event_cb(lv_event_t * e)
{
    if(settings_notifications_cb) {
        settings_notifications_cb(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    }
}

static void ui_settings_unpair_event_cb(lv_event_t * e)
{
    (void)e;
    if(settings_unpair_cb) settings_unpair_cb();
}

static void ui_track_title_reset(void)
{
    if(ui_track_title_label == NULL || ui_track_title_viewport == NULL) return;
    ui_track_title_offset = 0;
    ui_track_title_direction = -1;
    ui_track_title_phase = 0;
    ui_track_title_phase_tick = lv_tick_get();

    int32_t free_width = lv_obj_get_width(ui_track_title_viewport) - lv_obj_get_width(ui_track_title_label);
    lv_obj_set_x(ui_track_title_label, free_width > 0 ? free_width / 2 : 0);
}

static void ui_track_title_update_cb(lv_timer_t * timer)
{
    (void)timer;
    if(ui_track_title_label == NULL || ui_track_title_viewport == NULL) return;

    int32_t overflow = lv_obj_get_width(ui_track_title_label) - lv_obj_get_width(ui_track_title_viewport);
    if(overflow <= 0) {
        ui_track_title_reset();
        return;
    }

    uint32_t now = lv_tick_get();
    if(ui_track_title_phase == 0 || ui_track_title_phase == 2) {
        if(now - ui_track_title_phase_tick < 1400) return;
        ui_track_title_phase++;
        ui_track_title_phase_tick = now;
        ui_track_title_direction = ui_track_title_phase == 1 ? -1 : 1;
    }

    ui_track_title_offset += ui_track_title_direction * 2;
    if(ui_track_title_offset <= -overflow) {
        ui_track_title_offset = -overflow;
        ui_track_title_phase = 2;
        ui_track_title_phase_tick = now;
    } else if(ui_track_title_offset >= 0) {
        ui_track_title_offset = 0;
        ui_track_title_phase = 0;
        ui_track_title_phase_tick = now;
    }
    lv_obj_set_x(ui_track_title_label, ui_track_title_offset);
}

/**
 * @brief Helper per generare un riquadro notifica standard e compatto dentro la lista
 * @return puntatore alla card creata (per gestione dinamica della lista)
 */
static lv_obj_t * create_notify_item(lv_obj_t * parent, const char * app, const char * group,
                                     const char * person, const char * message, const char * time)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 190, 125);
    lv_obj_set_style_bg_color(card, lv_color_hex(GRAY_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 8, LV_STATE_DEFAULT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 2, LV_STATE_DEFAULT);

    lv_obj_t * header = lv_obj_create(card);
    lv_obj_set_size(header, 174, 18);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(header, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(header, 0, LV_STATE_DEFAULT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_sender = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_sender, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_sender, lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
    lv_label_set_text(lbl_sender, app);
    lv_obj_set_width(lbl_sender, 130);
    lv_label_set_long_mode(lbl_sender, LV_LABEL_LONG_DOT);

    lv_obj_t * lbl_time = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_time, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_label_set_text(lbl_time, time);
    lv_obj_set_width(lbl_time, 40);
    lv_obj_set_style_text_align(lbl_time, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);

    if(group[0] != '\0') {
        lv_obj_t * lbl_group = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_group, &Montserrat_custom_12, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl_group, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
        lv_label_set_text(lbl_group, group);
        lv_obj_set_width(lbl_group, 174);
        lv_label_set_long_mode(lbl_group, LV_LABEL_LONG_DOT);
    }
    if(person[0] != '\0') {
        lv_obj_t * lbl_person = lv_label_create(card);
        lv_obj_set_style_text_font(lbl_person, &Montserrat_custom_12, LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl_person, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
        lv_label_set_text(lbl_person, person);
        lv_obj_set_width(lbl_person, 174);
        lv_label_set_long_mode(lbl_person, LV_LABEL_LONG_DOT);
    }

    /* Testo del messaggio della notifica */
    lv_obj_t * lbl_body = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_body, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(lbl_body, message);
    lv_obj_set_width(lbl_body, 170); /* Forza il wrapping automatico */

    return card;
}

/**
 * @brief Crea la schermata delle Notifiche (A sinistra, scorrevole)
 */
static void create_notifications_screen(void)
{
    notify_scr = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(notify_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(notify_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_set_flag(notify_scr, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_add_event_cb(notify_scr, ui_gesture_event_handler, LV_EVENT_GESTURE, NULL);

    /* Header fisso in alto */
    lv_obj_t * title = lv_label_create(notify_scr);
    lv_obj_set_style_text_font(title, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_notify_screen_title = title;
    lv_label_set_text(title, "Notifications");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* ---------- CONTENITORE SCORREVOLE DELLE NOTIFICHE ---------- */
    ui_notify_list = lv_obj_create(notify_scr);
    lv_obj_set_scrollbar_mode(ui_notify_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(ui_notify_list, 220, 185);
    lv_obj_align(ui_notify_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui_notify_list, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_notify_list, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_notify_list, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_notify_list, 10, LV_STATE_DEFAULT);
    
    lv_obj_set_layout(ui_notify_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui_notify_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_notify_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Distanziatore invisibile inferiore */
    lv_obj_t * bot_spacer = lv_obj_create(ui_notify_list);
    lv_obj_set_size(bot_spacer, 1, 15);
    lv_obj_set_style_bg_opa(bot_spacer, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bot_spacer, 0, LV_STATE_DEFAULT);
}

/**
 * @brief Crea la schermata dedicata al Meteo
 */
static void create_weather_screen(void)
{
    weather_scr = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(weather_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(weather_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_set_flag(weather_scr, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_add_event_cb(weather_scr, ui_gesture_event_handler, LV_EVENT_GESTURE, NULL);

    lv_obj_t * title = lv_label_create(weather_scr);
    lv_obj_set_style_text_font(title, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_weather_screen_title = title;
    lv_label_set_text(title, "Weather");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    ui_scr_w_icon_main = lv_label_create(weather_scr);
        lv_obj_set_style_text_font(ui_scr_w_icon_main, &Montserrat_custom_48, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_scr_w_icon_main, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(ui_scr_w_icon_main, ui_weather_icon_str);
    lv_obj_align(ui_scr_w_icon_main, LV_ALIGN_TOP_MID, 0, 31);

    ui_scr_w_temp_main = lv_label_create(weather_scr);
        lv_obj_set_style_text_font(ui_scr_w_temp_main, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_scr_w_temp_main, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_obj_align(ui_scr_w_temp_main, LV_ALIGN_CENTER, 0, -12);

    ui_scr_w_cond_label = lv_label_create(weather_scr);
        lv_obj_set_style_text_font(ui_scr_w_cond_label, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_scr_w_cond_label, lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
    lv_label_set_text(ui_scr_w_cond_label, "");
    lv_obj_add_flag(ui_scr_w_cond_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ui_scr_w_cond_label, LV_ALIGN_CENTER, 0, 18);

    lv_obj_t * forecast_card = lv_obj_create(weather_scr);
    lv_obj_set_size(forecast_card, 190, 85);
    lv_obj_align(forecast_card, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_color(forecast_card, lv_color_hex(GRAY_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(forecast_card, lv_color_hex(0x2A2A2A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(forecast_card, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(forecast_card, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(forecast_card, 8, LV_STATE_DEFAULT);
    lv_obj_clear_flag(forecast_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_layout(forecast_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(forecast_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(forecast_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for(int i = 0; i < 3; i++) {
        ui_scr_w_forecast_labels[i] = lv_label_create(forecast_card);
        lv_obj_set_style_text_font(ui_scr_w_forecast_labels[i], &Montserrat_custom_12, LV_STATE_DEFAULT);
        if(i == 0) {
            lv_obj_set_style_text_color(ui_scr_w_forecast_labels[i], lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_text_color(ui_scr_w_forecast_labels[i], lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
        }
    }
}

/**
 * @brief Crea la schermata del Media Player
 */
static void create_player_screen(void)
{
    player_scr = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(player_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(player_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_set_flag(player_scr, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_add_event_cb(player_scr, ui_gesture_event_handler, LV_EVENT_GESTURE, NULL);

    lv_obj_t * title = lv_label_create(player_scr);
    lv_obj_set_style_text_font(title, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_player_screen_title = title;
    lv_label_set_text(title, "Music player");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    ui_track_title_viewport = lv_obj_create(player_scr);
    lv_obj_set_size(ui_track_title_viewport, 200, 28);
    lv_obj_set_style_bg_opa(ui_track_title_viewport, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_track_title_viewport, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_track_title_viewport, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_track_title_viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(ui_track_title_viewport, true, LV_STATE_DEFAULT);
    lv_obj_align(ui_track_title_viewport, LV_ALIGN_CENTER, 0, -35);

    ui_track_title_label = lv_label_create(ui_track_title_viewport);
    lv_obj_set_style_text_font(ui_track_title_label, &Montserrat_custom_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_track_title_label, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(ui_track_title_label, "No track");
    lv_label_set_long_mode(ui_track_title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui_track_title_label, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_track_title_label, 24);
    lv_obj_set_style_text_align(ui_track_title_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(ui_track_title_label, LV_ALIGN_LEFT_MID, 0, 0);
    ui_track_title_timer = lv_timer_create(ui_track_title_update_cb, 40, NULL);
    ui_track_title_reset();

    ui_track_artist_label = lv_label_create(player_scr);
        lv_obj_set_style_text_font(ui_track_artist_label, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_track_artist_label, lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
    lv_label_set_text(ui_track_artist_label, "");
    lv_obj_align(ui_track_artist_label, LV_ALIGN_CENTER, 0, -15);

    lv_obj_t * track = lv_obj_create(player_scr);
    lv_obj_set_size(track, 150, 6);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 15);
    lv_obj_set_style_bg_color(track, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(track, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(track, 3, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(track, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * bar = lv_bar_create(player_scr);
    lv_obj_set_size(bar, 150, 6);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 15);
    lv_bar_set_range(bar, 0, 1000);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(BLUE_NEON), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_STATE_DEFAULT);
    ui_track_progress_bar = bar;

    lv_obj_t * controls_cont = lv_obj_create(player_scr);
    lv_obj_set_size(controls_cont, 180, 45);
    lv_obj_align(controls_cont, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_set_style_bg_opa(controls_cont, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(controls_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(controls_cont, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(controls_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(controls_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_prev = lv_button_create(controls_cont);
    lv_obj_set_size(btn_prev, 35, 35);
    lv_obj_set_style_radius(btn_prev, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(GRAY_DARK), LV_STATE_DEFAULT);
    lv_obj_t * lbl_prev = lv_label_create(btn_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_PREV);
    lv_obj_center(lbl_prev);

    lv_obj_t * btn_play = lv_button_create(controls_cont);
    lv_obj_set_size(btn_play, 42, 42);
    lv_obj_set_style_radius(btn_play, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
    ui_btn_play_icon = lv_label_create(btn_play);
    lv_obj_set_style_text_color(ui_btn_play_icon, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_label_set_text(ui_btn_play_icon, LV_SYMBOL_PLAY);
    lv_obj_center(ui_btn_play_icon);

    lv_obj_t * btn_next = lv_button_create(controls_cont);
    lv_obj_set_size(btn_next, 35, 35);
    lv_obj_set_style_radius(btn_next, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(GRAY_DARK), LV_STATE_DEFAULT);
    lv_obj_t * lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_NEXT);
    lv_obj_center(lbl_next);
}

/**
 * @brief Crea la schermata del menu impostazioni
 */
static void create_settings_screen(void)
{
    settings_scr = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(settings_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(settings_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_set_flag(settings_scr, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_add_event_cb(settings_scr, ui_gesture_event_handler, LV_EVENT_GESTURE, NULL);

    lv_obj_t * title = lv_label_create(settings_scr);
    lv_obj_set_style_text_font(title, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_settings_screen_title = title;
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t * settings_list = lv_obj_create(settings_scr);
    lv_obj_set_size(settings_list, 220, 185);
    lv_obj_align(settings_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(settings_list, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(settings_list, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(settings_list, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(settings_list, 12, LV_STATE_DEFAULT);
    lv_obj_set_layout(settings_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * brightness_row = lv_obj_create(settings_list);
    lv_obj_set_size(brightness_row, 200, 42);
    lv_obj_set_style_bg_opa(brightness_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(brightness_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(brightness_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_layout(brightness_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(brightness_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(brightness_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * sub_title_bright = lv_label_create(brightness_row);
    lv_obj_set_style_text_font(sub_title_bright, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(sub_title_bright, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_brightness_label = sub_title_bright;
    lv_label_set_text(sub_title_bright, "Brightness");
    ui_settings_brightness_slider = lv_slider_create(brightness_row);
    lv_obj_set_size(ui_settings_brightness_slider, 180, 10);
    lv_slider_set_value(ui_settings_brightness_slider, 60, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_settings_brightness_slider, lv_color_hex(BLUE_NEON), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_settings_brightness_slider, lv_color_hex(WHITE_COLOR), LV_PART_KNOB);
    lv_obj_add_event_cb(ui_settings_brightness_slider, ui_settings_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t * timeout_row = lv_obj_create(settings_list);
    lv_obj_set_size(timeout_row, 200, 48);
    lv_obj_set_style_bg_opa(timeout_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(timeout_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(timeout_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_layout(timeout_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(timeout_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timeout_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * sub_title_timeout = lv_label_create(timeout_row);
    lv_obj_set_style_text_font(sub_title_timeout, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(sub_title_timeout, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_timeout_label = sub_title_timeout;
    lv_label_set_text(sub_title_timeout, "Screen Timeout");
    ui_settings_timeout_dropdown = lv_dropdown_create(timeout_row);
    lv_obj_set_size(ui_settings_timeout_dropdown, 92, LV_SIZE_CONTENT);
    lv_dropdown_set_options(ui_settings_timeout_dropdown, "5s\n10s\n15s");
    lv_dropdown_set_selected(ui_settings_timeout_dropdown, 1);
    lv_obj_set_style_text_font(ui_settings_timeout_dropdown, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_settings_timeout_dropdown, ui_settings_timeout_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * time_format_row = lv_obj_create(settings_list);
    lv_obj_set_size(time_format_row, 200, 48);
    lv_obj_set_style_bg_opa(time_format_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(time_format_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(time_format_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_layout(time_format_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(time_format_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_format_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * sub_title_time_format = lv_label_create(time_format_row);
    lv_obj_set_style_text_font(sub_title_time_format, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(sub_title_time_format, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    ui_time_format_label = sub_title_time_format;
    lv_label_set_text(sub_title_time_format, "Time format");
    ui_settings_time_format_dropdown = lv_dropdown_create(time_format_row);
    lv_obj_set_size(ui_settings_time_format_dropdown, 92, LV_SIZE_CONTENT);
    lv_dropdown_set_options(ui_settings_time_format_dropdown, "24h\n12h");
    lv_dropdown_set_selected(ui_settings_time_format_dropdown, 0);
    lv_obj_set_style_text_font(ui_settings_time_format_dropdown, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_settings_time_format_dropdown, ui_settings_time_format_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * notify_row = lv_obj_create(settings_list);
    lv_obj_set_size(notify_row, 200, 42);
    lv_obj_set_style_bg_opa(notify_row, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(notify_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(notify_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_layout(notify_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(notify_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(notify_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_notify = lv_label_create(notify_row);
    lv_obj_set_style_text_font(lbl_notify, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_notify, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    ui_notifications_label = lbl_notify;
    lv_label_set_text(lbl_notify, "Notifications");
    ui_settings_notify_switch = lv_switch_create(notify_row);
    lv_obj_set_style_bg_color(ui_settings_notify_switch, lv_color_hex(BLUE_NEON), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(ui_settings_notify_switch, ui_settings_notifications_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * unpair_btn = lv_button_create(settings_list);
    lv_obj_set_size(unpair_btn, 160, 36);
    lv_obj_set_style_bg_color(unpair_btn, lv_color_hex(0xD32F2F), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(unpair_btn, 8, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(unpair_btn, ui_settings_unpair_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_unpair = lv_label_create(unpair_btn);
    lv_obj_set_style_text_font(lbl_unpair, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_unpair, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    ui_unpair_label = lbl_unpair;
    lv_label_set_text(lbl_unpair, "Unpair BLE");
    lv_obj_center(lbl_unpair);

    lv_obj_t * info_card = lv_obj_create(settings_list);
    lv_obj_set_size(info_card, 200, 78);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(GRAY_DARK), LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(info_card, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(info_card, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(info_card, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(info_card, 4, LV_STATE_DEFAULT);
    lv_obj_clear_flag(info_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_layout(info_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(info_card, 2, LV_STATE_DEFAULT);

    lv_obj_t * lbl_info_name = lv_label_create(info_card);
    lv_obj_set_style_text_font(lbl_info_name, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_info_name, lv_color_hex(BLUE_NEON), LV_STATE_DEFAULT);
    ui_info_name_label = lbl_info_name;
    lv_label_set_text(lbl_info_name, "Stait Watch v1.0.0");

    char info_buf[32];
    ui_settings_info_mac_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(ui_settings_info_mac_label, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_settings_info_mac_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    snprintf(info_buf, sizeof(info_buf), "MAC: %s", mock_mac_address);
    lv_label_set_text(ui_settings_info_mac_label, info_buf);

    ui_settings_info_bat_label = lv_label_create(info_card);
    lv_obj_set_style_text_font(ui_settings_info_bat_label, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_settings_info_bat_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    snprintf(info_buf, sizeof(info_buf), "Battery: %d%%", mock_battery);
    lv_label_set_text(ui_settings_info_bat_label, info_buf);
}
/**
 * @brief Callback del tasto RIFIUTA nella schermata chiamata
 */
static void ui_call_reject_cb(lv_event_t * e)
{
    (void)e;
    ui_hide_call();
}

/**
 * @brief Crea la schermata della chiamata in arrivo
 */
static void create_call_screen(void)
{
    call_scr = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(call_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(call_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);

    lv_obj_set_flag(call_scr, LV_OBJ_FLAG_CLICKABLE, true);

    lv_obj_t * title = lv_label_create(call_scr);
    lv_obj_set_style_text_font(title, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00C864), LV_STATE_DEFAULT);
    ui_call_screen_title = title;
    lv_label_set_text(title, "Incoming call");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t * phone_icon = lv_label_create(call_scr);
    lv_obj_set_style_text_font(phone_icon, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(phone_icon, lv_color_hex(0x00FF64), LV_STATE_DEFAULT);
    lv_label_set_text(phone_icon, LV_SYMBOL_CALL);
    lv_obj_align(phone_icon, LV_ALIGN_CENTER, 0, -38);

    call_name_label = lv_label_create(call_scr);
    lv_obj_set_style_text_font(call_name_label, &Montserrat_custom_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(call_name_label, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(call_name_label, "Unknown");
    lv_obj_set_width(call_name_label, 200);
    lv_obj_set_style_text_align(call_name_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(call_name_label, LV_ALIGN_CENTER, 0, -5);

    call_reject_btn = lv_button_create(call_scr);
    lv_obj_set_size(call_reject_btn, 130, 40);
    lv_obj_set_style_bg_color(call_reject_btn, lv_color_hex(0xD32F2F), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(call_reject_btn, 20, LV_STATE_DEFAULT);
    lv_obj_align(call_reject_btn, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_obj_add_event_cb(call_reject_btn, ui_call_reject_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_reject = lv_label_create(call_reject_btn);
    lv_obj_set_style_text_font(lbl_reject, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_reject, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    ui_call_reject_label = lbl_reject;
    lv_label_set_text(lbl_reject, LV_SYMBOL_CLOSE "  REJECT");
    lv_obj_center(lbl_reject);
}

/**
 * @brief Auto-nasconde il popup notifica dopo 3 secondi
 */
static void ui_popup_timer_cb(lv_timer_t * t)
{
    (void)t;
    if(ui_popup_cont == NULL || lv_obj_has_flag(ui_popup_cont, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if(lv_tick_get() - ui_popup_shown_tick > 3000) {
        lv_obj_add_flag(ui_popup_cont, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Crea il popup notifica sovrapposto alla watchface
 */
static void create_watchface_popup(void)
{
    ui_popup_cont = lv_obj_create(watchface_scr);
    lv_obj_set_size(ui_popup_cont, 230, 120);
    lv_obj_align(ui_popup_cont, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_bg_color(ui_popup_cont, lv_color_hex(0x1E1E2E), LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_popup_cont, lv_color_hex(0xE600E6), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_popup_cont, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_popup_cont, 12, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_popup_cont, 10, LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_popup_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_popup_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_layout(ui_popup_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui_popup_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_popup_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(ui_popup_cont, 8, LV_STATE_DEFAULT);

    ui_popup_title = lv_label_create(ui_popup_cont);
    lv_obj_set_style_text_font(ui_popup_title, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_popup_title, lv_color_hex(0x00FF96), LV_STATE_DEFAULT);
    ui_popup_title_label = ui_popup_title;
    lv_label_set_text(ui_popup_title, LV_SYMBOL_BELL "  Notification");
    lv_obj_set_style_text_align(ui_popup_title, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);

    ui_popup_body = lv_label_create(ui_popup_cont);
    lv_obj_set_style_text_font(ui_popup_body, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_popup_body, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(ui_popup_body, "");
    lv_obj_set_width(ui_popup_body, 200);
    lv_obj_set_style_text_align(ui_popup_body, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);

    ui_popup_timer = lv_timer_create(ui_popup_timer_cb, 200, NULL);
}

/**********************
 * GLOBAL FUNCTIONS
 **********************/
void ui_init(void)
{
    watchface_scr = lv_scr_act();
    lv_obj_set_style_bg_color(watchface_scr, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(watchface_scr, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_flag(watchface_scr, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_add_event_cb(watchface_scr, ui_gesture_event_handler, LV_EVENT_GESTURE, NULL);

    /* ---------- CONTENITORE CENTRALE PER ORA DIGITALE COMPATTA ---------- */
    lv_obj_t * time_container = lv_obj_create(watchface_scr);
    lv_obj_set_size(time_container, 220, 80);
    lv_obj_align(time_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(time_container, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(time_container, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(time_container, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_set_layout(time_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(time_container, 2, LV_STATE_DEFAULT);

    ui_hour_label = lv_label_create(time_container);
    lv_obj_set_style_text_font(ui_hour_label, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_hour_label, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);

    lv_obj_t * lbl_colon = lv_label_create(time_container);
    lv_obj_set_style_text_font(lbl_colon, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_colon, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);
    lv_label_set_text(lbl_colon, ":");
    lv_obj_set_style_pad_bottom(lbl_colon, 4, LV_STATE_DEFAULT);

    ui_min_label = lv_label_create(time_container);
    lv_obj_set_style_text_font(ui_min_label, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_min_label, lv_color_hex(WHITE_COLOR), LV_STATE_DEFAULT);

    ui_ampm_label = lv_label_create(time_container);
    lv_obj_set_style_text_font(ui_ampm_label, &Montserrat_custom_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ampm_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_label_set_text(ui_ampm_label, "AM");
    lv_obj_set_style_pad_bottom(ui_ampm_label, 18, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ampm_label, LV_OBJ_FLAG_HIDDEN);

    /* ---------- IL TUO LOGO IMMAGINE CUSTOM ---------- */
    lv_obj_t * brand_logo_img = lv_image_create(watchface_scr);
    lv_image_set_src(brand_logo_img, &Staitwatchface);
    lv_image_set_scale(brand_logo_img, 128);
    lv_obj_set_style_image_recolor_opa(brand_logo_img, LV_OPA_TRANSP, LV_STATE_DEFAULT);  // Torniamo a LV_OPA_TRANSP: logo "pixel sparsi" come avevi prima, è la resa onesta con formato colore mismatched
    lv_obj_align(brand_logo_img, LV_ALIGN_TOP_MID, 0, 0);

    /* ---------- WIDGETS SUPERIORI (Batteria e Meteo) ---------- */
    ui_bat_label = lv_label_create(watchface_scr);
    lv_obj_set_style_text_font(ui_bat_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_bat_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_obj_align(ui_bat_label, LV_ALIGN_CENTER, -58, -52);

    ui_weather_label = lv_label_create(watchface_scr);
    lv_obj_set_style_text_font(ui_weather_label, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_weather_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_obj_align(ui_weather_label, LV_ALIGN_CENTER, 58, -52);

    /* ---------- DATA ESTESA IN BASSO ---------- */
    ui_date_label = lv_label_create(watchface_scr);
    lv_obj_set_style_text_font(ui_date_label, &Montserrat_custom_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_date_label, lv_color_hex(GRAY_TEXT), LV_STATE_DEFAULT);
    lv_obj_align(ui_date_label, LV_ALIGN_BOTTOM_MID, 0, -50);

    /* Costruisce in memoria le quattro sotto-pagine coordinate dello smartwatch */
    create_settings_screen();
    create_player_screen();
    create_weather_screen();
    create_notifications_screen();
    create_call_screen();
    create_watchface_popup();

    /* ---------- TIMER DI REFRESH ---------- */
    lv_timer_create(ui_clock_update_cb, 1000, NULL);
    ui_clock_update_cb(NULL);
}

void ui_set_language(bool english)
{
    ui_english = english;

    if(ui_notify_screen_title) lv_label_set_text(ui_notify_screen_title, english ? "Notifications" : "Notifiche");
    if(ui_weather_screen_title) lv_label_set_text(ui_weather_screen_title, english ? "Weather" : "Meteo");
    if(ui_player_screen_title) lv_label_set_text(ui_player_screen_title, english ? "Music player" : "Lettore musicale");
    if(ui_settings_screen_title) lv_label_set_text(ui_settings_screen_title, english ? "Settings" : "Impostazioni");
    if(ui_call_screen_title) lv_label_set_text(ui_call_screen_title, english ? "Incoming call" : "Chiamata in arrivo");
    if(ui_brightness_label) lv_label_set_text(ui_brightness_label, english ? "Brightness" : "Luminosità");
    if(ui_timeout_label) lv_label_set_text(ui_timeout_label, english ? "Screen Timeout" : "Timeout schermo");
    if(ui_time_format_label) lv_label_set_text(ui_time_format_label, english ? "Time format" : "Formato ora");
    if(ui_notifications_label) lv_label_set_text(ui_notifications_label, english ? "Notifications" : "Notifiche");
    if(ui_unpair_label) lv_label_set_text(ui_unpair_label, english ? "Unpair BLE" : "Disaccoppia BLE");
    if(ui_info_name_label) lv_label_set_text(ui_info_name_label, english ? "Stait Watch v1.1.0" : "Stait Watch v1.1.0");
    if(ui_call_reject_label) lv_label_set_text(ui_call_reject_label, english ? LV_SYMBOL_CLOSE "  REJECT" : LV_SYMBOL_CLOSE "  RIFIUTA");
    if(ui_popup_title_label) lv_label_set_text(ui_popup_title_label, english ? LV_SYMBOL_BELL "  Notification" : LV_SYMBOL_BELL "  Notifica");
    if(ui_track_title_label && strcmp(lv_label_get_text(ui_track_title_label), "No track") == 0) {
        lv_label_set_text(ui_track_title_label, english ? "No track" : "Nessun brano");
    }
    if(ui_settings_timeout_dropdown) {
        uint16_t sel = lv_dropdown_get_selected(ui_settings_timeout_dropdown);
        lv_dropdown_set_options(ui_settings_timeout_dropdown, english ? "5s\n10s\n15s" : "5 s\n10 s\n15 s");
        lv_dropdown_set_selected(ui_settings_timeout_dropdown, sel);
    }
}
/**
 * @brief Carica la schermata richiesta dal firmware (navigazione pilotata da LovyanGFX)
 * @param index 0=Watchface, 1=Impostazioni, 2=Media Player, 3=Notifiche, 4=Meteo, 5=Chiamata
 */
void ui_show_screen(int index)
{
    lv_obj_t * target = NULL;
    lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_IN;

    switch(index) {
        case 0: target = watchface_scr; break;
        case 1: target = settings_scr;  anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;  break;
        case 2: target = player_scr;    anim = LV_SCR_LOAD_ANIM_MOVE_BOTTOM; break;
        case 3: target = notify_scr;    anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT; break;
        case 4: target = weather_scr;   anim = LV_SCR_LOAD_ANIM_MOVE_TOP;   break;
        case 5: target = call_scr;      break;
        default: return;
    }

    lv_screen_load_anim(target, anim, 250, 0, false);
}

void ui_set_battery(int8_t percent)
{
    mock_battery = percent;
    if(ui_settings_info_bat_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Battery: %d%%", mock_battery);
        lv_label_set_text(ui_settings_info_bat_label, buf);
    }
}

void ui_set_settings_callbacks(void (*brightness_cb)(uint8_t level),
                               void (*timeout_cb)(uint8_t index),
                               void (*notifications_cb)(bool on),
                               void (*unpair_cb)(void),
                               void (*time_format_cb)(bool twelve_hour))
{
    settings_brightness_cb = brightness_cb;
    settings_timeout_cb = timeout_cb;
    settings_notifications_cb = notifications_cb;
    settings_unpair_cb = unpair_cb;
    settings_time_format_cb = time_format_cb;
}

void ui_set_temperature(int8_t temp_c)
{
    mock_temp = temp_c;
    /* I label orologio/previsioni vengono rinfrescati dal timer ui_clock_update_cb */
}

void ui_set_weather_icon(const char * icon)
{
    ui_weather_icon_str = icon;
    if(ui_scr_w_icon_main) {
        bool is_night = ui_current_hour >= 19 || ui_current_hour < 7;
        const char * display_icon = (is_night && strcmp(icon, "\u2600") == 0)
                                    ? "\U0001F319" : icon;
        lv_label_set_text(ui_scr_w_icon_main, display_icon);
    }
}

void ui_set_weather_label(const char * label)
{
    ui_weather_cond_str = label;
    if(ui_scr_w_cond_label) {
        lv_label_set_text(ui_scr_w_cond_label, "");
        lv_obj_add_flag(ui_scr_w_cond_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_set_mac(const char * mac)
{
    mock_mac_address = mac;
    if(ui_settings_info_mac_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MAC: %s", mac);
        lv_label_set_text(ui_settings_info_mac_label, buf);
    }
}

void ui_set_brightness(uint8_t level)
{
    if(level < 1) level = 1;
    if(level > 5) level = 5;
    if(ui_settings_brightness_slider) {
        lv_slider_set_value(ui_settings_brightness_slider, (int32_t)(level - 1) * 25, LV_ANIM_OFF);
    }
}

void ui_set_timeout(uint8_t index)
{
    if(index > 2) index = 2;
    if(ui_settings_timeout_dropdown) {
        lv_dropdown_set_selected(ui_settings_timeout_dropdown, index);
    }
}

void ui_set_time_format(bool twelve_hour)
{
    ui_time_12h = twelve_hour;
    if(ui_settings_time_format_dropdown) {
        uint16_t wanted = twelve_hour ? 1 : 0;
        if(lv_dropdown_get_selected(ui_settings_time_format_dropdown) != wanted) {
            lv_dropdown_set_selected(ui_settings_time_format_dropdown, wanted);
        }
    }
    ui_clock_update_cb(NULL);
}

void ui_set_notifications_enabled(bool on)
{
    if(ui_settings_notify_switch) {
        if(on) {
            lv_obj_add_state(ui_settings_notify_switch, LV_STATE_CHECKED);
        }
        else {
            lv_obj_clear_state(ui_settings_notify_switch, LV_STATE_CHECKED);
        }
    }
}

void ui_add_notification(const char * app, const char * group, const char * person,
                         const char * text, const char * time)
{
    if(ui_notify_list == NULL) return;

    /* Se la lista è piena elimina la card più vecchia */
    if(ui_notify_cards[MAX_NOTIFIES - 1] != NULL) {
        lv_obj_delete(ui_notify_cards[MAX_NOTIFIES - 1]);
    }

    /* Sposta le card esistenti di una posizione */
    for(int i = MAX_NOTIFIES - 1; i > 0; i--) {
        ui_notify_cards[i] = ui_notify_cards[i - 1];
    }

    lv_obj_t * card = create_notify_item(ui_notify_list, app, group, person, text, time);
    lv_obj_move_to_index(card, 0);
    ui_notify_cards[0] = card;
}

void ui_clear_notifications(void)
{
    if(ui_notify_list == NULL) return;
    for(int i = 0; i < MAX_NOTIFIES; i++) {
        if(ui_notify_cards[i] != NULL) {
            lv_obj_delete(ui_notify_cards[i]);
            ui_notify_cards[i] = NULL;
        }
    }
}

void ui_show_notification_popup(const char * sender, const char * text)
{
    if(ui_popup_cont == NULL) return;

    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), LV_SYMBOL_BELL "  %s", sender);
    lv_label_set_text(ui_popup_title, title_buf);
    lv_label_set_text(ui_popup_body, text);
    lv_obj_clear_flag(ui_popup_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_popup_cont);
    ui_popup_shown_tick = lv_tick_get();
}

void ui_scroll_notifications(int delta_y)
{
    if(ui_notify_list == NULL) return;
    // Invertito: delta positivo scende, ma UI vuole salire per effetto visuale
    lv_obj_scroll_by(ui_notify_list, 0, -delta_y, LV_ANIM_ON);
}

void ui_set_track(const char * title, const char * artist)
{
    if(ui_track_title_label) {
        const char * current_title = lv_label_get_text(ui_track_title_label);
        if(title == NULL) title = "";
        if(current_title == NULL || strcmp(current_title, title) != 0) {
            lv_label_set_text(ui_track_title_label, title);
            ui_track_title_reset();
        }
    }
    if(ui_track_artist_label) {
        lv_label_set_text(ui_track_artist_label, artist);
    }
}

void ui_set_playing(bool playing)
{
    if(ui_btn_play_icon) {
        lv_label_set_text(ui_btn_play_icon, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

void ui_set_track_progress(uint32_t position_ms, uint32_t duration_ms)
{
    if(ui_track_progress_bar == NULL) return;
    uint32_t progress = 0;
    if(duration_ms > 0 && position_ms < duration_ms) {
        progress = (position_ms * 1000UL) / duration_ms;
    } else if(duration_ms > 0 && position_ms >= duration_ms) {
        progress = 1000;
    }
    lv_bar_set_value(ui_track_progress_bar, (int32_t)progress, LV_ANIM_OFF);
}

void ui_show_call(const char * name)
{
    if(call_name_label) {
        lv_label_set_text(call_name_label, name);
    }
    lv_screen_load_anim(call_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void ui_hide_call(void)
{
    lv_screen_load_anim(watchface_scr, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}
