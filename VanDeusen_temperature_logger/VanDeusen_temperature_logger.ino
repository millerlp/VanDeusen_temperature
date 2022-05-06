/*  VanDeusen_temperature_logger.ino
 *  
 *  Designed to measure temperatures on 6 MAX31820 OneWire temperature sensors
 *  This is set up so that 3 sensors are designated as INFLOW sensors, and 3 are
 *  OUTFLOW sensors. The sensor serial numbers are hard-coded below (around line 67)
 *  and would need to be changed if sensors ever get swapped out. 
 *  The program will sample all 6 sensors once per second and write the results to 
 *  a csv file on the microSD card, as well as displaying the values on the OLED 
 *  display screen. The current csv filename is displayed at the top of the OLED 
 *  screen while running. The values are logged until power is turned off. 
 *  
 *  Note that there are two arrays below, INFLOWsensorsOffset and OUTFLOWsensorsOffset
 *  that can hold correction values to be added to each specific sensor's temperature
 *  value. Confirm that those values are appropriate for your sensors. If no correction
 *  is needed, enter 0 three times in each array. 
 *  
 *  Hardware: Arduino UNO, DS3231 real time clock, 128x64 OLED screen, microSD 
 *  breakout board, six MAX31820 OneWire temperature sensors. 
 *  
 *  
 */ 


#include "SdFat.h" // https://github.com/greiman/SdFat
#include "OneWire.h"  // For MAX31820 temperature sensor https://github.com/PaulStoffregen/OneWire
#include "DallasTemperature.h" // For MAX31820 sensors https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Wire.h>
#include "RTClib.h" // https://github.com/millerlp/RTClib
#include "SSD1306Ascii.h" // https://github.com/greiman/SSD1306Ascii
#include "SSD1306AsciiWire.h" // https://github.com/greiman/SSD1306Ascii




//******************************************
// 0X3C+SA0 - 0x3C or 0x3D for OLED screen on I2C bus
#define I2C_ADDRESS1 0x3C
SSD1306AsciiWire oled1; // create OLED display object, using I2C Wire

//*************
// Create real time clock object
RTC_DS3231 rtc;
DateTime newtime;
char buf[20]; // declare a string buffer to hold the time result

//*************
// Create sd card objects
SdFat sd;
SdFile logfile;  // for sd card, this is the file object to be written to
const byte CS_SD = 10; // define the Chip Select pin for SD card
// Declare initial name for output files written to SD card
char filename[] = "YYYYMMDD_HHMM_00.csv";

//----------------------------------------------------------
// OneWire MAX31820 temperature sensor macros and variables
// These are for the reference (unheated) mussels
#define ONE_WIRE_BUS 8  // Data pin for MAX31820s is D8 on VanDeusen temperature logger
#define TEMPERATURE_PRECISION 12 // 12-bit = 0.0625C resolution (0.75 sec update time)
// Create a OneWire object called max31820 that we will use to communicate
OneWire max31820(ONE_WIRE_BUS); 
// Pass our OneWire reference to DallasTemperature library object.
DallasTemperature refSensors(&max31820);
uint8_t numSensors; // Store total number of sensors found when scanning the bus
uint8_t numINFLOWsensors = 3; // store number of available MAX31820 outflow temperature sensors
uint8_t numOUTFLOWsensors = 3; // store number of available MAX31820 outflow temperature sensors
// Array to store MAX31820 addresses, sized for max possible number of sensors, 
// each 8 bytes long
uint8_t sensorAddr[6][8] = {}; // Up to 4 MAX31820 sensors on MusselBedHeater RevC boards
uint8_t addr[8] = {};  // Address array for MAX31820, 8 bytes long
unsigned int MAXSampleTime = 1000; // units milliseconds, time between MAX31820 readings
unsigned long prevMaxTime; 
double INFLOWtemps[3] = {0, 0, 0};  // Store each new temperature reading
double OUTFLOWtemps[3] = {0, 0, 0}; // Store each new temperature reading
double avgINFLOWtemp = 0; // Average of MAX31820 sensors on inflow
double avgOUTFLOWtemp = 0; // Average of MAX31820 sensors on outflow 

// Establish a set of known MAX31820 addresses that will be mapped to
// inflow or outflow temperatures. You need to determine the addresses using
// the MAX18B20_Temperature.ino sketch in the test_sketches directory

// Inflow sensors are tagged with green tape. List the 8-byte hex addresses here
uint8_t INFLOWsensors[3][8] = {  {0x28, 0x16, 0x95, 0x45, 0xD, 0x0, 0x0, 0xE3},
                                 {0x28, 0x36, 0xB0, 0x45, 0xD, 0x0, 0x0, 0x7F},
                                 {0x28, 0xF4, 0xAC, 0x45, 0xD, 0x0, 0x0, 0xD3}};
// Define a temperature correction to be ADDED to each raw temperature value
// The order of values in this array should match the order of sensor addresses
// in the INFLOWsensors array above. Units here are degrees Celsius.
double INFLOWsensorsOffset[3] = { 0.5, // first sensor's offset. Enter 0 if no correction is needed
                                  0.5, // second sensor's offset
                                  0.5}; // third sensor's offset
                                 
// Outflow sensors are tagged with orange tape. List the 8-byte hex addresses here
uint8_t OUTFLOWsensors[3][8] = { {0x28, 0xAA, 0xAC, 0x45, 0xD, 0x0, 0x0, 0xEE}, 
                                 {0x28, 0xA0, 0xA6, 0x45, 0xD, 0x0, 0x0, 0x9C},
                                 {0x28, 0x5E, 0x9D, 0x45, 0xD, 0x0, 0x0, 0x9}}; 
// Define a temperature correction to be ADDED to each raw temperature value
// The order of values in this array should match the order of sensor addresses
// in the OUTFLOWsensors array above. Units here are degrees Celsius.
double OUTFLOWsensorsOffset[3] = { 0.5, // first sensor's offset. Enter 0 if no correction is needed
                                   0.5, // second sensor's offset
                                   0.5}; // third sensor's offset






//*********************************************
// Setup 
//*********************************************
void setup() {

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

    // Initialize the real time clock DS3231M

  rtc.begin();  // Start the rtc object with default options
  newtime = rtc.now(); // read a time from the real time clock
  newtime.toString(buf, 20); 
  // Now extract the time by making another character pointer that
  // is advanced 10 places into buf to skip over the date. 
  char *timebuf = buf + 10;
  oled1.println();
  oled1.set2X();
  for (int i = 0; i<11; i++){
    oled1.print(buf[i]);
  }
  oled1.println();
  oled1.print(timebuf);
  delay(2000);

  bool stallFlag = true; // Used in error handling below
   if ( (newtime.year() < 2017) | (newtime.year() > 2035) ) {
    // There is an error with the clock, halt everything
    oled1.home();
    oled1.clear();
    oled1.set1X();
    oled1.println(F("RTC ERROR"));
    oled1.println(buf);
    oled1.set2X();
    oled1.println();
    Serial.println(F("Clock error"));

    // Consider removing this while loop and allowing user to plow
    // ahead without rtc (use button input?)
    while(stallFlag){
    // This permanently halts execution, no data will be collected
             
    } // end of while(stallFlag)
  } else {
    oled1.home();
    oled1.clear();
    oled1.set2X();
    oled1.println(F("RTC OKAY"));
    Serial.println(F("Clock okay"));
    Serial.println(buf);
  } // end of if ( (newtime.year() < 2017) | (newtime.year() > 2035) ) {

//*************************************************************
  // SD card setup and read (assumes Serial output is functional already)
  bool sdErrorFlag = false;
  pinMode(CS_SD, OUTPUT);  // set chip select pin for SD card to output
  // Initialize the SD card object
  // Try SPI_FULL_SPEED, or SPI_HALF_SPEED if full speed produces
  // errors on a breadboard setup. 
  if (!sd.begin(CS_SD, SPI_FULL_SPEED)) {
  // If the above statement returns FALSE after trying to 
  // initialize the card, enter into this section and
  // hold in an infinite loop.
    // There is an error with the SD card, halt everything
    oled1.home();
    oled1.clear();
    oled1.println(F("SD ERROR"));
    oled1.println();

    Serial.println(F("SD card error"));
  }  else {
    oled1.println(F("SD OKAY"));
    Serial.println(F("SD OKAY"));
  }  // end of (!sd.begin(chipSelect, SPI_FULL_SPEED))

  //---------- MAX31820 setup ---------------------------------
  // Start up the DallasTemperature library object
  refSensors.begin();  
  numSensors = refSensors.getDeviceCount();
  uint8_t addr[8]; // OneWire address array, 8 bytes long
    
  max31820.reset_search();
  for (uint8_t i = 0; i < numSensors; i++){
      max31820.search(addr); // read next sensor address into addr
      // Copy address values to sensorAddr array
      for (uint8_t j = 0; j < 8; j++){
          sensorAddr[i][j] = addr[j];
      }
      refSensors.setResolution(addr, TEMPERATURE_PRECISION);      
  }
  max31820.reset_search();
    
  // Tell the DallasTemperature library to not wait for the
  // temperature reading to complete after telling devices
  // to take a new temperature reading (so we can do other things
  // while the temperature reading is being taken by the devices).
  // You will have to arrange your code so that an appropriate
  // amount of time passes before you try to use getTempC() after
  // requestTemperatures() is used
  refSensors.setWaitForConversion(false);

  Serial.print(F("Number of MAX31820 sensors detected: "));
  Serial.println(numSensors);
  Serial.println(F("Addresses:"));
  for (byte i = 0; i < numSensors; i++){
    for (byte j = 0; j < 8; j++){
        Serial.write(' ');
        Serial.print(sensorAddr[i][j], HEX);
    }
    Serial.println();
  }

  // Tell all MAX31820s to begin taking the first temperature readings
  refSensors.requestTemperatures();

  initFileName(sd, logfile, newtime, filename); // generate a file name

  // Show the filename on the OLED screen
  oled1.clear();
  oled1.set1X();
  oled1.print(F("Filename: "));
  oled1.println();
  oled1.print(filename);
  delay(8000);
  // Print this stuff once to the OLED 
  oled1.clear();
  oled1.home();
  oled1.set2X();
  oled1.println(F(" IN   OUT"));

}

//********************************************************
// Main Loop
//*******************************************************
void loop() {


  byte goodReadings = 0;              // Number of good MAX31820 readings in a cycle
   //---------------------------------------------------------------------
    // Check to see if enough time has elapsed to read the mussel temps
    unsigned long newMillis = millis();
    // Check if enough time has elapsed to check MAX31820s
    if ( (newMillis - prevMaxTime) >= MAXSampleTime){
      prevMaxTime = newMillis; // update prevMaxTime
      newtime = rtc.now(); // update timestamp
      avgINFLOWtemp = 0; // reset average inflow temperature value
      avgOUTFLOWtemp = 0; // reset average outflow temperature value
      //------- MAX31820 readings -----------
  
      // Cycle through all INFLOW MAX31820s and read their temps
      goodReadings = 0; // reset
      Serial.print(F("INFLOW: "));
      for (byte i = 0; i < numINFLOWsensors; i++){
        //********** Faster version, using known addresses ********
        // Extract current device's 8-byte address from sensorAddr array
        for (int n = 0; n < 8; n++){
          addr[n] = INFLOWsensors[i][n];
        }
        INFLOWtemps[i] = refSensors.getTempC(addr); // query using device address to get temperature
        // Adjust the raw temperature value by adding on the offset value defined
        // at the start of the program above
        INFLOWtemps[i] = INFLOWtemps[i] + INFLOWsensorsOffset[i];
        Serial.print(INFLOWtemps[i],2); Serial.print(", ");
        // Sanity check the recorded temperature
        if ( (INFLOWtemps[i] > -10.0) & (INFLOWtemps[i] < 60.0) ){
          avgINFLOWtemp = avgINFLOWtemp + INFLOWtemps[i];
          goodReadings = goodReadings + 1;   
        } 
      }
      // Calculate average MAX31820 temperature of reference mussels
      avgINFLOWtemp = avgINFLOWtemp / (double)goodReadings;
      Serial.print(F("AVG: "));
      Serial.print(avgINFLOWtemp, 2);
      Serial.println();
      
      // Cycle through all INFLOW MAX31820s and read their temps
      goodReadings = 0; // reset
      Serial.print(F("OUTFLOW: "));
      for (byte i = 0; i < numOUTFLOWsensors; i++){
        //********** Faster version, using known addresses ********
        // Extract current device's 8-byte address from sensorAddr array
        for (int n = 0; n < 8; n++){
          addr[n] = OUTFLOWsensors[i][n];
        }
        OUTFLOWtemps[i] = refSensors.getTempC(addr); // query using device address to get temperature
        // Adjust the raw temperature value by adding on the offset value defined
        // at the start of the program above
        OUTFLOWtemps[i] = OUTFLOWtemps[i] + OUTFLOWsensorsOffset[i];
        Serial.print(OUTFLOWtemps[i],2); Serial.print(", ");
        // Sanity check the recorded temperature
        if ( (OUTFLOWtemps[i] > -10.0) & (OUTFLOWtemps[i] < 60.0) ){
          avgOUTFLOWtemp = avgOUTFLOWtemp + OUTFLOWtemps[i];
          goodReadings = goodReadings + 1;   
        } 
      }

      // Calculate average MAX31820 temperature of reference mussels
      avgOUTFLOWtemp = avgOUTFLOWtemp / (double)goodReadings;
      Serial.print(F("AVG: "));
      Serial.print(avgOUTFLOWtemp,2);
      Serial.println();
      // Start the next round of temperature readings on all sensors
      refSensors.requestTemperatures(); 

    writeToSD(newtime);
      
    // Clear the big numbers  
    oled1.set1X();
//    oled1.clear(0,128,2,3); // (c0, c1, r0, r1), returns to c0, r0 when finished
    oled1.setCursor(0,2); // Set the cursor to be on the 3rd row (count from zero)
    oled1.set2X(); // reset to 2x font
    oled1.print(avgINFLOWtemp, 2);
    oled1.set1X(); // do this to get a half-width space between the big fonts
    oled1.print(F(" "));
    oled1.set2X(); // reset to 2x font
    oled1.print(avgOUTFLOWtemp,2);
    oled1.println();
    oled1.set1X(); // set to 1x font for the individual temperature values
    // Print the inflow and outflow temps in 2 columns
    for (int i = 0; i < 3; i++){
      oled1.print(F("  "));
      oled1.print(INFLOWtemps[i],2);
      oled1.print(F("      "));
      oled1.print(OUTFLOWtemps[i],2);     
      oled1.println();
    }
    oled1.println();
    oled1.set1X();
    newtime.toString(buf, 20);
    for (int i = 0; i < 20; i++){
      oled1.print(buf[i]);
    }
     
    // Now extract the time by making another character pointer that
    // is advanced 10 places into buf to skip over the date. 
//    char *timebuf = buf + 10;
//    for (int i = 0; i<11; i++){
//      oled1.print(buf[i]);
//    }
//    oled1.println();
//    oled1.print(timebuf);    
  }


    

}





//-------------- initFileName --------------------------------------------------
// initFileName - a function to create a filename for the SD card based
// on the 4-digit year, month, day, hour, minutes and a 2-digit counter.
// The character array 'filename' was defined as a global array
// at the top of the sketch in the form "YYYYMMDD_HHMM_00_TEMPS.csv"
void initFileName(SdFat& sd, SdFile& logfile, DateTime time1, char *filename) {
    
    char buf[5];
    // integer to ascii function itoa(), supplied with numeric year value,
    // a buffer to hold output, and the base for the conversion (base 10 here)
    itoa(time1.year(), buf, 10);
    // copy the ascii year into the filename array
    for (byte i = 0; i < 4; i++){
        filename[i] = buf[i];
    }
    // Insert the month value
    if (time1.month() < 10) {
        filename[4] = '0';
        filename[5] = time1.month() + '0';
    } else if (time1.month() >= 10) {
        filename[4] = (time1.month() / 10) + '0';
        filename[5] = (time1.month() % 10) + '0';
    }
    // Insert the day value
    if (time1.day() < 10) {
        filename[6] = '0';
        filename[7] = time1.day() + '0';
    } else if (time1.day() >= 10) {
        filename[6] = (time1.day() / 10) + '0';
        filename[7] = (time1.day() % 10) + '0';
    }
    // Insert an underscore between date and time
    filename[8] = '_';
    // Insert the hour
    if (time1.hour() < 10) {
        filename[9] = '0';
        filename[10] = time1.hour() + '0';
    } else if (time1.hour() >= 10) {
        filename[9] = (time1.hour() / 10) + '0';
        filename[10] = (time1.hour() % 10) + '0';
    }
    // Insert minutes
    if (time1.minute() < 10) {
        filename[11] = '0';
        filename[12] = time1.minute() + '0';
    } else if (time1.minute() >= 10) {
        filename[11] = (time1.minute() / 10) + '0';
        filename[12] = (time1.minute() % 10) + '0';
    }
    // Insert another underscore after time
    filename[13] = '_';
    
    // Next change the counter in the filename
    // (digits 14+15) to increment count for files generated on
    // the same day. This shouldn't come into play
    // during a normal data run, but can be useful when
    // troubleshooting.
    for (uint8_t i = 0; i < 100; i++) {
        filename[14] = i / 10 + '0';
        filename[15] = i % 10 + '0';
        
        if (!sd.exists(filename)) {
            // when sd.exists() returns false, this block
            // of code will be executed to open the file
            if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
                // If there is an error opening the file, notify the
                // user. Otherwise, the file is open and ready for writing
                // Turn both indicator LEDs on to indicate a failure
                // to create the log file
                //        digitalWrite(ERRLED, !digitalRead(ERRLED)); // Toggle error led
                //        digitalWrite(GREENLED, !digitalRead(GREENLED)); // Toggle indicator led
                delay(5);
            }
            break; // Break out of the for loop when the
            // statement if(!logfile.exists())
            // is finally false (i.e. you found a new file name to use).
        } // end of if(!sd.exists())
    } // end of file-naming for loop
    //------------------------------------------------------------
    // Write 1st header line
    logfile.print(F("DateTime"));
  // write column headers for the 3 inflow mussel temperatures
  for (byte i = 1; i <=3; i++){
    logfile.print(F(",INFLOWTemp")); // column title
    logfile.print(i);   // add channel number to title
    logfile.print(F(".C")); // add units Celsius on end
  }
  logfile.print(F(",AVG_INFLOW"));
  for (byte i = 1; i <=3; i++){
    logfile.print(F(",OUTFLOWTemp")); // column title
    logfile.print(i);   // add channel number to title
    logfile.print(F(".C")); // add units Celsius on end
  }
  logfile.print(F(",AVG_OUTFLOW"));
  logfile.println();
  // Update the file's creation date, modify date, and access date.
  logfile.timestamp(T_CREATE, time1.year(), time1.month(), time1.day(), 
                    time1.hour(), time1.minute(), time1.second());
  logfile.timestamp(T_WRITE, time1.year(), time1.month(), time1.day(), 
                    time1.hour(), time1.minute(), time1.second());
  logfile.timestamp(T_ACCESS, time1.year(), time1.month(), time1.day(), 
                    time1.hour(), time1.minute(), time1.second());
  logfile.close(); // force the data to be written to the file by closing it
} // end of initFileName function


//------------- writeToSD -----------------------------------------------
// writeToSD function. This formats the available data in the
// data arrays and writes them to the SD card file in a
// comma-separated value format.
void writeToSD (DateTime timestamp) {
  // Reopen logfile. If opening fails, notify the user
  if (!logfile.isOpen()) {
    if (!logfile.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
      Serial.println(F("Could not open logfile"));
    }
  }
  // Write the unixtime
  printTimeToSD(logfile, timestamp); // human-readable 
  // Write the 3 INFLOW values in a loop
  for (byte i = 0; i < 3; i++){
    logfile.print(F(","));
    logfile.print(INFLOWtemps[i],3); 
  }
  logfile.print(F(","));
  logfile.print(avgINFLOWtemp,3);
  
  // Write the 3 OUTFLOW values in a loop
  for (byte i = 0; i < 3; i++){
    logfile.print(F(","));
    logfile.print(OUTFLOWtemps[i],3); 
  }
  logfile.print(F(","));
  logfile.print(avgOUTFLOWtemp,3);
  logfile.println();
  // logfile.close(); // force the buffer to empty

  if (timestamp.second() % 30 == 0){
      logfile.timestamp(T_WRITE, timestamp.year(),timestamp.month(), \
      timestamp.day(),timestamp.hour(),timestamp.minute(),timestamp.second());
  }
}

//---------------printTimeToSD----------------------------------------
// printTimeToSD function. This formats the available data in the
// data arrays and writes them to the SD card file in a
// comma-separated value format.
void printTimeToSD (SdFile& mylogfile, DateTime tempTime) {
    // Write the date and time in a human-readable format
    // to the file on the SD card.
    mylogfile.print(tempTime.year(), DEC);
    mylogfile.print(F("-"));
    if (tempTime.month() < 10) {
        mylogfile.print("0");
    }
    mylogfile.print(tempTime.month(), DEC);
    mylogfile.print(F("-"));
    if (tempTime.day() < 10) {
        mylogfile.print("0");
    }
    mylogfile.print(tempTime.day(), DEC);
    mylogfile.print(F(" "));
    if (tempTime.hour() < 10){
        mylogfile.print("0");
    }
    mylogfile.print(tempTime.hour(), DEC);
    mylogfile.print(F(":"));
    if (tempTime.minute() < 10) {
        mylogfile.print("0");
    }
    mylogfile.print(tempTime.minute(), DEC);
    mylogfile.print(F(":"));
    if (tempTime.second() < 10) {
        mylogfile.print("0");
    }
    mylogfile.print(tempTime.second(), DEC);
}
