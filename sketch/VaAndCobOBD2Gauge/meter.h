// All Meters display function 

const uint16_t screenWidth = tft.height();//landscape
const uint16_t screenHeight = tft.width();
bool blinkCell[9] = {false,false,false,false,false,false,false,false,false};//flag for blinking

uint8_t engine_off_count = 0;//counter to indicate engine is off then sleep
float old_data[kSlotCount] = {};//keep old data for each gauge to improve speed for pidIndex 0-6

//backlight variable average counter
uint8_t low_count = 0;
uint8_t high_count = 0; 

const float factoryECUOff = 12.5;//default off voltage
float ecu_off_volt = factoryECUOff;//turn engine off if voltage lower than 12.4

/* ####################################
 draw digital numeric meter  160x80 px
####################################
cell 
1 , 2 , 3
4 , 5 , 6
7 , 8 , 9 */
void numericMeter(uint8_t cell, uint8_t pid) {
  int w = screenWidth / 2;
  int h = screenHeight / 3;
  int x = cellRect(MeterType::Numeric, cell).x;
  int y = cellRect(MeterType::Numeric, cell).y;
  const char* label = pidTable[pid].label;
  tft.drawRect(x, y, w, screenHeight, TFT_GREY);
  tft.fillRect(x + 2, y + 2, w - 5, h - 5, TFT_BLACK);
  tft.fillRectVGradient(x + 2, y + 2, w - 5, h * 0.33,TFT_BLUE ,0x0011);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + w / 2, y + 2, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawRightString("---", x + w - 5, y + (h * 0.25) + 11, 6);

}  //numericMeter
/*---------------------------*/
//plot number on digital Meter
void plotNumeric(uint8_t slot, uint8_t cell, float data, int warn, bool digit) {
  if (isWarning(data, warn)) {
    int w = screenWidth / 2 - 1;
    int h = screenHeight / 3 - 1;
    int x = cellRect(MeterType::Numeric, cell).x;
    int y = cellRect(MeterType::Numeric, cell).y;
    if (blinkCell[cell]) {//show
      String result = String(data, digit);
      switch (result.length()) {
        case 2: result = "    " + result; break;
        case 3: result = "   " + result; break;
        case 4: result = "  " + result; break;
      }//switch
      old_data[slot] = data;//update old data
      tft.setTextColor(TFT_RED, TFT_BLACK);//red color font
      tft.drawRightString(result.c_str(), x + w - 5, y + (h * 0.25) + 11, 6);
  
    }  else {//hiding make blinking effect
      tft.fillRect(x + 2, y + 28, w - 5, 50, TFT_BLACK);//delete old number
    } 
    blinkCell[cell] = !blinkCell[cell];
  
  } else {//no warning

    if (data != old_data[slot]) {//if value change then redraw new data
     int w = screenWidth / 2 - 1;
     int h = screenHeight / 3 - 1;
     int x = cellRect(MeterType::Numeric, cell).x;
     int y = cellRect(MeterType::Numeric, cell).y;

    String result = String(data, digit);
    switch (result.length()) {
     case 2: result = "    " + result; break;
     case 3: result = "   " + result; break;
     case 4: result = "  " + result; break;
    }//if data
    tft.setTextColor(TFT_GREEN, TFT_BLACK);//green color font
    tft.drawRightString(result.c_str(), x + w - 5, y + (h * 0.25) + 11, 6);
    old_data[slot] = data;//update old data
    } //if nothing changed then skip
  }//if warn/data == 0
}

/* ##########################
draw analog meter 160x80 px
 ##########################
cell 
1 , 2 , 3
4 , 5 , 6
7 , 8 , 9 */
void arcMeter(uint8_t cell, uint8_t pid) {
  int w = screenWidth / 2;
  int h = screenHeight / 3;
  int x = cellRect(MeterType::Arc, cell).x;
  int y = cellRect(MeterType::Arc, cell).y;
  const char* label = pidTable[pid].label;
  const char* unit = pidTable[pid].unit;
  int min = pidTable[pid].minValue;
  int max = pidTable[pid].maxValue;
  // Meter outline
  tft.fillRect(x, y, w, h, TFT_GREY);
  tft.fillRect(x + 2, y + 2, 156, 76, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);  // Text colour
  tft.drawCentreString(String(min).c_str(), x + 16, y + 10, 2);
  tft.drawCentreString(String(max).c_str(), x + w - 16, y + 10, 2);
  tft.drawRightString(unit, x + 150, y + 60, 2);   // Units at bottom right
  tft.drawCentreString(label, x + 80, y + 58, 2);  // Comment out to avoid font 4
  tft.setTextColor(TFT_GREEN);
  tft.drawString("---", x + 10, y + 60, 2);  // data
  //draw arc animation
  for (int angle = 135; angle < 225; angle++) {
    tft.drawSmoothArc(x + 80, y + 90, 80, 60, 134, angle, TFT_GREEN, TFT_DARKGREY, false);
   }
  for (int angle = 224; angle > 135; angle--) {
    tft.drawSmoothArc(x + 80, y + 90, 80, 60, angle + 1, 226, TFT_WHITE, TFT_DARKGREY, false);
  }
}
// #########################################################################
void plotArc(uint8_t slot, uint8_t cell, float data, int warn, int min, int max, bool digit) {
if (data != old_data[slot]) {
  int x = cellRect(MeterType::Arc, cell).x;
  int y = cellRect(MeterType::Arc, cell).y;

  String result = String(data, digit);
  switch (result.length()) {
     case 2: result = result+"   "; break;
     case 3: result = result+"  "; break;
     case 4: result = result+" "; break;
    }
  
  if (data < min) data = min;
  if (data > max) data = max;

  int arcColor = isWarning(data, warn) ? TFT_RED : TFT_GREEN;//red font red arc when warning
  tft.setTextColor(arcColor, TFT_BLACK);
  tft.drawString(result.c_str(), x + 10, y + 60, 2);  // data
  int angle = map(data, min, max, 135, 225);
  if (data >= old_data[slot])//new data > old data -> draw only green arc
     tft.drawSmoothArc(x + 80, y + 90, 80, 60, 134, angle, arcColor, TFT_DARKGREY, false);
  else //new data < old data -> draw only white arc
     tft.drawSmoothArc(x + 80, y + 90, 80, 60, angle + 1, 226, TFT_WHITE, TFT_DARKGREY, false);
  old_data[slot] = data;//update old data
  }//if 

}
/* ######################################
draw linear vertical meter 80x240 px
######################################
cell  1 , 2 , 3 , 4 */
//vertical meter cell 1 to 4 full screen height
void vBarMeter(uint8_t cell, uint8_t pid) {
  int x = cellRect(MeterType::VBar, cell).x;  //start origin
  int w = screenWidth / 4;
  const char* label = pidTable[pid].label;
  const char* unit = pidTable[pid].unit;
  int max = pidTable[pid].maxValue;
  tft.drawRect(x, 0, w, screenHeight, TFT_DARKGREY);
  tft.fillRectVGradient(x + 2, 2, w - 3, screenHeight * 0.12 - 10, TFT_BLUE, 0x0011);
  tft.fillRect(x + 2, screenHeight * 0.12, w - 3, screenHeight - screenHeight * 0.24, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + w / 2, 2, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(unit,x + 3, screenHeight * 0.12-8, 1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("---", x + w / 2, screenHeight - (screenHeight * 0.12) + 1, 4);
  float smalltick = screenHeight * 0.75 / 50;
  //float bigtick = screenHeight*0.75/10;
  //draw number
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // tft.drawRigscreenHeighttString("120",x+w-2,y+screenHeight*0.12+2,1);
  for (int i = 0; i < 51; i++) {  //draw tick
    tft.drawFastHLine(x + w / 3, screenHeight * 0.12 + smalltick * i+3, 10, TFT_WHITE);
    if (i % 5 == 0) {
      tft.drawFastHLine(x + w / 3, screenHeight * 0.12 + smalltick * i+3, 20, TFT_WHITE);
      if (i % 10 == 0) {
        int mark = max - (max * i * 2 / 100);
        tft.drawRightString(String(mark), x + w - 3, screenHeight * 0.12 + smalltick * i - 7, 2);
      }                                         //i%10
    }                                           //i%5
  }                                             //for i
                                                //animation
  for (int barH = 0; barH < 183; barH++) {  //draw bar
   tft.fillRect(x + 5, 212 - barH, 20, barH, TFT_RED);//lower
  }
  for (int barH = 182; barH >= 0; barH--) {  //draw bar  
    tft.fillRect(x + 5, 29, 20, 183 - barH, TFT_BLACK);  //upper
  }
}  //drawLinerVmeter
/*-------------------*/
//vertical meter cell 1 to 4 full screen height
void plotVBar(uint8_t slot, uint8_t cell, float data, int warn, int min, int max, bool digit) {  //cell 1-4
if (data != old_data[slot]) {//if value change then redraw new data
  int x = cellRect(MeterType::VBar, cell).x;
  int w = screenWidth / 4;
  if (data < min) data = min;
  if (data > max) data = max;
  int barColor = isWarning(data, warn) ? TFT_RED : TFT_GREEN;//red bar red font when warning
  //draw new bar
  int barH = map(data, min, max, 0, 182);
  if (data > old_data[slot]) //new data > old data -> draw red bar
    tft.fillRectVGradient(x + 5, 212 - barH, 20, barH, barColor, 0x8000);//lower
  else //new data < old data -> draw black bar
    tft.fillRect(x + 5, 29, 20, 183 - barH, TFT_BLACK);  //upper

  tft.setTextColor(barColor, TFT_BLACK);//draw text
  String result = String(data, digit);
  switch (result.length()) {
    case 2: result = "   " + result + "   "; break;
    case 3: result = "  " + result + "  "; break;
    case 4: result = " " + result + " "; break;
  }
  tft.drawCentreString(result.c_str(), x + w / 2, screenHeight - (screenHeight * 0.12) + 1, 4);
  old_data[slot] = data;//update old data
 }//if
}
/*-------------------*/
//initialize dislay by layout choose
void initScreen() {
  #ifdef SERIAL_DEBUG
  Serial.printf("Display layout -> %d\n",layout);
  #endif
  const char* codes[kSlotCount];
  uint8_t skips[kSlotCount];
  for (uint8_t slot = 0; slot < kSlotCount; slot++) {
    const PidInfo& pid = pidTable[layoutTable[layout][slot].pid];
    codes[slot] = pid.code;
    skips[slot] = pid.skip;
    old_data[slot] = 0;//clear old data
  }
  elm.setRequests(codes, skips, kSlotCount);
  tft.fillScreen(TFT_BLACK);  //clear screen
  for (uint8_t slot = 0; slot < kSlotCount; slot++) {
    const SlotPlan& plan = layoutTable[layout][slot];
    switch (plan.meter) {
      case MeterType::Numeric: numericMeter(plan.cell, plan.pid); break;
      case MeterType::Arc: arcMeter(plan.cell, plan.pid); break;
      case MeterType::VBar: vBarMeter(plan.cell, plan.pid); break;
      case MeterType::None: break;
    }
  }
}  //initscreen
/*-------------------------------*/
//check engine status if off then turn off gauge
void engine_onoff(float data) {  //use pcm volt
  int compare = ecu_off_volt/data;
  if (compare > 0)  //check lower voltage
    engine_off_count++;
  else
    engine_off_count = 0;  //reset
  if (engine_off_count > 20) {  //engine is stop
    if(pref.getUShort("layout",false) != layout) {//if current layout not NVR load layout
      pref.putUShort("layout", layout);//save current layout to NVR
    }      
    show_spiffs_jpeg_image("/mypic.jpg", 0, 0);//show mypic
    ledcWriteTone(buzzerChannel,3136);
    delay(100);
    ledcWriteTone(buzzerChannel,2637);
    delay(100);
    ledcWriteTone(buzzerChannel,2093);
    delay(100);
    ledcWriteTone(buzzerChannel,0);
    for (int i=255;i>0;i--) {//fading effect
      ledcWrite(backlightChannel, i);//full bright
      delay(10);
    }//turn off backlight
    elm.send("ATLP");//set ELM327 to Low Power
    disconnectAdapter();//disconnect bluetooth
    tft.fillScreen(TFT_BLACK);
    tft.writecommand(0x10); //TFT sleep
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27,LOW); //wake when button pressed
    esp_deep_sleep_start();//sleep shutdown backlight auto off with esp32

    }//if engine_off_count>20
}
/*-------------------------------*/
void updateMeter(uint8_t slot, const char* reply) {  //update parameter on screen
  const SlotPlan& plan = layoutTable[layout][slot];
  const PidInfo& pid = pidTable[plan.pid];
  int warn = warningValue[plan.pid].toInt();

  decodeBytes(reply, pid.code, &A, &B);
  float data = toReading(pid.formula, A, B, old_data[slot]);

  switch (plan.meter) {
    case MeterType::Numeric: plotNumeric(slot, plan.cell, data, warn, pid.showDecimal); break;
    case MeterType::Arc: plotArc(slot, plan.cell, data, warn, pid.minValue, pid.maxValue, pid.showDecimal); break;
    case MeterType::VBar: plotVBar(slot, plan.cell, data, warn, pid.minValue, pid.maxValue, pid.showDecimal); break;
    case MeterType::None: break;
  }
  if (countsTowardAutoOff(layout, slot)) engine_onoff(data);
}  //updatemeter
/*-------------------------------*/

//auto dim backlight function
void autoDim() {
  int light = analogRead(LDR_PIN);//read light
  if (light > LIGHT_LEVEL) {//low light
    low_count++;
    if (low_count > 10) {
      low_count = 10;//low amb light 10 times count
      if (!dim) {
        ledcWrite(backlightChannel, 50);//dim light
        dim = true;
        #ifdef SERIAL_DEBUG
        Serial.println(F("Backlight -> dim"));
        #endif
      }//if !dim
    }//if low_count
    high_count = 0;
  
  } else {
    high_count++;
    if (high_count >10) {
      high_count = 10;//high amb light 10 times count
      if (dim) {
        ledcWrite(backlightChannel, 255);//brightest
        dim = false;
        #ifdef SERIAL_DEBUG
        Serial.println(F("Backlight -> bright"));
        #endif
      }//if dim
    }//if high_count  
    low_count = 0;
  }//if light > lightlevel
}//autodim
//---------------------------
/*############################################*/
