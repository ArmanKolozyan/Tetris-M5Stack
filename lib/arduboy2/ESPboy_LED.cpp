/*
ESPboy LED class
for www.ESPboy.com project by RomanS
*/

#include "ESPboy_LED.h"


void ESPboyLED::begin(){
  pinMode(LEDPIN, OUTPUT);
  LEDflagOnOff = 1;
  LEDr = 0; 
  LEDg = 0; 
  LEDb = 0;
  ledset(LEDr, LEDg, LEDb);
  digitalWrite(LEDPIN, 0);
}


void ESPboyLED::off(){
  LEDflagOnOff = 0;
  digitalWrite(LEDPIN, 0);
}


void ESPboyLED::on(){
  LEDflagOnOff = 1;
  digitalWrite(LEDPIN, 1);
}


uint8_t ESPboyLED::getState(){
  return (LEDflagOnOff);
}

void ESPboyLED::setRGB (uint8_t red, uint8_t green, uint8_t blue){
  LEDr = red;
  LEDg = green;
  LEDb = blue;
  //if (LEDflagOnOff) ledset(LEDr, LEDg, LEDb);
}


void ESPboyLED::setR (uint8_t red){
  LEDr = red;
  //if (LEDflagOnOff) ledset(LEDr, LEDg, LEDb);
}


void ESPboyLED::setG (uint8_t green){
  LEDg = green;
  //if (LEDflagOnOff) ledset(LEDr, LEDg, LEDb);
}


void ESPboyLED::setB (uint8_t blue){
  LEDb = blue;
  //if (LEDflagOnOff) ledset(LEDr, LEDg, LEDb);
}


uint32_t ESPboyLED::getRGB(){
  return (((uint32_t)LEDb<<16) + ((uint32_t)LEDg<<8) + ((uint32_t)LEDr) );
}


uint8_t ESPboyLED::getR(){
  return (LEDr);
}


uint8_t ESPboyLED::getG(){
  return (LEDg);
}


uint8_t ESPboyLED::getB(){
  return (LEDb);
}


void ICACHE_RAM_ATTR ESPboyLED::ledset(uint8_t rled, uint8_t gled, uint8_t bled) {

}
