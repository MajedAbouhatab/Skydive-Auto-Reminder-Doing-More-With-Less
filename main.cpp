#include <M5Unified.h>
#include <WiFiManager.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <Ticker.h>
#include <Preferences.h>

#define Time2Seconds(S) 3600 * S.substring(11, 13).toInt() + 60 * S.substring(14, 16).toInt() + S.substring(17, 19).toInt();

StaticJsonDocument<2000> JDoc;
WiFiClient client;
HTTPClient http;
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;
Ticker RebootTimer;
Preferences prefs;

String DZLocation, URL, TempText, TextToSay = "";

// Write any text on LCD
void LCDText(const char *txt, int C, int X, int Y, int S)
{
  M5.Lcd.fillRect(X, Y, M5.Lcd.width(), S * 8, BLACK);
  M5.Lcd.setTextColor(C);
  M5.Lcd.setCursor(X, Y);
  M5.Lcd.setTextSize(S);
  M5.Lcd.printf(txt);
}

void DZLocationFunction(String S = "")
{
  prefs.begin("storage", false);
  if (S == "")
    S = prefs.getString("DZLocation");
  else
    prefs.putString("DZLocation", S);
  prefs.end();
  DZLocation = S;
}

void CheckButton(m5::Button_Class B, String D)
{
  if (!B.isPressed())
    return;
  DZLocation = D;
  DZLocationFunction(DZLocation);
}

void setup(void)
{
  http.setReuse(false);
  M5.begin(M5.config());
  M5.Speaker.tone(0, 0);
  LCDText("Connecting\n\nto Wi-Fi...", GREEN, 0, 10, 4);
  FastLED.addLeds<SK6812, 25, BGR>(NULL, 10);
  FastLED.setBrightness(100);
  FastLED.showColor(RED);
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  if (M5.BtnB.isPressed())
  {
    wm.resetSettings();
    ESP.restart();
  }
  // Auto-exit after 30 seconds if no connection
  wm.setConfigPortalTimeout(30);
  M5.Display.printf("\n\n%s", wm.getWiFiSSID().c_str());
  // Try to connect to saved WiFi; if failed, start config portal
  if (!wm.autoConnect("M5Stack-Config"))
  {
    M5.Display.clear();
    LCDText("Config\n\nportal\n\ntimed out!", GREEN, 0, 10, 4);
    M5.delay(1000);
    ESP.restart();
  }
  FastLED.showColor(YELLOW);
  ArduinoOTA.setHostname(HostName);
  ArduinoOTA.begin();
  M5.Display.clear();
  LCDText("WiFi\n\nConnected!", GREEN, 0, 10, 4);
  M5.Display.printf("\n\n%s", WiFi.localIP().toString().c_str());
  M5.delay(1000);

  // Configure I2S output
  out = new AudioOutputI2S();
  out->SetPinout(12, 0, 2);
  out->SetGain(3.9); //.1// 3.9

  // Get location from NVS
  DZLocationFunction();

  // Loop until there is something to say
  while (TextToSay == "")
  {
    ArduinoOTA.handle();
    M5.update();
    CheckButton(M5.BtnA, "HOU");
    CheckButton(M5.BtnB, "SSM");
    CheckButton(M5.BtnC, "DAL");
    LCDText("HOU    SSM    DAL", WHITE, 10, 200, 3);
    M5.Lcd.fillRect(10, 230, 320, 10, BLACK);
    if (DZLocation == "HOU")
      M5.Lcd.fillRect(10, 230, 55, 10, WHITE);
    if (DZLocation == "SSM")
      M5.Lcd.fillRect(135, 230, 55, 10, WHITE);
    if (DZLocation == "DAL")
      M5.Lcd.fillRect(260, 230, 50, 10, WHITE);
    // Drop Zone URL
    URL = "http://houstonclock.skydivespaceland.com/socket.io/?EIO=3&transport=polling&sid=";
    // GET SID
    if (http.begin(client, URL) && http.GET() == 200)
    {
      TempText = http.getString();
      http.end();
      if (!deserializeJson(JDoc, TempText.substring(TempText.indexOf("{"))))
      {
        // Add SID to base URL
        URL += String((const char *)JDoc["sid"]);
        http.setURL(URL);
        // Some kind of subscription
        http.begin(client, URL);
        http.POST("26:42[\"join\",\"announcements\"]");
        http.end();
        if (http.begin(client, URL) && http.GET() == 200)
        {
          // GET the real data
          TempText = http.getString();
          http.end();
          if (!deserializeJson(JDoc, TempText.substring(TempText.indexOf("{"), TempText.lastIndexOf("}"))))
          {
            // Some data for testing
            // JDoc.clear();
            // JDoc["SSM"]["loads"][0]["loadNumber"] = millis() % 10;
            // JDoc["SSM"]["loads"][0]["departureTime"] = "2020-12-23T11:" + String("14") + ":00.000Z";
            // JDoc["SSM"]["loads"][0]["jumpRunDbTime"] = "2020-12-23T11:" + String("00") + ":00.000Z";
            // JDoc["SSM"]["loads"][0]["plane"] = "Otter N7581F";
            // JDoc["DAL"]["loads"][0]["loadNumber"] = millis() % 10;
            // JDoc["DAL"]["loads"][0]["departureTime"] = "2020-12-23T11:" + String("14") + ":00.000Z";
            // JDoc["DAL"]["loads"][0]["jumpRunDbTime"] = "2020-12-23T11:" + String("00") + ":00.000Z";
            // JDoc["DAL"]["loads"][0]["plane"] = "Otter N7581F";
            // deserializeJson(JDoc, "{\"ATL\":{\"loadsFlownToday\":0},\"CLW\":{\"location\":\"CLW\",\"time\":\"2020-12-22T13:53:32.513Z\",\"loadsFlownToday\":0,\"loads\":[],\"iat\":1608645212},\"DAL\":{\"location\":\"DAL\",\"time\":\"2020-12-23T13:52:16.809Z\",\"loadsFlownToday\":0,\"loads\":[],\"iat\":1608731536},\"HOU\":{\"location\":\"HOU\",\"time\":\"2020-12-23T20:17:25.541Z\",\"loadsFlownToday\":1,\"loads\":[{\"status\":1,\"loadNumber\":5,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("05") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"},{\"status\":1,\"loadNumber\":4,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("00") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"}],\"iat\":1608754645},\"SSM\":{\"location\":\"SSM\",\"time\":\"2020-12-23T20:18:20.895Z\",\"loadsFlownToday\":0,\"loads\":[{\"status\":1,\"loadNumber\":11,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("05") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Caravan N7581F\"}],\"iat\":1608754700}}"); // String((millis() / 1000) % 10)
            // deserializeJson(JDoc, "{\"HOU\":{\"location\":\"HOU\",\"time\":\"2020-12-23T20:17:25.541Z\",\"loadsFlownToday\":1,\"loads\":[{\"status\":1,\"loadNumber\":5,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("05") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"},{\"status\":1,\"loadNumber\":4,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("90") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"}],\"iat\":1608754645}");
            // deserializeJson(JDoc, "{\"HOU\":{\"location\":\"HOU\",\"time\":\"2020-12-23T20:17:25.541Z\",\"loadsFlownToday\":1,\"loads\":[{\"status\":1,\"loadNumber\":4,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("59") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"}],\"iat\":1608754645}");

            //  Get DZ data
            for (JsonPair DZ : JDoc.as<JsonObject>())
            {
              if (DZ.key() == DZLocation.c_str())
              {
                M5.Lcd.fillRect(0, 0, 320, 200, BLACK);
                // Only need loads data
                for (JsonVariant Load : DZ.value()["loads"].as<JsonArray>())
                {
                  // One load text
                  String LoadText;
                  // To calculate time remaining
                  int S1 = 0, S2 = 0, M = 0;
                  LoadText = Load["loadNumber"].as<String>() + ",";
                  TempText = Load["plane"].as<String>();
                  LoadText = TempText.substring(0, (TempText.indexOf("-") > -1 ? TempText.indexOf("-") : TempText.indexOf(" "))) + LoadText;
                  S2 = Time2Seconds(Load["departureTime"].as<String>());
                  S1 = Time2Seconds(Load["jumpRunDbTime"].as<String>());
                  // Minutes only
                  M = ((S2 - S1) / 60) % 60;
                  char TextBuffer[64];
                  if ((S2 - S1) >= 0)
                    sprintf(TextBuffer, "%s\n%s\n%02d:%02d", Load["plane"].as<String>().c_str(), Load["loadNumber"].as<String>().c_str(), M, (S2 - S1) - M * 60);
                  else
                    sprintf(TextBuffer, "%s\n%s\nHOLD", Load["plane"].as<String>().c_str(), Load["loadNumber"].as<String>().c_str());
                  M5.Lcd.fillRect(0, 0, 320, 200, BLACK);
                  LCDText(TextBuffer, GREEN, 0, 10, 4);
                  M5.delay(1000);
                  // Every five minutes if load is not on hold
                  if (S2 >= S1 && M % 5 == 0)
                    TextToSay += (TextToSay == "" ? "" : ",and") + LoadText + (M == 0 ? "now%20call" : String(M) + "minutes");
                }
              }
            }
          }
        }
      }
    }
  }
  RebootTimer.attach(30, []()
                     { ESP.restart(); });
  FastLED.showColor(GREEN);
  M5.Display.clear();
  LCDText(TextToSay.c_str(), GREEN, 0, 10, 4);
  URL = "http://translate.google.com/translate_tts?ie=UTF-8&q=" + TextToSay + "&tl=en&client=tw-ob";

  file = new AudioFileSourceICYStream(URL.c_str());
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
  while (mp3->loop())
    M5.delay(1);
}

void loop(void) {}
