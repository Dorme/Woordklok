/* 
 * Javelin Word Clock
 * 
 * This clock is built to display spelled out words depending on the time of day.
 * The code depends on both an RTC and Addressable RGB LEDs
 * 
 * RTC Chip used: DS1307 and connect via I2C interface (pins SCL & SDA)
 * RGB LEDs used: WS2812B LEDs on a 5v strip connected to pin 6
 *
 * To set the RTC for the first time you have to send a string consisting of
 * the letter T followed by ten digit time (as seconds since Jan 1 1970) Also known as EPOCH time.
 *
 * You can send the text "T1357041600" on the next line using Serial Monitor to set the clock to noon Jan 1 2013  
 * Or you can use the following command via linux terminal to set the clock to the current time (UTC time zone)
 * date +T%s > /dev/ttyACM0
 * Inside the processSyncMessage() function I'm offsetting the UTC time with Central time.
 * If you want the clock to be accurate for your time zone, you may need to update the value.
 */
#include <Adafruit_NeoPixel.h>
#include <Time.h>
#include <Wire.h>  
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
 
#define RGBLEDPIN    6
#define FWDButtonPIN 8
#define REVButtonPIN 9
#define LED13PIN     13

#define N_LEDS 121 // 11 x 11 grid
#define TIME_HEADER  "T"   // header tag for serial time sync message
#define BRIGHTNESSDAY 100 // full on (max 255)
#define BRIGHTNESSNIGHT 50 // half on

Adafruit_NeoPixel grid = Adafruit_NeoPixel(N_LEDS, RGBLEDPIN, NEO_GRB + NEO_KHZ800);

// globale vars om het spel niet te doen ontploffen
int intBrightness = BRIGHTNESSDAY; // the brightness of the clock (0 = off and 255 = 100%)
int intTestMode; // set when both buttons are held down
String strTime = ""; // used to detect if word time has changed
int intTimeUpdated = 0; // used to tell if we need to turn on the brightness again when words change

// kleurtjes!
uint32_t colorWhite = grid.Color(255, 255, 255);
uint32_t colorBlack = grid.Color(0, 0, 0);
uint32_t colorRed = grid.Color(255, 0, 0);
uint32_t colorGreen = grid.Color(0, 255, 0);
uint32_t colorBlue = grid.Color(0, 0, 255);

// de woorden worden gemaakt vanaf rechtsonder (0) tot linksboven (120) in zigzag beweging
int arrHET[] = {120,119,118,-1};
int arrIS[] = {116,115,-1};
int arrTIEN_TOP[] = {113,112,111,110,-1};
int arrVIJF_TOP[] = {99,100,101,102,-1};
int arrVIJFENTWINTIG[] = {99,100,101,102,103,104,98,97,96,95,94,93,92,-1};
int arrEN[] = {103,104,-1};
int arrHALF[] = {106,107,108,109,-1};
int arrTWINTIG[] = {98,97,96,95,94,93,92,-1};
int arrKWART[] = {77,78,79,80,81,-1};
int arrNA[] = {84,85,-1};
int arrHAPPY[] = {87,65,66,44,43,-1};
int arrVOOR[] = {76,75,74,73,-1};
int arrEEN[] = {69,68,67,-1};
int arrTWEE[] = {71,70,69,68,-1};
int arrDRIE[] = {55,56,57,58,-1};
int arrVIER[] = {60,61,62,63,-1};
int arrVIJF[] {54,53,52,51,-1};
int arrZEVEN[] = {49,48,47,46,45,-1};
int arrBIRTHDAY[] = {36,37,38,39,40,41,42,43,-1};
int arrZES[] = {32,31,30,-1};
int arrACHT[] = {29,28,27,26,-1};
int arrELF[] = {24,23,22,-1};
int arrNEGEN[] = {11,12,13,14,15,-1};
int arrTIEN[] = {17,18,19,20,-1};
int arrTWAALF[] = {5,6,7,8,9,10,-1};
int arrUUR[] = {0,1,2,-1};
int arrBART[] = {43,79,83,39,-1};
int arrBIEKE[] = {43,112,24,77,12,-1};
int arrLIEN[] = {34,88,103,25,-1};
int arrKENNY[] = {77,69,95,11,43,-1};

// print out the software version number
void printVersion(void) {
  Serial.println("+--------------------------+");
  Serial.println("| WordClock - Arduino v0.1 |");
  Serial.println("+--------------------------+");
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, uint8_t wait) {
  for(uint16_t i=0; i<grid.numPixels(); i++) {
      grid.setPixelColor(i, color);
  }
  grid.show();
  delay(wait);
}

void spellWord(int arrWord[], uint32_t intColor){
  for(int i = 0; i < grid.numPixels() + 1; i++){
    if(arrWord[i] == -1){
      break;
    }else{
      grid.setPixelColor(arrWord[i],intColor);
      grid.show();
      delay(500);
    }
  }
}

void fadeOut(int time){
  for (int i = intBrightness; i > 0; --i){
    grid.setBrightness(i);
    grid.show();
    delay(time);
  }
}

void fadeIn(int time){
  for(int i = 1; i < intBrightness; ++i){
    grid.setBrightness(i);
    grid.show();
    delay(time);  
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return grid.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return grid.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return grid.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

void paintWord(int arrWord[], uint32_t intColor){
  for(int i = 0; i < grid.numPixels() + 1; i++){
    if(arrWord[i] == -1){
      grid.show();
      break;
    }else{
      grid.setPixelColor(arrWord[i],intColor);
    }
  }
}

unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     pctime = pctime - 18000;
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
      Serial.println("Tijd niet correct! FAIL!");
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
     Serial.println();
     Serial.println("Tijd gezet via seriële verbinding, good job!");
     Serial.println();
  }
  return pctime;
}

// runs throught the various displays, testing
void test_grid(){
  printVersion();
  colorWipe(colorBlack, 0);
  spellWord(arrHAPPY, colorGreen);
  delay(1000);
  fadeOut(50);
  grid.setBrightness(intBrightness);
  paintWord(arrBIRTHDAY, colorGreen);
  delay(1000);
  fadeOut(50);
  grid.setBrightness(intBrightness);
  colorWipe(colorGreen, 50);
  chase(colorRed,2); // Red
  chase(colorGreen,2); // Green
  chase(colorBlue,2); // Blue
  chase(colorWhite,2); // White
  colorWipe(colorBlack, 0);
  paintWord(arrBIRTHDAY, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrBART, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrBIEKE, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrLIEN, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrKENNY, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrKWART, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrNA, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrUUR, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrVOOR, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrEEN, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrTWEE, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrDRIE, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrVIER, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrVIJF_TOP, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrVIJF, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrZES, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrZEVEN, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrACHT, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrNEGEN, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrTIEN_TOP, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrTIEN, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrELF, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrTWAALF, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrTWINTIG, colorWhite);
  grid.show();
  delay(1000);
  colorWipe(colorBlack, 0);
  paintWord(arrVIJFENTWINTIG, colorWhite);
  grid.show();
  delay(1000);

  fadeOut(100);
  colorWipe(colorBlack, 0);
  grid.setBrightness(intBrightness);
  intTestMode = !intTestMode;
}

void setup() {
  // set up the debuging serial output
  Serial.begin(9600);
  // print the version of code to the console
  printVersion();
  delay(200);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  setSyncInterval(60); // sync the time every 60 seconds (1 minutes)
  if(timeStatus() != timeSet){
     Serial.println("Fout bij sync met RTC :-(");
     RTC.set(1406278800);   // set the RTC to Jul 25 2014 9:00 am
     setTime(1406278800);
  }else{
     Serial.println("RTC is bereikbaar en heeft de systeemtijd gezet, all is happy!");
  }

  // setup the LED strip  
  grid.begin();
  grid.show();

  // set the brightness of the strip
  grid.setBrightness(intBrightness);

  colorWipe(colorBlack, 0);
  spellWord(arrLIEN, colorGreen);
  delay(1000);
  spellWord(arrKENNY, colorBlue);
  delay(1000);
  fadeOut(10);
  colorWipe(colorBlack, 0);

  // set the brightness of the strip
  grid.setBrightness(intBrightness);
  
  // initialize the onboard led on pin 13
  pinMode(LED13PIN, OUTPUT);

  // initialize the buttons
  pinMode(FWDButtonPIN, INPUT);
  pinMode(REVButtonPIN, INPUT);
  
  // lets kick off the clock
  digitalClockDisplay();
}

void loop(){
  // if there is a serial connection lets see if we need to set the time
  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      Serial.print("Tijd gezet via seriele verbinding naar: ");
      Serial.print(t);
      Serial.println();
      RTC.set(t);   // set the RTC and the system time to the received value
      setTime(t);          
    }
  }
  // check to see if the time has been set
  if (timeStatus() == timeSet){
    // time is set lets show the time
    if((hour() < 7) | (hour() >= 19)){
      intBrightness =  BRIGHTNESSNIGHT;
    }else{
      intBrightness =  BRIGHTNESSDAY;
    }
    grid.setBrightness(intBrightness);
    
    // test to see if both buttons are being held down
    // if so  - start a self test till both buttons are held down again.
    if((digitalRead(FWDButtonPIN) == LOW) && (digitalRead(REVButtonPIN) == LOW)){
      intTestMode = !intTestMode;
      if(intTestMode){
        Serial.println("Selftest Mode ACTIEF");
        // run through a quick test
        test_grid();
      }else{
        Serial.println("Selftest mode NIET ACTIEF");
      }
    }
    // test to see if a forward button is being held down for time setting
    if(digitalRead(FWDButtonPIN) == LOW){
      digitalWrite(LED13PIN, HIGH);
      Serial.println("Voorwaards knop ingedrukt >>");
      incrementTime(300);
      delay(100);
      digitalWrite(LED13PIN, LOW);
    }
  
    // test to see if the back button is being held down for time setting
    if(digitalRead(REVButtonPIN) == LOW){
      digitalWrite(LED13PIN, HIGH);
      Serial.println("<< Achterwaards knop ingedrukt");
      incrementTime(-300);
      delay(100);
      digitalWrite(LED13PIN, LOW);
    }

    // and finaly we display the time (provided we are not in self test mode)
    if(!intTestMode){
      displayTime();
    }
  }else{
    colorWipe(colorBlack, 0);
    paintWord(arrEEN, colorRed);
    Serial.println("De tijd is nog niet gezet.  Gelieve de tijd te zetten");
    Serial.println("TimeRTCSet voorbeeld, of DS1307RTC SetTime voorbeeld.");
    Serial.println();
    delay(4000);
  }
  delay(1000);
}

void incrementTime(int intSeconds){
  // increment the time counters keeping care to rollover as required
  if(timeStatus() == timeSet){
    Serial.print(intSeconds);
    Serial.println(" seconden toegevoegd aan de RTC");
//    colorWipe(colorBlack, 0);
    adjustTime(intSeconds);
    RTC.set(now() + intSeconds);
    digitalClockDisplay();
    displayTime();
  }
}  

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day()); 
  Serial.print("-");
  Serial.print(month());
  Serial.print("-");
  Serial.print(year());
  Serial.println();
}

void happyBirthday(){
  paintWord(arrHAPPY, colorGreen);
  delay(1000);
  paintWord(arrBIRTHDAY, colorGreen);
  delay(1000);
  paintWord(arrHAPPY, colorBlue);
  delay(1000);
  paintWord(arrBIRTHDAY, colorBlue);
  delay(1000);
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void rainbow(uint8_t wait) {
  //secret rainbow mode
  uint16_t i, j;
 
  for(j=0; j<256; j++) {
    for(i=0; i<grid.numPixels(); i++) {
      grid.setPixelColor(i, Wheel((i+j) & 255));
    }
    grid.show();
    delay(wait);
  }
}

static void chase(uint32_t color, uint8_t wait) {
  for(uint16_t i=0; i<grid.numPixels()+4; i++) {
      grid.setPixelColor(i  , color); // Draw new pixel
      grid.setPixelColor(i-4, 0); // Erase pixel a few steps back
      grid.show();
      delay(wait);
  }
}

void displayTime(){
  String strCurrentTime; // build the current time
  //colorWipe(colorBlack, 0);
  // Now, turn on the "It is" leds
  paintWord(arrHET, colorWhite);
  // Indien de tijd effectief juist is, zal IS groen worden --> nice :-)
  if((minute()==5)
    |(minute()==10)
    |(minute()==15)
    |(minute()==20)
    |(minute()==25)
    |(minute()==30)
    |(minute()==35)
    |(minute()==40)
    |(minute()==45)
    |(minute()==50)
    |(minute()==55)){
    paintWord(arrIS, colorGreen);
  }else{
    paintWord(arrIS, colorWhite);
  }
  // now we display the appropriate minute counter
  if((minute()>4) && (minute()<10)) {
    // VIJF MINUTEN
    strCurrentTime = "vijf ";
    paintWord(arrVIJF_TOP, colorWhite);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  } 
  if((minute()>9) && (minute()<15)) { 
    //TIEN MINUTEN;
    strCurrentTime = "tien ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorWhite);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  }
  if((minute()>14) && (minute()<20)) {
    // KWART
    strCurrentTime = "kwart ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorWhite);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  }
  if((minute()>19) && (minute()<25)) { 
    //TWINTIG MINUTEN
    strCurrentTime = "twintig ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorWhite);
  }
  if((minute()>24) && (minute()<30)) { 
    //VIJVENTWINTIG MINUTEN
    strCurrentTime = "vijventwintig ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorWhite);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  }  
  if((minute()>29) && (minute()<35)) {
    strCurrentTime = "half ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorWhite);
    paintWord(arrTWINTIG, colorBlack);
  }
  if((minute()>34) && (minute()<40)) { 
    //VIJVENTWINTIG MINUTEN
    strCurrentTime = "vijventwintig ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorWhite);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);

  }  
  if((minute()>39) && (minute()<45)) {
    //TWINTIG MINUTEN
    strCurrentTime = "twintig ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorWhite);
  }
  if((minute()>44) && (minute()<50)) {
    //KWART
    strCurrentTime = "kwart ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorWhite);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  }
  if((minute()>49) && (minute()<55)) {
    //TIEN MINUTEN
    strCurrentTime = "tien ";
    paintWord(arrVIJF_TOP, colorBlack);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorWhite);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack); 
  } 
  if(minute()>54){
    strCurrentTime = "vijf ";
  //VIJF MINUTEN
    paintWord(arrVIJF_TOP, colorWhite);
  paintWord(arrVIJFENTWINTIG, colorBlack);
    paintWord(arrTIEN_TOP, colorBlack);
    paintWord(arrKWART, colorBlack);
    paintWord(arrHALF, colorBlack);
    paintWord(arrTWINTIG, colorBlack);
  }
  if(minute()<5){
    switch(hour()){
      case 1:
      case 13:
      strCurrentTime = strCurrentTime + "een ";
        paintWord(arrEEN, colorWhite);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 2:
    case 14:
      strCurrentTime = strCurrentTime + "twee ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorWhite);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 3: 
    case 15:
      strCurrentTime = strCurrentTime + "drie ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorWhite);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 4: 
    case 16:
      strCurrentTime = strCurrentTime + "vier ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorWhite);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 5: 
    case 17:
      strCurrentTime = strCurrentTime + "vijf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorWhite);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 6: 
    case 18:
      strCurrentTime = strCurrentTime + "zes ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorWhite);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 7: 
    case 19:
      strCurrentTime = strCurrentTime + "zeven ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorWhite);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 8: 
    case 20:
      strCurrentTime = strCurrentTime + "acht ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorWhite);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 9: 
    case 21:
      strCurrentTime = strCurrentTime + "negen ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorWhite);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 10:
    case 22:
      strCurrentTime = strCurrentTime + "tien ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorWhite);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 11:
    case 23:
      strCurrentTime = strCurrentTime + "elf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorWhite);
        paintWord(arrTWAALF, colorBlack);
      break;
    case 0:
    case 12: 
    case 24:
      strCurrentTime = strCurrentTime + "twaalf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorWhite);
      break;
    }
    strCurrentTime = strCurrentTime + "uur ";
    paintWord(arrNA, colorBlack);
    paintWord(arrUUR, colorWhite);
    paintWord(arrVOOR, colorBlack);
  }else if((minute()<30) && (minute()>4)){
    strCurrentTime = strCurrentTime + "na ";
    paintWord(arrNA, colorWhite);
    paintWord(arrUUR, colorBlack);
    paintWord(arrVOOR, colorBlack);
    switch (hour()) {
      case 1:
      case 13:
        strCurrentTime = strCurrentTime + "een ";
        paintWord(arrEEN, colorWhite);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 2: 
      case 14:
        strCurrentTime = strCurrentTime + "twee ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorWhite);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 3: 
      case 15:
        strCurrentTime = strCurrentTime + "drie ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorWhite);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 4: 
      case 16:
        strCurrentTime = strCurrentTime + "vier ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorWhite);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 5: 
      case 17:
        strCurrentTime = strCurrentTime + "vijf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorWhite);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 6: 
      case 18:
        strCurrentTime = strCurrentTime + "zes ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorWhite);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 7: 
      case 19:
        strCurrentTime = strCurrentTime + "zeven ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorWhite);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 8: 
      case 20:
        strCurrentTime = strCurrentTime + "acht ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorWhite);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 9: 
      case 21:
        strCurrentTime = strCurrentTime + "negen ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorWhite);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 10:
      case 22:
        strCurrentTime = strCurrentTime + "tien ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorWhite);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 11:
      case 23:
        strCurrentTime = strCurrentTime + "elf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorWhite);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 0:
      case 12:
      case 24:
        strCurrentTime = strCurrentTime + "twaalf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorWhite);
        break;
      }
    }else{
    // als we groter zijn dan 30 minuten voorbij het uur dan moeten we het volgende uur tonen
    // want we tonen dan een 'voor' teken
      strCurrentTime = strCurrentTime + "voor ";
      paintWord(arrNA, colorBlack);
      paintWord(arrUUR, colorBlack);
      paintWord(arrVOOR, colorWhite);
      switch (hour()) {
        case 1: 
        case 13:
        strCurrentTime = strCurrentTime + "twee ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorWhite);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 14:
      case 2:
        strCurrentTime = strCurrentTime + "drie ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorWhite);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 15:
      case 3:
        strCurrentTime = strCurrentTime + "vier ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorWhite);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 4: 
      case 16:
        strCurrentTime = strCurrentTime + "vijf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorWhite);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 5: 
      case 17:
        strCurrentTime = strCurrentTime + "zes ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorWhite);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 6: 
      case 18:
        strCurrentTime = strCurrentTime + "zeven ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorWhite);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 7: 
      case 19:
        strCurrentTime = strCurrentTime + "acht ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorWhite);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 8: 
      case 20:
        strCurrentTime = strCurrentTime + "negen ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorWhite);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack); 
        break;
      case 9: 
      case 21:
        strCurrentTime = strCurrentTime + "tien ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorWhite);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 10: 
      case 22:
        strCurrentTime = strCurrentTime + "elf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorWhite);
        paintWord(arrTWAALF, colorBlack);
        break;
      case 11: 
      case 23:
        strCurrentTime = strCurrentTime + "twaalf ";
        paintWord(arrEEN, colorBlack);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorWhite);
        break;
      case 0:
      case 12: 
      case 24:
        strCurrentTime = strCurrentTime + "een ";
        paintWord(arrEEN, colorWhite);
        paintWord(arrTWEE, colorBlack);
        paintWord(arrDRIE, colorBlack);
        paintWord(arrVIER, colorBlack);
        paintWord(arrVIJF, colorBlack);
        paintWord(arrZES, colorBlack);
        paintWord(arrZEVEN, colorBlack);
        paintWord(arrACHT, colorBlack);
        paintWord(arrNEGEN, colorBlack);
        paintWord(arrTIEN, colorBlack);
        paintWord(arrELF, colorBlack);
        paintWord(arrTWAALF, colorBlack);
        break;
    }
  }

  if(strCurrentTime != strTime){
    digitalClockDisplay();
    strTime = strCurrentTime;
    if(strTime == ""){
      fadeIn(20);
    }
    // Happy Birthday!
  if((day()==13 && month()==3) | (day()==2 && month()==6)){
    if((minute()==0) | (minute()==30)){
      fadeOut(20);
      colorWipe(colorBlack, 0);
      grid.setBrightness(intBrightness);
      happyBirthday();
      fadeOut(20);
      colorWipe(colorBlack, 10);
      grid.setBrightness(intBrightness);
      // print the version of code to the console
      printVersion();
    }
  }
  }else{
//    grid.show();
  }
}
