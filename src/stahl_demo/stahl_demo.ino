#include <FastLED.h>

// Konfiguration Hardware
#define LED_PIN     8          
#define NUM_LEDS    12         
#define BRIGHTNESS  255        
#define LED_TYPE    WS2812B    
#define COLOR_ORDER GRB        

CRGB leds[NUM_LEDS];

// ==========================================
// ZEIT- UND ABSTIMMUNGS-KONFIGURATION
// ==========================================
// 1. Wie lange soll der Stahl in Phase 3 komplett gelb glühen? (in Millisekunden)
const unsigned long phase3GluehDauer = 2000; // 2000 ms = 2 Sekunden

// 2. Wie lange soll das Band am Ende komplett schwarz/kalt bleiben, bevor es von vorne beginnt? (in Millisekunden)
const unsigned long neustartWartezeit = 1000; // 1000 ms = 1 Sekunde


// --- Gemeinsame Helligkeits- und Farbgrenzen ---
const uint8_t startHelligkeit = 30;
const uint8_t zielHelligkeit = 160;
const uint8_t zielFarbton = 20; // 20 = Warmes Gelb/Orange

// --- Variablen für die flüssigen Animationen ---
uint16_t flussPosition = 0; 
const uint16_t geschwindigkeitPhase1 = 15;  // Geschwindigkeit beim Befüllen
const uint16_t geschwindigkeitPhase2 = 12;  // Geschwindigkeit beim Gelb-Wechsel
const uint16_t geschwindigkeitPhase4 = 15;  // Geschwindigkeit beim Abkühlen

// Variablen für das zeitgesteuerte, extra langsame Aufdimmen von LED 0
uint8_t startDimmWert = 0;
unsigned long letzterDimmSchrittZeit = 0;
const unsigned long dimmIntervall = 40; 

// Variablen für das unregelmäßige Flackern (Perlin-Noise)
uint16_t rauschZeit = 0;
const uint16_t rauschGeschwindigkeit = 15; 
const uint8_t flackerIntensitaet = 25;     

// Status-Steuerung
int phase = 1; 
unsigned long warteStartzeit = 0;

// ==========================================
// VORWÄRTSDEKLARATIONEN (Für VS Code)
// ==========================================
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
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

void loop() {
  rauschZeit += rauschGeschwindigkeit;

  switch (phase) {
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
// FLÜSSIGE UNTERFUNKTIONEN (50 FPS / 20ms)
// ==========================================

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

// Phase 3: Stabil glühen lassen (Nutzt jetzt die obere Variable)
void halteGlutFluessig() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(zielFarbton, 255, berechneFlackern(i, zielHelligkeit));
  }

  // Abfrage angepasst auf die Konfigurationsvariable
  if (millis() - warteStartzeit >= phase3GluehDauer) {
    flussPosition = 0; 
    phase = 4; 
  }
  FastLED.show();
  delay(20); 
}

// Phase 4: Auskühlen (Nutzt jetzt die obere Variable für den Wiederanlauf)
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
    
    // Abfrage angepasst auf die Konfigurationsvariable
    delay(neustartWartezeit); 
    
    flussPosition = 0; 
    startDimmWert = 0; 
    phase = 1; 
  }
  FastLED.show();
  delay(20); 
}
