/*=========  Va&Cob OBD2 Gauge ==========
Programmed:  by Ratthanin W.
Hardware:
- Hardware 2.8" TFT ESP32 LVGL https://www.youtube.com/watch?v=d2OXlVcRYrU
Configuration:
- Partition scheme
  Minimal SPIFFS 1.9MB APP/ 190kB SPIFFS /OTA (for OTA ,must reduce code size)

Important Note:
- New device must flash "gauge_factory_init.ino" first to get it work by checking 'serialno' in pref.
  or you can skip genuine checking by comment out line no. 338
- CPU temp read never work until Bluetooth or Wifi is connected

About opening logo image and goodbye image. 
- you can save welcome page logo into gauge by saving file name "vaandcob.jpg"  size 320x240 pixels into littleFS storage.
- you can save goodbye page image into gauge by saving file name "mypic.jpg" size 320z240 pixels into littleFS storage
* if you don't want to use it, you can comment out code line no. 320 - 322
*/

//--- FLAG SETTING ----- for debugging
//#define TERMINAL //temrinal mode (no gauge)
//#define SERIAL_DEBUG //to show data in serial port
//#define SKIP_CONNECTION //skip elm327 BT connection to view meter
//#define TEST_DTC //test DTC

//FORD_T5 (the vehicle variant) is set at the top of gauge_tables.cpp

//Intermal temperature sensor function declaration
#ifdef __cplusplus
extern "C" {
#endif
  uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();  //declare intermal temperature sensor functio

//Macro
#define array_length(x) (sizeof(x) / sizeof(x[0]))  //macro to calculate array length

//library load
#include <Preferences.h>  //save permanent data
Preferences pref;         //create preference
#include <BluetoothSerial.h>
#include <TFT_eSPI.h>  //Hardware-specific library
#include "obd_reply.h"  //turns ELM327 replies into values
#include "gauge_tables.h"  //the PIDs and layouts
#include "elm327_link.h"  //the connection to the ELM327 adapter


//Pin configuration
#define SCA_PIN 21  //not used same pin with TFT_BL
#define SCL_PIN 22  //not used (avaialable) CAN_TX
#define ADC_PIN 35  //not used (available)  CAN_RX
#define LED_RED_PIN 4
#define LED_GREEN_PIN 16
#define LED_BLUE_PIN 17
#define LDR_PIN 34       //LDR sensor
#define BUZZER_PIN 26    //speaker
#define SELECTOR_PIN 27  //push button
// setting PWM properties
#define backlightChannel 0
#define buzzerChannel 2

//DISPLAY
TFT_eSPI tft = TFT_eSPI();
#define TFT_GREY 0x5AEB


//hold warning value
String warningValue[kPidCount];

/*---------------------------*/
//if NO DATA value will set to 0 to skip reading
uint8_t layout = 0;                                   //current layout, saved in NVS
uint8_t A = 0;                                        //get A
uint8_t B = 0;                                        //get B

//Global variable
const uint32_t SLEEP_DURATION = 180 * 1000000;                         //180 sec* us = 3 min
String line[12] = { "", "", "", "", "", "", "", "", "", "", "", "" };  //buffer for each line in terminal function
bool dim = false;                                                      //back light dim flag
const uint8_t LIGHT_LEVEL = 40;                                        //backlight control by light level Higher = darker light
long runtime = 0;                                                      //to check FPS
long holdtime = 0;                                                     //hold time for button pressed check
const String compile_date = __DATE__ " - " __TIME__;                   //get built date and time
bool press = false;                                                    // button press flag
bool showsystem = false;                                               //show fps and temp EEPROM 0x01
uint8_t pidRead = 0;                                                   //counter that hold number of pids been read to check pid/sec
String serial_no = "";                                                 //keep serial no.

//CPU Temp
const uint8_t tempOverheat = 60;       //max operating cpu temp 60c
const int8_t factoryTempOffset = -50;  //default offset adjustment temp up to each tested esp32 max at -50
float tempRead = 0.0;                  //current reading cpu temp
int8_t tempOffset = 0;                 //offset adjustment temp up to each tested esp32
uint8_t temp_read_delay = 0;           //hold delay counter to read temp
uint8_t temp_overheat_count = 0;       //counter to read continuiosly overheat

//BLUETOOTH
String se_message = "";                                                     //serial port message buffer
String deviceName[8] = { "", "", "", "", "", "", "", "" };                  //discoveryBT name
String deviceAddr[8] = { "", "", "", "", "", "", "", "" };                  //discover BT addr
uint8_t btDeviceCount = 0;                                                  //discovered bluetooth devices counter
#define BT_DISCOVER_TIME 5000                                               //bluetooth discoery time
esp_bd_addr_t client_addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };         //obdII mac addr
esp_bd_addr_t recent_client_addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  //keep last btaddr in RTC memory
const String client_name = "OBDII";                                         //adaptor name to search
esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;                                  // or ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE to request pincode confirmation
esp_spp_role_t role = ESP_SPP_ROLE_SLAVE;                                   // or ESP_SPP_ROLE_MASTER
bool foundOBD2 = false;

//---- Include Header File ---
#include "image.h"
#include "touchscreen.h"

/*----------- global function ---------*/
void disconnectAdapter();  //in bluetooth.h

//------------------------------------------------------------------------------------------
void checkCPUTemp() {  //temperature can read only when BT or Wifi Connected
  temp_read_delay++;
  if ((temp_read_delay >= 50) && (foundOBD2)) {  //delay loop 50 then read temp, avoid error eading
    temp_read_delay = 0;
    uint8_t temperature = temprature_sens_read();      //read internal temp sensor
    tempRead = (temperature - 32) / 1.8 + tempOffset;  //change unit F to C
#ifdef SERIAL_DEBUG
    Serial.print(tempRead, 1);  //print Celsius temperature and tab
    Serial.println("°c");
#endif
    //read 10 times to confirm it's really overtemp.
    if (tempRead >= tempOverheat) temp_overheat_count++;  //count overheat time
    else temp_overheat_count = 0;                         //reset
    if (temp_overheat_count > 10) {                       //overheat read more than 10 times.
      Serial.printf("CPU Overheat Shutdown at %d°C\n", tempOverheat);
      tft.fillRectVGradient(0, 0, 320, 240, TFT_RED, TFT_BLACK);
      tft.pushImage(127, 5, 64, 64, overheat, TFT_RED);  //show sdcard icon
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("CPU Overheat Shutdown!", 159, 75, 4);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
      tft.drawCentreString("Auto-restart within 3 minutes", 159, 140, 4);
      tft.drawCentreString("OR", 159, 170, 4);
      tft.drawCentreString("Press button to restart", 159, 200, 4);
      ledcWriteTone(buzzerChannel, 1500);
      delay(5000);
      ledcWriteTone(buzzerChannel, 0);
      disconnectAdapter();  //disconnect bluetooth
      tft.fillScreen(TFT_BLACK);
      tft.writecommand(0x10);  //TFT sleep
      //enter deep sleep
      esp_sleep_enable_timer_wakeup(SLEEP_DURATION);   //sleep timer 3 min
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);  //wake when button pressed
      esp_deep_sleep_start();                          //sleep shutdown backlight auto off with esp32
    }                                                  //if temp_overheat_coun
  }                                                    //if temp_read_delay
}  //check CPU Temp
/*------------------*/
//TERMINAL terminal for debugging display text
void Terminal(String texts, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint8_t max_line = round((h - y) / 16.0);
  tft.fillRect(x, y, x + w, y + h, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  for (uint8_t i = 0; i < max_line; i++) {
    if (i <= max_line - 2) {
      line[i] = line[i + 1];
    } else {
      line[i] = texts;
    }
    tft.drawString(line[i], x, i * 20 + y, 2);
  }                 //for
}
/*------------------*/

void checkGenuine() {                          //check if genuine obd2 gauge - must flash "gauge_factory_init.ino" first
                                               //security {serialno: "V&C-OBDII-001"}
  pref.begin("security", true);                //read only
  serial_no = pref.getString("serialno", "");  //if no serial no, then empty
  Serial.print(F("Serial No: "));
  if (serial_no == "") {
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("Not genuine Va&Cob Gauge!", 0, 115, 4);
    Serial.println(F("Not genuine Va&Cob Gauge!"));
    while (true) {  //beep
      ledcWriteTone(buzzerChannel, 2000);
      digitalWrite(LED_RED_PIN, LOW);  //red on
      delay(500);
      ledcWriteTone(buzzerChannel, 0);
      digitalWrite(LED_RED_PIN, HIGH);  //red off
      delay(500);
    }
  } else {
    Serial.println(serial_no);  //print serial no
    pref.end();
  }
}
//----------------------------------------------

//---- Include Header File --------------------
//#include "image.h"
#include "bluetooth.h"
#include "meter.h"
#include "config.h"

//shows each reply to the ELM327 setup commands when debugging
void logInitReply(const char* reply) {
#ifdef SERIAL_DEBUG
  Serial.println(reply);
#endif
#ifdef TERMINAL
  Terminal(reply, 0, 48, 320, 191);
#endif
}

//----SET UP -----------------------------------
void setup() {
  //pin configuration
  analogSetAttenuation(ADC_0db);        // 0dB(1.0 ครั้ง) 0~800mV   for LDR
  pinMode(LDR_PIN, ANALOG);             //ldr analog input read brightness
  pinMode(BUZZER_PIN, OUTPUT);          //speaker
  pinMode(SELECTOR_PIN, INPUT_PULLUP);  //button
  pinMode(LED_RED_PIN, OUTPUT);         //red
  pinMode(LED_GREEN_PIN, OUTPUT);       //green
  pinMode(LED_BLUE_PIN, OUTPUT);        //blue
  digitalWrite(LED_RED_PIN, LOW);       //on
  digitalWrite(LED_GREEN_PIN, HIGH);    //off
  digitalWrite(LED_BLUE_PIN, HIGH);     //off
  //pwm setup
  ledcSetup(buzzerChannel, 1500, 10);        //buzzer 10 bit
  ledcSetup(backlightChannel, 12000, 8);     //backlight 8 bit
  ledcAttachPin(BUZZER_PIN, buzzerChannel);  //attach buzzer

  //init communication
  Serial.begin(115200);
  //memory check
  psramInit();
  Serial.println(F("\n---------------------------------------\n"));
  Serial.printf("Free Internal Heap %d/%d\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("Free SPIRam Heap %d/%d\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
  Serial.println(F("\n---------------------------------------\n"));
  Serial.println(F("<<  Va&Cob OBDII Gauge  >>"));
  Serial.print(F("by Ratthanin W. BUILD -> "));
  Serial.println(compile_date);



  //init TFT display
  tft.init();
  tft.setRotation(1);  //landcape
                       // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchscreenSPI);
  ts.setRotation(1);

  touch_calibrate();  //hold button at start to calibrate touch
  //testTouch();//test touchscreen

  tft.setSwapBytes(true);  //to display correct image color
  show_spiffs_jpeg_image("/vaandcob.jpg", 0, 0);// display logo image
  delay(3000);

  //backlight ledcAttachPin must be set after tft.init()
  ledcAttachPin(TFT_BL, backlightChannel);  //attach backlight
  for (uint8_t i = 255; i > 0; i--) {       //fading effect
    ledcWrite(backlightChannel, i);         //full bright
    delay(5);
  }
  digitalWrite(LED_RED_PIN, HIGH);  //red off
  tft.fillScreen(TFT_BLACK);        //show start page
  tft.fillRectVGradient(0, 0, 320, 26, TFT_YELLOW, 0x8400);
  tft.setTextColor(TFT_BLACK);
  tft.pushImage(0, 0, 320, 25, obd2gauge);  //show logo
  ledcWrite(backlightChannel, 255);         //full bright
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawRightString("   Config button ->", 319, 26, 2);
  String txt = " BUILD : " + compile_date;
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString(txt, 0, 26, 2);

  // checkGenuine();//check guniune
  //init variable
  pref.begin("setting", false);
  /* Create a namespace called "setting" with read/write mode
{ pref.get...(key,default)
layout : UShort 
showsystem: bool
tempOffset : uint16_t
warning value: uint16_t
recent_client_addr : {0x00,0x00,0x00,0x00,0x00,0x00} array of bytes[6]
}*/
  layout = pref.getUShort("layout", 0);  //load layout setting from NVR
  if (layout > kLayoutCount) layout = 0;
  showsystem = pref.getBool("showsystem", false);             //load showsytem
  tempOffset = pref.getInt("tempOffset", factoryTempOffset);  //load defaultTempoffset
#ifdef SERIAL_DEBUG
  Serial.printf("Show System: %d\n", showsystem);
  Serial.printf("Temp Offset: %d\n", tempOffset);
#endif
  for (uint8_t i = 0; i < kPidCount; i++) {  //read warning value if not found load from default
    warningValue[i] = pref.getString(pidTable[i].settingKey, String(pidTable[i].defaultWarning));
  }
  pref.getBytes("recent_client", recent_client_addr, pref.getBytesLength("recent_client"));  //read last bt address
  ecu_off_volt = pref.getFloat("ecu_off_volt", factoryECUOff);                               //read ecu 0ff voltage


  //start bluetooth
  if (!startBluetooth()) {
    Serial.println(F("Bluetooth..error!"));
    Terminal("Bluetooth..error!", 0, 48, 320, 191);
    abort();
  } else {
    Serial.println(F("Bluetooth..OK"));
    Terminal("Bluetooth..OK", 0, 48, 320, 191);
  }
  runtime = millis();
#ifdef SKIP_CONNECTION
  foundOBD2 = true;
#endif

  //Connect to ELM327
  connectLastOBDII();   //try connect last BT
  while (!foundOBD2) {  //not success try scan and connect another OBD2
    scanBTdevice();
    autoDim();  //auto backlight handle
    //checkCPUTemp(); //check temp never work if BT or WIFI not connected
    if (digitalRead(SELECTOR_PIN) == LOW) {  //button pressed
      if (!press) {
        press = true;                          //set flag
        holdtime = millis();                   //set timer
      } else if (holdtime - millis() > 500) {  //press once and longer
        configMenu();                          //open config menu
        press = false;                         //reset flag
      }                                        //else if holdtimer > 30000
      delay(200);                              //delay avoid bounce
    }
  }

  //Initialize ELM327
  Terminal("Initializing...", 0, 48, 320, 191);
  elm.initialise(logInitReply);

  initScreen();
  beepbeep();
  elm.requestStep();
  //----------------------
}  //setup

/*###################################*/
void loop() {
  //SCAN BUTON (button press HOLD to config menu)
  if (digitalRead(SELECTOR_PIN) == LOW) {  //button pressed
    if (!press) {
      press = true;                           //set flag
      holdtime = millis();                    //set timer
    } else if (holdtime - millis() > 3000) {  //press once and longer than 3 sec
      configMenu();                           //open config menu
      press = false;                          //reset flag
      if (foundOBD2) initScreen();            //open new layout screen
    }                                         //else if holdtimer > 30000
    delay(200);                               //delay avoid bounce
  } else {                                    //button release
    if (press) {                              //change layout next page
      layout++;                               //change to next layout page
      if (layout == kLayoutCount) layout = 0;
      ledcWriteTone(buzzerChannel, 5000);  //play click sound
      delay(5);
      ledcWriteTone(buzzerChannel, 0);
      initScreen();  //open meter screen
                     //reset variable
      for (uint8_t i = 0; i < kSlotCount - 1; i++) old_data[i] = 0.0;
      engine_off_count = 0;
      elm.restartRound();          //discard replies for the old layout
      press = false;               //reset flag
      delay(200);                  //delay avoid bounce

    }  //if press

  }  //else digitalRead
  //----------------------
  //SERIAL PORT read
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();
    //check CR/NL
    if ((incomingChar != '\r') && (incomingChar != '\n')) {
      se_message.concat(incomingChar);
      //Serial.print(incomingChar,HEX);
    } else {

      if (se_message != "") {
#ifdef TERMINAL
        Terminal(se_message, 0, 48, 320, 191);  //display terminal
#endif
        elm.send(se_message.c_str());
        se_message = "";
        Serial.flush();
      }  //if se_message

    }  //else if incomingChar
  }    //if Serial.available

  //Send another PID to elm327
  if (elm.stepDue()) {  //> ELM327 ready! -> request next PID
    if (elm.reply()[0] != '\0') {
#ifdef SERIAL_DEBUG
      Serial.println(elm.reply());
#endif
#ifdef TERMINAL  //for degbugging
      decodeBytes(elm.reply(), pidTable[layoutTable[layout][elm.slot()].pid].code, &A, &B);
      Terminal(String(elm.reply()) + "->" + String(A) + "," + String(B), 0, 48, 320, 191);
#endif
    }
    checkCPUTemp();  //check temperature.
    if (elm.hasReply()) {
      updateMeter(elm.slot(), elm.reply());  //update meter screen
      pidRead++;                             //for calculate pid/s
    }
    if (elm.advance()) {  //end of a round
      autoDim();          //auto backlight handle
      if (showsystem) {
        String status = String(pidRead * 1000.0 / (millis() - runtime), 0) + " p/s " + String(tempRead, 1) + "`c";
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString(status.c_str(), 159, 0, 2);
        runtime = millis();
      }             //if showsystem
      pidRead = 0;  //reset pid read counter
    }
  }  //if stepDue

  /*------------------*/
}  // loop
