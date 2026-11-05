/* Putting the gauge to sleep. shutdown_watch decides when and this does it. Both ways
   end in deep sleep, and waking re-runs setup() from scratch. */

const uint32_t SLEEP_DURATION = 180 * 1000000;  //180 sec* us = 3 min

AutoOffWatch autoOffWatch;
OverheatWatch overheatWatch;

enum class ShutdownReason : uint8_t { AutoOff, Overheat };

/* The two shutdowns differ on purpose:
                    auto-off                  overheat
   layout           saved                     not saved
   screen           my picture, then fade     alert
   sound            three falling tones       1500 Hz for 5 s
   ELM327 adapter   told to go to low power   left as it is
   wakes on         the button                the button, or after 3 minutes */
void shutDown(ShutdownReason reason) {
  if (reason == ShutdownReason::AutoOff) {
    settings.saveLayout();
    show_spiffs_jpeg_image("/mypic.jpg", 0, 0);  //show mypic
    ledcWriteTone(buzzerChannel, 3136);
    delay(100);
    ledcWriteTone(buzzerChannel, 2637);
    delay(100);
    ledcWriteTone(buzzerChannel, 2093);
    delay(100);
    ledcWriteTone(buzzerChannel, 0);
    for (int i = 255; i > 0; i--) {  //fading effect
      ledcWrite(backlightChannel, i);
      delay(10);
    }
    elm.send("ATLP");  //set ELM327 to Low Power
  } else {
    tft.fillRectVGradient(0, 0, 320, 240, TFT_RED, TFT_BLACK);
    tft.pushImage(127, 5, 64, 64, overheat, TFT_RED);  //show overheat icon
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("CPU Overheat Shutdown!", 159, 75, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawCentreString("Auto-restart within 3 minutes", 159, 140, 4);
    tft.drawCentreString("OR", 159, 170, 4);
    tft.drawCentreString("Press button to restart", 159, 200, 4);
    ledcWriteTone(buzzerChannel, 1500);
    delay(5000);
    ledcWriteTone(buzzerChannel, 0);
  }

  disconnectAdapter();  //disconnect bluetooth
  tft.fillScreen(TFT_BLACK);
  tft.writecommand(0x10);  //TFT sleep
  if (reason == ShutdownReason::Overheat) esp_sleep_enable_timer_wakeup(SLEEP_DURATION);  //sleep timer 3 min
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);  //wake when button pressed
  esp_deep_sleep_start();  //sleep shutdown backlight auto off with esp32
}

//---------------------------
//temperature can read only when BT or Wifi Connected
void checkCPUTemp() {
  if (!overheatWatch.readDue(foundOBD2)) return;
  uint8_t temperature = temprature_sens_read();  //read internal temp sensor
  tempRead = (temperature - 32) / 1.8 + settings.tempOffset;  //change unit F to C
#ifdef SERIAL_DEBUG
  Serial.print(tempRead, 1);  //print Celsius temperature and tab
  Serial.println("°c");
#endif
  if (overheatWatch.update(tempRead)) {
    Serial.printf("CPU Overheat Shutdown at %d°C\n", OverheatWatch::kOverheatC);
    shutDown(ShutdownReason::Overheat);
  }
}
