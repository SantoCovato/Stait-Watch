#include <Arduino.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include "esp_sleep.h"
#include "esp_pm.h"
#include <time.h> 
#include <nvs_flash.h>
#include <sys/time.h>
#include <Preferences.h> 
#include <string>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// --- LVGL (rendering UI) ---
// Richiede la libreria lvgl (versione 9.x). Puoi:
//   - copiare la cartella lvgl/ e questo main/lv_conf.h nella cartella dello sketch, oppure
//   - installare lvgl come libreria Arduino e mettere lv_conf.h (LV_COLOR_DEPTH 16) accanto a lvgl.h.
// Anche i file ui.h / ui.c / Staitwatchface.c devono stare nella cartella dello sketch.
#include "ui.h"   // ui.h include già lvgl.h in modo compatibile con entrambi i layout

// --- CONFIGURAZIONE PIN HARDWARE ---
#define LCD_BL_PIN      2   
#define TOUCH_INT_PIN   5   // GPIO 5 per l'interrupt del Touch (CST816S)
#define TOUCH_SDA       6   
#define TOUCH_SCL       7   
#define TOUCH_CHIP_ADDR 0x15 
#define BAT_ADC_PIN     1   // GPIO 1 per la lettura analogica della batteria

// --- CONFIGURAZIONE BLE ---
#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_UUID "BEA56A26-34EF-44B4-A36F-272A7762AF31"

#define MSG_DATETIME          0x01
#define MSG_WEATHER_TODAY     0x02
#define MSG_WEATHER_FORECAST  0x03
#define MSG_NOTIFICATION      0x04
#define CMD_PLAY_PAUSE        0x10
#define CMD_NEXT              0x11
#define CMD_PREV              0x12
#define MAX_BLE_PACKET        240
#define MSG_LANGUAGE          0x05
#define MSG_INCOMING_CALL     0x06
#define MSG_CALL_END          0x07
#define CMD_REJECT_CALL       0x13
#define MSG_MEDIA_STATE       0x08

bool uiEnglish = true;

const char* uiText(const char* it, const char* en) {
  return uiEnglish ? en : it;
}

const String VERSIONE_FW = "v1.1.0";
const String NOME_WATCH  = "Stait Watch";

// --- DRIVER SCHERMO E TOUCH CONFIGURATO ---
class LGFX_Waveshare : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;
  lgfx::Light_PWM    _light_instance;
  lgfx::Touch_CST816S _touch_instance; 
public:
  LGFX_Waveshare(void) {
    // Configurazione del BUS SPI
    { 
      auto cfg = _bus_instance.config(); 
      cfg.spi_host = SPI2_HOST; 
      cfg.spi_mode = 0; 
      cfg.freq_write = 40000000; 
      cfg.freq_read = 16000000; 
      cfg.pin_sclk = 10; 
      cfg.pin_mosi = 11; 
      cfg.pin_miso = 12; 
      cfg.pin_dc = 8; 
      _bus_instance.config(cfg); 
      _panel_instance.setBus(&_bus_instance); 
    }
    
    // CONFIGURAZIONE REGISTRI HARDWARE DEL PANNELLO
    { 
      auto cfg = _panel_instance.config(); 
      cfg.pin_cs = 9; 
      cfg.pin_rst = 14; 
      cfg.panel_width = 240; 
      cfg.panel_height = 240; 
      cfg.offset_x = 0; 
      cfg.offset_y = 0; 
      cfg.offset_rotation = 0; 
      cfg.dummy_read_pixel = 8; 
      cfg.readable = true; 
      cfg.invert = true; 
      cfg.rgb_order = false; 
      _panel_instance.config(cfg); 
    }
    
    // Retroilluminazione
    { 
      auto cfg = _light_instance.config(); 
      cfg.pin_bl = LCD_BL_PIN; 
      cfg.freq = 12000; 
      cfg.pwm_channel = 7; 
      _light_instance.config(cfg); 
      _panel_instance.setLight(&_light_instance); 
    }
    
    // Touchscreen
    { 
      auto cfg = _touch_instance.config(); 
      cfg.x_min = 0; 
      cfg.x_max = 239; 
      cfg.y_min = 0; 
      cfg.y_max = 239; 
      cfg.pin_int = TOUCH_INT_PIN; 
      cfg.bus_shared = false; 
      cfg.i2c_port = 0; 
      cfg.i2c_addr = TOUCH_CHIP_ADDR; 
      cfg.pin_sda = TOUCH_SDA; 
      cfg.pin_scl = TOUCH_SCL; 
      cfg.freq = 400000; 
      _touch_instance.config(cfg); 
      _panel_instance.setTouch(&_touch_instance); 
    }
    setPanel(&_panel_instance);
  }
};

LGFX_Waveshare lcd;
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCharacteristic = nullptr;
uint16_t connHandleAttivo = 0xFFFF;
Preferences preferences;

// =========================================================================
// DRIVER DISPLAY LVGL -> LovyanGFX (RGB565 su pannello GC9A01 240x240)
// =========================================================================
#if LV_COLOR_DEPTH != 16
  #error "Per l'integrazione LVGL su ESP32, lv_conf.h deve avere LV_COLOR_DEPTH 16 (RGB565)."
#endif

static lv_display_t * displayLvgl = nullptr;
static lv_indev_t * touchIndev = nullptr;
static bool lvglTouchPressed = false;
static lv_point_t lvglTouchPoint = { 0, 0 };
static lv_color_t displayBuffer[240 * 30] __attribute__((aligned(4)));

void lvglFlushCb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
  int32_t w = lv_area_get_width(area);
  int32_t h = lv_area_get_height(area);
  lcd.pushImage(area->x1, area->y1, w, h, (uint16_t *)px_map);
  lv_display_flush_ready(disp);
}

uint32_t lvglTickCb(void) {
  return millis();
}

void lvglTouchReadCb(lv_indev_t * indev, lv_indev_data_t * data) {
  (void)indev;
  data->point = lvglTouchPoint;
  data->state = lvglTouchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void initLvgl() {
  lv_init();
  lv_tick_set_cb(lvglTickCb);
  displayLvgl = lv_display_create(240, 240);
  lv_display_set_flush_cb(displayLvgl, lvglFlushCb);
  lv_display_set_buffers(displayLvgl, displayBuffer, NULL, sizeof(displayBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(displayLvgl);
  touchIndev = lv_indev_create();
  lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchIndev, lvglTouchReadCb);
  ui_init();
}

// --- VARIABILI PERSISTENTI RTC ---
RTC_DATA_ATTR bool rtcSincronizzato = false;
RTC_DATA_ATTR int gradi = 20;               
RTC_DATA_ATTR char meteoStatoBuffer[16] = "ATTESA";

// --- STRUTTURE DATI METEO E NOTIFICHE ---
struct PrevisioneMeteo {
  int temp;
  String stato;
};
PrevisioneMeteo previsioniFuture[3] = { {0, "---"}, {0, "---"}, {0, "---"} };

struct Notifica {
  String mittente;
  String testo;
  String orario; 
};
Notifica listaNotifiche[5];
int numeroNotificheSalvate = 0;
int indiceNotificaVisualizzata = 0; 

// --- VARIABILI DI STATO DEL SISTEMA ---
int schermataAttuale = 0;      
int luminositaLivello = 3;     
int timeoutOpzioni[] = {5000, 10000, 15000};
int indiceTimeout = 1; 
bool mostraNotifiche = true;
bool formato12Ore = false;
String indirizzoMacStr = "00:00:00:00:00:00";

// --- STATO HARDWARE BATTERIA ---
int percentualeBatteria = 0;
bool inCarica = false;
float batteryVoltage = 0.0;
unsigned long ultimoControlloBatteria = 0; 

// --- STATO MUSICA E CONNESSIONE ---
bool inRiproduzione = false;
String titoloBrano = "";
String autoreBrano = "";
uint32_t posizioneBranoMs = 0;
uint32_t durataBranoMs = 0;
bool dispositivoConnesso = false;
unsigned long ultimoMomentoAttivo = 0;
bool schermoAcceso = true;

// --- GESTIONE POPUP NOTIFICA ---
bool mostraPopupNotifica = false;
unsigned long momentoInizioPopup = 0;
const unsigned long DURATA_POPUP = 3000; 

// --- GESTIONE CHIAMATA IN ARRIVO ---
bool chiamataInCorso = false;
String nomeChiamante = "";

volatile bool richiestaAggiornamentoGrafico = false;

// --- GESTIONE SWIPE ---
int touchInizioX = 0; int touchInizioY = 0;
int touchFineX = 0;   int touchFineY = 0;
bool inAscoltoSwipe = false;
bool touchSliderLuminosita = false;
const int SOGLIA_SWIPE = 40; 

// --- BUFFER CODA BLE ---
const int DIMENSIONE_CODA = 10;
struct BlePacket {
  uint8_t data[MAX_BLE_PACKET];
  uint16_t length;
};
BlePacket codaMessaggiBLE[DIMENSIONE_CODA];
int indiceTestaCoda = 0;  
int indiceCodaCoda = 0;   
int messaggiInCoda = 0;

// --- ANTI-FALSO TOCCO ---
unsigned long ultimoToccoRilevato = 0;
const unsigned long FINESTRA_DOPPIO_TOCCO = 400; 
bool flagPrimoToccoAvvenuto = false;

// --- GESTIONE INTERRUPT TOUCH ---
volatile bool interruptTouchRilevato = false;
void IRAM_ATTR touchISR() {
  interruptTouchRilevato = true;
}

// --- FUNZIONI DI SERVIZIO HARDWARE ---
int ottieniValorePWM(int livello) {
  switch (livello) {
    case 0: return 0;   case 1: return 12;  case 2: return 45; case 3: return 100;
    case 4: return 180; case 5: return 255; default: return 100;
  }
}

String ottieniMacHardware() {
  return String(NimBLEDevice::getAddress().toString().c_str());
}

void aggiornaStatoBatteria() {
  long sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(BAT_ADC_PIN);
    delayMicroseconds(50); 
  }
  float rawADC = sum / 100.0;
  batteryVoltage = (rawADC * 3.3 / 4095.0) * 3.0;

  if (batteryVoltage > 3.35) {
    inCarica = true;
    percentualeBatteria = 100;
  } else {
    inCarica = false;
    float percentage = ((batteryVoltage - 2.60) / (3.15 - 2.60)) * 100.0;
    if (percentage > 100.0) percentage = 100.0;
    if (percentage < 0.0) percentage = 0.0;
    percentualeBatteria = (int)percentage;
  }
}

void disegnaWatchface(); 
void cambiaSchermata(int nuova);
void sincronizzaDatiUI();
void aggiornaGrafica();

const char* getDayName(int index) {
  static const char* giorniEn[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char* giorniIt[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
  return uiEnglish ? giorniEn[index] : giorniIt[index];
}

const char* getMonthName(int index) {
  static const char* mesiEn[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  static const char* mesiIt[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};
  return uiEnglish ? mesiEn[index] : mesiIt[index];
}

void forzaAccensioneSchermo() {
  ultimoMomentoAttivo = millis();
  if (!schermoAcceso) {
    lcd.setBrightness(0); 
    lcd.wakeup();         
    schermoAcceso = true;
    flagPrimoToccoAvvenuto = false; 
    delay(100);           
    aggiornaStatoBatteria();
    sincronizzaDatiUI();
    disegnaWatchface();   
    delay(20); 
    lcd.setBrightness(ottieniValorePWM(luminositaLivello)); 
  }
}

void aggiungiNuovaNotifica(String mittente, String testo) {
  time_t oraGrezza; struct tm * infoTempo; time(&oraGrezza); infoTempo = localtime(&oraGrezza);
  char timestamp[6];
  sprintf(timestamp, "%02d:%02d", infoTempo->tm_hour, infoTempo->tm_min);

  for (int i = 4; i > 0; i--) {
    listaNotifiche[i] = listaNotifiche[i - 1];
  }
  
  listaNotifiche[0].mittente = mittente;
  listaNotifiche[0].testo = testo;
  listaNotifiche[0].orario = rtcSincronizzato ? String(timestamp) : "--:--";

  if (numeroNotificheSalvate < 5) {
    numeroNotificheSalvate++;
  }
  indiceNotificaVisualizzata = 0; 
}

// --- FUNZIONE UNPAIR HARDWARE BLE POTENZIATA ---
void eseguiUnpairDispositivi() {
  Serial.println("Richiesta Unpair ricevuta. Formattazione memoria accoppiamenti...");
  
  if (dispositivoConnesso && pServer != nullptr && connHandleAttivo != 0xFFFF) {
    pServer->disconnect(connHandleAttivo);
    dispositivoConnesso = false;
    connHandleAttivo = 0xFFFF;
  }
  
  if (pServer != nullptr) {
    NimBLEDevice::getAdvertising()->stop();
  }

  Serial.println("Svuotamento partizione NVS Bluetooth...");
  nvs_flash_erase();
  nvs_flash_init();
  
  Serial.println("Memoria Flash resettata con successo. Riavvio dell'advertising pulito...");
  
  delay(600);
  if (pServer != nullptr) {
    NimBLEDevice::getAdvertising()->start();
  }
}

// =========================================================================
// INTERFACCIA GRAFICA: LVGL (RENDER) + LOVYANGFX (SWIPE/TOUCH)
// =========================================================================

// Mantenuta per compatibilità con il codice di accensione/timeout.
void disegnaWatchface() {
  cambiaSchermata(0);
}

// Sincronizza i dati reali del firmware con la UI LVGL.
void sincronizzaDatiUI() {
  ui_set_battery((int8_t)percentualeBatteria);
  ui_set_temperature((int8_t)gradi);
  ui_set_mac(indirizzoMacStr.c_str());
  ui_set_timeout((uint8_t)indiceTimeout);
  ui_set_time_format(formato12Ore);
  ui_set_notifications_enabled(mostraNotifiche);
  ui_set_playing(inRiproduzione);
  ui_set_track(titoloBrano.c_str(), autoreBrano.c_str());
  ui_set_track_progress(posizioneBranoMs, durataBranoMs);

  const char * iconaMeteo = "\u2600";
  String stato = String(meteoStatoBuffer);
  stato.toUpperCase();
  if (stato == "SOLE" || stato == "SUN") iconaMeteo = "\u2600";
  else if (stato == "NUBI" || stato == "CLOUDS" || stato == "CLOUDY") iconaMeteo = "\u2601";
  else if (stato == "PIOGGIA" || stato == "RAIN") iconaMeteo = "\u2614";
  else if (stato == "NEVE" || stato == "SNOW") iconaMeteo = "\u2744";
  else if (stato == "TEMPESTA" || stato == "STORM") iconaMeteo = "\u26C8";
  ui_set_weather_icon(iconaMeteo);
  ui_set_weather_label(meteoStatoBuffer);
}

// Render LVGL immediato (chiamato quando i dati cambiano).
void aggiornaGrafica() {
  sincronizzaDatiUI();
  lv_timer_handler();
}

// Navigazione schermate: Lovyan rileva lo swipe e qui LVGL carica la schermata.
void cambiaSchermata(int nuova) {
  schermataAttuale = nuova;
  ui_show_screen(nuova);
}

void settingsBrightnessChanged(uint8_t level) {
  luminositaLivello = level;
  lcd.setBrightness(ottieniValorePWM(luminositaLivello));
  preferences.begin("watch_settings", false);
  preferences.putInt("bright", luminositaLivello);
  preferences.end();
}

void settingsTimeoutChanged(uint8_t index) {
  indiceTimeout = index;
  preferences.begin("watch_settings", false);
  preferences.putInt("tout", indiceTimeout);
  preferences.end();
}

void settingsNotificationsChanged(bool on) {
  mostraNotifiche = on;
  preferences.begin("watch_settings", false);
  preferences.putBool("dnd", mostraNotifiche);
  preferences.end();
}

void settingsTimeFormatChanged(bool twelveHour) {
  formato12Ore = twelveHour;
  preferences.begin("watch_settings", false);
  preferences.putBool("h12", formato12Ore);
  preferences.end();
}

void settingsUnpairRequested() {
  eseguiUnpairDispositivi();
}

void inviaComandoMedia(uint8_t comando) {
  if (pCharacteristic == nullptr) return;
  pCharacteristic->setValue(&comando, 1);
  pCharacteristic->notify();
}

String normalizzaStatoMeteo(const String& statoRicevuto) {
  String stato = statoRicevuto;
  stato.trim();
  stato.toUpperCase();

  if (stato == "1" || stato == "SOLE" || stato == "SUN") return uiText("SOLE", "SUN");
  if (stato == "2" || stato == "NUBI" || stato == "CLOUD" || stato == "CLOUDY") return uiText("NUBI", "CLOUDS");
  if (stato == "3" || stato == "PIOGGIA" || stato == "RAIN") return uiText("PIOGGIA", "RAIN");
  if (stato == "4" || stato == "NEVE" || stato == "SNOW") return uiText("NEVE", "SNOW");
  if (stato == "5" || stato == "TEMPESTA" || stato == "STORM") return uiText("TEMPESTA", "STORM");
  return uiText("NUBI", "CLOUDS");
}

void impostaLinguaSistema(const uint8_t* data, size_t len) {
  if (len < 2) return;

  uint8_t localeLen = data[1];
  if (len < (size_t)(2 + localeLen)) return;

  String locale = "";
  for (uint8_t i = 0; i < localeLen; i++) {
    locale += (char)data[2 + i];
  }
  locale.toLowerCase();

  uiEnglish = locale != "it";
  ui_set_language(uiEnglish);
  aggiornaGrafica();
}

// --- ELABORAZIONE DATI ---
void elaboraPayloadBinario(const uint8_t* data, size_t len) {
  if (len < 1) return;

  switch (data[0]) {
    case MSG_DATETIME:
      if (len >= 8) {
        uint16_t anno = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
        uint8_t mese = data[3];
        uint8_t giorno = data[4];
        uint8_t ora = data[5];
        uint8_t minuto = data[6];
        uint8_t secondo = data[7];

        struct tm tempoImpostato;
        tempoImpostato.tm_year = anno - 1900;
        tempoImpostato.tm_mon = mese - 1;
        tempoImpostato.tm_mday = giorno;
        tempoImpostato.tm_hour = ora;
        tempoImpostato.tm_min = minuto;
        tempoImpostato.tm_sec = secondo;
        tempoImpostato.tm_isdst = -1;

        time_t tempoLocale = mktime(&tempoImpostato);
        struct timeval tv = { .tv_sec = tempoLocale, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        rtcSincronizzato = true;
      }
      break;

    case MSG_LANGUAGE:
      impostaLinguaSistema(data, len);
      break;

    case MSG_WEATHER_TODAY:
      if (len >= 3) {
        gradi = (int8_t)data[1];
        uint8_t stateLen = data[2];
        String statoRicevuto = "";
        if (stateLen > 0 && len >= (size_t)(3 + stateLen)) {
          for (uint8_t i = 0; i < stateLen; i++) {
            statoRicevuto += (char)data[3 + i];
          }
        }

        String statoFinale = normalizzaStatoMeteo(statoRicevuto);
        strncpy(meteoStatoBuffer, statoFinale.c_str(), sizeof(meteoStatoBuffer) - 1);
        meteoStatoBuffer[sizeof(meteoStatoBuffer) - 1] = '\0';
      }
      break;

    case MSG_WEATHER_FORECAST:
      if (len >= 4) {
        uint8_t indiceGiorno = data[1];
        int8_t temp = (int8_t)data[2];
        uint8_t stateLen = data[3];
        if (indiceGiorno < 3) {
          previsioniFuture[indiceGiorno].temp = temp;
          String statoRicevuto = "";
          if (stateLen > 0 && len >= (size_t)(4 + stateLen)) {
            for (uint8_t i = 0; i < stateLen; i++) {
              statoRicevuto += (char)data[4 + i];
            }
          }
          previsioniFuture[indiceGiorno].stato = normalizzaStatoMeteo(statoRicevuto);
        }
      }
      break;

    case MSG_NOTIFICATION:
      if (mostraNotifiche && len >= 7) {
        uint8_t appLen = data[1];
        uint8_t groupLen = data[2];
        uint8_t personLen = data[3];
        uint8_t textLen = data[4];
        uint8_t timeLen = data[5];
        if (textLen > 0 && len >= (size_t)(7 + appLen + groupLen + personLen + textLen + timeLen)) {
          String app = "";
          String gruppo = "";
          String persona = "";
          String testo = "";
          String orario = timeLen > 0 ? "" : "--:--";
          size_t offset = 7;
          for (uint8_t i = 0; i < appLen; i++) app += (char)data[offset + i];
          offset += appLen;
          for (uint8_t i = 0; i < groupLen; i++) gruppo += (char)data[offset + i];
          offset += groupLen;
          for (uint8_t i = 0; i < personLen; i++) persona += (char)data[offset + i];
          offset += personLen;
          for (uint8_t i = 0; i < textLen; i++) testo += (char)data[offset + i];
          offset += textLen;
          for (uint8_t i = 0; i < timeLen; i++) orario += (char)data[offset + i];

          aggiungiNuovaNotifica(persona.length() > 0 ? persona : app, testo);
          ui_add_notification(app.c_str(), gruppo.c_str(), persona.c_str(), testo.c_str(), orario.c_str());
          forzaAccensioneSchermo();
          ultimoMomentoAttivo = millis();
          schermataAttuale = 0;
          mostraPopupNotifica = true;
          momentoInizioPopup = millis();
          // Popup notifica renderizzato da LVGL sulla watchface (auto-hide 3s)
          const String popupMittente = persona.length() > 0 ? persona : app;
          ui_show_notification_popup(popupMittente.c_str(), testo.c_str());
        }
      }
      break;

    case MSG_MEDIA_STATE:
      if (len >= 12) {
        uint8_t titleLen = data[1];
        uint8_t artistLen = data[2];
        if (len >= (size_t)(12 + titleLen + artistLen)) {
          inRiproduzione = data[3] != 0;
          posizioneBranoMs = (uint32_t)data[4] |
                             ((uint32_t)data[5] << 8) |
                             ((uint32_t)data[6] << 16) |
                             ((uint32_t)data[7] << 24);
          durataBranoMs = (uint32_t)data[8] |
                          ((uint32_t)data[9] << 8) |
                          ((uint32_t)data[10] << 16) |
                          ((uint32_t)data[11] << 24);
          titoloBrano = "";
          autoreBrano = "";
          for (uint8_t i = 0; i < titleLen; i++) titoloBrano += (char)data[12 + i];
          for (uint8_t i = 0; i < artistLen; i++) autoreBrano += (char)data[12 + titleLen + i];
          ui_set_track(titoloBrano.c_str(), autoreBrano.c_str());
          ui_set_playing(inRiproduzione);
          ui_set_track_progress(posizioneBranoMs, durataBranoMs);
        }
      }
      break;

    case MSG_INCOMING_CALL:
      if (len >= 2) {
        uint8_t nameLen = data[1];
        if (nameLen > 0 && len >= (size_t)(2 + nameLen)) {
          nomeChiamante = "";
          for (uint8_t i = 0; i < nameLen; i++) {
            nomeChiamante += (char)data[2 + i];
          }
        } else {
          nomeChiamante = uiText("Sconosciuto", "Unknown");
        }
        chiamataInCorso = true;
        forzaAccensioneSchermo();
        ultimoMomentoAttivo = millis();
        schermataAttuale = 5;
        mostraPopupNotifica = false;
        ui_show_call(nomeChiamante.c_str());
      }
      break;

    case MSG_CALL_END:
      chiamataInCorso = false;
      nomeChiamante = "";
      schermataAttuale = 0;
      ui_hide_call();
      break;
  }

  richiestaAggiornamentoGrafico = true;
}

// --- CALLBACK ASINCRONE BLE ---
class TargetServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
      Serial.println("BLE: Client connected");
      dispositivoConnesso = true;
      connHandleAttivo = connInfo.getConnHandle();
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
      Serial.printf("BLE: Client disconnected, reason=%d\n", reason);
      dispositivoConnesso = false;
      connHandleAttivo = 0xFFFF;
      NimBLEDevice::startAdvertising();
    }
};

class NotificaCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
      std::string valoreRicevuto = pCharacteristic->getValue();
      if (valoreRicevuto.empty() || valoreRicevuto.length() > MAX_BLE_PACKET) return;

      if (messaggiInCoda < DIMENSIONE_CODA) {
        codaMessaggiBLE[indiceTestaCoda].length = valoreRicevuto.length();
        memcpy(codaMessaggiBLE[indiceTestaCoda].data, valoreRicevuto.data(), valoreRicevuto.length());
        indiceTestaCoda = (indiceTestaCoda + 1) % DIMENSIONE_CODA;
        messaggiInCoda++;
      }
    }
};

// =========================================================================
// SETUP DEL SISTEMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();

  preferences.begin("watch_settings", false); 
  luminositaLivello = preferences.getInt("bright", 3); 
  if (luminositaLivello < 1 || luminositaLivello > 5) luminositaLivello = 1;
  indiceTimeout     = preferences.getInt("tout", 1);
  mostraNotifiche   = preferences.getBool("dnd", true);
  formato12Ore      = preferences.getBool("h12", false);
  preferences.end(); 

  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT_PIN), touchISR, FALLING);

    lcd.init(); 
  lcd.setSwapBytes(true);  // RGB565: LVGL produce byte nativi, GC9A01 SPI li vuole swappati
  lcd.setRotation(0); 
  lcd.setBrightness(0);  

  initLvgl();               // Avvia LVGL con display driver verso LovyanGFX
  ui_set_settings_callbacks(settingsBrightnessChanged, settingsTimeoutChanged,
                            settingsNotificationsChanged, settingsUnpairRequested,
                            settingsTimeFormatChanged);
  
  NimBLEDevice::init("Stait Watch");
  indirizzoMacStr = ottieniMacHardware();
  pinMode(BAT_ADC_PIN, INPUT);
  analogSetAttenuation(ADC_11db); 
  aggiornaStatoBatteria();
  gradi = 20;
  strncpy(meteoStatoBuffer, "SOLE", sizeof(meteoStatoBuffer) - 1);
  meteoStatoBuffer[sizeof(meteoStatoBuffer) - 1] = '\0';

  schermoAcceso = true;
  sincronizzaDatiUI();      // Spinge batteria/meteo/MAC/luminosità nella UI LVGL
  ui_set_brightness((uint8_t)luminositaLivello);
  lv_timer_handler();
  lcd.setBrightness(ottieniValorePWM(luminositaLivello));

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new TargetServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::WRITE_NR |
    NIMBLE_PROPERTY::NOTIFY |
    NIMBLE_PROPERTY::INDICATE
  );
  pCharacteristic->setCallbacks(new NotificaCallbacks());
  pService->start();

  const char* deviceName = NOME_WATCH.c_str();
  NimBLEDevice::setDeviceName(std::string(deviceName));
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(std::string(deviceName));
  pAdvertising->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  esp_pm_config_esp32s3_t pm_config;
  pm_config.max_freq_mhz = 240; pm_config.min_freq_mhz = 40; pm_config.light_sleep_enable = true; 
  esp_pm_configure(&pm_config);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_INT_PIN, 0); 
  ultimoMomentoAttivo = millis();
}

// =========================================================================
// LOOP PRINCIPALE
// =========================================================================
void loop() {
  if (messaggiInCoda > 0) {
    BlePacket datoDaElaborare = codaMessaggiBLE[indiceCodaCoda];
    indiceCodaCoda = (indiceCodaCoda + 1) % DIMENSIONE_CODA;
    messaggiInCoda--;
    elaboraPayloadBinario(datoDaElaborare.data, datoDaElaborare.length);
  }

  if (richiestaAggiornamentoGrafico) { richiestaAggiornamentoGrafico = false; if (schermoAcceso) aggiornaGrafica(); }
  if (millis() - ultimoControlloBatteria > 30000) { ultimoControlloBatteria = millis(); aggiornaStatoBatteria(); if (schermoAcceso) richiestaAggiornamentoGrafico = true; }

  static unsigned long ultimoSeguno = 0;
  if (schermoAcceso && (millis() - ultimoSeguno > 1000)) { ultimoSeguno = millis(); aggiornaGrafica(); }
  if (schermataAttuale == 0 && mostraPopupNotifica && (millis() - momentoInizioPopup >= DURATA_POPUP)) { mostraPopupNotifica = false; aggiornaGrafica(); }

  uint16_t touchX, touchY;
  if (interruptTouchRilevato || inAscoltoSwipe) {
    interruptTouchRilevato = false; 
    if (lcd.getTouch(&touchX, &touchY)) {
      lvglTouchPoint.x = touchX;
      lvglTouchPoint.y = touchY;
      lvglTouchPressed = true;
      if (!schermoAcceso) {
        if (chiamataInCorso) {
          forzaAccensioneSchermo();
          aggiornaGrafica();
          delay(150);
        } else {
          unsigned long momentoToccoAttuale = millis();
          if (!flagPrimoToccoAvvenuto) { flagPrimoToccoAvvenuto = true; ultimoToccoRilevato = momentoToccoAttuale; } 
          else {
            if (momentoToccoAttuale - ultimoToccoRilevato <= FINESTRA_DOPPIO_TOCCO) { forzaAccensioneSchermo(); aggiornaStatoBatteria(); aggiornaGrafica(); } 
            else { ultimoToccoRilevato = momentoToccoAttuale; }
          }
          delay(150); 
        }
      } 
      else {
        ultimoMomentoAttivo = millis(); 
        
        if (schermataAttuale == 5) {
          if (!inAscoltoSwipe) {
            touchInizioX = touchX;
            touchInizioY = touchY;
            inAscoltoSwipe = true;
          }
          touchFineX = touchX; touchFineY = touchY;
        } else {
          if (mostraPopupNotifica) { mostraPopupNotifica = false; aggiornaGrafica(); delay(150); } 
          if (!inAscoltoSwipe) {
            touchInizioX = touchX;
            touchInizioY = touchY;
            touchSliderLuminosita = schermataAttuale == 1 &&
                                    touchInizioX >= 10 && touchInizioX <= 230 &&
                                    touchInizioY >= 68 && touchInizioY <= 108;
            inAscoltoSwipe = true;
          }
          touchFineX = touchX; touchFineY = touchY;
        }
      }
    }
    else if (inAscoltoSwipe) {
      lvglTouchPressed = false;
      bool sliderLuminositaAttivo = touchSliderLuminosita;
      touchSliderLuminosita = false;
      inAscoltoSwipe = false; 
      int deltaX = touchFineX - touchInizioX; int deltaY = touchFineY - touchInizioY;
      
      if (schermataAttuale == 5) {
        if (abs(deltaX) <= SOGLIA_SWIPE && abs(deltaY) <= SOGLIA_SWIPE) {
          // Pulsante RIFIUTA della schermata chiamata LVGL (x 55..185, y 150..205)
          if (touchInizioX >= 55 && touchInizioX <= 185 && touchInizioY >= 150 && touchInizioY <= 205) {
            inviaComandoMedia(CMD_REJECT_CALL);
            chiamataInCorso = false;
            nomeChiamante = "";
            cambiaSchermata(0);
            lv_timer_handler();
            delay(300);
          }
        }
      }
      else if (!sliderLuminositaAttivo && (abs(deltaX) > SOGLIA_SWIPE || abs(deltaY) > SOGLIA_SWIPE)) {
        if (schermataAttuale == 3 && abs(deltaY) > abs(deltaX) && abs(deltaY) > SOGLIA_SWIPE) {
          // Swipe verticale nella lista notifiche -> scroll manuale della lista LVGL
          ui_scroll_notifications(-deltaY);
        } 
        else {
          if (abs(deltaX) > abs(deltaY)) {
            if (deltaX > SOGLIA_SWIPE) { if (schermataAttuale == 0) schermataAttuale = 3; else if (schermataAttuale == 1) schermataAttuale = 0; } 
            else if (deltaX < -SOGLIA_SWIPE) { if (schermataAttuale == 0) schermataAttuale = 1; else if (schermataAttuale == 3) schermataAttuale = 0; }
          } else {
            if (deltaY > SOGLIA_SWIPE) { if (schermataAttuale == 0) schermataAttuale = 2; else if (schermataAttuale == 4) schermataAttuale = 0; } 
            else if (deltaY < -SOGLIA_SWIPE) { if (schermataAttuale == 0) schermataAttuale = 4; else if (schermataAttuale == 2) schermataAttuale = 0; }
          }
        }
        // Lovyan ha rilevato lo swipe: LVGL renderizza la nuova schermata
        cambiaSchermata(schermataAttuale);
        lv_timer_handler();
      } else {
        if (schermataAttuale == 2) { 
          // Controlli media player LVGL in basso (y 168..216)
          if (touchInizioY >= 168 && touchInizioY <= 216) {
            if (touchInizioX >= 40 && touchInizioX < 90) { inviaComandoMedia(CMD_PREV); }
            else if (touchInizioX >= 90 && touchInizioX <= 150) { inRiproduzione = !inRiproduzione; ui_set_playing(inRiproduzione); inviaComandoMedia(CMD_PLAY_PAUSE); }
            else if (touchInizioX > 150 && touchInizioX <= 210) { inviaComandoMedia(CMD_NEXT); }
            aggiornaGrafica(); delay(150);
          }
        }
      }
    }
  }

  if (!schermoAcceso && flagPrimoToccoAvvenuto && (millis() - ultimoToccoRilevato > FINESTRA_DOPPIO_TOCCO)) { flagPrimoToccoAvvenuto = false; }

  // Il timeout non si applica quando c'è una chiamata in corso
  if (schermoAcceso && !chiamataInCorso && (millis() - ultimoMomentoAttivo > timeoutOpzioni[indiceTimeout])) {
    lcd.setBrightness(0); lcd.sleep(); schermoAcceso = false; schermataAttuale = 0; mostraPopupNotifica = false; flagPrimoToccoAvvenuto = false; interruptTouchRilevato = false; inAscoltoSwipe = false;
    if (!dispositivoConnesso) { delay(50); esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_INT_PIN, 0); esp_deep_sleep_start(); } 
    else { cambiaSchermata(0); }
  }

  // Render + timer LVGL (animazioni schermate, orologio, popup notifiche)
  lv_timer_handler();
  delay(10); 
}