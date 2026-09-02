/**
 * @file ui.h
 * @brief UI personalizzata per Waveshare ESP32-S3 Round LCD 1.28"
 */

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief Inizializza la UI personalizzata.
 *        Deve essere chiamata dopo lv_init() (e dopo aver creato il display).
 */
void ui_init(void);

/** Imposta la lingua UI: true inglese, false italiano. */
void ui_set_language(bool english);

/**
 * @brief Carica la schermata indicata.
 * @param index 0=Watchface, 1=Impostazioni, 2=Media Player,
 *              3=Notifiche, 4=Meteo, 5=Chiamata in arrivo
 */
void ui_show_screen(int index);

/* ------------------- Dati in tempo reale ------------------- */

/** Aggiorna la percentuale batteria mostrata. @param percent 0..100 */
void ui_set_battery(int8_t percent);

/** Aggiorna la temperatura esterna. @param temp_c gradi centigradi */
void ui_set_temperature(int8_t temp_c);

/** Imposta l'icona meteo principale (es. "☀️", "☁️", "🌧", "⛈"). */
void ui_set_weather_icon(const char * icon);

/** Imposta il testo dello stato meteo (es. "SOLE", "NUBI", "PIOGGIA"). */
void ui_set_weather_label(const char * label);

/** Imposta l'indirizzo MAC mostrato nelle impostazioni. */
void ui_set_mac(const char * mac);

/** Aggiorna lo slider luminosità. @param level 1..5 */
void ui_set_brightness(uint8_t level);

/** Aggiorna il dropdown timeout. @param index 0=5s,1=10s,2=15s */
void ui_set_timeout(uint8_t index);

/** Imposta il formato ora. @param twelve_hour true=12h (AM/PM), false=24h */
void ui_set_time_format(bool twelve_hour);

/** Aggiorna lo switch "mostra notifiche" (DND). */
void ui_set_notifications_enabled(bool on);

/** Collega le azioni firmware ai controlli della schermata impostazioni. */
void ui_set_settings_callbacks(void (*brightness_cb)(uint8_t level),
                               void (*timeout_cb)(uint8_t index),
                               void (*notifications_cb)(bool on),
                               void (*unpair_cb)(void),
                               void (*time_format_cb)(bool twelve_hour));

/** Aggiunge una notifica in cima alla lista (max 5). */
void ui_add_notification(const char * app, const char * group, const char * person,
                         const char * text, const char * time);

/** Svuota la lista delle notifiche. */
void ui_clear_notifications(void);

/** Scorri la lista notifiche di delta_y pixel (positivo = verso il basso). */
void ui_scroll_notifications(int delta_y);

/** Mostra un popup notifica sovrapposto alla watchface (auto-hide 3s). */
void ui_show_notification_popup(const char * sender, const char * text);

/* ------------------- Media Player ------------------- */

/** Imposta titolo e artista del brano mostrato. */
void ui_set_track(const char * title, const char * artist);

/** Aggiorna l'icona play/pausa. */
void ui_set_playing(bool playing);
void ui_set_track_progress(uint32_t position_ms, uint32_t duration_ms);

/* ------------------- Chiamata ------------------- */

/** Mostra la schermata chiamata con il nome del chiamante. */
void ui_show_call(const char * name);

/** Chiude la schermata chiamata e torna alla watchface. */
void ui_hide_call(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_H*/