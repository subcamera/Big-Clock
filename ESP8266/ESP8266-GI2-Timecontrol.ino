/* ++++++++++++++++ ToDos ++++++++++++++++++
 *  
USAGE:
  Auf Accesspoint GD4-Timecontrol/pwd anmelden
  Weboberfläche aufrufen    http:/http://192.168.4.1/  
   alternativ
  App installieren 

  Funktion:
          Stoppuhr
                  - Reset -> 0 00  
                  - Start -> 0:00 ":" blinkt
                  - Stop/Pause lässt Stopuhr pausieren bzw. weiterzählen
          Countdown
                  - Über  Zeitfelder den gewünschte Zeitraum einstellen
                  - SET   Zeitraum setzen
                  - START Countdown starten
                  - Stop/Pause unterbricht bzw. setzt Countdown fort
          Uhr
                  - Über       Zeitfelder den aktuelle Uhrzeit einstellen
                  - SET        Uhrzeit setzen
                  - START      Uhr starten
                  - Stop/Pause ohne Funktion
             
                                   -         


*/
 /*     
  * OTA-Update aktivieren     
  *                http://192.168.4.1:88/update  
  * 
  * Weboberfläche aufrufen    
  *                http:/http://192.168.4.1/  
  *
  * Uhr            http://192.168.4.1/?MODUS=0
  * Countdown      http://192.168.4.1/?MODUS=1
  * Stopppuhr      http://192.168.4.1/?MODUS=2
  * Start          http://192.168.4.1/?ACTION=1
  * Stop/Pause     http://192.168.4.1/?ACTION=2
  * Reset/Set      http://192.168.4.1/?ACTION=3&HOURS=01&MINUTES=01&SECONDS=01
  * COUNTDOWN_HOT  http://192.168.4.1/?COUNTDOWN_HOT=5
  * DISPLAY        http://192.168.4.1/?DISPLAY=1/0
  *      
  *      
  *      
  * PINOUT:
  *  
  *                       ________________
  *                     /    ANTENNA      \
  *                    | RST           TX |
  *                    | A0            RX |
  *    LDR-Eingang     | D0     W      D1 | SCL  RealtimeClock
  *    TM1637  DIO     | D5     E      D2 | SDA  RealtimeClock 
  *    TM1637  CLK     | D6     M      D3 | LED WS2811 3-Wire 
  *                    | D7     O      D4 | LED-/intern / Kontrollanzeige für Kommandoempfang
  *                    | D8     S     GND | GND RealtimeClock 
  *                    | 3V3           5V | PWR RealtimeClock
  *                    +-+    D1 mini     |
  *                      |                |
  *                      +---- USB -------+ 
  *                      
  *                      
  *                      
  *  WebUpdater
  *  based on https://forum.arduino.cc/index.php?topic=633882.0
  *  establishes ESP8266 as Accesspoint and enable WebUpdate via
  *  http://192.168.4.1/update  to load external bin-file
  *  
  *                      
  */

//---------- DEBUG-Ausgabe definieren  -----------------------------------------

//#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif

//---------- Timer-Library  -----------------------------------------

#include <CountUpDownTimer.h>  // https://github.com/AndrewMascolo/CountUpDownTimer

CountUpDownTimer Countdown(DOWN);
CountUpDownTimer Stopwatch(UP);

//---------- LED-Strip Library -----------------------------------------
  
#include <Wire.h> 
#include "FastLED.h"
//
#define NUM_LEDS 60 // Number of LED controles 7*2*4 + 4 (remember I have 3 leds / controler
#define COLOR_ORDER BRG  // Define color order for your strip
#define DATA_PIN D3  // Data pin for led comunication

CRGB leds[NUM_LEDS]; // Define LEDs strip

bool Dot = true;  //Dot state blinkenden (Doppel)Punkt
bool DST = false; //DST state
int ledColor = 0x0000FF; // Color used (in hex)
int ledColorOrg = ledColor;  // Store org Color for restoring
int DisplayNumbers=0;
//
// taken from http://www.instructables.com/id/Big-auto-dim-room-clock-using-arduino-and-WS2811/
//
//
//   Strip/Array-Order
//
//              3 3 3 3 3
//             2         4 
//             2         4 
//             2         4
//     OUT <-   1 1 1 1 1 <-- IN  
//     next    7         5
//     number  7         5
//             7         5 
//              6 6 6 6 6
//
//  2D Array for numbers on 7 segment
//
byte digits[10][7] = {{0,1,1,1,1,1,1},  // Digit 0
                      {0,0,0,1,1,0,0},   // Digit 1 
                      {1,0,1,1,0,1,1},   // Digit 2
                      {1,0,1,1,1,1,0},   // Digit 3
                      {1,1,0,1,1,0,0},   // Digit 4
                      {1,1,1,0,1,1,0},   // Digit 5
                      {1,1,1,0,1,1,1},   // Digit 6
                      {0,0,1,1,1,0,0},   // Digit 7
                      {1,1,1,1,1,1,1},   // Digit 8  
                      {1,1,1,1,1,1,0}};  // Digit 9 

//---------------------------------------------------------------------
// include the SevenSegmentTM1637 library
#include "SevenSegmentTM1637.h" //https://github.com/bremme/arduino-tm1637
const byte PIN_CLK = D6;   // define CLK pin (any digital pin)
const byte PIN_DIO = D5;   // define DIO pin (any digital pin)
SevenSegmentTM1637    display(PIN_CLK, PIN_DIO);
  char myOutput[4];

//---------------------------------------------------------------------

// Date and time functions using a DS1307 RTC connected via I2C and Wire lib
#include <Wire.h>
#include "RTClib.h" 

RTC_DS1307 rtc;
DateTime now;

//---------------------------------------------------------------------
// ESP-WebUpdate
// #include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

//#include <ESP8266mDNS.h>
//const char* host = "esp8266";

ESP8266WebServer webServer(88);
ESP8266HTTPUpdateServer webUpdater;

//---------------------------------------------------------------------

//ESP866 HTML Demo 02
#include <ESP8266WiFi.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

//---------------------------------------------------------------------
// WiFi

byte my_WiFi_Mode = 2;  // WIFI_STA = 1 = Workstation  WIFI_AP = 2  = Accesspoint   0= Automatic

const char * ssid_ap      = "GD4-Timecontrol";
const char * password_ap  = "fluxkompensator";    

WiFiServer server(80);
WiFiClient client;

#define MAX_PACKAGE_SIZE 2048
char HTML_String[5000];
char HTTP_Header[150];

boolean newaction = false;  // wurde Aktionstaste gedrückt?

int ledPin = D4; //interne LED STD=13 / WEMOS=D4

//---------------------------------------------------------------------
// Allgemeine Variablen

#define ACTION_START 1
#define ACTION_STOP 2
#define ACTION_RESET 3

int Action;

// Uhrzeit Datum
byte Uhrzeit_HH = 0;
byte Uhrzeit_MM = 20;
byte Uhrzeit_SS = 0;

byte CountDown_Hot = 5; // Minuten, ab wann Countddown rot angezeigt werden soll.

byte DisplayOn = 1; // Anzeige an/aus

// Radiobutton
char Modus_tab[3][15] = {"Uhr", "Countdown", "Stoppuhr"};
byte Modus = 0;

char tmp_string[20];

//-------- Blinken --------------------
bool bShowIt     = true;
int blink_period = 800;  //ms
int actTime;
int remTime;
bool is_blinking = false;
//---------------------------------------------------------------------
// Convert time to array needed for display 
void DisplayNumbersToArray(int numbers = 4){  //  Globale Variable : DisplayNumbers=1234 als int
  int cursor;
  for(int i=28;i<=31;i++){
    if (Dot){leds[i]=ledColor;}
    else    {leds[i]=0x000000;};
  };
      
  for(int i=1;i<=numbers;i++){
    int digit = DisplayNumbers % 10; // get last digit in time
    if (i==1){
      DPRINT("Digit 4 is : ");DPRINT(digit);DPRINT(" ");
      cursor =0;
      for(int k=0; k<=6;k++){ 
        DPRINT(digits[digit][k]);
       if (digits[digit][k]== 1){leds[cursor]=ledColor;leds[cursor+1]=ledColor;}
         else if (digits[digit][k]==0){leds[cursor]=0x000000;leds[cursor+1]=0x000000;};
         cursor=cursor+2;   
        };
      DPRINTLN();
      }
    else if (i==2){
      DPRINT("Digit 3 is : ");DPRINT(digit);DPRINT(" ");
      cursor =14;
      for(int k=0; k<=6;k++){ 
        DPRINT(digits[digit][k]);
       if (digits[digit][k]== 1){leds[cursor]=ledColor;leds[cursor+1]=ledColor;}
         else if (digits[digit][k]==0){leds[cursor]=0x000000;leds[cursor+1]=0x000000;};
         cursor=cursor+2;   
        };
      DPRINTLN();
      }
    else if (i==3){
      DPRINT("Digit 2 is : ");DPRINT(digit);DPRINT(" ");
      cursor =14*2+4;
      for(int k=0; k<=6;k++){ 
        DPRINT(digits[digit][k]);
        if (digits[digit][k]== 1){leds[cursor]=ledColor;leds[cursor+1]=ledColor;}
         else if (digits[digit][k]==0){leds[cursor]=0x000000;leds[cursor+1]=0x000000;};
         cursor=cursor+2;   
        };
      DPRINTLN();
      }
    else if (i==4){
      DPRINT("Digit1 is : ");DPRINT(digit);DPRINT(" ");
      cursor =14*3+4;
      for(int k=0; k<=6;k++){ 
        DPRINT(digits[digit][k]);
       if (digits[digit][k]== 1){leds[cursor]=ledColor;leds[cursor+1]=ledColor;}
         else if (digits[digit][k]==0){leds[cursor]=0x000000;leds[cursor+1]=0x000000;};
         cursor=cursor+2;   
        };
      DPRINTLN();
     }
    DisplayNumbers /= 10;
  }; 
  FastLED.show(); // Display leds array
};

//---------------------------------------------------------------------

void DisplayNumbersToTM1636(){
  char myOutput[4];
//  Serial.print("DisplayNumbersToTM1636=");
//  Serial.println(DisplayNumbers);
  
  display.clear();                      // clear the display

//  if (DisplayNumbers%2==0)
    if (Dot)
    display.setColonOn(true);
  else
    display.setColonOn(false);

  sprintf(myOutput,"%2d%02d",DisplayNumbers/100,DisplayNumbers%100);
  display.print(myOutput);  
//  Serial.print("DisplayNumbersToTM1636/myOutput=");
//  Serial.println(myOutput);        
};

//---------------------------------------------------------------------
void setup() {
Serial.begin(115200);
#ifdef DEBUG
  Serial.begin(115200);

  for (int i = 5; i > 0; i--) {
    Serial.print("Warte ");
    Serial.print(i);
    Serial.println(" sec");
    delay(1000);
  }
#endif
// -----------  Control-LED ------------

  pinMode(ledPin, OUTPUT);      // Setzt den ledPin als Ausgang


// ----------- TM1637  -----------------

  display.begin();            // initializes the display
  display.setBacklight(100);  // set the brightness to 100 %
  display.print("Welcome to GD4");// print COUNTING SOME DIGITS

// ----------- TIMER -----------------

  Countdown.SetTimer(0,0,30);
  Countdown.StartTimer();

  Stopwatch.SetTimer(0,0,0); 
  Stopwatch.StartTimer();

  
  //---------------------------------------------------------------------
  // RTC prüfen

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  //---------------------------------------------------------------------
  // LED-Stripes
//  FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  LEDS.addLeds<WS2811, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS); // Set LED strip type
  LEDS.setBrightness(255); // Set initial brightness

  //---------------------------------------------------------------------
  // WiFi starten

  WiFi_Start_AP();
//  WiFi_Start_STA();
  if (my_WiFi_Mode == 0) WiFi_Start_AP();

//---------------------------------------------------------------------
  webUpdater.setup(&webServer);
  webServer.begin();
}

//---------------------------------------------------------------------
void loop() {

static bool PauseResume; 

  webServer.handleClient();  // Updateserver 
  
  WiFI_Traffic();
  if (newaction){       
      // Später ggfls. auf LED-Stripe in Grün/Blau ausführen
       digitalWrite(ledPin, LOW);   // LED einschalten (LED ist invertiert!)
  } else{
       digitalWrite(ledPin, HIGH);   // LED ausschalten
  }
   delay(100); 

   Countdown.Timer();
   Stopwatch.Timer();

#ifdef DEBUG
  Serial.print("-- ");
  Serial.print(Modus);
  Serial.print("-- ");
  Serial.print(Action);
  Serial.print("-- ");
  Serial.print(Uhrzeit_HH);
  Serial.print(":");
  Serial.print(Uhrzeit_MM);
  Serial.print(":");
  Serial.println(Uhrzeit_SS);
  Serial.println();
#endif
 switch (DisplayOn) {
   case 0:  
    LEDS.setBrightness(0);
    break;   
   case 1:
    LEDS.setBrightness(255); 
    break;     
  }
  
  switch (Modus) {
    // ------------------------ UHR ------------------------
    case 0:      
        ledColor=ledColorOrg;  
        is_blinking = false;          
        switch (Action) {
          case ACTION_START:
            break;
          case ACTION_STOP:
            break;
          case ACTION_RESET:
            rtc.adjust(DateTime(1962, 12, 3, Uhrzeit_HH, Uhrzeit_MM, Uhrzeit_SS));
            break;
          default:
            break;
        }  // switch Action case 0
        now = rtc.now();
        DisplayNumbers=now.hour()*100+now.minute();
        if (now.second() % 2==0) {Dot = false;} else {Dot = true;};  // make the dots blinking
        break;
    // ------------------------ Countdown -------------------
    case 1:               
        switch (Action) {
          case ACTION_START:
            Countdown.StartTimer();
            PauseResume = false;
            break;
          case ACTION_STOP:
            if (newaction) {
                PauseResume = !PauseResume;
                if(PauseResume)
                  Countdown.PauseTimer();
                else
                  Countdown.ResumeTimer();
            }
            break;
          case ACTION_RESET:
            Countdown.SetTimer(0,Uhrzeit_MM,Uhrzeit_SS);
            break;
          default:
            break;
        } // switch Action case 1
        
        DisplayNumbers=Countdown.ShowMinutes()*100+Countdown.ShowSeconds();
        if (Countdown.ShowSeconds() % 2==0) {Dot = false;} else {Dot = true;};  // make the dot binking

        if (Countdown.ShowMinutes() <= 0) {is_blinking=true;}
       
        if (Countdown.ShowSeconds() <= 0 && Countdown.ShowMinutes() <= 0) {
            Countdown.PauseTimer();
            is_blinking=true; //xx ALARM LEDs blinken lassen
          } else {is_blinking=false;};
        if (Countdown.ShowMinutes() < CountDown_Hot) {ledColor = 0xFF0000;} else {ledColor = ledColorOrg;}; // make last 5 Minutes red LEDs
          
      break;
    // ------------------------ Stopwatch ------------------------
    case 2: 
        ledColor=ledColorOrg;  
        is_blinking = false;              
        switch (Action) {
          case ACTION_START:
           PauseResume = false;
            break;
          case ACTION_STOP:
            if (newaction) {
                PauseResume = !PauseResume;
                if(PauseResume)
                  Stopwatch.PauseTimer();
                else
                  Stopwatch.ResumeTimer();
            }
            break;
          case ACTION_RESET:
            Serial.println("Reset Stopwatch");
            Stopwatch.StartTimer();
            break;
          default:
            // if nothing else matches, do the default
            // default is optional
            break;
        } // switch Action case 2
        DisplayNumbers=Stopwatch.ShowMinutes()*100+Stopwatch.ShowSeconds();
        if (Stopwatch.ShowSeconds() % 2==0) {Dot = false;} else {Dot = true;};  // make the dot blinking
      break;
   } // switch Modus

  DPRINT("Modus=");
  DPRINT(Modus);
  DPRINT(" Action=");
  DPRINT(Action);
  if (Dot){DPRINT(" *");} else {DPRINT("  ");}
  DPRINT("Display=");
  DPRINT(DisplayNumbers);
  DPRINTLN();

  newaction=false;  


//+++++++++++++++++ Blinken ++++++++++++++++++++++ 

  actTime = millis();
  if (!is_blinking) {
        bShowIt=true;
        DisplayNumbersToTM1636(); // muss for DisplayNumbersToArray stehen!
        DisplayNumbersToArray(); // Get leds array with required configuration
        FastLED.show();
  }
  else
  {
    if (actTime - remTime >= blink_period){
      remTime = actTime;
      if(bShowIt) {
          bShowIt = false;
          DisplayNumbersToTM1636(); // muss for DisplayNumbersToArray stehen!
          DisplayNumbersToArray(); // Get leds array with required configuration
          FastLED.show();
      } else {
          bShowIt = true;
          if (is_blinking) {
            display.print("    ");  
            fill_solid(leds, NUM_LEDS, CRGB::Black); // all leds off
            FastLED.show();
          }
      }
    }
  }
//--------------- Blinken --------------------------- 
}  // loop

//---------------------------------------------------------------------
void WiFi_Start_STA() {
  unsigned long timeout;

  WiFi.mode(WIFI_STA);   //  Workstation

  timeout = millis() + 12000L;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    my_WiFi_Mode = WIFI_STA;

#ifdef DEBUG
    Serial.print("Connected IP - Address : ");
    for (int i = 0; i < 3; i++) {
      Serial.print( WiFi.localIP()[i]);
      Serial.print(".");
    }           
    Serial.println(WiFi.localIP()[3]);
#endif


  } else {
    WiFi.mode(WIFI_OFF);
#ifdef DEBUG
    Serial.println("WLAN-Connection failed");
#endif

  }
}

//---------------------------------------------------------------------
void WiFi_Start_AP() {
  WiFi.mode(WIFI_AP);   // Accesspoint
  WiFi.softAP(ssid_ap, password_ap);
 
  server.begin();
  IPAddress myIP = WiFi.softAPIP();
  my_WiFi_Mode = WIFI_AP;

#ifdef DEBUG
  Serial.print("Accesspoint started - Name : ");
  Serial.print(ssid_ap);
  Serial.print( " IP address: ");
  Serial.println(myIP);
#endif
}
//---------------------------------------------------------------------
void WiFI_Traffic() {

  char my_char;
  int htmlPtr = 0;
  int myIdx;
  int myIndex;
  unsigned long my_timeout;

  // Check if a client has connected
  client = server.available();
  if (!client)  {
    return;
  }

  my_timeout = millis() + 250L;
  while (!client.available() && (millis() < my_timeout) ) delay(10);
  delay(10);
  if (millis() > my_timeout)  {
    return;
  }
  //---------------------------------------------------------------------
  htmlPtr = 0;
  my_char = 0;
  while (client.available() && my_char != '\r') {
    my_char = client.read();
    HTML_String[htmlPtr++] = my_char;
  }
  client.flush();
  HTML_String[htmlPtr] = 0;
#ifdef DEBUG
  exhibit ("Request : ", HTML_String);
#endif

  if (Find_Start ("/?", HTML_String) < 0 && Find_Start ("GET / HTTP", HTML_String) < 0 ) {
    send_not_found();
    return;
  }

  //---------------------------------------------------------------------
  // Benutzereingaben einlesen und verarbeiten
  //---------------------------------------------------------------------

  if (Pick_Parameter_Zahl("HOURS=", HTML_String)        != -1) Uhrzeit_HH = Pick_Parameter_Zahl("HOURS=", HTML_String);
  if (Pick_Parameter_Zahl("MINUTES=", HTML_String)      != -1) Uhrzeit_MM = Pick_Parameter_Zahl("MINUTES=", HTML_String);
  if (Pick_Parameter_Zahl("SECONDS=", HTML_String)      != -1) Uhrzeit_SS = Pick_Parameter_Zahl("SECONDS=", HTML_String);
  if (Pick_Parameter_Zahl("DISPLAY=", HTML_String)      != -1) DisplayOn  = Pick_Parameter_Zahl("DISPLAY=", HTML_String);
  if (Pick_Parameter_Zahl("COUNTDOWN_HOT=", HTML_String)!= -1) CountDown_Hot =  Pick_Parameter_Zahl("COUNTDOWN_HOT=", HTML_String);

  myIndex = Find_End("MODUS=", HTML_String);
  if (myIndex >= 0) {
      Modus = Pick_Parameter_Zahl("MODUS=", HTML_String);
  }

  newaction=false;
  myIndex = Find_End("ACTION=", HTML_String);
  if (myIndex >= 0) {
      Action = Pick_Parameter_Zahl("ACTION=", HTML_String);
      newaction=true;
  }

   // UHRZEIT=12%3A35%3A25
    myIndex = Find_End("UHRZEIT=", HTML_String);
    if (myIndex >= 0) {
      Pick_Text(tmp_string, &HTML_String[myIndex], 8);
//      Uhrzeit_HH = Pick_N_Zahl(tmp_string, ':', 1);  // Umbelegung: H->Min  Min-> Sec Sec-> NULL
//      Uhrzeit_HH = 0;
      Uhrzeit_MM = Pick_N_Zahl(tmp_string, ':', 1);
      Uhrzeit_SS = Pick_N_Zahl(tmp_string, ':', 2);
//      Uhrzeit_HH = Pick_N_Zahl(tmp_string, ':', 1); // Original
//      Uhrzeit_MM = Pick_N_Zahl(tmp_string, ':', 2);
//      Uhrzeit_SS = Pick_N_Zahl(tmp_string, ':', 3);
      #ifdef DEBUG
            Serial.print("-- Zeit ");
//            Serial.print(Uhrzeit_HH);
//            Serial.print(":");
            Serial.print(Uhrzeit_MM);
            Serial.print(":");
            Serial.println(Uhrzeit_SS);
      #endif

    }


  //---------------------------------------------------------------------
  //Antwortseite aufbauen

  make_HTML01();

  //---------------------------------------------------------------------
  strcpy(HTTP_Header , "HTTP/1.1 200 OK\r\n");
  strcat(HTTP_Header, "Content-Length: ");
  strcati(HTTP_Header, strlen(HTML_String));
  strcat(HTTP_Header, "\r\n");
  strcat(HTTP_Header, "Content-Type: text/html\r\n");
  strcat(HTTP_Header, "Connection: close\r\n");
  strcat(HTTP_Header, "\r\n");

#ifdef DEBUG
  exhibit("Header : ", HTTP_Header);
  exhibit("Laenge Header : ", strlen(HTTP_Header));
  exhibit("Laenge HTML   : ", strlen(HTML_String));
#endif

  client.print(HTTP_Header);
  delay(20);

  send_HTML();

}

//---------------------------------------------------------------------
// HTML Seite 01 aufbauen
//---------------------------------------------------------------------
void make_HTML01() {

  strcpy( HTML_String, "<!DOCTYPE html>");
  strcat( HTML_String, "<html>");
  strcat( HTML_String, "<head>");
  strcat( HTML_String, "<title>GD4-Timecontrol</title>");
  strcat( HTML_String, "</head>");
  strcat( HTML_String, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.2\">");  
  strcat( HTML_String, "<body bgcolor=\"MediumSeaGreen\">");
  strcat( HTML_String, "<font color=\"#000000\" face=\"VERDANA,ARIAL,HELVETICA\">");
  strcat( HTML_String, "<h1>GD4-Timecontrol</h1>");
  strcat( HTML_String, "<form>");
  strcat( HTML_String, "<table>");
//-----------------------------------------------
  for (int i = 0; i < 3; i++) {
    strcat( HTML_String, "<tr>");
    if (i == 0)  strcat( HTML_String, "<td><b>Funktion</b></td>");
    else strcat( HTML_String, "<td> </td>");
//   strcati(HTML_String, i);
//    strcat( HTML_String, "\">Reset</button></td>");
    
    strcat( HTML_String, "<td><button style= \"width:100px;height:25px\" name=\"MODUS\" value=\"");
    strcati( HTML_String, i);
    strcat( HTML_String, "\"");
    if (Modus == i)strcat( HTML_String, " disabled");
    strcat( HTML_String, ">");
    strcat( HTML_String, Modus_tab[i]);
    strcat( HTML_String, "</button></td></tr>");
  }

  strcat( HTML_String, "<tr> </tr>");  
  strcat( HTML_String, "<tr> </tr>");  
  strcat( HTML_String, "<tr> </tr>");  

//-----------------------------------------------

  strcat( HTML_String, "<tr>");

  strcat( HTML_String, "<td><b>Zeit</b></td><td>");

  strcat( HTML_String, "<input type=\"number\" name=\"HOURS\" min=\"0\" max=\"24\" style=\"width:30px\" title=\"Stunden\" value=\"");
  strcati2( HTML_String, Uhrzeit_HH); 
  strcat( HTML_String, "\">");
  
  strcat( HTML_String, " : ");

  strcat( HTML_String, "<input type=\"number\" name=\"MINUTES\" min=\"0\" max=\"59\" style=\"width:30px\" title=\"Minuten\" value=\"");
  strcati2( HTML_String, Uhrzeit_MM); 
  strcat( HTML_String, "\">");
  
  strcat( HTML_String, " : ");
  
  strcat( HTML_String, "<input type=\"number\" name=\"SECONDS\" min=\"0\" max=\"59\" style=\"width:30px\" title=\"Sekunden\" value=\"");
  strcati2( HTML_String, Uhrzeit_SS); 
  strcat( HTML_String, "\">");

  strcat( HTML_String, " HMS");

//  strcat( HTML_String, "<td><input type=\"time\"   style= \"width:100px\" name=\"UHRZEIT\" value=\"");
//  strcati2( HTML_String, Uhrzeit_HH);  // Original
//  strcat( HTML_String, ":");
//  strcati2( HTML_String, Uhrzeit_MM);
//  strcat( HTML_String, ":");
//  strcati2( HTML_String, Uhrzeit_SS);
  strcat( HTML_String, "</td>");

//-----------------------------------------------

  strcat( HTML_String, "<tr> </tr>");  strcat( HTML_String, "<td> </td>");

  strcat( HTML_String, "<td><button style= \"width:100px;height:50px;BACKGROUND-COLOR: Green\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_START);
  strcat( HTML_String, "\">Start</button></td>");
https://github.com/G6EJD/ESP-Web-Server-v2
  strcat( HTML_String, "<tr> </tr>");  strcat( HTML_String, "<td> </td>");

  strcat( HTML_String, "<td><button style= \"width:100px;height:50px;BACKGROUND-COLOR: Red\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_STOP);
  strcat( HTML_String, "\">Stop/Pause</button></td>");

  strcat( HTML_String, "<tr> </tr>");  strcat( HTML_String, "<td> </td>");

  strcat( HTML_String, "<td><button style= \"width:100px;height:50px;BACKGROUND-COLOR: Yellow\" name=\"ACTION\" value=\"");
  strcati(HTML_String, ACTION_RESET);
  strcat( HTML_String, "\">Reset/Set</button></td>");

  strcat( HTML_String, "</tr>");
  strcat( HTML_String, "</td>");

//-----------------------------------------------

  strcat( HTML_String, "</table>");
  strcat( HTML_String, "</form>");
  strcat( HTML_String, "</body>");
  strcat( HTML_String, "</html>");
}

//--------------------------------------------------------------------------
void send_not_found() {
#ifdef DEBUG
  Serial.println("Sende Not Found");
#endif
  client.print("HTTP/1.1 404 Not Found\r\n\r\n");
  delay(20);
  client.stop();
}

//--------------------------------------------------------------------------
void send_HTML() {
  char my_char;
  int  my_len = strlen(HTML_String);
  int  my_ptr = 0;
  int  my_send = 0;

  //--------------------------------------------------------------------------
  // in Portionen senden
  while ((my_len - my_send) > 0) {
    my_send = my_ptr + MAX_PACKAGE_SIZE;
    if (my_send > my_len) {
      client.print(&HTML_String[my_ptr]);
      delay(20);
#ifdef DEBUG
      Serial.println(&HTML_String[my_ptr]);
#endif
      my_send = my_len;
    } else {
      my_char = HTML_String[my_send];
      // Auf Anfang eines Tags positionieren
      while ( my_char != '<') my_char = HTML_String[--my_send];
      HTML_String[my_send] = 0;
      client.print(&HTML_String[my_ptr]);
      delay(20);
#ifdef DEBUG
      Serial.println(&HTML_String[my_ptr]);
#endif
      HTML_String[my_send] =  my_char;
      my_ptr = my_send;
    }
  }
  client.stop();
}

//----------------------------------------------------------------------------------------------
void set_colgroup(int w1, int w2, int w3, int w4, int w5) {
  strcat( HTML_String, "<colgroup>");
  set_colgroup1(w1);
  set_colgroup1(w2);
  set_colgroup1(w3);
  set_colgroup1(w4);
  set_colgroup1(w5);
  strcat( HTML_String, "</colgroup>");

}
//------------------------------------------------------------------------------------------
void set_colgroup1(int ww) {
  if (ww == 0) return;
  strcat( HTML_String, "<col width=\"");
  strcati( HTML_String, ww);
  strcat( HTML_String, "\">");
}


//---------------------------------------------------------------------
void strcati(char* tx, int i) {
  char tmp[8];

  itoa(i, tmp, 10);
  strcat (tx, tmp);
}

//---------------------------------------------------------------------
void strcati2(char* tx, int i) {
  char tmp[8];

  itoa(i, tmp, 10);
  if (strlen(tmp) < 2) strcat (tx, "0");
  strcat (tx, tmp);
}

//---------------------------------------------------------------------
int Pick_Parameter_Zahl(const char * par, char * str) {
  int myIdx = Find_End(par, str);

  if (myIdx >= 0) return  Pick_Dec(str, myIdx);
  else return -1;
}
//---------------------------------------------------------------------
int Find_End(const char * such, const char * str) {
  int tmp = Find_Start(such, str);
  if (tmp >= 0)tmp += strlen(such);
  return tmp;
}

//---------------------------------------------------------------------
int Find_Start(const char * such, const char * str) {
  int tmp = -1;
  int ww = strlen(str) - strlen(such);
  int ll = strlen(such);

  for (int i = 0; i <= ww && tmp == -1; i++) {
    if (strncmp(such, &str[i], ll) == 0) tmp = i;
  }
  return tmp;
}
//---------------------------------------------------------------------
int Pick_Dec(const char * tx, int idx ) {
  int tmp = 0;

  for (int p = idx; p < idx + 5 && (tx[p] >= '0' && tx[p] <= '9') ; p++) {
    tmp = 10 * tmp + tx[p] - '0';
  }
  return tmp;
}
//----------------------------------------------------------------------------
int Pick_N_Zahl(const char * tx, char separator, byte n) {

  int ll = strlen(tx);
  int tmp = -1;
  byte anz = 1;
  byte i = 0;
  while (i < ll && anz < n) {
    if (tx[i] == separator)anz++;
    i++;
  }
  if (i < ll) return Pick_Dec(tx, i);
  else return -1;
}

//---------------------------------------------------------------------
int Pick_Hex(const char * tx, int idx ) {
  int tmp = 0;

  for (int p = idx; p < idx + 5 && ( (tx[p] >= '0' && tx[p] <= '9') || (tx[p] >= 'A' && tx[p] <= 'F')) ; p++) {
    if (tx[p] <= '9')tmp = 16 * tmp + tx[p] - '0';
    else tmp = 16 * tmp + tx[p] - 55;
  }

  return tmp;
}

//---------------------------------------------------------------------
void Pick_Text(char * tx_ziel, char  * tx_quelle, int max_ziel) {

  int p_ziel = 0;
  int p_quelle = 0;
  int len_quelle = strlen(tx_quelle);

  while (p_ziel < max_ziel && p_quelle < len_quelle && tx_quelle[p_quelle] && tx_quelle[p_quelle] != ' ' && tx_quelle[p_quelle] !=  '&') {
    if (tx_quelle[p_quelle] == '%') {
      tx_ziel[p_ziel] = (HexChar_to_NumChar( tx_quelle[p_quelle + 1]) << 4) + HexChar_to_NumChar(tx_quelle[p_quelle + 2]);
      p_quelle += 2;
    } else if (tx_quelle[p_quelle] == '+') {
      tx_ziel[p_ziel] = ' ';
    }
    else {
      tx_ziel[p_ziel] = tx_quelle[p_quelle];
    }
    p_ziel++;
    p_quelle++;
  }

  tx_ziel[p_ziel] = 0;
}
//---------------------------------------------------------------------
char HexChar_to_NumChar( char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 55;
  return 0;
}

#ifdef DEBUG
//---------------------------------------------------------------------
void exhibit(const char * tx, int v) {
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, unsigned int v) {
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, unsigned long v) {
  Serial.print(tx);
  Serial.println(v);
}
//---------------------------------------------------------------------
void exhibit(const char * tx, const char * v) {
  Serial.print(tx);
  Serial.println(v);
}
#endif
