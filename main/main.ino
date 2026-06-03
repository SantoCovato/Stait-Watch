#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_mac.h> 
#include "esp_sleep.h"
#include "esp_pm.h"
#include <time.h> 
#include <nvs_flash.h>
#include <sys/time.h>
#include <Preferences.h> 

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

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

const String VERSIONE_FW = "v1.0.0";
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
LGFX_Sprite canvas(&lcd); 
BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic;
Preferences preferences; 

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
String indirizzoMacStr = "00:00:00:00:00:00";

// --- STATO HARDWARE BATTERIA ---
int percentualeBatteria = 0;
bool inCarica = false;
float batteryVoltage = 0.0;
unsigned long ultimoControlloBatteria = 0; 

// --- STATO MUSICA E CONNESSIONE ---
bool inRiproduzione = false;
bool dispositivoConnesso = false;
unsigned long ultimoMomentoAttivo = 0;
bool schermoAcceso = true;

// --- GESTIONE POPUP NOTIFICA ---
bool mostraPopupNotifica = false;
unsigned long momentoInizioPopup = 0;
const unsigned long DURATA_POPUP = 3000; 

volatile bool richiestaAggiornamentoGrafico = false;

// --- GESTIONE SWIPE ---
int touchInizioX = 0; int touchInizioY = 0;
int touchFineX = 0;   int touchFineY = 0;
bool inAscoltoSwipe = false;
const int SOGLIA_SWIPE = 40; 

// --- BUFFER CODA BLE ---
const int DIMENSIONE_CODA = 10;
String codaMessaggiBLE[DIMENSIONE_CODA];
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
    case 1: return 12;  case 2: return 45; case 3: return 100;
    case 4: return 180; case 5: return 255; default: return 100;
  }
}

String ottieniMacHardware() {
  uint8_t macBuffer[6];
  esp_read_mac(macBuffer, ESP_MAC_WIFI_STA);
  char bufferFormattato[18];
  sprintf(bufferFormattato, "%02X:%02X:%02X:%02X:%02X:%02X", macBuffer[0], macBuffer[1], macBuffer[2], macBuffer[3], macBuffer[4], macBuffer[5]);
  return String(bufferFormattato);
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

void forzaAccensioneSchermo() {
  ultimoMomentoAttivo = millis();
  if (!schermoAcceso) {
    lcd.setBrightness(0); 
    lcd.wakeup();         
    schermoAcceso = true;
    flagPrimoToccoAvvenuto = false; 
    delay(100);           
    aggiornaStatoBatteria();
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
  
  // 1. Forza la disconnessione immediata se un dispositivo è connesso
  if (dispositivoConnesso && pServer != nullptr) {
    uint16_t connId = pServer->getConnId();
    pServer->disconnect(connId); 
    dispositivoConnesso = false;
  }
  
  // 2. Ferma temporaneamente l'advertising per non disturbare la memoria radio
  if (pServer != nullptr) {
    pServer->getAdvertising()->stop();
  }

  // 3. CANCELLAZIONE RADICALE TRAMITE RESET DELLA MEMORIA FLASH (NVS)
  // Questo metodo bypassa le funzioni BLE e cancella direttamente i file fisici dei bonding
  Serial.println("Svuotamento partizione NVS Bluetooth...");
  nvs_flash_erase();      // Cancella l'intera memoria di sistema (inclusi i bonding)
  nvs_flash_init();       // Inizializza nuovamente la memoria pulita da zero
  
  Serial.println("Memoria Flash resettata con successo. Riavvio dell'advertising pulito...");
  
  // 4. Pausa di assestamento per permettere all'hardware di stabilizzarsi e ripartenza
  delay(600);
  if (pServer != nullptr) {
    pServer->getAdvertising()->setScanFilter(false, false); 
    pServer->getAdvertising()->start();
  }
}

// =========================================================================
// INTERFACCIA GRAFICA SCHERMATE
// =========================================================================
void disegnaWatchface() {
  canvas.fillScreen(TFT_BLACK);
  time_t oraGrezza; struct tm * infoTempo; time(&oraGrezza); infoTempo = localtime(&oraGrezza); 
  char bufferOrario[6]; char bufferData[32];
  sprintf(bufferOrario, "%02d:%02d", infoTempo->tm_hour, infoTempo->tm_min);

  const char* giorniSettimana[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
  const char* mesiAnno[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};
  
  canvas.drawArc(120, 120, 114, 110, 0, 360, canvas.color565(255, 100, 0));
  canvas.setFont(&fonts::Font0); canvas.setTextSize(2); canvas.setTextColor(canvas.color565(180, 180, 180)); canvas.setTextDatum(MC_DATUM);

  if (rtcSincronizzato) {
    sprintf(bufferData, "%s %d %s", giorniSettimana[infoTempo->tm_wday], infoTempo->tm_mday, mesiAnno[infoTempo->tm_mon]);
    canvas.drawString(bufferData, 120, 55);
  } else {
    canvas.drawString("AVVIA APP", 120, 55);
  }

  canvas.drawFastHLine(80, 70, 80, canvas.color565(60, 60, 60));
  canvas.setFont(&fonts::Font4); canvas.setTextSize(2.5); canvas.setTextColor(TFT_WHITE);
  canvas.drawString(bufferOrario, 120, 115);
  canvas.drawFastHLine(80, 160, 80, canvas.color565(60, 60, 60));

  int xBat = 98, yBat = 12, wBat = 32, hBat = 14;
  canvas.drawRect(xBat, yBat, wBat, hBat, canvas.color565(140, 140, 140)); 
  canvas.fillRect(xBat + wBat, yBat + 4, 3, 6, canvas.color565(140, 140, 140)); 
  
  int barWidth = map(percentualeBatteria, 0, 100, 0, wBat - 4);
  uint16_t coloreBarra = TFT_GREEN;
  if (percentualeBatteria < 20) coloreBarra = TFT_RED;
  else if (percentualeBatteria < 50) coloreBarra = TFT_ORANGE;

  if (inCarica) {
    canvas.fillRect(xBat + 2, yBat + 2, wBat - 4, hBat - 4, canvas.color565(0, 150, 255));
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1); canvas.setTextColor(TFT_WHITE); canvas.setTextDatum(MC_DATUM);
    canvas.drawString("CHG", xBat + (wBat / 2), yBat + (hBat / 2) + 1);
  } else {
    canvas.fillRect(xBat + 2, yBat + 2, barWidth, hBat - 4, coloreBarra);
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1); canvas.setTextColor(canvas.color565(200, 200, 200)); canvas.setTextDatum(CL_DATUM);
    canvas.drawString(String(percentualeBatteria) + "%", xBat + wBat + 8, yBat + (hBat / 2) + 1);
  }

  canvas.setFont(&fonts::Font4); canvas.setTextSize(1); canvas.setTextColor(canvas.color565(0, 220, 255)); 
  String stringaNumeroGradi = String(gradi);
  int larghezzaNumero = canvas.textWidth(stringaNumeroGradi);
  int xInizioTesto = 95 - (larghezzaNumero / 2);
  canvas.setTextDatum(TL_DATUM);
  canvas.drawString(stringaNumeroGradi + " C", xInizioTesto, 172);
  canvas.drawCircle(xInizioTesto + larghezzaNumero + 4, 175, 2, canvas.color565(0, 220, 255));

  canvas.setTextDatum(MC_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.5); canvas.setTextColor(TFT_YELLOW); 
  canvas.drawString(String(meteoStatoBuffer), 155, 185);

  if (mostraPopupNotifica) {
    if (millis() - momentoInizioPopup < DURATA_POPUP) {
      canvas.fillRoundRect(15, 60, 210, 120, 10, canvas.color565(30, 30, 45));
      canvas.drawRoundRect(15, 60, 210, 120, 10, canvas.color565(230, 0, 230));
      canvas.setTextDatum(TL_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.2);
      canvas.setTextColor(canvas.color565(0, 255, 150));
      canvas.drawString(listaNotifiche[0].mittente.substring(0,15), 25, 70); 
      canvas.drawFastHLine(25, 85, 190, canvas.color565(80, 80, 100));
      canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1);
      String anteprima = listaNotifiche[0].testo.length() > 50 ? listaNotifiche[0].testo.substring(0, 47) + "..." : listaNotifiche[0].testo;
      if(anteprima.length() > 24) {
        canvas.drawString(anteprima.substring(0, 24), 25, 95);
        canvas.drawString(anteprima.substring(24), 25, 110);
      } else {
        canvas.drawString(anteprima, 25, 95);
      }
    } else {
      mostraPopupNotifica = false; 
    }
  }
  canvas.pushSprite(0, 0);
}

// --- FUNZIONE IMPOSTAZIONI GRAFICAMENTE BLINDATA ---
void disegnaImpostazioni() {
  canvas.fillScreen(TFT_BLACK);
  
  // Titolo Principale
  canvas.setTextDatum(MC_DATUM); 
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.5); 
  canvas.setTextColor(canvas.color565(255, 100, 0));
  canvas.drawString("IMPOSTAZIONI", 120, 22);
  canvas.drawFastHLine(50, 32, 140, canvas.color565(80, 80, 80));

  // 1. Sezione Luminosità
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.2); 
  canvas.setTextColor(canvas.color565(180, 180, 180));
  canvas.drawString("LUMINOSITA", 120, 44);
  
  for(int i = 0; i < 5; i++) {
    uint16_t coloreTacca = (i < luminositaLivello) ? TFT_ORANGE : canvas.color565(50, 50, 50);
    canvas.fillRect(65 + (i * 24), 52, 18, 7, coloreTacca);
  }
  
  // Ripristino forzato dei puntatori font del canvas dopo fillRect geometrici
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.2); 
  canvas.setTextColor(canvas.color565(180, 180, 180));

  // 2. Sezione Timeout Schermo
  canvas.drawString("TIMEOUT SCHERMO", 120, 75);
  canvas.fillRect(55, 84, 130, 18, canvas.color565(40, 40, 40));
  canvas.drawRect(55, 84, 130, 18, canvas.color565(90, 90, 90));
  
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.1);
  canvas.setTextColor(TFT_WHITE);
  int secondiTimeout = timeoutOpzioni[indiceTimeout] / 1000;
  String timeoutStr = String(secondiTimeout) + " SECONDI";
  canvas.drawString(timeoutStr, 120, 93);

  // 3. Sezione Notifiche ON/OFF
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.2);
  canvas.setTextColor(canvas.color565(180, 180, 180)); 
  canvas.drawString("MOSTRA NOTIFICHE", 120, 114);
  
  if (mostraNotifiche) {
    canvas.fillRect(98, 123, 44, 15, canvas.color565(0, 160, 40)); 
    canvas.fillRect(126, 125, 14, 11, TFT_WHITE);
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1.0); canvas.setTextColor(TFT_WHITE); 
    canvas.drawString("ON", 112, 131);
  } else {
    canvas.fillRect(98, 123, 44, 15, canvas.color565(90, 90, 90)); 
    canvas.fillRect(100, 125, 14, 11, TFT_WHITE);
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1.0); canvas.setTextColor(canvas.color565(180, 180, 180)); 
    canvas.drawString("OFF", 128, 131);
  }

  // 4. Tasto Unpair Bluetooth
  canvas.fillRoundRect(70, 146, 100, 22, 5, canvas.color565(200, 30, 30)); 
  canvas.drawRoundRect(70, 146, 100, 22, 5, TFT_WHITE);
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("UNPAIR BLE", 120, 157);

  // Informazioni Hardware / Fondo Schermo
  canvas.drawFastHLine(40, 176, 160, canvas.color565(60, 60, 60));
  canvas.setFont(&fonts::Font0); 
  canvas.setTextSize(1.0); 
  
  canvas.setTextColor(canvas.color565(140, 140, 140));
  canvas.drawString(NOME_WATCH + " " + VERSIONE_FW, 120, 188);
  
  canvas.setTextColor(canvas.color565(100, 190, 240)); 
  canvas.drawString("MAC: " + indirizzoMacStr, 120, 202);
  
  canvas.setTextColor(canvas.color565(140, 140, 140));
  canvas.drawString("Batt: " + String(batteryVoltage, 2) + "V (" + String(percentualeBatteria) + "%)", 120, 216);
  
  canvas.pushSprite(0, 0);
}

void disegnaMusica() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFont(&fonts::Font0); canvas.setTextSize(1.5); canvas.setTextColor(canvas.color565(0, 200, 255));
  canvas.drawString("MEDIA PLAYER", 120, 25);
  canvas.drawFastHLine(50, 38, 140, canvas.color565(40, 60, 80));
  canvas.setTextColor(TFT_WHITE); canvas.drawString("CONTROLLO REMOTO", 120, 80);
  canvas.setTextColor(canvas.color565(150, 150, 150)); canvas.setTextSize(1); canvas.drawString("Usa i tasti sotto", 120, 100);
  canvas.fillCircle(60, 150, 22, canvas.color565(40, 40, 40));
  canvas.fillTriangle(53, 150, 65, 142, 65, 158, TFT_WHITE); canvas.fillRect(50, 142, 3, 16, TFT_WHITE);
  uint16_t colorePlay = inRiproduzione ? canvas.color565(0, 200, 80) : canvas.color565(255, 100, 0);
  canvas.fillCircle(120, 150, 28, colorePlay);
  if (!inRiproduzione) canvas.fillTriangle(114, 138, 114, 162, 132, 150, TFT_WHITE);
  else { canvas.fillRect(111, 138, 5, 24, TFT_WHITE); canvas.fillRect(124, 138, 5, 24, TFT_WHITE); }
  canvas.fillCircle(180, 150, 22, canvas.color565(40, 40, 40));
  canvas.fillTriangle(187, 150, 175, 142, 175, 158, TFT_WHITE); canvas.fillRect(187, 142, 3, 16, TFT_WHITE);
  canvas.pushSprite(0, 0);
}

void disegnaNotifiche() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(MC_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.5);
  canvas.setTextColor(canvas.color565(230, 0, 230)); canvas.drawString("NOTIFICHE", 120, 21);
  canvas.drawFastHLine(50, 32, 140, canvas.color565(80, 20, 80));

  if (!mostraNotifiche) {
    canvas.setTextColor(canvas.color565(150, 150, 150)); canvas.setTextSize(1.2);
    canvas.drawString("MODALITA DND", 120, 110); canvas.drawString("(Disattivate)", 120, 130);
  } 
  else if (numeroNotificheSalvate == 0) {
    canvas.setTextColor(canvas.color565(120, 120, 120)); canvas.setTextSize(1.1);
    canvas.drawString("Nessuna notifica", 120, 110);  canvas.drawString("recente.", 120, 126);
  } 
  else {
    canvas.fillRoundRect(22, 42, 196, 154, 8, canvas.color565(22, 22, 32));
    canvas.drawRoundRect(22, 42, 196, 154, 8, canvas.color565(85, 30, 95));
    canvas.setFont(&fonts::Font0); canvas.setTextSize(1); canvas.setTextColor(canvas.color565(130, 130, 150));
    String strPagine = String(indiceNotificaVisualizzata + 1) + " / " + String(numeroNotificheSalvate);
    canvas.drawString(strPagine, 120, 185);
    canvas.setTextDatum(TR_DATUM); canvas.setTextColor(canvas.color565(130, 130, 130));
    canvas.drawString(listaNotifiche[indiceNotificaVisualizzata].orario, 205, 53);
    canvas.setTextDatum(TL_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.2);
    canvas.setTextColor(canvas.color565(0, 255, 140)); 
    String mittenteTroncato = listaNotifiche[indiceNotificaVisualizzata].mittente;
    if(mittenteTroncato.length() > 14) mittenteTroncato = mittenteTroncato.substring(0, 12) + "..";
    canvas.drawString(mittenteTroncato, 32, 51);
    canvas.drawFastHLine(32, 68, 176, canvas.color565(60, 50, 75));
    canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1);
    int xInizio = 34, yInizio = 76, larghezzaMax = 172, altezzaRiga = 11, yMassima = 174;     
    int cursoreX = xInizio, cursoreY = yInizio;
    String testoRimanente = listaNotifiche[indiceNotificaVisualizzata].testo;
    while (testoRimanente.length() > 0 && cursoreY < yMassima) {
      int indiceSpazio = testoRimanente.indexOf(' ');
      String parola = (indiceSpazio != -1) ? testoRimanente.substring(0, indiceSpazio) : testoRimanente;
      testoRimanente = (indiceSpazio != -1) ? testoRimanente.substring(indiceSpazio + 1) : "";
      int larghezzaParola = canvas.textWidth(parola + " ");
      if (cursoreX + larghezzaParola > xInizio + larghezzaMax) { cursoreX = xInizio; cursoreY += altezzaRiga; }
      if (cursoreY < yMassima) { canvas.drawString(parola + " ", cursoreX, cursoreY); cursoreX += larghezzaParola; }
    }
  }
  canvas.pushSprite(0, 0);
}

void disegnaMeteo() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextDatum(MC_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.5);
  canvas.setTextColor(canvas.color565(0, 225, 120)); canvas.drawString("METEO", 120, 25);
  canvas.drawFastHLine(50, 38, 140, canvas.color565(0, 80, 40));
  canvas.setFont(&fonts::Font4); canvas.setTextSize(1.8); canvas.setTextColor(TFT_WHITE);
  String stringaGradiOggi = String(gradi);
  int larghezzaOggi = canvas.textWidth(stringaGradiOggi);
  int xInizioOggi = 120 - ((larghezzaOggi + canvas.textWidth(" C")) / 2);
  canvas.setTextDatum(TL_DATUM); canvas.drawString(stringaGradiOggi + " C", xInizioOggi, 48);
  canvas.drawCircle(xInizioOggi + larghezzaOggi + 6, 53, 3, TFT_WHITE);
  canvas.setTextDatum(MC_DATUM); canvas.setFont(&fonts::Font0); canvas.setTextSize(1.5); canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(String(meteoStatoBuffer), 120, 100);
  canvas.drawFastHLine(35, 122, 170, canvas.color565(50, 50, 50));
  int yPartenzaPrevisioni = 140;
  canvas.setFont(&fonts::Font0); canvas.setTextSize(1.2);
  time_t oraGrezza; struct tm * infoTempo; time(&oraGrezza); infoTempo = localtime(&oraGrezza);
  int giornoSettimanaOggi = infoTempo->tm_wday; 
  const char* nomiGiorni[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
  for (int i = 0; i < 3; i++) {
    int indiceGiornoFuturo = (giornoSettimanaOggi + 1 + i) % 7;
    canvas.setTextDatum(TL_DATUM); canvas.setTextColor(canvas.color565(180, 180, 180));
    canvas.drawString(nomiGiorni[indiceGiornoFuturo], 45, yPartenzaPrevisioni + (i * 22));
    canvas.setTextDatum(MC_DATUM);
    if (String(previsioniFuture[i].stato) == "SOLE") canvas.setTextColor(TFT_YELLOW);
    else if (String(previsioniFuture[i].stato) == "PIOGGIA") canvas.setTextColor(canvas.color565(100, 180, 255));
    else canvas.setTextColor(canvas.color565(150, 150, 150));
    canvas.drawString(previsioniFuture[i].stato, 120, yPartenzaPrevisioni + (i * 22) + 5);
    canvas.setTextDatum(TL_DATUM); canvas.setTextColor(TFT_WHITE);
    String stringaTempFutura = String(previsioniFuture[i].temp);
    int larghezzaTempFutura = canvas.textWidth(stringaTempFutura);
    int xInizioFutura = 175; 
    canvas.drawString(stringaTempFutura + " C", xInizioFutura, yPartenzaPrevisioni + (i * 22));
    canvas.drawCircle(xInizioFutura + larghezzaTempFutura + 3, yPartenzaPrevisioni + (i * 22) + 2, 1, TFT_WHITE);
  }
  canvas.pushSprite(0, 0);
}

void aggiornaGrafica() {
  if (schermataAttuale == 0) disegnaWatchface();
  else if (schermataAttuale == 1) disegnaImpostazioni();
  else if (schermataAttuale == 2) disegnaMusica();
  else if (schermataAttuale == 3) disegnaNotifiche();
  else if (schermataAttuale == 4) disegnaMeteo();
}

// --- ELABORAZIONE DATI ---
void elaboraStringaDatiDallApp(String data) {
  if (data.startsWith("DT:")) {
    int anno = 0, mese = 0, giorno = 0, ora = 0, minuto = 0, secondo = 0;
    int parsingSuccesso = sscanf(data.c_str(), "DT:%d-%d-%d %d:%d:%d", &anno, &mese, &giorno, &ora, &minuto, &secondo);
    if (parsingSuccesso == 6) {
      struct tm tempoImpostato;
      tempoImpostato.tm_year = anno - 1900; tempoImpostato.tm_mon = mese - 1; tempoImpostato.tm_mday = giorno;          
      tempoImpostato.tm_hour = ora; tempoImpostato.tm_min = minuto; tempoImpostato.tm_sec = secondo; tempoImpostato.tm_isdst = -1; 
      time_t tempoLocale = mktime(&tempoImpostato);
      struct timeval tv = { .tv_sec = tempoLocale, .tv_usec = 0 };
      settimeofday(&tv, NULL); rtcSincronizzato = true;
    }
  }
  else if (data.startsWith("W0:")) {
    int virgola = data.indexOf(',');
    if (virgola != -1) {
      gradi = data.substring(3, virgola).toInt();
      String tempMeteo = data.substring(virgola + 1); tempMeteo.trim();
      memset(meteoStatoBuffer, 0, sizeof(meteoStatoBuffer));
      strncpy(meteoStatoBuffer, tempMeteo.c_str(), sizeof(meteoStatoBuffer) - 1);
    }
  }
  else if (data.startsWith("W1:") || data.startsWith("W2:") || data.startsWith("W3:")) {
    int indiceGiorno = data.substring(1, 2).toInt() - 1; 
    int virgola = data.indexOf(',');
    if (virgola != -1 && indiceGiorno >= 0 && indiceGiorno < 3) {
      previsioniFuture[indiceGiorno].temp = data.substring(3, virgola).toInt();
      previsioniFuture[indiceGiorno].stato = data.substring(virgola + 1); previsioniFuture[indiceGiorno].stato.trim();
    }
  }
  else if (data.startsWith("NT:") && mostraNotifiche) {
    int divisore = data.indexOf('|');
    if (divisore != -1) {
      String m = data.substring(3, divisore); String t = data.substring(divisore + 1);
      aggiungiNuovaNotifica(m, t); forzaAccensioneSchermo();
      schermataAttuale = 0; mostraPopupNotifica = true; momentoInizioPopup = millis();
    }
  }
  richiestaAggiornamentoGrafico = true; 
}

// --- CALLBACK ASINCRONE BLE ---
class TargetServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { dispositivoConnesso = true; }
    void onDisconnect(BLEServer* pServer) { dispositivoConnesso = false; pServer->getAdvertising()->setScanFilter(true, true); pServer->getAdvertising()->start(); }
};

class NotificaCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String valoreRicevuto = String(pCharacteristic->getValue().c_str());
      if (valoreRicevuto.length() > 0) {
        if (messaggiInCoda < DIMENSIONE_CODA) {
          codaMessaggiBLE[indiceTestaCoda] = valoreRicevuto;
          indiceTestaCoda = (indiceTestaCoda + 1) % DIMENSIONE_CODA;
          messaggiInCoda++;
        }
      }
    }
};

class WatchSecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() { return 123456; }
    void onPassKeyNotify(uint32_t pass_key) {}
    bool onSecurityRequest() { return true; }
    void onAuthenticationComplete(ble_gap_conn_desc desc) { Serial.println("Autenticazione BLE completata."); }
};

// =========================================================================
// SETUP DEL SISTEMA
// =========================================================================
void setup() {
  Serial.begin(115200);
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();

  preferences.begin("watch_settings", false); 
  luminositaLivello = preferences.getInt("bright", 3); 
  indiceTimeout     = preferences.getInt("tout", 1);
  mostraNotifiche   = preferences.getBool("dnd", true);
  preferences.end(); 

  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT_PIN), touchISR, FALLING);

  // Inizializza LovyanGFX
  lcd.init(); 
  lcd.setRotation(0); 
  lcd.setBrightness(0); 

  // Creazione dello Sprite bufferizzato
  canvas.createSprite(240, 240);
  
  indirizzoMacStr = ottieniMacHardware();
  pinMode(BAT_ADC_PIN, INPUT);
  analogSetAttenuation(ADC_11db); 
  aggiornaStatoBatteria();

  schermoAcceso = true;
  disegnaWatchface();
  lcd.setBrightness(ottieniValorePWM(luminositaLivello));

  BLEDevice::init("Stait Watch");
  BLEDevice::setSecurityCallbacks(new WatchSecurityCallbacks());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new TargetServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED); 
  pCharacteristic->setCallbacks(new NotificaCallbacks());
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true); pAdvertising->setScanFilter(false, false);
  BLEDevice::startAdvertising();

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); pSecurity->setCapability(ESP_IO_CAP_NONE); pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  esp_pm_config_esp32s3_t pm_config;
  pm_config.max_freq_mhz = 240; pm_config.min_freq_mhz = 40; pm_config.light_sleep_enable = true; 
  esp_pm_configure(&pm_config);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_INT_PIN, 0); 
  ultimoMomentoAttivo = millis();
}

// =========================================================================
// LOOP LOOP PRINCIPALE
// =========================================================================
void loop() {
  if (messaggiInCoda > 0) {
    String datoDaElaborare = codaMessaggiBLE[indiceCodaCoda];
    indiceCodaCoda = (indiceCodaCoda + 1) % DIMENSIONE_CODA; messaggiInCoda--;
    ultimoMomentoAttivo = millis(); elaboraStringaDatiDallApp(datoDaElaborare);
  }

  if (richiestaAggiornamentoGrafico) { richiestaAggiornamentoGrafico = false; if (schermoAcceso) aggiornaGrafica(); }
  if (millis() - ultimoControlloBatteria > 30000) { ultimoControlloBatteria = millis(); aggiornaStatoBatteria(); if (schermoAcceso) richiestaAggiornamentoGrafico = true; }

  static unsigned long ultimoSegundo = 0;
  if (schermoAcceso && (millis() - ultimoSegundo > 1000)) { ultimoSegundo = millis(); aggiornaGrafica(); }
  if (schermataAttuale == 0 && mostraPopupNotifica && (millis() - momentoInizioPopup >= DURATA_POPUP)) { mostraPopupNotifica = false; aggiornaGrafica(); }

  uint16_t touchX, touchY;
  if (interruptTouchRilevato || inAscoltoSwipe) {
    interruptTouchRilevato = false; 
    if (lcd.getTouch(&touchX, &touchY)) {
      if (!schermoAcceso) {
        unsigned long momentoToccoAttuale = millis();
        if (!flagPrimoToccoAvvenuto) { flagPrimoToccoAvvenuto = true; ultimoToccoRilevato = momentoToccoAttuale; } 
        else {
          if (momentoToccoAttuale - ultimoToccoRilevato <= FINESTRA_DOPPIO_TOCCO) { forzaAccensioneSchermo(); aggiornaStatoBatteria(); aggiornaGrafica(); } 
          else { ultimoToccoRilevato = momentoToccoAttuale; }
        }
        delay(150); 
      } 
      else {
        ultimoMomentoAttivo = millis(); 
        if (mostraPopupNotifica) { mostraPopupNotifica = false; aggiornaGrafica(); delay(150); } 
        if (!inAscoltoSwipe) { touchInizioX = touchX; touchInizioY = touchY; inAscoltoSwipe = true; }
        touchFineX = touchX; touchFineY = touchY;
      }
    }
    else if (inAscoltoSwipe) {
      inAscoltoSwipe = false; 
      int deltaX = touchFineX - touchInizioX; int deltaY = touchFineY - touchInizioY;
      if (abs(deltaX) > SOGLIA_SWIPE || abs(deltaY) > SOGLIA_SWIPE) {
        if (schermataAttuale == 3 && abs(deltaY) > abs(deltaX) && abs(deltaY) > SOGLIA_SWIPE) {
          if (deltaY < -SOGLIA_SWIPE) { if (indiceNotificaVisualizzata < numeroNotificheSalvate - 1) indiceNotificaVisualizzata++; } 
          else if (deltaY > SOGLIA_SWIPE) { if (indiceNotificaVisualizzata > 0) indiceNotificaVisualizzata--; }
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
        aggiornaGrafica(); delay(200);
      } else {
        if (schermataAttuale == 1) { 
          if (touchInizioY >= 40 && touchInizioY <= 65) { 
            preferences.begin("watch_settings", false); luminositaLivello = (luminositaLivello % 5) + 1; 
            lcd.setBrightness(ottieniValorePWM(luminositaLivello)); preferences.putInt("bright", luminositaLivello); preferences.end();
            aggiornaGrafica(); delay(150);
          } 
          else if (touchInizioY >= 70 && touchInizioY <= 100) { 
            preferences.begin("watch_settings", false); indiceTimeout = (indiceTimeout + 1) % 3; 
            preferences.putInt("tout", indiceTimeout); preferences.end();
            aggiornaGrafica(); delay(150);
          } 
          else if (touchInizioY >= 110 && touchInizioY <= 135) { 
            preferences.begin("watch_settings", false); mostraNotifiche = !mostraNotifiche; 
            preferences.putBool("dnd", mostraNotifiche); preferences.end();
            aggiornaGrafica(); delay(150);
          }
          else if (touchInizioY >= 140 && touchInizioY <= 165 && touchInizioX >= 70 && touchInizioX <= 170) {
            eseguiUnpairDispositivi(); aggiornaGrafica(); delay(300); 
          }
        }
        else if (schermataAttuale == 2) { 
          if (touchInizioY >= 120 && touchInizioY <= 180) {
            if (touchInizioX >= 35 && touchInizioX < 85) { pCharacteristic->setValue("CMD:PREV"); pCharacteristic->notify(); }
            else if (touchInizioX >= 90 && touchInizioX <= 150) { inRiproduzione = !inRiproduzione; aggiornaGrafica(); pCharacteristic->setValue("CMD:PLAYPAUSE"); pCharacteristic->notify(); }
            else if (touchInizioX > 155 && touchInizioX <= 205) { pCharacteristic->setValue("CMD:NEXT"); pCharacteristic->notify(); }
            delay(150);
          }
        }
      }
    }
  }

  if (!schermoAcceso && flagPrimoToccoAvvenuto && (millis() - ultimoToccoRilevato > FINESTRA_DOPPIO_TOCCO)) { flagPrimoToccoAvvenuto = false; }

  if (schermoAcceso && (millis() - ultimoMomentoAttivo > timeoutOpzioni[indiceTimeout])) {
    lcd.setBrightness(0); lcd.sleep(); schermoAcceso = false; schermataAttuale = 0; mostraPopupNotifica = false; flagPrimoToccoAvvenuto = false; interruptTouchRilevato = false; inAscoltoSwipe = false;
    if (!dispositivoConnesso) { delay(50); esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_INT_PIN, 0); esp_deep_sleep_start(); } 
    else { disegnaWatchface(); }
  }
  delay(10); 
}