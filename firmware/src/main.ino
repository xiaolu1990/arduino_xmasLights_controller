/**
 * @file main.ino
 * @author Zhangshun Lu
 * @brief mini led controller for WS2812 LED strip based on Arduino platform
 * @version 1.0.1
 * @date 2022-12-15
 *
 * @copyright Copyright (c) 2022
 *
 * @hardware connections:
 * A4 (SDA)   -   OLED SDA
 * A5 (SCL)   -   OLED SCL
 * 5V         -   OLED Vin
 * GND        -   OLED GND
 *
 * D3         -   RGB Rotaryencoder A / Normal Rotary Encoder CLK
 * D4         -   RGB Rotaryencoder B / Normal Rotary Encoder DT
 * GND        -   RGB Rotaryencoder C / Normal Rotary Encoder GND
 * D2         -   RGB Rotaryencoder Pushbutton (add ext.10k pulldown R)
 *            -   Normal Rotary Encoder SW (add ext.10k pullup R)
 * 5V         -   RGB Rotaryencoder pin5 / Normal Rotary Encoder +
 *
 * D9         -   WS2812B LED strip DATA
 * 5V         -   WS2812B LED strip Vin
 * GND        -   WS2812B LED strip GND
 *
 * A1         -   Poti middle pin
 *
 * D10        -   Buzzer + (connect a 100R series resistor between Buzzer+ and D10)
 *
 * @references
 * 1. OLED display emulator https://rickkas7.github.io/DisplayGenerator/index.html
 * 2. Buzzer play music https://create.arduino.cc/projecthub/joshi/piezo-christmas-songs-fd1ae9
 * 3. Rotary encoder https://lastminuteengineers.com/rotary-encoder-arduino-tutorial/
 * 4. WS2812 Tutorial https://www.hackster.io/bitamind/colourful-arduino-ws2811-christmas-tree-833905
 */

#include <Arduino.h>
#include <stdint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <FastLED.h>
#include <EasyButton.h>
#include "pitches.h"

/********************************/
/******** CONFIG MACROS *********/
/********************************/
// #define USE_RGB_ROTARY      // uncomment to use rotary encoder with rgb led integrated (SparkFun COM-15141)
#define MUSIC_ENABLE // uncomment to disable buzzer functions

/********************************/
/************* OLED *************/
/********************************/
#define SCREEN_WIDTH        128       // change this to adapt your display resolution
#define SCREEN_HEIGHT       32        // change this to adapt your display resolution
#define OLED_RESET          -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS      0x3C      // 0x3D for 128x64, 0x3C for 128x32

// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SSD1306AsciiWire display; // use SSD1306Ascii library to reduce the MCU resources
boolean clear_display = false;

/*********************************************/
/**** Rotary Encoder, POTI, BUZZER PINOUT ****/
/*********************************************/
#ifdef USE_RGB_ROTARY
#define ROT_B_PIN   4   // rotary B
#define ROT_A_PIN   3   // rotary A
#define ROT_SW_PIN  2   // rotary puhbutton (need an external pull down resistor)
// https://easybtn.earias.me/docs/fundamentals

EasyButton rotaryEncoderBtn(ROT_SW_PIN, 10, false, false); // active high

#else
#define ROT_SW_PIN  2   // normal encoder module pushbutton pin (need external pull up resistor)
#define ROT_CLK_PIN 3
#define ROT_DT_PIN  4

EasyButton rotaryEncoderBtn(ROT_SW_PIN, 10, false, true); // active low
#endif

#define POT_PIN     A1    // potentiometer connect to Nano pin A1
#define BUZZER_PIN  10    // buzzer + pin

/*********************************************/
/************* WS2812B LED STRIP *************/
/*********************************************/
#define LED_DAT_PIN     9 // WS2812B DATA PIN
#define MAX_BRIGHTNESS  128
#define MIN_BRIGHTNESS  8
#define NUM_LEDS        67 // change this to apply the total number of leds on the strip

CRGB leds[NUM_LEDS];

// variable from poti reading to adjust brightness in solid color mode
uint16_t pot_val = 0;

typedef enum PatternEffect
{
  NO_PATTERN,
  TWINKLE,
  BREATHE,
  COMET,
  RAINBOW_1,
  RAINBOW_2
} PatternEffect_t;

PatternEffect_t pattern = NO_PATTERN;

// set variables for twinkle effect
#define NUM_COLORS 5

static const CRGB TwinkleColors[NUM_COLORS] =
    {
        CRGB::Red,
        CRGB::Blue,
        CRGB::Purple,
        CRGB::Green,
        CRGB::Orange};

// set variables for breathing effect
static float pulse_speed = 0.5; // larger value gives faster pulse
float hue_val_min = 120.0;      // pulse minimum value
uint8_t hue_start = 15;         // start hue at hue_val_min
uint8_t sat_start = 230;        // start saturation at val_min

float hue_val_max = 255.0; // pulse maximum value
uint8_t hue_stop = 95;     // end hue at hue_val_max
uint8_t sat_stop = 255;    // end saturation at val_max

uint8_t hue = hue_start;
uint8_t sat = sat_start;
float hue_val = hue_val_min;
uint8_t hue_delta = hue_start - hue_stop;
static float pulse_delta = (hue_val_max - hue_val_min) / 2.35040238; // DO NOT EDIT

// set variables for rainbow effect
uint8_t hue_rainbow = 0;

/*********************************************/
/************ USER MACROS, VARs *************/
/*********************************************/
volatile int8_t rotary_counter = 0;     // current "position" of rotary encoder (increments CW)
volatile int8_t counter_mapped = 0;     // mapped current "position" of rotary encoder (increments CW)
volatile boolean rotary_change = false; // will turn true if rotary_counter has changed

#define LONG_PRESS_THRES 1000L // threshold value for detecting long press (1s)

volatile boolean short_press = false;
volatile boolean long_press = false;

typedef enum ButtonStatus
{
  UNPRESSED,
  SHORT_PRESS,
  LONG_PRESS
} ButtonStatus_t;

ButtonStatus_t btn_status = UNPRESSED;

typedef struct Menu
{
  uint8_t level; // menu hierarchy
  uint8_t idx;   // navigation id in the menu list
} Menu_t;

Menu_t menu = {0, 0};

#define MODE_OFF 0
#define MODE_SOLID 1
#define MODE_PATTERN 2

#ifdef MUSIC_ENABLE
#define MODE_MUSIC 3
#endif

uint8_t mode = MODE_OFF;

/*********************************************/
/*********** MUSIC FUNCTION VARs *************/
/*********************************************/
uint8_t song = 0;
float noteDuration;

// Jingle Bells
uint16_t bell_melody[26] = {
    NOTE_E5, NOTE_E5, NOTE_E5,
    NOTE_E5, NOTE_E5, NOTE_E5,
    NOTE_E5, NOTE_G5, NOTE_C5, NOTE_D5,
    NOTE_E5,
    NOTE_F5, NOTE_F5, NOTE_F5, NOTE_F5,
    NOTE_F5, NOTE_E5, NOTE_E5, NOTE_E5, NOTE_E5,
    NOTE_E5, NOTE_D5, NOTE_D5, NOTE_E5,
    NOTE_D5, NOTE_G5};

uint8_t bell_tempo[26] = {
    8, 8, 4,
    8, 8, 4,
    8, 8, 8, 8,
    2,
    8, 8, 8, 8,
    8, 8, 8, 16, 16,
    8, 8, 8, 8,
    4, 4};

// We wish you a merry Christmas
uint16_t wish_melody[30] = {
    NOTE_B3,
    NOTE_F4, NOTE_F4, NOTE_G4, NOTE_F4, NOTE_E4,
    NOTE_D4, NOTE_D4, NOTE_D4,
    NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_F4,
    NOTE_E4, NOTE_E4, NOTE_E4,
    NOTE_A4, NOTE_A4, NOTE_B4, NOTE_A4, NOTE_G4,
    NOTE_F4, NOTE_D4, NOTE_B3, NOTE_B3,
    NOTE_D4, NOTE_G4, NOTE_E4,
    NOTE_F4};

uint8_t wish_tempo[30] = {
    4,
    4, 8, 8, 8, 8,
    4, 4, 4,
    4, 8, 8, 8, 8,
    4, 4, 4,
    4, 8, 8, 8, 8,
    4, 4, 8, 8,
    4, 4, 4,
    2};

// Santa Claus is coming to town
uint16_t santa_melody[28] = {
    NOTE_G4,
    NOTE_E4, NOTE_F4, NOTE_G4, NOTE_G4, NOTE_G4,
    NOTE_A4, NOTE_B4, NOTE_C5, NOTE_C5, NOTE_C5,
    NOTE_E4, NOTE_F4, NOTE_G4, NOTE_G4, NOTE_G4,
    NOTE_A4, NOTE_G4, NOTE_F4, NOTE_F4,
    NOTE_E4, NOTE_G4, NOTE_C4, NOTE_E4,
    NOTE_D4, NOTE_F4, NOTE_B3,
    NOTE_C4};

uint8_t santa_tempo[28] = {
    8,
    8, 8, 4, 4, 4,
    8, 8, 4, 4, 4,
    8, 8, 4, 4, 4,
    8, 8, 4, 2,
    4, 4, 4, 4,
    4, 2, 4,
    1};

void setup()
{
  Serial.begin(9600);
  /* OLED SETUP */
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  // {
  //   Serial.println(F("SSD1306 allocation failed"));
  //   for(;;);
  // }
  Wire.begin();
  Wire.setClock(400000L);
  display.begin(&Adafruit128x32, SCREEN_ADDRESS);

  // Show initial display with an Adafruit splash screen after power on
  // display.display();
  // delay(1000);
  // display.setTextColor(WHITE);
  // Show the default welcome page
  showWelcome();

/* RotaryEncoder SETUP */
#ifdef USE_RGB_ROTARY
  pinMode(ROT_B_PIN, INPUT_PULLUP);
  pinMode(ROT_A_PIN, INPUT_PULLUP);
#else
  pinMode(ROT_CLK_PIN, INPUT);
  pinMode(ROT_DT_PIN, INPUT);
#endif

  rotaryEncoderBtn.begin();
  if (rotaryEncoderBtn.supportsInterrupt())
  {
    rotaryEncoderBtn.enableInterrupt(buttonISR);
  }

  rotaryEncoderBtn.onPressed(shortPressCB);
  rotaryEncoderBtn.onPressedFor(LONG_PRESS_THRES, longPressCB);

  attachInterrupt(1, rotaryIRQ, CHANGE);

  // attachInterrupt(0, buttonIRQ, CHANGE);

  /* LED strip SETUP */
  FastLED.addLeds<WS2812B, LED_DAT_PIN, GRB>(leds, NUM_LEDS); // GRB ordering is typical
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setCorrection(TypicalPixelString);

  /* Buzzer SETUP */
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop()
{
  btn_status = getButtonStatus();

  if ((rotary_change) && (menu.level != 0))
  {
    display.clear();
    rotary_change = false;
  }

  menu = setMenu(menu, btn_status);
}

/**
 * @brief rotary encoder push button ISR
 *
 */
void buttonISR()
{
  rotaryEncoderBtn.read();
}

/**
 * @brief rotary encoder push button short press ISR callback
 *
 */
void shortPressCB()
{
  short_press = true;
  long_press = false;
}

/**
 * @brief rotary encoder push button long press ISR callback
 *
 */
void longPressCB()
{
  short_press = true;
  long_press = true;
}

/**
 * @brief ISR for rotary encoder rotation interrupt
 *
 */
void rotaryIRQ()
{
  static uint8_t rotary_state = 0;
  rotary_state <<= 2; // remember previous state
#ifdef USE_RGB_ROTARY
  rotary_state |= (digitalRead(ROT_A_PIN) | (digitalRead(ROT_B_PIN) << 1)); // mask in current state
#else
  rotary_state |= (digitalRead(ROT_DT_PIN) | (digitalRead(ROT_CLK_PIN) << 1)); // mask in current state
#endif
  rotary_state &= 0x0F; // current state

  if (rotary_state == 0x09)
  {
#ifdef USE_RGB_ROTARY
    rotary_counter++;
#else
    rotary_counter--;
#endif

    rotary_change = true;
  }
  else if (rotary_state == 0x03)
  {
#ifdef USE_RGB_ROTARY
    rotary_counter--;
#else
    rotary_counter++;
#endif

    rotary_change = true;
  }

  counter_mapped = map(rotary_counter, -128, 127, -43, 42);
  // counter_mapped = map(rotary_counter, -128, 127, -26, 25);
}

/**
 * @brief Get the Rotary Encoder Button Status
 *
 */
ButtonStatus getButtonStatus()
{
  ButtonStatus status;

  // update() function must be called repeatedly since we use interrupt to detect long press
  rotaryEncoderBtn.update();

  if (short_press)
  {
    status = SHORT_PRESS;
    if (long_press)
    {
      status = LONG_PRESS;
      long_press = false;
    }

    short_press = false;

    display.clear();
  }
  else
  {
    status = UNPRESSED;
  }

  return status;
}

/**
 * @brief OLED displays welcome screen
 *
 */
void showWelcome()
{
  // https://github.com/greiman/SSD1306Ascii/issues/53
  display.set1X();
  display.setCursor(5, 1);
  display.print("*");
  display.setCursor(10, 2);
  display.print("*");
  display.setCursor(120, 1);
  display.print("*");
  display.setCursor(115, 2);
  display.print("*");
  display.setCursor(28, 1);
  // display.setTextSize(2);
  display.setFont(lcd5x7);
  display.set2X();
  display.print("Welcome");
  display.set1X();
  display.setCursor(20, 3);
  display.print("Press to set Mode");
  display.set2X();
}

/**
 * @brief OLED displays the mode select menu picker
 *
 * @param id index of mode selector
 */
void showMenuModePicker(uint8_t id)
{
  if (id == 0) // Solid Menu Page
  {
    display.setCursor(34, 1);
    display.print("Solid");
  }
  else if (id == 1) // Pattern Menu Page
  {
    display.setCursor(28, 1);
    display.print("Pattern");
  }
  else if (id == 2) // Music Menu Page
  {
    display.setCursor(34, 1);
    display.print("Music");
  }
}

/**
 * @brief OLED displays the menu of selecting colors for solid effect
 *
 * @param id color index picker
 */
void showMenuSolidColorPicker(uint8_t id)
{
  if (id == 0) // Set Tomato
  {
    display.setCursor(28, 1);
    display.print("Tomato");
  }
  else if (id == 1) // Set Steelblue
  {
    display.setCursor(10, 1);
    display.print("Steelblue");
  }
  else if (id == 2) // Set Teal
  {
    display.setCursor(40, 1);
    display.print("Teal");
  }
  else if (id == 3) // Set Violet
  {
    display.setCursor(28, 1);
    display.print("Violet");
  }
  else if (id == 4) // Set Snow
  {
    display.setCursor(40, 1);
    display.print("Snow");
  }
  else if (id == 5) // Back to main
  {
    display.setCursor(40, 1);
    display.print("Back");
  }
}

/**
 * @brief OLED displays menu of selecting different patterns
 *
 * @param id patterns identifier
 */
void showMenuPatternPicker(uint8_t id)
{
  if (id == 0) // twinkle pattern
  {
    display.setCursor(22, 1);
    display.print("Twinkle");
  }
  else if (id == 1) // breathe pattern
  {
    display.setCursor(22, 1);
    display.print("Breathe");
  }
  else if (id == 2) // comet pattern
  {
    display.setCursor(34, 1);
    display.print("Comet");
  }
  else if (id == 3) // rainbow pattern 1
  {
    display.setCursor(10, 1);
    display.print("Rainbow 1");
  }
  else if (id == 4) // rainbow pattern 2
  {
    display.setCursor(10, 1);
    display.print("Rainbow 2");
  }
  else if (id == 5) // Back to main
  {
    display.setCursor(40, 1);
    display.print("Back");
  }
}

/**
 * @brief OLED displays menu of selecting which songs to play
 *
 * @param id
 */
void showMenuSongsPicker(uint8_t id)
{
  if (id == 0) // jingle bells
  {
    display.setCursor(4, 1);
    display.print("JingleBell");
  }
  else if (id == 1) // Santa Claus
  {
    display.setCursor(4, 1);
    display.print("SantaClaus");
  }
  else if (id == 2) // Merry Christmas
  {
    display.setCursor(10, 1);
    display.print("MerryXmas");
  }
  else if (id == 3) // Back to main
  {
    display.setCursor(40, 1);
    display.print("Back");
  }
}

/**
 * @brief Set the Menu object
 *
 * @param m menu object
 * @param st rotary encoder status
 * @return Menu_t
 */
Menu_t setMenu(Menu_t m, ButtonStatus_t st)
{
  if (st == LONG_PRESS) // Turn LED off by long pressing knob
  {
    setLedOff();
    // reinitialize everything
    m.level = 0;
    pattern = NO_PATTERN;
    mode = MODE_OFF;
    rotary_counter = 0;
    song = 0; // stop music
    display.clear();

    return m;
  }

  if (m.level == 0)
  {
    showWelcome();

    if (st == SHORT_PRESS)
    {
      m.level++;

      display.clear();

      showMenuModePicker(0);

      rotary_counter = 0;
    }
  }

  else if (m.level == 1)
  {

    m.idx = rotary_counter % 3;

    showMenuModePicker(m.idx);

    if (st == SHORT_PRESS)
    {
      m.level++;

      rotary_counter = 0;

      if (m.idx == 0)
      {
        showMenuSolidColorPicker(0);

        mode = MODE_SOLID;
      }

      else if (m.idx == 1)
      {
        showMenuPatternPicker(0);

        mode = MODE_PATTERN;
      }

      else if (m.idx == 2)
      {
        showMenuSongsPicker(0);

        mode = MODE_MUSIC;
      }
    }
  }

  else if ((m.level == 2) && (mode == MODE_SOLID))
  {
    m.idx = rotary_counter % 6;

    showMenuSolidColorPicker(m.idx);

    // adjust led strip max brightness from potentiometer reading

    pot_val = analogRead(POT_PIN);

    // Serial.println(pot_val>>2);

    // use only half of the max. brightness as upper threshold to reduce power

    FastLED.setBrightness(pot_val >> 3);

    if (st == SHORT_PRESS)
    {
      if (m.idx == 0)
      {
        setLedSolidTomato();
      }

      else if (m.idx == 1)
      {
        setLedSolidSteelBlue();
      }

      else if (m.idx == 2)
      {
        setLedSolidTeal();
      }

      else if (m.idx == 3)
      {
        setLedSolidViolet();
      }

      else if (m.idx == 4)
      {
        setLedSolidSnow();
      }

      else if (m.idx == 5)
      {
        // back to welcome screen, initialize everything
        m.level = 0;

        mode = MODE_OFF;

        pattern = NO_PATTERN;

        rotary_counter = 0;

        display.clear();
      }
    }

    FastLED.show();
  }

  else if ((m.level == 2) && (mode == MODE_PATTERN))
  {
    m.idx = rotary_counter % 6;

    showMenuPatternPicker(m.idx);

    if (st == SHORT_PRESS)
    {
      if (m.idx == 0)
      {
        pattern = TWINKLE;
      }

      else if (m.idx == 1)
      {
        pattern = BREATHE;
      }

      else if (m.idx == 2)
      {
        pattern = COMET;
      }

      else if (m.idx == 3)
      {
        pattern = RAINBOW_1;
      }

      else if (m.idx == 4)
      {
        pattern = RAINBOW_2;
      }

      else if (m.idx == 5)
      {
        // back to welcome screen, initialize everything
        m.level = 0;

        mode = MODE_OFF;

        pattern = NO_PATTERN;

        rotary_counter = 0;

        display.clear();
      }
    }

    setLedPattern(pattern);
  }

  else if ((m.level == 2) && (mode == MODE_MUSIC))
  {
    m.idx = rotary_counter % 4;

    showMenuSongsPicker(m.idx);

    if (st == SHORT_PRESS)
    {
      if (m.idx != 3)
      {
        song = m.idx + 1;
      }

      else
      {
        // back to welcome screen, initialize everything
        m.level = 0;
        mode = MODE_OFF;
        pattern = NO_PATTERN;
        rotary_counter = 0;

        display.clear();
      }
    }

    setLedPatternWithMusic(song);
  }

  return m;
}

/**
 * @brief Turn off the LED strip
 *
 */
void setLedOff()
{
  FastLED.clear();

  FastLED.show();
}

void setLedSolidTomato()
{
  fill_solid(leds, NUM_LEDS, CRGB::Tomato);
}

void setLedSolidSteelBlue()
{
  fill_solid(leds, NUM_LEDS, CRGB::SteelBlue);
}

void setLedSolidTeal()
{
  fill_solid(leds, NUM_LEDS, CRGB::Teal);
}

void setLedSolidViolet()
{
  fill_solid(leds, NUM_LEDS, CRGB::Violet);
}

void setLedSolidSnow()
{
  fill_solid(leds, NUM_LEDS, CRGB::Snow);
}

/**
 * @brief Set the Led Pattern from the given parameters
 *
 * @param pattern
 */
void setLedPattern(PatternEffect_t pattern)
{
  switch (pattern)
  {
  case NO_PATTERN:
    // do nothing
    break;

  case TWINKLE:
    setLedPatternTwinkle();
    break;

  case BREATHE:
    setLedPatternBreathe();
    break;

  case COMET:
    setLedPatternComet();
    break;

  case RAINBOW_1:
    setLedPatternRainbow1();
    break;

  case RAINBOW_2:
    setLedPatternRainbow2();
    break;

  default:
    break;
  }
}

/**
 * @brief Set the Led Pattern Rainbow effect 1
 * https://www.youtube.com/watch?v=FQpXStjJ4Vc
 */
void setLedPatternRainbow1()
{
  for (uint16_t i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(hue_rainbow, 255, 128);
  }

  EVERY_N_MILLISECONDS(15)

  {
    hue_rainbow++;
  }

  FastLED.show();
}

/**
 * @brief Set the Led Pattern Rainbow effect 2
 * https://www.youtube.com/watch?v=FQpXStjJ4Vc
 */
void setLedPatternRainbow2()
{
  for (uint16_t i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(hue_rainbow + (i * 10), 255, 128);
  }

  EVERY_N_MILLISECONDS(15)

  {
    hue_rainbow++;
  }

  FastLED.show();
}

/**
 * @brief Set the Led Pattern Breathe
 * https://codebender.cc/sketch:99494#FastLed%20Breath.ino
 * https://thingpulse.com/breathing-leds-cracking-the-algorithm-behind-our-breathing-pattern/
 * https://github.com/marmilicious/FastLED_examples/blob/master/breath_effect_v2.ino
 */
void setLedPatternBreathe()
{
  float dV = (exp(sin(pulse_speed * millis() / 2000.0 * PI)) - 0.36787944) * pulse_delta;

  hue_val = hue_val_min + dV;

  hue = map(hue_val, hue_val_min, hue_val_max, hue_start, hue_stop);

  sat = map(sat, hue_val_min, hue_val_max, sat_start, sat_stop);

  for (uint16_t i = 0; i < NUM_LEDS; i++)

  {
    leds[i] = CHSV(hue, sat, hue_val);
    leds[i].r = dim8_video(leds[i].r);
    leds[i].g = dim8_video(leds[i].g);
    leds[i].b = dim8_video(leds[i].b);
  }

  FastLED.show();
}

/**
 * @brief Set the Led Pattern Twinkle effect
 * https://www.youtube.com/watch?v=yM5dY7K2KHM
 */
void setLedPatternTwinkle()
{
  static uint16_t pass_count = 0;

  pass_count++;

  if (pass_count == NUM_LEDS)
  {
    pass_count = 0;
    FastLED.clear(false);
  }

  leds[random(NUM_LEDS)] = TwinkleColors[random(NUM_COLORS)];

  FastLED.show();

  delay(100);
}

/**
 * @brief Set the Led Pattern Comet effect
 *
 */
void setLedPatternComet()
{
  const byte fade_amount = 128; // should be a fraction of 256
  const uint8_t comet_size = 5;
  const uint8_t delta_hue = 4;
  static byte hue = HUE_RED;
  static int8_t i_dir = 1;  // current direction, either -1 or 1
  static uint8_t i_pos = 0; // current comet position on strip

  hue += delta_hue;

  i_pos += i_dir;

  if (i_pos == (NUM_LEDS - comet_size) || i_pos == 0)
  {
    i_dir *= -1;
  }

  for (uint8_t i = 0; i < comet_size; i++)
  {
    leds[i_pos + i].setHue(hue);
  }

  for (uint8_t j = 0; j < NUM_LEDS; j++)
  {
    leds[j] = leds[j].fadeToBlackBy(fade_amount);
  }

  FastLED.show();
}

/**
 * @brief discover the appropriate calibration profile for the led strip
 *        the one that matches most closely to white color is the profile
 *        we used to calibrate
 */
void calibLedStrip()
{
  fill_solid(leds, NUM_LEDS, CRGB::White);
  Serial.println("Profile: UncorrectedColor");
  FastLED.setCorrection(UncorrectedColor);
  FastLED.show();
  delay(2000);

  Serial.println("Profile: TypicalLEDStrip");
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.show();
  delay(2000);

  Serial.println("Profile: TypicalPixelString");
  FastLED.setCorrection(TypicalPixelString);
  FastLED.show();
  delay(2000);

  Serial.println("Profile: TypicalSMD5050");
  FastLED.setCorrection(TypicalSMD5050);
  FastLED.show();
  delay(2000);
}

/******************* Music Part ******************/
void setLedPatternWithMusic(uint8_t s)
{
  if (s == 1)
  {
    // Serial.println("Jingle Bells");
    for (int thisNote = 0; thisNote < 26; thisNote++)
    {
      if (rotary_change)
      {
        song = 0;
        FastLED.clear();
        FastLED.show();
        break;
      }

      // led pattern
      fadeToBlackBy(leds, NUM_LEDS, 32); // first fade all
      leds[random16(NUM_LEDS)] += CRGB::DarkGoldenrod;
      // to calculate the note duration, take one second
      // divided by the note type.
      // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
      noteDuration = 1000.0 / bell_tempo[thisNote];
      buzz(BUZZER_PIN, bell_melody[thisNote], noteDuration);
      // to distinguish the notes, set a minimum time between them.
      // the note's duration + 30% seems to work well:
      float pauseBetweenNotes = noteDuration * 1.30;

      delay(pauseBetweenNotes);

      // stop the tone playing:
      buzz(BUZZER_PIN, 0, noteDuration);

      // show LED
      FastLED.show();
    }
  }

  else if (s == 2)
  {
    // Serial.println(" 'Santa Claus is coming to town'");
    for (int thisNote = 0; thisNote < 28; thisNote++)
    {
      if (rotary_change)
      {
        song = 0;
        FastLED.clear();
        FastLED.show();
        break;
      }
      // led pattern
      fadeToBlackBy(leds, NUM_LEDS, 32); // first fade all
      leds[random16(NUM_LEDS)] += CRGB::Wheat;
      // to calculate the note duration, take one second
      // divided by the note type.
      // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
      noteDuration = 900.0 / santa_tempo[thisNote];
      buzz(BUZZER_PIN, santa_melody[thisNote], noteDuration);
      // to distinguish the notes, set a minimum time between them.
      // the note's duration + 30% seems to work well:
      float pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);

      // stop the tone playing:
      buzz(BUZZER_PIN, 0, noteDuration);

      // show LED
      FastLED.show();
    }
  }

  else if (s == 3)
  {
    // Serial.println(" 'We wish you a Merry Christmas'");
    for (int thisNote = 0; thisNote < 30; thisNote++)
    {
      if (rotary_change)
      {
        song = 0;
        FastLED.clear();
        FastLED.show();
        break;
      }

      // led pattern
      fadeToBlackBy(leds, NUM_LEDS, 32); // first fade all"
      leds[random16(NUM_LEDS)] += CRGB::PaleVioletRed;
      // to calculate the note duration, take one second
      // divided by the note type.
      // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
      noteDuration = 1000.0 / wish_tempo[thisNote];
      buzz(BUZZER_PIN, wish_melody[thisNote], noteDuration);
      // to distinguish the notes, set a minimum time between them.
      // the note's duration + 30% seems to work well:"
      float pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);

      // stop the tone playing:
      buzz(BUZZER_PIN, 0, noteDuration);
      // show LED
      FastLED.show();
    }
  }

  else
  {
    // do nothing
  }
}

void buzz(int targetPin, float pitch, float duration)
{
  float delayValue = 1000000 / pitch / 2;

  float numCycles = pitch * duration / 1000;

  for (float i = 0; i < numCycles; i++)
  {
    digitalWrite(targetPin, HIGH);  // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue);  // wait for the calculated delay value
    digitalWrite(targetPin, LOW);   // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue);  // wait again or the calculated delay value
  }
}
