


#include <Wire.h>  // built in library, for I2C communications
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii



//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C // for oled1
//#define I2C_ADDRESS2 0x3D // for oled on alternate address

SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire


void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  Serial.println(F("Hello"));
  Serial.println();
  Wire.begin(); // Start the I2C library with default options
  //----------------------------------
  // Start up the oled displays
  oled1.begin(&Adafruit128x64, I2C_ADDRESS1);
  oled1.set400kHz();  
  oled1.setFont(Adafruit5x7);    
  oled1.clear(); 
  oled1.print(F("Hello"));
  //----------------------------------


  oled1.println();
  oled1.set2X();
  oled1.print("Screen1");


}

void loop() {
  // put your main code here, to run repeatedly:

}
