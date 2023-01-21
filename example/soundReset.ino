#include <Arduino.h>
#include <VescUart.h>
/** Initiate VescUart class */
VescUart UART;

void setup() {
  // put your setup code here, to run once:
 /** Setup Serial port to display data */
  Serial.begin(115200);

  /** Setup UART port (Serial1 on Atmega32u4) */
  Serial2.begin(115200);
  
  while (!Serial) {;}

  /** Define which ports to use as UART */
  UART.setSerialPort(&Serial2);
  UART.setDebugPort(&Serial);

}

void loop() {
//get the engine sound data ...
UART.getSoundData();
//check vesc horn button is clicked 
if (UART.is_sound_horn_triggered())
{
    // If the button is triggered, generate a horn sound
    
    //After reset the button , send command to reset .
    UART.reset_sound_triggered(FLOAT_COMMAND_HORN_RESET);
  

}

 
}