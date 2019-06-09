// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Button_Debounce.h>

// WARNING Because the RX/TX pins are used, the main trigger must be held down in order to flash!!

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* OLED Display preprocessor */

// If using software SPI (the default case):
#define OLED_MOSI   9
#define OLED_CLK   A2
#define OLED_DC    11
#define OLED_CS    12
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);



#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 


#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
/* End Oled Display PreProcessor stuff */

const int cycle_switch = A1;
const int other_cycle_switch = A3;
const int mag_switch = 8;
const int voltimeter_pin = A0;
/* End the section on pins */

BasicDebounce cycler = BasicDebounce(cycle_switch,5);
BasicDebounce magazine_in = BasicDebounce(mag_switch,50);

void update_buttons() {
  cycler.update();
  magazine_in.update();
}

void render_display();

uint8_t shots_fired = 0;

void magazine_change_render_display(BasicDebounce* button) {
  render_display();
  shots_fired = 0;
}

// Wait until millis is >= than this to refresh.
long refresh_after = 0;
void increment_shots_fired(BasicDebounce* button) {
  refresh_after = millis() + 200;
  ++shots_fired;
}

void set_up_magazine_release_to_render_display() {
  magazine_in.set_pressed_command(&magazine_change_render_display);
  magazine_in.set_released_command(&magazine_change_render_display);
}

void setup()   {   
  //Display Set up begins 
  //--------------------------------------------------             
  Serial.begin(9600);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  // init done
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(100); 
  display.setRotation(2); //rotate display
  //Display setup end
  // ----------------------------------------

  // Using pin 7 as an extra GND reference. 
  pinMode(7, OUTPUT);
  digitalWrite(7,LOW);

  // Switch inputs
  //---------------------------------------
  pinMode(cycle_switch, INPUT_PULLUP);
  pinMode(other_cycle_switch,INPUT_PULLUP);
  pinMode(mag_switch, INPUT_PULLUP);

  //---------------------------------------

  // Volt meter
  //---------------------
   pinMode(voltimeter_pin, INPUT);
  //---------------------

  // Have the mag release forcibly rerender the display
  set_up_magazine_release_to_render_display();

  cycler.AddSecondaryPin(other_cycle_switch);
  // Set up the cycle handler
  cycler.set_pressed_command(&increment_shots_fired);

  // Render at boot to avoid the adafruit logo sticking around!
  render_display();
}

float calculate_voltage() {
//http://www.electroschematics.com/9351/arduino-digital-voltmeter/
  //R1 = 68000.0;  -see text!
  //R2 =  9890.0;  -see text!
   // Instead of dividing, 
  // used a voltimeter to measure at the analog read point, 
  // and then divided the actual bat voltage by the reading to get the multiplier,
  // which is then hardcoded in.
  float value = analogRead(voltimeter_pin);
  float vout = (value * 5.0) / 1024.0; // see text
  // Our reading was (vin/vout) = 15.42/1.955
  float vin = vout * 8.17554;
  if (vin<0.09) {
    vin=0.0;//statement to quash undesired reading !
  }
  return vin;
}

unsigned long last_updated_voltage_at = 0;

void render_ammo_counter() {
    if ( !magazine_in.query()) {
      // Can either fill the circle, draw an X, or display something like C.O. in circle. (Or graphic for mag out, but that's above me. 
      // I like the X idea best, simple, and easy to understand meaning.
      display.drawLine(display.width()/2-19,display.height()/2-19,display.width()/2+19, display.height()/2+19, WHITE);
      display.drawLine(display.width()/2+19,display.height()/2-19,display.width()/2-19, display.height()/2+19, WHITE);
      shots_fired = 0;
    } else {
      // Font width is 5, height is 8, * scale factor.
      const uint8_t scale_factor = 4;
      const uint8_t font_width = 5*scale_factor;
      const uint8_t font_height = 8*scale_factor;
      const char tens_place = '0'+shots_fired/10;
      const char ones_place = '0'+shots_fired%10;
      display.drawChar(display.width()/2-(font_width)-1, display.height()/2-(font_height/2)+2,tens_place,1,0,scale_factor); // Take the center of the screen, and shift over enough for 2 chars.
      display.drawChar(display.width()/2+3, display.height()/2-(font_height/2)+2,ones_place,1,0,scale_factor); // Take the center of the screen, and shift over enough for 1 chars.
    }
     display.drawCircle(display.width()/2,display.height()/2,31, WHITE);
}

void render_battery_indicator() {
  constexpr byte num_of_cells = 4;
  constexpr float battery_min_voltage = 3.4*num_of_cells;
  constexpr float battery_full_voltage = 4.2*num_of_cells;
  static int battery_percentage = 0;
  static short voltage_to_print = 0; // Doing this to avoid float operations every single display update when printing, probably not needed though.
  // First handle updating voltage reading if enough time has passed!
    if ( millis() - last_updated_voltage_at > 512 || last_updated_voltage_at == 0 ) {
      float voltage_to_disp = calculate_voltage();
      voltage_to_print = voltage_to_disp*100;
      last_updated_voltage_at = millis();
      // Max voltage will be considered to be 8.4, and min will be 7.0 (limit of regulator, and close limit of safe lipo usage.) If using this code with a different battery, 
      // change these constants if you want an accurate battery meter!!
      // Could probably avoid float altogther if really wanted.
      battery_percentage = 20*((voltage_to_disp-battery_min_voltage)/(battery_full_voltage-battery_min_voltage));
      if ( battery_percentage > 20 ) { battery_percentage = 20; }
      if ( battery_percentage < 0 ) { battery_percentage = 0; }
    }

  // hard code in a battery icon.
  display.drawRect(4,3,3,2,1);
  display.drawRect(0,5,11,20,1);
  display.fillRect(0,5,11,20-battery_percentage,1);
  
  // Now add a voltage display.
  display.setCursor(13, 2);
  display.setTextColor(1);
  display.setTextSize(1);

  // Per cell voltage guess
  display.print((voltage_to_print/num_of_cells)/100);
  display.print('.');
  byte decimal = (voltage_to_print/num_of_cells)%100;
  if ( decimal < 10 ) {
    display.print('0');
  }
  display.print(decimal);
  display.print('V');
  // Overall voltage
  display.setCursor(90, 2);
  display.print(voltage_to_print/100);
  display.print('.');
  decimal = voltage_to_print%100;
  if ( decimal < 10 ) {
    display.print('0');
  }
  display.print(decimal);
  display.print('V');
  
}


void render_display() {
  display.clearDisplay();
  render_ammo_counter();
  render_battery_indicator();
  display.display();
}

void loop() {
  update_buttons();
  if ( millis()%500 == 0 && millis() >= refresh_after) {
    render_display();
  }
}
