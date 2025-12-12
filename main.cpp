#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "AudioOutputI2SNoDAC.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorMP3.h"
#include <ArduinoJson.h>
#define Time2Seconds(S) 3600 * S.substring(11, 13).toInt() + 60 * S.substring(14, 16).toInt() + S.substring(17, 19).toInt();

AudioOutputI2SNoDAC *out;
AudioFileSourceHTTPStream *file;
AudioGeneratorMP3 *mp3;
StaticJsonDocument<2000> JDoc;
WiFiClient client;
HTTPClient http;

unsigned long TimeStamp;

void setup()
{
  // audioLogger = &Serial;
  // Control audio
  pinMode(D1, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  // Blink while not online
  tone(LED_BUILTIN, 1);

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  //   wm.resetSettings();
  // Auto-exit after 30 seconds if no connection
  wm.setConfigPortalTimeout(30);
  // Try to connect to saved WiFi; if failed, start config portal
  if (!wm.autoConnect("WeMos-Config"))
    ESP.restart();

  ArduinoOTA.setHostname(HostName);
  ArduinoOTA.begin();

  noTone(LED_BUILTIN);
  digitalWrite(LED_BUILTIN, 0);
}

void loop()
{
  ArduinoOTA.handle();

  static String TextToSay = "", URL = "";

  if (TextToSay == "")
  {
    // Drop Zone URL
    URL = "http://houstonclock.skydivespaceland.com/socket.io/?EIO=3&transport=polling&sid=";
    // GET SID
    if (http.begin(client, URL) && http.GET() == 200)
    {
      if (!deserializeJson(JDoc, http.getString().substring(http.getString().indexOf("{"))))
      {
        // Add SID to base URL
        URL += String((const char *)JDoc["sid"]);
        // Some kind of subscription
        if (http.begin(client, URL) && http.POST("26:42[\"join\",\"announcements\"]") == 200)
        {
          // GET the real data
          if (http.begin(client, URL) && http.GET() == 200)
          {
            if (!deserializeJson(JDoc, http.getString().substring(http.getString().indexOf("{"), http.getString().lastIndexOf("}"))))
            {
              // deserializeJson(JDoc, "{\"ATL\":{\"loadsFlownToday\":0},\"CLW\":{\"location\":\"CLW\",\"time\":\"2020-12-22T13:53:32.513Z\",\"loadsFlownToday\":0,\"loads\":[],\"iat\":1608645212},\"DAL\":{\"location\":\"DAL\",\"time\":\"2020-12-23T13:52:16.809Z\",\"loadsFlownToday\":0,\"loads\":[],\"iat\":1608731536},\"HOU\":{\"location\":\"HOU\",\"time\":\"2020-12-23T20:17:25.541Z\",\"loadsFlownToday\":1,\"loads\":[{\"status\":1,\"loadNumber\":5,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("05") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"},{\"status\":1,\"loadNumber\":4,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("00") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Otter N7581F\"}],\"iat\":1608754645},\"SSM\":{\"location\":\"SSM\",\"time\":\"2020-12-23T20:18:20.895Z\",\"loadsFlownToday\":0,\"loads\":[{\"status\":1,\"loadNumber\":11,\"slotsRemaining\":13,\"departureTime\":\"2020-12-23T11:" + String("05") + ":48.000Z\",\"jumpRunDbTime\":\"2020-12-23T11:00:48.153Z\",\"plane\":\"Caravan N7581F\"}],\"iat\":1608754700}}"); // String((millis() / 1000) % 10)
              // Get DZ data
              for (JsonPair DZ : JDoc.as<JsonObject>())
              {
                if (DZ.key() == "HOU")
                {
                  // Only need loads data
                  for (JsonVariant Load : DZ.value()["loads"].as<JsonArray>())
                  {
                    // One load text
                    String LoadText;
                    // To calculate time remaining
                    int S1 = 0, S2 = 0, M = 0;
                    LoadText = Load["loadNumber"].as<String>() + ",";
                    String Str = Load["plane"].as<String>();
                    LoadText = Str.substring(0, (Str.indexOf("-") > -1 ? Str.indexOf("-") : Str.indexOf(" "))) + LoadText;
                    S2 = Time2Seconds(Load["departureTime"].as<String>());
                    S1 = Time2Seconds(Load["jumpRunDbTime"].as<String>());
                    // Minutes only
                    M = ((S2 - S1) / 60) % 60;
                    // Every five minutes if load is not on hold
                    if (S2 >= S1 && M % 5 == 0)
                      TextToSay += (TextToSay == "" ? "" : ",and") + LoadText + (M == 0 ? "now%20call" : String(M) + "minutes");
                  }
                }
              }
            }
            http.end();
          }
          http.end();
        }
      }
      http.end();
    }
    digitalWrite(LED_BUILTIN, 1);
  }
  else if (millis() - TimeStamp > 20000)
  {
    TimeStamp = millis();
    digitalWrite(LED_BUILTIN, 0);
    out = new AudioOutputI2SNoDAC();
    file = new AudioFileSourceHTTPStream(String("http://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + TextToSay).c_str());
    file->useHTTP10();
    mp3 = new AudioGeneratorMP3();
    mp3->begin(file, out);
    while (mp3->loop())
      digitalWrite(D1, 1);
    digitalWrite(D1, 0);
    mp3->stop();
    TextToSay = "";
  }
}
