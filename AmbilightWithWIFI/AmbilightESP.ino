#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager


#define pixelCount 148
#define pixelPin   13 //D7
const int SerialSpeed = 115200;




//for WIFI LED status
#include <Ticker.h>
Ticker ticker;
String incomingStr = "";
void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}


//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;
//reset settings - for testing
//wifiManager.resetSettings();



WiFiServer server(23);
WiFiClient serverClients[1];




NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod> strip(pixelCount, pixelPin);
uint8_t*  pixelsPOINT = (uint8_t*)strip.Pixels();
uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i; 
uint16_t effectbuf_position = 0;
enum mode { MODE_INITIALISE = 0, MODE_HEADER, MODE_CHECKSUM, MODE_DATA, MODE_SHOW, MODE_FINISH};
mode state = MODE_INITIALISE;
int effect_timeout = 0;
uint8_t prefixcount = 0;
unsigned long ada_sent = 0; 
unsigned long pixellatchtime = 0; 
const unsigned long serialTimeout = 15000; 
long update_strip_time = 0; 

void Adalight_Flash() {
    for(uint16_t i = 0; i < strip.PixelCount(); i++) {
      strip.SetPixelColor(i,RgbColor(255,0,0));
    }
    strip.Show(); 
    delay(200);
    for (uint16_t i = 0; i < strip.PixelCount(); i++) {
      strip.SetPixelColor(i,RgbColor(0,255,0));
    }
    strip.Show();
    delay(200);
    for (uint16_t i = 0; i < strip.PixelCount(); i++) {
      strip.SetPixelColor(i,RgbColor(0,0,255));
    }
    strip.Show();    
    delay(200);
    for (uint16_t i = 0; i < strip.PixelCount(); i++) {
      strip.SetPixelColor(i,RgbColor(0,0,0));
    }
    strip.Show(); 
}

void Adalight () {

  switch (state) {

    case MODE_INITIALISE:
      Serial.println("Begining of Adalight");
      Adalight_Flash(); 
      state = MODE_HEADER;

    case MODE_HEADER:

      effectbuf_position = 0; // reset the buffer position for DATA collection...

          //if(Serial.available()) { // if there is serial available... process it... could be 1  could be 100....
          if(serverClients[0].available()) {
               
            //for (int i = 0; i < Serial.available(); i++) {  // go through every character in serial buffer looking for prefix...
            for (int i = 0; i < serverClients[0].available(); i++) {  // go through every character in serial buffer looking for prefix...

              //if (Serial.read() == prefix[prefixcount]) { // if character is found... then look for next...
              if (serverClients[0].read() == prefix[prefixcount]) { // if character is found... then look for next...
                  prefixcount++;
              } else prefixcount = 0;  //  otherwise reset....  ////

            if (prefixcount == 3) {
              effect_timeout = millis(); // generates START TIME.....
              state = MODE_CHECKSUM; // Move on to next state
              prefixcount = 0; // keeps track of prefixes found... might not be the best way to do this. 
              break; 
            } // end of if prefix == 3
            
            } // end of for loop going through serial....
            //} else if (!Serial.available() && (ada_sent + 5000) < millis()) {
            } else if (!serverClients[0].available() && (ada_sent + 5000) < millis()) {
                  //Serial.print("Ada\n"); // Send "Magic Word" string to host every 5 seconds... 
                  if(serverClients[0].connected()) serverClients[0].print("Ada\n"); // Send "Magic Word" string to host every 5 seconds... 
                  ada_sent = millis(); 
            }

    break;

    case MODE_CHECKSUM:

        //if (Serial.available() >= 3) {
        if (serverClients[0].available() >= 3) {
          hi  = serverClients[0].read();
          lo  = serverClients[0].read();
          chk = serverClients[0].read();
          //hi  = Serial.read();
          //lo  = Serial.read();
          //chk = Serial.read();
          if(chk == (hi ^ lo ^ 0x55)) {
            state = MODE_DATA;
          } else {
            state = MODE_HEADER; // ELSE RESET.......
          }
        }

      if((effect_timeout + 1000) < millis()) state = MODE_HEADER; // RESET IF BUFFER NOT FILLED WITHIN 1 SEC.

      break;

    case MODE_DATA:

        //while(Serial.available() && effectbuf_position < 3 * strip.PixelCount()) {   
        while(serverClients[0].available() && effectbuf_position < 3 * strip.PixelCount()) {   
          //pixelsPOINT[effectbuf_position++] = Serial.read(); 
          pixelsPOINT[effectbuf_position++] = serverClients[0].read();
        }

      if(effectbuf_position >= 3*pixelCount) { // goto show when buffer has recieved enough data...
        state = MODE_SHOW;
        break;
      } 

      if((effect_timeout + 1000) < millis()) state = MODE_HEADER; // RESUM HEADER SEARCH IF BUFFER NOT FILLED WITHIN 1 SEC.
      
      break;

    case MODE_SHOW:

      strip.Dirty(); // MUST USE if you're using the direct buffer version... 
      pixellatchtime = millis();
      state = MODE_HEADER;
      break;

    case MODE_FINISH:

    // nothing in here...
    
    break; 
    }

}


/*void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            break;
    }
}*/

void setup() {
    Serial.begin(SerialSpeed);
    
     //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "GaganAmbiLight"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("GaganAmbiLight")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  Serial.println(WiFi.localIP());
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);
    
    Serial.println();
    strip.Begin();
    strip.Show();
   // WiFi.onEvent(WiFiEvent);
   // WiFi.begin("NPMGroup-Guest");
    server.begin();
    server.setNoDelay(true);
}

void loop() {
    if (Serial.available() > 0) {
              incomingStr = Serial.readString();
              incomingStr.trim();
              if (incomingStr.equals("reset")) {
                  Serial.print("Resetting......");
                  wifiManager.resetSettings();
              }
        }
    if(millis() - update_strip_time > 30) {
      strip.Show();
      update_strip_time = millis();
    };
    tcp();
    Adalight();
    
}

void tcp() {
   if(server.hasClient()) {
     serverClients[0] = server.available();
   }
}
