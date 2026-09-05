#include <FastLED.h>

// Konfiguration Hardware
#define LED_PIN     8          // Data Pin der die WS2812B steuert
#define TASTER_PIN  2          // Start-Taster an Pin 2 gegen GND geschaltet
#define NUM_LEDS    12         // Anzahl LED's im Streifen
#define BRIGHTNESS  255        
#define LED_TYPE    WS2812B    
#define COLOR_ORDER GRB        

#define tasterAvalible        // muss einkommentiert werden wenn eine Taster an PIN 2 gegen GND angeschloßen wird.

CRGB leds[NUM_LEDS];

// ==========================================
// ZEIT- UND ABSTIMMUNGS-KONFIGURATION
// ==========================================
const unsigned long phase3GluehDauer = 2000; // Dauer der maximalen Hitze in ms
const unsigned long neustartWartezeit = 1000; // Pause nach dem Abkühlen in ms

// --- Gemeinsame Helligkeits- und Farbgrenzen ---
const uint8_t startHelligkeit = 30;
const uint8_t zielHelligkeit = 160;
const uint8_t zielFarbton = 20; 

// --- Variablen für die flüssigen Animationen ---
uint16_t flussPosition = 0; 
const uint16_t geschwindigkeitPhase1 = 15;  
const uint16_t geschwindigkeitPhase2 = 12;  
const uint16_t geschwindigkeitPhase4 = 15;  

// Variablen für das zeitgesteuerte Aufdimmen von LED 0
uint8_t startDimmWert = 0;
unsigned long letzterDimmSchrittZeit = 0;
const unsigned long dimmIntervall = 40; 

// Variablen für das unregelmäßige Flackern (Perlin-Noise)
uint16_t rauschZeit = 0;
const uint16_t rauschGeschwindigkeit = 15; 
const uint8_t flackerIntensitaet = 25;     

// Status-Steuerung
// Phase 0 = Standby (Warten auf Taster), 1 = Rot, 2 = Gelb, 3 = Glühen, 4 = Abkühlen
int phase = 0; 
unsigned long warteStartzeit = 0;

// ==========================================
// VORWÄRTSDEKLARATIONEN (Für VS Code)
// ==========================================
void warteAufTaster();
void befuelleRotFluessig();
void schiebeGelbFluessig();
void halteGlutFluessig();
void kuehleAbFluessig();
uint8_t berechneFlackern(int ledIndex, uint8_t basisHelligkeit);

// ==========================================
// ARDUINO CORE FUNKTIONEN
// ==========================================
void setup() {
  delay(1000); 
  
  // TASTER-PIN als Eingang mit internem Pull-Up konfigurieren
  pinMode(TASTER_PIN, INPUT_PULLUP); 

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void loop() {
  rauschZeit += rauschGeschwindigkeit;

  switch (phase) {
    case 0: warteAufTaster();        break; // Neuer Standby-Modus
    case 1: befuelleRotFluessig();   break;
    case 2: schiebeGelbFluessig();   break;
    case 3: halteGlutFluessig();     break;
    case 4: kuehleAbFluessig();      break;
  }
}

// ==========================================
// HILFSFUNKTION FÜR DAS NATÜRLICHE FLACKERN
// ==========================================
uint8_t berechneFlackern(int ledIndex, uint8_t basisHelligkeit) {
  if (basisHelligkeit < 10) return basisHelligkeit; 
  uint8_t rauschWert = inoise8(ledIndex * 50, rauschZeit);
  int8_t abweichung = map(rauschWert, 0, 255, -flackerIntensitaet, 5);
  return qadd8(qsub8(basisHelligkeit, -abweichung > 0 ? -abweichung : 0), abweichung > 0 ? abweichung : 0);
}

// ==========================================
// FLÜSSIGE UNTERFUNKTIONEN
// ==========================================

// Phase 0: Standby - Wartet blockierungsfrei auf den Knopfdruck
void warteAufTaster() {
  // Durch den internen Pull-Up ist der Pin standardmäßig HIGH (1). 
  // Beim Drücken wird er mit GND verbunden und wird LOW (0).
  if (digitalRead(TASTER_PIN) == LOW) {
    delay(50); // Kleines Entprellen des mechanischen Tasters
    if (digitalRead(TASTER_PIN) == LOW) {
      phase = 1; // Starte den Hochofen-Zyklus
    }
  }
  delay(20); 
}

// Phase 1: Rotes Befüllen
void befuelleRotFluessig() {
  if (startDimmWert < startHelligkeit) {
    if (millis() - letzterDimmSchrittZeit >= dimmIntervall) {
      letzterDimmSchrittZeit = millis(); 
      startDimmWert++;                   
    }
    leds[0] = CHSV(0, 255, berechneFlackern(0, startDimmWert));
    for (int i = 1; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
  } 
  else {
    flussPosition += geschwindigkeitPhase1;
    uint8_t aktuellerPixelIndex = flussPosition >> 8; 
    uint8_t bruchteil = flussPosition & 0xFF;         

    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < aktuellerPixelIndex) {
        uint16_t pixelFortschritt = flussPosition - (i << 8);
        uint8_t dynamischeHelligkeit = map(pixelFortschritt, 0, (NUM_LEDS - 1) << 8, startHelligkeit, zielHelligkeit);
        if (dynamischeHelligkeit > zielHelligkeit) dynamischeHelligkeit = zielHelligkeit;
        leds[i] = CHSV(0, 255, berechneFlackern(i, dynamischeHelligkeit));
      } 
      else if (i == aktuellerPixelIndex) {
        uint8_t weicheHelligkeit = map(bruchteil, 0, 255, 0, startHelligkeit);
        leds[i] = CHSV(0, 255, berechneFlackern(i, weicheHelligkeit));
      } 
      else {
        leds[i] = CRGB::Black;
      }
    }

    if (aktuellerPixelIndex >= NUM_LEDS) {
      flussPosition = 0; 
      phase = 2; 
    }
  }
  FastLED.show();
  delay(20); 
}

// Phase 2: Gelb-Wechsel
void schiebeGelbFluessig() {
  flussPosition += geschwindigkeitPhase2;
  uint8_t aktuellerPixelIndex = flussPosition >> 8; 
  uint8_t bruchteil = flussPosition & 0xFF;         

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t roteHelligkeit = map(i, 0, NUM_LEDS - 1, zielHelligkeit, startHelligkeit);

    if (i < aktuellerPixelIndex) {
      leds[i] = CHSV(zielFarbton, 255, berechneFlackern(i, zielHelligkeit));
    } 
    else if (i == aktuellerPixelIndex) {
      uint8_t weicherFarbton = map(bruchteil, 0, 255, 0, zielFarbton);
      uint8_t weicheHelligkeit = map(bruchteil, 0, 255, roteHelligkeit, zielHelligkeit);
      leds[i] = CHSV(weicherFarbton, 255, berechneFlackern(i, weicheHelligkeit));
    } 
    else {
      leds[i] = CHSV(0, 255, berechneFlackern(i, roteHelligkeit));
    }
  }

  if (aktuellerPixelIndex >= NUM_LEDS) {
    phase = 3; 
    warteStartzeit = millis(); 
  }
  FastLED.show();
  delay(20); 
}

// Phase 3: Stabil glühen lassen
void halteGlutFluessig() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(zielFarbton, 255, berechneFlackern(i, zielHelligkeit));
  }

  if (millis() - warteStartzeit >= phase3GluehDauer) {
    flussPosition = 0; 
    phase = 4; 
  }
  FastLED.show();
  delay(20); 
}

// Phase 4: Auskühlen
void kuehleAbFluessig() {
  flussPosition += geschwindigkeitPhase4;
  uint8_t aktuellerPixelIndex = flussPosition >> 8; 
  uint8_t bruchteil = flussPosition & 0xFF;         

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < aktuellerPixelIndex) {
      leds[i] = CRGB::Black;
    } 
    else if (i == aktuellerPixelIndex) {
      uint8_t weicheHelligkeit = map(bruchteil, 0, 255, zielHelligkeit, 0);
      leds[i] = CHSV(zielFarbton, 255, berechneFlackern(i, weicheHelligkeit));
    } 
    else {
      leds[i] = CHSV(zielFarbton, 255, zielHelligkeit);
    }
  }

  if (aktuellerPixelIndex >= NUM_LEDS) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    // Nachlauf-Pause einhalten
    delay(neustartWartezeit); 
    
    // Variablen zurücksetzen und zurück in Phase 0 (Standby) wechseln
    flussPosition = 0; 
    startDimmWert = 0; 
    phase = 0; 
  }
  FastLED.show();
  delay(20); 
}
