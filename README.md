# VanDeusen_temperature
 Temperature logger for Vanessa Van Deusen's respirometer
 
 Uses 6 MAX31820 OneWire temperature sensors, divided into two sets of 3, to 
 measure inflow and outflow temperatures for a respirometer. Data get logged
 to a microSD card once a second along with a timestamp. 
 
 The hardware is based on an Arduino Uno, DS3231 real time clock (chronodot),
 128x64 OLED display, and microSD card adapter, all mounted to a protoshield. 

 
 ![datalogger image](./pics/VanDeusen_temperature_logger.png)