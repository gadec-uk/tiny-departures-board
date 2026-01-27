/*
 * Tiny Departures Board (c) 2025-2026 Gadec Software
 *
 * https://github.com/gadec-uk/tiny-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 *
 * ESP32 C3 Super Mini Board with 0.91" 128x32 OLED Display Panel with SSD1306 controller on-board.
 *
 * OLED PANEL     ESP32 C3 SUPER MINI
 * GND            G
 * VCC            5v
 * SCK            9
 * SDA            8
 *
 */

// Release version number
#define VERSION_MAJOR 1
#define VERSION_MINOR 0

// Set a safe default - some ESP32 C3 SuperMini boards don't handle high output power WiFi
#define DEFAULT_WIFI_POWER WIFI_POWER_15dBm

#define MAXLINESIZE 20

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <HTTPUpdateGitHub.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <weatherClient.h>
#include <stationData.h>
#include <raildataXmlClient.h>
#include <busDataClient.h>
#include <githubClient.h>
#include <webgui/webgraphics.h>
#include <webgui/index.h>
#include <webgui/keys.h>

#include <time.h>
#include <U8g2lib.h>

#define msDay 86400000 // 86400000 milliseconds in a day
#define msHour 3600000 // 3600000 milliseconds in an hour
#define msMin 60000 // 60000 milliseconds in a second

WebServer server(80);     // Hosting the Web GUI
File fsUploadFile;        // File uploads

// Shorthand for response formats
static const char contentTypeJson[] PROGMEM = "application/json";
static const char contentTypeText[] PROGMEM = "text/plain";
static const char contentTypeHtml[] PROGMEM = "text/html";

// Using NTP to set and maintain the clock
static const char ntpServer[] PROGMEM = "europe.pool.ntp.org";
static struct tm timeinfo;
static const char ukTimezone[] = "GMT0BST,M3.5.0/1,M10.5.0";

// Default hostname
static const char defaultHostname[] = "TinyDeparturesBoard";

// Local firmware updates via /update Web GUI
static const char updatePage[] PROGMEM =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h2>Tiny Departures Board Manual Update</h2><p>Upload a <b>firmware.bin</b> file.</p>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('Progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script></body></html>";

// /upload page
static const char uploadPage[] PROGMEM =
"<html><body style=\"font-family:Helvetica,Arial,sans-serif\">"
"<h2>Upload a file to the file system</h2><form method='post' enctype='multipart/form-data'><input type='file' name='name'>"
"<input class='button' type='submit' value='Upload'></form></body></html>";

// /success page
static const char successPage[] PROGMEM =
"<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h3>Upload completed successfully.</h3>\n"
"<p><a href=\"/dir\">List file system directory</a></p>\n"
"<h2>Upload another file</h2><form method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form>\n"
"</body></html>";

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define DIMMED_BRIGHTNESS 1 // OLED display brightness level when in sleep/screensaver mode

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE,9,8);

// Vertical line positions on the OLED display
#define LINE1 0
#define LINE2 9
#define LINE3 18
#define LINE4 24

//
// Custom fonts - replicas of those used on the real display boards
//
static const uint8_t NatRailTiny7[970] U8G2_FONT_SECTION("NatRailTiny7") =
  "d\0\3\2\3\3\2\4\5\7\7\0\0\7\0\7\0\1\60\2e\3\261 \5\200\70\1!\7\271("
  "\61(\1\42\7\223M\221(\1#\16\275hSJ\222A\251\14J)I\0$\13\275h\225-\265-"
  "I\266\10%\11\275h\241IY'M&\15\275h#\225\244$\221\222(R\2'\5\231,\61(\7"
  "\272\70\243t\12)\11\272\70\21%-\12\0*\13\275hU\251\34\224\245)\2+\12\255i\25F\203"
  "\24F\0,\7\232\70\223(\0-\5\213K\61.\6\222\70\61\4/\13\274X\27I\221\24I\31\0"
  "\60\11\275h\263d\336\222\5\61\11\275h\25\215\235\6\1\62\12\275h\263da\326\66\10\63\13\275h"
  "\263da\244j\311\2\64\14\275h\227II)\31\264\60\1\65\13\275hq\34\322PK\26\0\66\14"
  "\275h\263d\342\220dZ\262\0\67\11\275h\61\210Yc\15\70\14\275h\263dZ\262dZ\262\0\71"
  "\14\275h\263dZ\62\204Z\262\0:\6\241)\21\5;\7\252\70\223*\0<\7\274X\27\65\66="
  "\10\234Z\61\204C\0>\10\274X\21\66\265\1?\13\275h\263da\244\345P\4@\15\275h\263d"
  "J\242\14\311\220.\0A\13\275h\263d\332\60d\266\0B\15\275h\61$\231\66(\231\66(\0C"
  "\12\275h\263db[\262\0D\12\275h\61$\231\267A\1E\13\275hq\14\207$\14\7\1F\12"
  "\275hq\14\207$,\2G\14\275h\263db\62dZ\262\0H\12\275h\221\331\206!\263\5I\10"
  "\273H\261D]\6J\11\275h\331QK\26\0K\14\275h\221IIIK\242J\26L\10\275h\21"
  "\366\70\10M\12\275h\221-K\242\271\5N\13\275h\221MJ\42m\266\0O\11\275h\263d\336\222"
  "\5P\13\275h\61$\231\66(a\21Q\13\275h\263d.\211\24)\1R\14\275h\61$\231\66("
  "\245J\26S\13\275h\263d\352\252%\13\0T\11\275h\61HaO\0U\11\275h\221\371\226,\0"
  "V\12\275h\221yKj\21\0W\12\275h\221\271$Jr\13X\13\275h\221iI\255R\323\2Y"
  "\12\275h\221iI-l\2Z\11\275h\61\210Y\307A[\7\272\70\261t\21\134\12\274X\221i\231"
  "\226i\1]\7\272\70\241t\31^\6\223M\323\0_\6\214X\61\4`\6\222=\21\5a\11\254X"
  "#&C\224\14b\13\274X\221eKd\32\22\0c\10\254X\63d\305\1d\11\274XW\31\42S"
  "\62e\10\254X\243D\303\70f\11\274XU)MY\11g\12\254X\63D\311\66$\0h\11\274X"
  "\221eK\344\24i\7\271(\221\14\2j\13\274X\227\3Y&%\12\0k\13\274X\221\225\224DJ"
  "J\1l\6\271(q\10m\13\255h\241,\211\222hZ\0n\10\254X\261DN\1o\11\254X\243"
  "D\246D\1p\12\254X\261DC\222e\0q\10\254X\63D\311Vr\10\254X\261DZ\15s\11"
  "\254X\63\204\342\220\0t\13\274X\223ESV\211\22\0u\11\254X\21\71%\12\0v\11\255h\221"
  "\331\222Z\4w\13\255h\221)\211\222(]\0x\11\255h\221%\265J-y\12\254X\21I\311\66"
  "$\0z\11\254X\61DmC\0{\15\277\210\241-KO\221EM\6\5|\6\271(q\10}\6"
  "\215k\61\10~\14\276x\63$\241qxH\242\4\14\275h\221d\225b\224D\211R\200\11\344k"
  "\243DR\242\0\201\13\265h\227\14Z\224\15J\6\202\16\276x\63$\241b\211\24c\62$\0\203\10"
  "\225h\221$J\1\0\0\0";

static const uint8_t tinyClockReg5[123] U8G2_FONT_SECTION("tinyClockReg5") =
  "\13\0\3\2\3\3\1\2\4\5\5\0\0\5\0\5\0\0\0\0\0\0b\60\11m=KfK\26\0"
  "\61\7k\71\211T\31\62\11m\35C\232,\341 \63\11m\35C\232\254\203\2\64\12m]RRJ"
  "\6-\1\65\11m\35\307!\35\24\0\66\12m=K\70$Y\262\0\67\10m\35\203\230\265\1\70\12"
  "m=K\226,Y\262\0\71\11m=K\226\254\311\2:\6\332\27\222\0\0\0\0";

static const uint8_t NatRailSmall9[985] U8G2_FONT_SECTION("NatRailSmall9") =
  "b\0\3\2\3\4\2\5\5\7\11\0\0\11\0\11\2\1\71\2r\3\300 \5\0\63\5!\7\71\245"
  "\304\240\4\42\7\33-Eb\11#\16=\245M)I\6\245\62(\245$\1$\14=\245U\266\324\266"
  "$\331\42\0%\14<eE\244H\221\24)R\0&\15=\245\215T\222\222DJ\242H\11'\6\31"
  "\255\304\0(\10;%UR\252\25)\11;%EV\252\224\0*\12-\247ERY,K\3+\12"
  "-\247U\30\15R\30\1,\7\32\341L\242\0-\6\13+\305\0.\6\11\245D\0/\13<e]"
  "$ER$e\0\60\12=\245\315\222yK\26\0\61\10\273\245M\42u\31\62\12=\245\315\222\205Y"
  "\333 \63\14=\245\315\222\205\221\252%\13\0\64\14=\245]&%\245d\320\302\4\65\13=\245\305q"
  "HC-Y\0\66\14=\245\315\222\211C\222i\311\2\67\12=\245\305 f\305&\0\70\14=\245\315"
  "\222i\311\222i\311\2\71\14=\245\315\222i\311\20j\311\2:\6!\247D\24;\7*\345L\252\0"
  "<\10<e]\324\330\0=\10\34i\305\20\16\1>\10<eE\330\324\6?\14=\245\315\222\205\221"
  "\226C\21\0@\14=\245\315\222Y\22eH\27\0A\13=\245\315\222i\303\220\331\2B\15=\245\305"
  "\220d\332\240d\332\240\0C\12=\245\315\222\211m\311\2D\12=\245\305\220d\336\6\5E\13=\245"
  "\305\61\34\222\60\34\4F\12=\245\305\61\34\222\260\10G\14=\245\315\222\211\311\220i\311\2H\12="
  "\245Ef\33\206\314\26I\10;%\305\22u\31J\11=\245eG-Y\0K\15=\245E&%%"
  "-\211*Y\0L\10=\245E\330\343 M\12=\245E\266,\211\346\26N\13=\245E\66)\211\264"
  "\331\2O\12=\245\315\222yK\26\0P\14=\245\305\220d\332\240\204E\0Q\12M\241\315\222yK"
  "\306\64R\15=\245\305\220d\332\240\224*Y\0S\13=\245\315\222\251\253\226,\0T\11=\245\305 "
  "\205=\1U\11=\245E\346[\262\0V\12=\245E\346-\251E\0W\12=\245E\346\222(\311-"
  "X\13=\245E\246%\265JM\13Y\12=\245E\246%\265\260\11Z\12=\245\305 f\35\7\1["
  "\7:\345\304\322E\134\12<eE\246eZ\246\5]\7:e\206\322e^\6\23/M\3_\6\14"
  "c\305\20`\6\22\357D\24a\12-\245\315\232\14Z\62\4b\13=\245EX\61i\332\240\0c\11"
  "-\245\315 V\207\0d\12=\245e\305\264i\311\20e\12-\245\315\222\15C:\4f\12<eU"
  "\245\64e%\0g\14=\241\315\240\331\222!\34\24\0h\12=\245EX\61i\266\0i\10;%M"
  "(\265\14j\13La]\16dmR\242\0k\13<eEVR\22))\5l\10;%\205\324\313"
  "\0m\12-\245M\27%\321\264\0n\11-\245Eb\322l\1o\11-\245\315\222\331\222\5p\14="
  "\241\305\220d\266A\11C\0q\12=\241\315\240\331\222!,r\11-\245Eb\22\213\0s\11-\245"
  "\315\240\36\24\0t\12<eM\26\15IV\24u\11-\245E\346\244(\1v\12-\245EfKj"
  "\21\0w\13-\245E\246$J\242t\1x\12-\245E\226\324*\265\0y\13=\241E\346\226\14\341"
  "\240\0z\11-\245\305\240\265\15\2{\17G#\206\266,=E\26\65\311\222d\11|\6\71\245\304!"
  "}\6\15\253\305 ~\15>\345\315\220\204\306\341!\211\22\0\15=\245E\222U\212Q\22%J\1"
  "\200\11$k\215\22I\211\2\201\14\265%^\62hQ\66(\31\0\0\0\0";

// Service attribution texts
const char nrAttributionn[] = "National Rail Enquiries";
const char btAttribution[] = "Powered by bustimes.org";

//
// GitHub Client for firmware updates
//  - Pass a GitHub token if updates are to be loaded from a private repository
//
github ghUpdate("");

#define DATAUPDATEINTERVAL 150000     // How often we fetch data from National Rail (ms - 2.5 mins) - "default" option
#define FASTDATAUPDATEINTERVAL 45000  // How often we fetch data from National Rail (ms - 45 secs) - "fast" option
#define BUSDATAUPDATEINTERVAL 45000   // How often we fetch data from bustimes.org (ms - 45 secs)

// Bit and bobs
unsigned long timer = 0;
bool weatherEnabled = false;        // Showing weather at station location. Requires an OpenWeatherMap API key.
bool enableBus = false;             // Include Bus services on the board?
bool firmwareUpdates = true;        // Check for and install firmware updates automatically at boot?
int brightness = 50;                // Initial brightness level of the OLED screen
unsigned long lastWiFiReconnect=0;  // Last WiFi reconnection time (millis)
bool firstLoad = true;              // Are we loading for the first time (no station config)?
int prevProgressBarPosition=0;      // Used for progress bar smooth animation
int startupProgressPercent;         // Initialisation progress
bool wifiConnected = false;         // Connected to WiFi?
unsigned long nextDataUpdate = 0;   // Next National Rail update time (millis)
int dataLoadSuccess = 0;            // Count of successful data downloads
int dataLoadFailure = 0;            // Count of failed data downloads
unsigned long lastLoadFailure = 0;  // When the last failure occurred
int dateDay;                        // Day of the month of displayed date
bool noScrolling = false;           // Suppress all horizontal scrolling
bool flipScreen = false;            // Rotate screen 180deg
String timezone = "";               // custom (non UK) timezone for the clock
bool apiKeys = false;               // Does apikeys.json exist?

char hostname[33];                  // Network hostname (mDNS)
char myUrl[24];                     // Stores the board's own url

// WiFi Manager status
bool wifiConfigured = false;        // Is WiFi configured successfully?

// Station Board Data
char nrToken[37] = "";              // National Rail Darwin Lite Tokens are in the format nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn, where each 'n' represents a hexadecimal character (0-9 or a-f).
char crsCode[4] = "";               // Station code (3 character)
float stationLat=0;                 // Selected station Latitude/Longitude (used to get weather for the location)
float stationLon=0;
char callingCrsCode[4] = "";        // Station code to filter routes on
char callingStation[45] = "";       // Calling filter station friendly name
char platformFilter[MAXPLATFORMFILTERSIZE]; // CSV list of platforms to filter on
char cleanPlatformFilter[MAXPLATFORMFILTERSIZE]; // Cleaned up platform filter (for performance)
char busAtco[13]="";                // Bus Stop ATCO location
String busName="";                  // Bus Stop long name
int busDestX;                       // Variable margin for bus destination
char busFilter[25]="";              // CSV list of services to filter on
char cleanBusFilter[25];            // Cleaned up bus filter (for performance)
float busLat=0;                     // Bus stop Latitude/Longitude (used to get weather for the location)
float busLon=0;

// tiny board has two possible modes.
enum boardModes {
  MODE_RAIL = 0,
  MODE_BUS = 1
};
boardModes boardMode = MODE_RAIL;

// Coach class availability
static const char firstClassSeating[] PROGMEM = " First class seating only.";
static const char standardClassSeating[] PROGMEM = " Standard class seating only.";
static const char dualClassSeating[] PROGMEM = " First and Standard class seating available.";

// Animation vars
int numMessages=0;
int scrollStopsXpos = 0;
int scrollStopsYpos = 0;
int scrollStopsLength = 0;
bool isScrollingStops = false;
int currentMessage = 0;
int prevMessage = 0;
int prevScrollStopsLength = 0;
char line2[4+MAXBOARDMESSAGES][MAXCALLINGSIZE+12];

// Line 3 (additional services)
int line3Service = 0;
int scrollServiceYpos = 0;
bool isScrollingService = false;
int prevService = 0;
bool isShowingVia=false;
unsigned long serviceTimer=0;
unsigned long viaTimer=0;
bool showingMessage = false;
int scrollPrimaryYpos = 0;
bool isScrollingPrimary = false;

char displayedTime[29] = "";        // The currently displayed time
unsigned long nextClockUpdate = 0;  // Next time we need to check/update the clock display
int fpsDelay=25;                    // Total ms between text movement (for smooth animation)
unsigned long refreshTimer = 0;

// Weather Stuff
char weatherMsg[46];                            // Current weather at station location
unsigned long nextWeatherUpdate = 0;            // When the next weather update is due
String openWeatherMapApiKey = "";               // The API key to use
weatherClient currentWeather;                   // Create a weather client

bool noDataLoaded = true;                       // True if no data received for the station
int lastUpdateResult = 0;                       // Result of last data refresh
unsigned long lastDataLoadTime = 0;             // Timestamp of last data load
long apiRefreshRate = DATAUPDATEINTERVAL;       // User selected refresh rate for National Rail API

#define MAXHOSTSIZE 48                          // Maximum size of the wsdl Host
#define MAXAPIURLSIZE 48                        // Maximum size of the wsdl url

char wsdlHost[MAXHOSTSIZE];                     // wsdl Host name
char wsdlAPI[MAXAPIURLSIZE];                    // wsdl API url

// RailData XML Client
raildataXmlClient* raildata = nullptr;
// Bus Client
busDataClient* busdata = nullptr;
// Station Data (shared)
rdStation station;
// Station Messages (shared)
stnMessages messages;

/*
 * Graphics helper functions for OLED panel
*/
void blankArea(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x,y,w,h);
  u8g2.setDrawColor(1);
}

int getStringWidth(const char *message) {
  return u8g2.getStrWidth(message);
}

int getStringWidth(const __FlashStringHelper *message) {
  String temp = String(message);
  char buff[temp.length()+1];
  temp.toCharArray(buff,sizeof(buff));
  return u8g2.getStrWidth(buff);
}

void drawTruncatedText(const char *message, int line) {
  char buff[strlen(message)+4];
  int maxWidth = SCREEN_WIDTH - 6;
  strcpy(buff,message);
  int i = strlen(buff);
  while (u8g2.getStrWidth(buff)>maxWidth && i) buff[i--] = '\0';
  strcat(buff,"\x83");
  u8g2.drawStr(0,line-1,buff);
}

void centreText(const char *message, int line) {
  int width = u8g2.getStrWidth(message);
  if (width<=SCREEN_WIDTH) u8g2.drawStr((SCREEN_WIDTH-width)/2,line-1,message);
  else drawTruncatedText(message,line);
}

void centreText(const __FlashStringHelper *message, int line) {
  String temp = String(message);
  char buff[temp.length()+1];
  temp.toCharArray(buff,sizeof(buff));
  int width = u8g2.getStrWidth(buff);
  if (width<=SCREEN_WIDTH) u8g2.drawStr((SCREEN_WIDTH-width)/2,line-1,buff);
  else drawTruncatedText(buff,line);
}

void drawProgressBar(int percent) {
  int newPosition = percent;
  u8g2.drawFrame(13,13,102,8);
  if (prevProgressBarPosition>newPosition) {
    for (int i=prevProgressBarPosition;i>=newPosition;i--) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(14,14,100,6);
      u8g2.setDrawColor(1);
      u8g2.drawBox(14,14,i,6);
      u8g2.updateDisplayArea(0,0,16,3);
      delay(5);
    }
  } else {
    for (int i=prevProgressBarPosition;i<=newPosition;i++) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(14,14,100,6);
      u8g2.setDrawColor(1);
      u8g2.drawBox(14,14,i,6);
      u8g2.updateDisplayArea(0,0,16,3);
      delay(5);
    }
  }
  prevProgressBarPosition=newPosition;
}

void progressBar(const char *text, int percent) {
  u8g2.setFont(NatRailTiny7);
  blankArea(0,0,128,24);
  centreText(text,0);
  drawProgressBar(percent);
}

void progressBar(const __FlashStringHelper *text, int percent) {
  u8g2.setFont(NatRailTiny7);
  blankArea(0,0,128,24);
  centreText(text,0);
  drawProgressBar(percent);
}

void drawBuildTime() {
  char buildtime[10];
  sprintf(buildtime,"v%d.%d",VERSION_MAJOR,VERSION_MINOR);
  u8g2.drawStr(0,24,buildtime);
}

// Draw the clock (if the time has changed)
void drawCurrentTime(bool update) {
  char sysTime[29];
  getLocalTime(&timeinfo);

  sprintf(sysTime,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  if (strcmp(displayedTime,sysTime)) {
    u8g2.setFont(tinyClockReg5);
    // Clock width is 42 pixels
    blankArea(43,27,42,5);
    u8g2.drawStr(43,26,sysTime);
    u8g2.setFont(NatRailTiny7);
    if (update) u8g2.updateDisplayArea(4,3,7,1);
    strcpy(displayedTime,sysTime);
    u8g2.setFont(NatRailTiny7);
  }
}

void showUpdateIcon(bool show) {
  if (show) {
    u8g2.drawStr(0,23,"\x81");
  } else {
    blankArea(0,25,7,6);
  }
  u8g2.updateDisplayArea(0,3,1,1);
}

/*
 * Setup / Notification Screen Layouts
*/
void showSetupScreen() {
  u8g2.clearBuffer();
  centreText(F("WiFi Setup. Connect to"),0);
  centreText(F("\"Departures Board\""),8);
  centreText(F("and then go to"),16);
  centreText(F("http://192.168.4.1"),24);
  u8g2.sendBuffer();
}

void showNoDataScreen() {
  u8g2.clearBuffer();
  centreText(F("No data for the selected"),0);
  centreText(F("location is available."),8);
  u8g2.sendBuffer();
}

void showSetupKeysHelpScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailSmall9);
  centreText(F("Next, enter your"),0);
  centreText(F("API keys at:"),10);
  centreText(myUrl,20);
  u8g2.setFont(NatRailTiny7);
  u8g2.sendBuffer();
}

void showSetupCrsHelpScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailSmall9);
  centreText(F("Next, select a"),0);
  if (nrToken[0]) centreText(F("station or bus stop at:"),10);
  else centreText(F("bus stop at:"),10);
  centreText(myUrl,20);
  u8g2.setFont(NatRailTiny7);
  u8g2.sendBuffer();
}

void showWsdlFailureScreen() {
  u8g2.clearBuffer();
  centreText(F("NatRail data feed is"),0);
  centreText(F("unavailable. The board"),8);
  centreText(F("cannot be loaded."),16);
  centreText(F("Try again later. :("),24);
  u8g2.sendBuffer();
}

void showTokenErrorScreen() {
  char msg[60];
  u8g2.clearBuffer();
  switch (boardMode) {
    case MODE_RAIL:
      centreText(F("NatRail access denied."),0);
      strcpy(nrToken,"");
      break;
    case MODE_BUS:
      centreText(F("Bustimes.org access denied."),0);
      break;
  }
  centreText(F("Check you have entered your"),8);
  centreText(F("API keys correctly at:"),16);
  sprintf(msg,"%s/keys.htm",myUrl);
  centreText(msg,24);
  u8g2.sendBuffer();
}

void showCRSErrorScreen() {
  char msg[60];
  u8g2.clearBuffer();
  switch (boardMode) {
    case MODE_RAIL:
      sprintf(msg,"Station code \"%s\"",crsCode);
      break;
    case MODE_BUS:
      sprintf(msg,"The ATCO \"%s\"",busAtco);
      break;
  }
  centreText(msg,0);
  centreText(F("is not valid. Select a"),8);
  centreText(F("valid code at:"),16);
  centreText(myUrl,24);
  u8g2.sendBuffer();
}

void showFirmwareUpdateWarningScreen(int secs) {
  char countdown[60];
  u8g2.clearBuffer();
  centreText(F("Firmware Update Available"),0);
  centreText(F("The update will begin"),8);
  sprintf(countdown,"installing in %d seconds.",secs);
  centreText(countdown,16);
  centreText(F("*DO NOT REMOVE POWER*"),24);
  u8g2.sendBuffer();
}

void showFirmwareUpdateProgress(int percent) {
  u8g2.clearBuffer();
  progressBar(F("Updating Firmware"),percent);
  centreText(F("*DO NOT REMOVE POWER*"),24);
  u8g2.sendBuffer();
}

void showUpdateCompleteScreen(const char *msg1, const char *msg2, const char *msg3, int secs, bool showReboot) {
  char countdown[60];
  u8g2.clearBuffer();
  centreText(msg1,0);
  centreText(msg2,8);
  centreText(msg3,16);
  if (showReboot) sprintf(countdown,"Restarting in %d seconds",secs);
  else sprintf(countdown,"Continuing in %d seconds",secs);
  centreText(countdown,24);
  u8g2.sendBuffer();
}

/*
 * Utility functions
*/

// Saves a file (string) to the FFS
bool saveFile(String fName, String fData) {
  File f = LittleFS.open(fName,"w");
  if (f) {
    f.println(fData);
    f.close();
    return true;
  } else return false;
}

// Loads a file (string) from the FFS
String loadFile(String fName) {
  File f = LittleFS.open(fName,"r");
  if (f) {
    String result = f.readString();
    f.close();
    return result;
  } else return "";
}

// Get the Build Timestamp of the running firmware
String getBuildTime() {
  char timestamp[22];
  char buildtime[11];
  struct tm tm = {};

  sprintf(timestamp,"%s %s",__DATE__,__TIME__);
  strptime(timestamp,"%b %d %Y %H:%M:%S",&tm);
  sprintf(buildtime,"%02d%02d%02d%02d%02d",tm.tm_year-100,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
  return String(buildtime);
}

void checkPostWebUpgrade() {
  String prevGUI = loadFile(F("/webver"));
  prevGUI.trim();
  String currentGUI = String(WEBAPPVER_MAJOR) + F(".") + String(WEBAPPVER_MINOR);
  if (prevGUI != currentGUI) {
    // clean up old/dev files
    progressBar(F("Cleaning up..."),45);
    LittleFS.remove(F("/index_d.htm"));
    LittleFS.remove(F("/index.htm"));
    LittleFS.remove(F("/keys.htm"));
    LittleFS.remove(F("/nrelogo.webp"));
    LittleFS.remove(F("/btlogo.webp"));
    LittleFS.remove(F("/nr.webp"));
    LittleFS.remove(F("/favicon.png"));
    saveFile(F("/webver"),currentGUI);
  }
}

// Check if the NR clock needs to be updated
void doClockCheck() {
  if (!firstLoad) {
    if (millis()>nextClockUpdate) {
      drawCurrentTime(true);
      nextClockUpdate=millis()+500;
    }
  }
}

// Callback from the raildataXMLclient library when processing data. As this can take some time, this callback is used to keep the clock working
// and to provide progress on the initial load at boot
void raildataCallback(int stage, int nServices) {
  if (firstLoad) {
    int percent = ((nServices*20)/MAXBOARDSERVICES)+80;
    progressBar(F("Initialising Nat'l Rail"),percent);
  } else doClockCheck();
}

// Stores/updates the url of our Web GUI
void updateMyUrl() {
  IPAddress ip = WiFi.localIP();
  snprintf(myUrl,sizeof(myUrl),"http://%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
}

/*
 * Start-up configuration functions
 */

// Load the API keys from the file system (if they exist)
void loadApiKeys() {
  JsonDocument doc;

  if (LittleFS.exists(F("/apikeys.json"))) {
    File file = LittleFS.open(F("/apikeys.json"), "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings[F("nrToken")].is<const char*>()) {
          strlcpy(nrToken, settings[F("nrToken")], sizeof(nrToken));
        }

        if (settings[F("owmToken")].is<const char*>()) {
          openWeatherMapApiKey = settings[F("owmToken")].as<String>();
        }
        apiKeys = true;

      } else {
        // JSON deserialization failed - TODO
      }
      file.close();
    }
  }
}

// Write a default config file so that the Web GUI works initially (force Tube mode if no NR token)
void writeDefaultConfig() {
    String defaultConfig = "{\"crs\":\"\",\"station\":\"\",\"lat\":0,\"lon\":0,\"weather\":" + String((openWeatherMapApiKey.length())?"true":"false") + F(",\"showBus\":false,\"update\":true,\"brightness\":20,\"mode\":") + String((!nrToken[0])?"1":"0") + "}";
    saveFile(F("/config.json"),defaultConfig);
    strcpy(crsCode,"");
}

// Load the configuration settings (if they exist, if not create a default set for the Web GUI page to read)
void loadConfig() {
  JsonDocument doc;

  // Set defaults
  strcpy(hostname,defaultHostname);
  timezone = String(ukTimezone);

  if (LittleFS.exists(F("/config.json"))) {
    File file = LittleFS.open(F("/config.json"), "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings[F("crs")].is<const char*>())        strlcpy(crsCode, settings[F("crs")], sizeof(crsCode));
        if (settings[F("callingCrs")].is<const char*>()) strlcpy(callingCrsCode, settings[F("callingCrs")], sizeof(callingCrsCode));
        if (settings[F("callingStation")].is<const char*>()) strlcpy(callingStation, settings[F("callingStation")], sizeof(callingStation));
        if (settings[F("platformFilter")].is<const char*>())  strlcpy(platformFilter, settings[F("platformFilter")], sizeof(platformFilter));
        if (settings[F("hostname")].is<const char*>())   strlcpy(hostname, settings[F("hostname")], sizeof(hostname));
        if (settings[F("wsdlHost")].is<const char*>())   strlcpy(wsdlHost, settings[F("wsdlHost")], sizeof(wsdlHost));
        if (settings[F("wsdlAPI")].is<const char*>())    strlcpy(wsdlAPI, settings[F("wsdlAPI")], sizeof(wsdlAPI));
        if (settings[F("showBus")].is<bool>())           enableBus = settings[F("showBus")];
        if (settings[F("fastRefresh")].is<bool>())       apiRefreshRate = settings[F("fastRefresh")] ? FASTDATAUPDATEINTERVAL : DATAUPDATEINTERVAL;
        if (settings[F("weather")].is<bool>() && openWeatherMapApiKey.length())
                                                    weatherEnabled = settings[F("weather")];
        if (settings[F("update")].is<bool>())            firmwareUpdates = settings[F("update")];
        if (settings[F("brightness")].is<int>())         brightness = settings[F("brightness")];
        if (settings[F("lat")].is<float>())              stationLat = settings[F("lat")];
        if (settings[F("lon")].is<float>())              stationLon = settings[F("lon")];

        if (settings[F("mode")].is<int>())               boardMode = settings[F("mode")];

        if (settings[F("busId")].is<const char*>())      strlcpy(busAtco, settings[F("busId")], sizeof(busAtco));
        if (settings[F("busName")].is<const char*>())    busName = String(settings[F("busName")]);
        if (settings[F("busLat")].is<float>())           busLat = settings[F("busLat")];
        if (settings[F("busLon")].is<float>())           busLon = settings[F("busLon")];
        if (settings[F("busFilter")].is<const char*>())  strlcpy(busFilter, settings[F("busFilter")], sizeof(busFilter));

        if (settings[F("noScroll")].is<bool>())          noScrolling = settings[F("noScroll")];
        if (settings[F("flip")].is<bool>())              flipScreen = settings[F("flip")];
        if (settings[F("TZ")].is<const char*>())         timezone = settings[F("TZ")].as<String>();
      } else {
        // JSON deserialization failed - TODO
      }
      file.close();
    }
  } else if (nrToken[0]) writeDefaultConfig();
}

// Soft reset/reload
void softResetBoard() {
  int previousMode = boardMode;

  // Reload the settings
  loadConfig();
  if (flipScreen) u8g2.setFlipMode(0); else u8g2.setFlipMode(1);
  if (timezone!="") {
    setenv("TZ",timezone.c_str(),1);
  } else {
    setenv("TZ",ukTimezone,1);
  }
  tzset();
  u8g2.clearBuffer();
  drawBuildTime();
  u8g2.updateDisplay();

  // Force an update asap
  nextDataUpdate = 0;
  nextWeatherUpdate = 0;
  isScrollingService = false;
  isScrollingStops = false;
  isScrollingPrimary = false;
  firstLoad=true;
  noDataLoaded=true;
  viaTimer=0;
  timer=0;
  prevProgressBarPosition=70;
  startupProgressPercent=70;
  currentMessage=0;
  prevMessage=0;
  prevScrollStopsLength=0;
  isShowingVia=false;
  line3Service=0;
  prevService=0;
  if (!weatherEnabled) strcpy(weatherMsg,"");
  if (previousMode!=boardMode) {
    // Board mode has changed!
    switch (previousMode) {
      case MODE_RAIL:
        // Delete the NR client from memory
        delete raildata;
        raildata = nullptr;
        break;

      case MODE_BUS:
        // Delete the Bus client from memory
        delete busdata;
        busdata = nullptr;
        break;
    }

    switch (boardMode) {
      case MODE_RAIL:
        // Create the NR client
        raildata = new raildataXmlClient();
        if (boardMode == MODE_RAIL) {
          int res = raildata->init(wsdlHost, wsdlAPI, &raildataCallback);
          if (res != UPD_SUCCESS) {
            showWsdlFailureScreen();
             while (true) { server.handleClient(); yield();}
          }
        }
        break;

      case MODE_BUS:
        // Create the Bus client
        busdata = new busDataClient();
        break;
    }
  }

  switch (boardMode) {
    case MODE_RAIL:
      // Create a cleaned platform filter (if any)
      raildata->cleanFilter(platformFilter,cleanPlatformFilter,sizeof(platformFilter));
      break;

    case MODE_BUS:
      progressBar(F("Initialising BusTimes"),70);
      // Create a cleaned filter
      busdata->cleanFilter(busFilter,cleanBusFilter,sizeof(busFilter));
      break;
  }
  station.numServices=0;
  messages.numMessages=0;
}

// WiFiManager callback, entered config mode
void wmConfigModeCallback (WiFiManager *myWiFiManager) {
  showSetupScreen();
  wifiConfigured = true;
}

/*
 * Firmware / Web GUI Update functions
*/
bool isFirmwareUpdateAvailable() {
  int releaseMajor = ghUpdate.releaseId.substring(1,ghUpdate.releaseId.indexOf(".")).toInt();
  int releaseMinor = ghUpdate.releaseId.substring(ghUpdate.releaseId.indexOf(".")+1,ghUpdate.releaseId.indexOf("-")).toInt();
  if (VERSION_MAJOR > releaseMajor) return false;
  if ((VERSION_MAJOR == releaseMajor) && (VERSION_MINOR >= releaseMinor)) return false;
  return true;
}

// Callback function for displaying firmware update progress
void update_progress(int cur, int total) {
  int percent = ((cur * 100)/total);
  showFirmwareUpdateProgress(percent);
}

// Attempts to install newer firmware if available
bool checkForFirmwareUpdate() {
  bool result = true;

  if (!isFirmwareUpdateAvailable()) return result;

  // Find the firmware binary in the release assets
  String updatePath="";
  for (int i=0;i<ghUpdate.releaseAssets;i++){
    if (ghUpdate.releaseAssetName[i] == "firmware.bin") {
      updatePath = ghUpdate.releaseAssetURL[i];
      break;
    }
  }
  if (updatePath.length()==0) {
    //  No firmware binary in release assets
    return result;
  }

  unsigned long tmr=millis()+1000;
  for (int i=30;i>=0;i--) {
    showFirmwareUpdateWarningScreen(i);
    while (tmr>millis()) {
      yield();
      server.handleClient();
    }
    tmr=millis()+1000;
  }
  u8g2.clearDisplay();
  prevProgressBarPosition=0;
  showFirmwareUpdateProgress(0);  // So we don't have a blank screen
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.onProgress(update_progress);
  httpUpdate.rebootOnUpdate(false); // Don't auto reboot, we'll handle it

  HTTPUpdateResult ret = httpUpdate.handleUpdate(client, updatePath, ghUpdate.accessToken);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      char msg[60];
      sprintf(msg,"with error code %d.",httpUpdate.getLastError());
      result=false;
      for (int i=20;i>=0;i--) {
        showUpdateCompleteScreen("Firmware update failed",msg,"",i,false);
        delay(1000);
      }
      break;

    case HTTP_UPDATE_NO_UPDATES:
      for (int i=10;i>=0;i--) {
        showUpdateCompleteScreen("Firmware Update.","No firmware updates","were available.",i,false);
        delay(1000);
      }
      break;

    case HTTP_UPDATE_OK:
      for (int i=20;i>=0;i--) {
        showUpdateCompleteScreen("The firmware update","completed successfully","",i,true);
        delay(1000);
      }
      ESP.restart();
      break;
  }
  u8g2.clearDisplay();
  drawBuildTime();
  u8g2.sendBuffer();
  return result;
}

/*
 * Station Board functions - pulling updates and animating the Departures Board main display
 */

// Request a data update via the raildataClient
bool getStationBoard() {
  if (!firstLoad) showUpdateIcon(true);
  lastUpdateResult = raildata->updateDepartures(&station,&messages,crsCode,nrToken,MAXBOARDSERVICES,enableBus,callingCrsCode,cleanPlatformFilter);
  nextDataUpdate = millis()+apiRefreshRate;
  if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_NO_CHANGE) {
    showUpdateIcon(false);
    lastDataLoadTime=millis();
    noDataLoaded=false;
    dataLoadSuccess++;
    return true;
  } else if (lastUpdateResult == UPD_DATA_ERROR || lastUpdateResult == UPD_TIMEOUT) {
    lastLoadFailure=millis();
    dataLoadFailure++;
    nextDataUpdate = millis() + 30000; // 30 secs
    showUpdateIcon(false);
    return false;
  } else if (lastUpdateResult == UPD_UNAUTHORISED) {
    showTokenErrorScreen();
    while (true) { server.handleClient(); yield();}
  } else {
    showUpdateIcon(false);
    dataLoadFailure++;
    return false;
  }
}

// Draw the primary service line
void drawPrimaryService(bool showVia) {
  int destPos;
  char clipDestination[MAXLOCATIONSIZE];
  char etd[16];

  blankArea(0,LINE1,SCREEN_WIDTH,LINE2-LINE1);
  destPos = u8g2.drawStr(0,LINE1-1,station.service[0].sTime) + 3;
  if (isDigit(station.service[0].etd[0])) sprintf(etd,"Exp %s",station.service[0].etd);
  else strcpy(etd,station.service[0].etd);
  int etdWidth = getStringWidth(etd);
  u8g2.drawStr(SCREEN_WIDTH - etdWidth,LINE1-1,etd);
  // Space available for destination name
  int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 3;
  if (showVia) strcpy(clipDestination,station.service[0].via);
  else strcpy(clipDestination,station.service[0].destination);
  if (getStringWidth(clipDestination) > spaceAvailable) {
    while (getStringWidth(clipDestination) > (spaceAvailable - 6)) {
      clipDestination[strlen(clipDestination)-1] = '\0';
    }
    // check if there's a trailing space left
    if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
    strcat(clipDestination,"\x83");
  }
  u8g2.drawStr(destPos,LINE1-1,clipDestination);
}

// Draw the secondary service line
void drawServiceLine(int line, int y) {
  char clipDestination[30];
  blankArea(0,y,SCREEN_WIDTH,7);

  if (line<station.numServices) {
    int destPos = u8g2.drawStr(0,y-1,station.service[line].sTime) + 3;
    char etd[16];
    if (isDigit(station.service[line].etd[0])) sprintf(etd,"Exp %s",station.service[line].etd);
    else strcpy(etd,station.service[line].etd);
    int etdWidth = getStringWidth(etd);
    u8g2.drawStr(SCREEN_WIDTH - etdWidth,y-1,etd);
    // work out if we need to clip the destination
    strcpy(clipDestination,station.service[line].destination);
    int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 3;
    if (getStringWidth(clipDestination) > spaceAvailable) {
      while (getStringWidth(clipDestination) > spaceAvailable - 6) {
        clipDestination[strlen(clipDestination)-1] = '\0';
      }
      // check if there's a trailing space left
      if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
      strcat(clipDestination,"\x83");
    }
    u8g2.drawStr(destPos,y-1,clipDestination);
  } else {
    if (weatherMsg[0] && line==station.numServices) {
      // We're showing the weather
      centreText(weatherMsg,y);
    } else {
      // We're showing the mandatory attribution
      centreText(nrAttributionn,y);
    }
  }
}

// Draw the initial Departures Board
void drawStationBoard() {
  numMessages=0;
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    u8g2.clearBuffer();
    u8g2.setContrast(brightness);
    firstLoad=false;
    line3Service = noScrolling ? 1 : 0;
  } else {
    // Clear the top line
    blankArea(0,LINE1,SCREEN_WIDTH,LINE2-LINE1);
  }

  // Draw the primary service line
  isShowingVia=false;
  viaTimer=millis()+300000;  // effectively don't check for via
  if (station.numServices) {
    drawPrimaryService(false);
    if (station.service[0].via[0]) viaTimer=millis()+4000;
    if (station.service[0].isCancelled) {
      // This train is cancelled
      if (station.serviceMessage[0]) {
        strcpy(line2[0],station.serviceMessage);
        numMessages=1;
      }
    } else {
      // The train is not cancelled
      if (station.service[0].isDelayed && station.serviceMessage[0]) {
        // The train is delayed and there's a reason
        strcpy(line2[0],station.serviceMessage);
        numMessages++;
      }
      if (station.calling[0]) {
        // Add the calling stops message
        sprintf(line2[numMessages],"Calling at: %s",station.calling);
        numMessages++;
      }
      if (strcmp(station.origin, station.location)==0) {
        // Service originates at this station
        if (station.service[0].opco[0]) {
          sprintf(line2[numMessages],"This %s service starts here.",station.service[0].opco);
        } else {
          strcpy(line2[numMessages],"This service starts here.");
        }
        // Add the seating if available
        switch (station.service[0].classesAvailable) {
          case 1:
            strcat(line2[numMessages],firstClassSeating);
            break;
          case 2:
            strcat(line2[numMessages],standardClassSeating);
            break;
          case 3:
            strcat(line2[numMessages],dualClassSeating);
            break;
        }
        numMessages++;
      } else {
        // Service originates elsewhere
        strcpy(line2[numMessages],"");
        if (station.service[0].opco[0]) {
          if (station.origin[0]) {
            sprintf(line2[numMessages],"This is the %s service from %s.",station.service[0].opco,station.origin);
          } else {
            sprintf(line2[numMessages],"This is the %s service.",station.service[0].opco);
          }
        } else {
          if (station.origin[0]) {
            sprintf(line2[numMessages],"This service originated at %s.",station.origin);
          }
        }
        // Add the seating if available
        switch (station.service[0].classesAvailable) {
          case 1:
            strcat(line2[numMessages],firstClassSeating);
            break;
          case 2:
            strcat(line2[numMessages],standardClassSeating);
            break;
          case 3:
            strcat(line2[numMessages],dualClassSeating);
            break;
        }
        if (line2[numMessages][0]) numMessages++;
      }
      if (station.service[0].trainLength) {
        // Add the number of carriages message
        sprintf(line2[numMessages],"This train is formed of %d coaches.",station.service[0].trainLength);
        numMessages++;
      }
    }
    // Add any nrcc messages
    for (int i=0;i<messages.numMessages;i++) {
      strcpy(line2[numMessages],messages.messages[i]);
      numMessages++;
    }
    // Setup for the first message to rollover to
    isScrollingStops=false;
    currentMessage=numMessages-1;
    if (noScrolling && station.numServices>1) {
      drawServiceLine(1,LINE2);
    }
  } else {
    blankArea(0,LINE1,SCREEN_WIDTH,LINE4-LINE1);
    centreText(F("No scheduled services."),LINE1);
    numMessages = messages.numMessages;
    for (int i=0;i<messages.numMessages;i++) {
      strcpy(line2[i],messages.messages[i]);
    }
    // Setup for the first message to rollover to
    isScrollingStops=false;
    currentMessage=numMessages-1;
  }
  u8g2.sendBuffer();
}

// Callback from the busDataClient library when processing data. Shows progress at startup and keeps clock running
void busCallback() {
  if (firstLoad) {
    if (startupProgressPercent<95) {
      startupProgressPercent+=5;
      progressBar(F("Initialising BusTimes"),startupProgressPercent);
    }
  } else if (millis()>nextClockUpdate) {
    nextClockUpdate = millis()+500;
    drawCurrentTime(true);
  }
}

/*
 *
 * Bus Departures Board
 *
 */
bool getBusDeparturesBoard() {
  if (!firstLoad) showUpdateIcon(true);
  lastUpdateResult = busdata->updateDepartures(&station,busAtco,cleanBusFilter,&busCallback);
  nextDataUpdate = millis()+BUSDATAUPDATEINTERVAL; // default update freq
  if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_NO_CHANGE) {
    showUpdateIcon(false);
    lastDataLoadTime=millis();
    noDataLoaded=false;
    dataLoadSuccess++;
    // Work out the max column size for service numbers
    busDestX=0;
    for (int i=0;i<station.numServices;i++) {
      int svcWidth = getStringWidth(station.service[i].via);
      busDestX = (busDestX > svcWidth) ? busDestX : svcWidth;
    }
    busDestX+=3;
    return true;
  } else if (lastUpdateResult == UPD_DATA_ERROR || lastUpdateResult == UPD_TIMEOUT) {
    lastLoadFailure=millis();
    dataLoadFailure++;
    nextDataUpdate = millis() + 30000; // 30 secs
    showUpdateIcon(false);
    return false;
  } else if (lastUpdateResult == UPD_UNAUTHORISED) {
    showTokenErrorScreen();
    while (true) { server.handleClient(); yield();}
  } else {
    showUpdateIcon(false);
    dataLoadFailure++;
    return false;
  }
}

void drawBusService(int serviceId, int y, int destPos) {
  char clipDestination[MAXLOCATIONSIZE];
  char etd[16];

  if (serviceId < station.numServices) {
    blankArea(0,y,SCREEN_WIDTH,8);
    u8g2.drawStr(0,y-1,station.service[serviceId].via);
    if (isDigit(station.service[serviceId].etd[0])) {
      sprintf(etd,"Exp %s",station.service[serviceId].etd);
    } else strcpy(etd,station.service[serviceId].sTime);
    int etdWidth = getStringWidth(etd);
    u8g2.drawStr(SCREEN_WIDTH - etdWidth,y-1,etd);

    // work out if we need to clip the destination
    strcpy(clipDestination,station.service[serviceId].destination);
    int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 3;
    if (getStringWidth(clipDestination) > spaceAvailable) {
      while (getStringWidth(clipDestination) > spaceAvailable - 6) {
        clipDestination[strlen(clipDestination)-1] = '\0';
      }
      // check if there's a trailing space left
      if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
      strcat(clipDestination,"\x83");
    }
    u8g2.drawStr(destPos,y-1,clipDestination);
  }
}

// Draw/update the Bus Departures Board
void drawBusDeparturesBoard() {

  if (line3Service==0) line3Service=1;
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    u8g2.clearBuffer();
    u8g2.setContrast(brightness);
    firstLoad=false;
  } else {
      // Clear the top two lines
      blankArea(0,LINE1,SCREEN_WIDTH,LINE3-LINE1);
  }

  if (station.boardChanged) {
    // prepare to scroll up primary services
    scrollPrimaryYpos = 10;
    isScrollingPrimary = true;
    // reset line3
    if (station.numServices>2) {
      line3Service=2;
    } else {
      line3Service=99;
    }
    currentMessage = -1;
    blankArea(0,LINE3,SCREEN_WIDTH,8);
    serviceTimer=0;
  } else {
    // Draw the primary service line(s)
    if (station.numServices) {
      drawBusService(0,LINE1,busDestX);
      if (station.numServices>1) drawBusService(1,LINE2,busDestX);
    } else {
      centreText(F("No scheduled services"),LINE1-1);
    }
  }
  messages.numMessages=0;
  if (weatherEnabled && weatherMsg[0]) {
    strcpy(line2[messages.numMessages++],weatherMsg);
  }
  strcpy(line2[messages.numMessages++],btAttribution);
  u8g2.sendBuffer();
}

/*
 * Web GUI functions
 */

// Helper function for returning text status messages
void sendResponse(int code, const __FlashStringHelper* msg) {
  server.send(code, contentTypeText, msg);
}

void sendResponse(int code, String msg) {
  server.send(code, contentTypeText, msg);
}

// Return the correct MIME type for a file name
String getContentType(String filename) {
  if (server.hasArg(F("download"))) {
    return F("application/octet-stream");
  } else if (filename.endsWith(F(".htm"))) {
    return F("text/html");
  } else if (filename.endsWith(F(".html"))) {
    return F("text/html");
  } else if (filename.endsWith(F(".css"))) {
    return F("text/css");
  } else if (filename.endsWith(F(".js"))) {
    return F("application/javascript");
  } else if (filename.endsWith(F(".png"))) {
    return F("image/png");
  } else if (filename.endsWith(F(".gif"))) {
    return F("image/gif");
  } else if (filename.endsWith(F(".jpg"))) {
    return F("image/jpeg");
  } else if (filename.endsWith(F(".ico"))) {
    return F("image/x-icon");
  } else if (filename.endsWith(F(".xml"))) {
    return F("text/xml");
  } else if (filename.endsWith(F(".pdf"))) {
    return F("application/x-pdf");
  } else if (filename.endsWith(F(".zip"))) {
    return F("application/x-zip");
  } else if (filename.endsWith(F(".json"))) {
    return F("application/json");
  } else if (filename.endsWith(F(".gz"))) {
    return F("application/x-gzip");
  } else if (filename.endsWith(F(".svg"))) {
    return F("image/svg+xml");
  } else if (filename.endsWith(F(".webp"))) {
    return F("image/webp");
  }
  return F("text/plain");
}

// Stream a file from the file system
bool handleStreamFile(String filename) {
  if (LittleFS.exists(filename)) {
    File file = LittleFS.open(filename,"r");
    String contentType = getContentType(filename);
    server.streamFile(file, contentType);
    file.close();
    return true;
  } else return false;
}

// Stream a file stored in PROGMEM flash (default graphics are now included in the firmware image)
void handleStreamFlashFile(String filename, const uint8_t *filedata, size_t contentLength) {

  String contentType = getContentType(filename);
  WiFiClient client = server.client();
  // Send the headers
  client.println(F("HTTP/1.1 200 OK"));
  client.print(F("Content-Type: "));
  client.println(contentType);
  client.print(F("Content-Length: "));
  client.println(contentLength);
  client.println(F("Connection: close"));
  client.println(); // End of headers

  const size_t chunkSize = 512;
  uint8_t buffer[chunkSize];
  size_t sent = 0;

  while (sent < contentLength) {
    size_t toSend = min(chunkSize, contentLength - sent);
    // Copy from PROGMEM to buffer
    for (size_t i=0;i<toSend;i++) {
      buffer[i] = pgm_read_byte(&filedata[sent + i]);
    }
    client.write(buffer, toSend);
    sent += toSend;
  }
}

// Save the API keys POSTed from the keys.htm page
// If an OWM key is passed, this is tested before being committed to the file system. It's not possible
// to check the National Rail token at this point.
//
void handleSaveKeys() {
  String newJSON, owmToken, nrToken;
  JsonDocument doc;
  bool result = true;
  String msg = F("API keys saved successfully.");

  if ((server.method() == HTTP_POST) && (server.hasArg("plain"))) {
    newJSON = server.arg("plain");
    // Deserialise to get the OWM API key
    DeserializationError error = deserializeJson(doc, newJSON);
    if (!error) {
      JsonObject settings = doc.as<JsonObject>();
      if (settings[F("owmToken")].is<const char*>()) {
        owmToken = settings[F("owmToken")].as<String>();
        if (owmToken.length()) {
          // Check if this is a valid token...
          if (!currentWeather.updateWeather(owmToken, "51.52", "-0.13")) {
            msg = F("The OpenWeather Map API key is not valid. Please check you have copied your key correctly. It may take up to 30 minutes for a newly created key to become active.\n\nNo changes have been saved.");
            result = false;
          }
        }
      }
      if (result) {
        if (!saveFile(F("/apikeys.json"),newJSON)) {
          msg = F("Failed to save the API keys to the file system (file system corrupt or full?)");
          result = false;
        } else {
          nrToken = settings[F("nrToken")].as<String>();
          if (!nrToken.length()) msg+=F("\n\nNote: Only Bus Departures will be available without a National Rail token.");
        }
      }
    } else {
      msg = F("Invalid JSON format. No changes have been saved.");
      result = false;
    }
    if (result) {
      // Load/Update the API Keys in memory
      loadApiKeys();
      // If all location codes are blank we're in the setup process. If not, the keys have been changed so just reboot.
      if (!crsCode[0] && !busAtco[0]) {
        sendResponse(200,msg);
        writeDefaultConfig();
        showSetupCrsHelpScreen();
      } else {
        msg += F("\n\nThe system will now restart.");
        sendResponse(200,msg);
        delay(500);
        ESP.restart();
      }
    } else {
      sendResponse(400,msg);
    }
  } else {
    sendResponse(400,F("Invalid"));
  }
}

// Save configuration setting POSTed from index.htm
void handleSaveSettings() {
  String newJSON;

  if ((server.method() == HTTP_POST) && (server.hasArg("plain"))) {
    newJSON = server.arg("plain");
    saveFile(F("/config.json"),newJSON);
    if ((!crsCode[0] && !busAtco[0]) || (!nrToken[0] && boardMode==MODE_RAIL) || server.hasArg("reboot")) {
      // First time setup or base config change, we need a full reboot
      sendResponse(200,F("Configuration saved. The system will now restart."));
      delay(1000);
      ESP.restart();
    } else {
      sendResponse(200,F("Configuration updated. The system will update shortly."));
      softResetBoard();
    }
  } else {
    // Something went wrong saving the config file
    sendResponse(400,F("The configuration could not be updated. The system will restart."));
    delay(1000);
    ESP.restart();
  }
}

/*
 * Expose the file system via the Web GUI with some basic functions for directory browsing, file reading and deletion.
 */

 // Return storage information
String getFSInfo() {
  char info[70];

  sprintf(info,"Total: %d bytes, Used: %d bytes\n",LittleFS.totalBytes(), LittleFS.usedBytes());
  return String(info);
}

// Send a basic directory listing to the browser
void handleFileList() {
  String path;
  if (!server.hasArg("dir")) path="/"; else path = server.arg("dir");
  File root = LittleFS.open(path);

  String output=F("<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h2>Tiny Departures Board File System</h2>");
  if (!root) {
    output+=F("<p>Failed to open directory</p>");
  } else if (!root.isDirectory()) {
    output+=F("<p>Not a directory</p>");
  } else {
    output+=F("<table>");
    File file = root.openNextFile();
    while (file) {
      output+=F("<tr><td>");
      if (file.isDirectory()) {
        output+="[DIR]</td><td><a href=\"/rmdir?f=" + String(file.path()) + F("\" title=\"Delete\">X</a></td><td><a href=\"/dir?dir=") + String(file.path()) + F("\">") + String(file.name()) + F("</a></td></tr>");
      } else {
        output+=String(file.size()) + F("</td><td><a href=\"/del?f=")+ String(file.path()) + F("\" title=\"Delete\">X</a></td><td><a href=\"/cat?f=") + String(file.path()) + F("\">") + String(file.name()) + F("</a></td></tr>");
      }
      file = root.openNextFile();
    }
  }

  output += F("</table><br>");
  output += getFSInfo() + F("<p><a href=\"/upload\">Upload</a> a file</p></body></html>");
  server.send(200,contentTypeHtml,output);
}

// Stream a file to the browser
void handleCat() {
  String filename;

  if (server.hasArg(F("f"))) {
    handleStreamFile(server.arg("f"));
  } else sendResponse(404,F("Not found"));
}

// Delete a file from the file system
void handleDelete() {
  String filename;

  if (server.hasArg(F("f"))) {
    if (LittleFS.remove(server.arg(F("f")))) {
      // Successfully removed go back to directory listing
      server.sendHeader(F("Location"),F("/dir"));
      server.send(303);
    } else sendResponse(400,F("Failed to delete file"));
  } else sendResponse(404,F("Not found"));

}

// Format the file system
void handleFormatFFS() {
  String message;

  if (LittleFS.format()) {
    message=F("File System was successfully formatted\n\n");
    message+=getFSInfo();
  } else message=F("File System could not be formatted!");
  sendResponse(200,message);
}

// Upload a file from the browser
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith(F("/"))) {
      filename = "/" + filename;
    }
    fsUploadFile = LittleFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    WiFiClient client = server.client();
    if (fsUploadFile) {
      fsUploadFile.close();
      client.println(F("HTTP/1.1 302 Found"));
      client.println(F("Location: /success"));
      client.println(F("Connection: close"));
    } else {
      client.println(F("HTTP/1.1 500 Could not create file"));
      client.println(F("Connection: close"));
    }
    client.println();
  }
}

/*
 * Web GUI handlers
 */

// Fallback function for browser requests
void handleNotFound() {
  if ((LittleFS.exists(server.uri())) && (server.method() == HTTP_GET)) handleStreamFile(server.uri());
  else if (server.uri() == F("/keys.htm")) handleStreamFlashFile(server.uri(), keyshtm, sizeof(keyshtm));
  else if (server.uri() == F("/index.htm")) handleStreamFlashFile(server.uri(), indexhtm, sizeof(indexhtm));
  else if (server.uri() == F("/nrelogo.webp")) handleStreamFlashFile(server.uri(), nrelogo, sizeof(nrelogo));
  else if (server.uri() == F("/btlogo.webp")) handleStreamFlashFile(server.uri(), btlogo, sizeof(btlogo));
  else if (server.uri() == F("/nr.webp")) handleStreamFlashFile(server.uri(), nricon, sizeof(nricon));
  else if (server.uri() == F("/favicon.png")) handleStreamFlashFile(server.uri(), faviconpng, sizeof(faviconpng));
  else sendResponse(404,F("Not Found"));
}

// Send some useful system & station information to the browser
void handleInfo() {
  unsigned long uptime = millis();
  char sysUptime[30];
  int days = uptime / msDay ;
  int hours = (uptime % msDay) / msHour;
  int minutes = ((uptime % msDay) % msHour) / msMin;

  sprintf(sysUptime,"%d days, %d hrs, %d min", days,hours,minutes);

  String message = "Hostname: " + String(hostname) + F("\nFirmware version: v") + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + " " + getBuildTime() + F("\nSystem uptime: ") + String(sysUptime) + F("\nFree Heap: ") + String(ESP.getFreeHeap()) + F("\nFree LittleFS space: ") + String(LittleFS.totalBytes() - LittleFS.usedBytes());
  message+="\nCore Plaform: " + String(ESP.getCoreVersion()) + F("\nCPU speed: ") + String(ESP.getCpuFreqMHz()) + F("MHz\nCPU Temperature: ") + String(temperatureRead()) + F("\nWiFi network: ") + String(WiFi.SSID()) + F("\nWiFi signal strength: ") + String(WiFi.RSSI()) + F("dB");
  getLocalTime(&timeinfo);

  sprintf(sysUptime,"%02d:%02d:%02d %02d/%02d/%04d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
  message+="\nSystem clock: " + String(sysUptime);
  message+="\nCRS station code: " + String(crsCode) + F("\nSuccessful: ") + String(dataLoadSuccess) + F("\nFailures: ") + String(dataLoadFailure) + F("\nTime since last data load: ") + String((int)((millis()-lastDataLoadTime)/1000)) + F(" seconds");
  if (dataLoadFailure) message+="\nTime since last failure: " + String((int)((millis()-lastLoadFailure)/1000)) + F(" seconds");
  message+=F("\nLast Result: ");
  switch (boardMode) {
    case MODE_RAIL:
      message+=raildata->getLastError();
      break;

    case MODE_BUS:
      message+=busdata->lastErrorMsg;
      break;
  }
  message+="\nServices: " + String(station.numServices) + F("\nMessages: ");
  message+=String(messages.numMessages);
  message+=F("\n");
  if (boardMode != MODE_BUS) for (int i=0;i<messages.numMessages;i++) message+=String(messages.messages[i]) + "\n";
  message+=F("\nUpdate result code: ");
  switch (lastUpdateResult) {
    case UPD_SUCCESS:
      message+=F("SUCCESS");
      break;
    case UPD_NO_CHANGE:
      message+=F("SUCCESS (NO CHANGES)");
      break;
    case UPD_DATA_ERROR:
      message+=F("DATA ERROR");
      break;
    case UPD_UNAUTHORISED:
      message+=F("UNAUTHORISED");
      break;
    case UPD_HTTP_ERROR:
      message+=F("HTTP ERROR");
      break;
    case UPD_INCOMPLETE:
      message+=F("INCOMPLETE JSON RECEIVED");
      break;
    case UPD_NO_RESPONSE:
      message+=F("NO RESPONSE FROM SERVER");
      break;
    case UPD_TIMEOUT:
      message+=F("TIMEOUT WAITING FOR SERVER");
      break;
    default:
      message+="ERROR CODE (" + String(lastUpdateResult) + F(")");
      break;
  }
  sendResponse(200,message);
}

// Stream the index.htm page unless we're in first time setup and need the api keys
void handleRoot() {
  if (!apiKeys) {
    if (LittleFS.exists(F("/keys.htm"))) handleStreamFile(F("/keys.htm")); else handleStreamFlashFile(F("/keys.htm"),keyshtm,sizeof(keyshtm));
  } else {
    if (LittleFS.exists(F("/index_d.htm"))) handleStreamFile(F("/index_d.htm")); else handleStreamFlashFile(F("/index.htm"),indexhtm,sizeof(indexhtm));
  }
}

// Send the firmware version to the client (called from index.htm)
void handleFirmwareInfo() {
  String response = "{\"firmware\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "-W" + String(WEBAPPVER_MAJOR) + "." + String(WEBAPPVER_MINOR) + F(" Build:") + getBuildTime() + F("\"}");
  server.send(200,contentTypeJson,response);
}

// Force a reboot of the ESP32
void handleReboot() {
  sendResponse(200,F("The Departures Board is restarting..."));
  delay(1000);
  ESP.restart();
}

// Erase the stored WiFiManager credentials
void handleEraseWiFi() {
  sendResponse(200,F("Erasing stored WiFi. You will need to connect to the \"Departures Board\" network and use WiFi Manager to reconfigure."));
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

// "Factory reset" the app - delete WiFi, format file system and reboot
void handleFactoryReset() {
  sendResponse(200,F("Factory reseting the Departures Board..."));
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  LittleFS.format();
  delay(500);
  ESP.restart();
}

// Interactively change the brightness of the OLED panel (called from index.htm)
void handleBrightness() {
  if (server.hasArg(F("b"))) {
    int level = server.arg(F("b")).toInt();
    if (level>0 && level<256) {
      u8g2.setContrast(level);
      brightness = level;
      sendResponse(200,F("OK"));
      return;
    }
  }
  sendResponse(200,F("invalid request"));
}

// Display the test screen
void handleTestCard() {
  sendResponse(200,F("Displaying alignment screen. Use the \"Restart System\" option when finished."));
  u8g2.clearBuffer();
  u8g2.setFont(NatRailSmall9);
  u8g2.drawFrame(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
  centreText("Screen Alignment Test",5);
  u8g2.setFont(NatRailTiny7);
  centreText("Restart system when done",20);
  u8g2.sendBuffer();
  while (true) {
    server.handleClient();
  }
}

// Web GUI has requested updates be installed
void handleOtaUpdate() {
  sendResponse(200,F("Update initiated - check Departure Board display for progress"));
  delay(500);
  u8g2.clearBuffer();
  centreText(F("Getting latest firmware"),LINE2);
  centreText(F("details from GitHub..."),LINE3);
  u8g2.sendBuffer();

  if (ghUpdate.getLatestRelease()) {
    checkForFirmwareUpdate();
  } else {
    for (int i=15;i>=0;i--) {
      showUpdateCompleteScreen("Firmware check failed.","Unable to retrieve latest","release information.",i,false);
      delay(1000);
    }
    log_e("FW Update failed: %s\n",ghUpdate.getLastError().c_str());
  }
  // Always restart
  ESP.restart();
}

/*
 * External data functions - weather, stationpicker, firmware updates
 */

// Call the National Rail Station Picker (called from index.htm)
void handleStationPicker() {
  if (!server.hasArg(F("q"))) {
    sendResponse(400, F("Missing Query"));
    return;
  }

  String query = server.arg(F("q"));
  if (query.length() <= 2) {
    sendResponse(400, F("Query Too Short"));
    return;
  }

  const char* host = "stationpicker.nationalrail.co.uk";
  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();
  httpsClient.setTimeout(10000);

  int retryCounter = 0;
  while (!httpsClient.connect(host, 443) && retryCounter++ < 20) {
    delay(50);
  }

  if (retryCounter >= 20) {
    sendResponse(408, F("NR Timeout"));
    return;
  }

  httpsClient.print(String("GET /stationPicker/") + query + F(" HTTP/1.0\r\n") +
                    F("Host: stationpicker.nationalrail.co.uk\r\n") +
                    F("Referer: https://www.nationalrail.co.uk\r\n") +
                    F("Origin: https://www.nationalrail.co.uk\r\n") +
                    F("Connection: close\r\n\r\n"));

  // Wait for response header
  retryCounter = 0;
  while (!httpsClient.available() && retryCounter++ < 15) {
    delay(100);
  }

  if (!httpsClient.available()) {
    httpsClient.stop();
    sendResponse(408, F("NRQ Timeout"));
    return;
  }

  // Parse status code
  String statusLine = httpsClient.readStringUntil('\n');
  if (!statusLine.startsWith(F("HTTP/")) || statusLine.indexOf(F("200 OK")) == -1) {
    httpsClient.stop();

    if (statusLine.indexOf(F("401")) > 0) {
      sendResponse(401, F("Not Authorized"));
    } else if (statusLine.indexOf(F("500")) > 0) {
      sendResponse(500, F("Server Error"));
    } else {
      sendResponse(503, statusLine.c_str());
    }
    return;
  }

  // Skip the remaining headers
  while (httpsClient.connected() || httpsClient.available()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Start sending response
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, contentTypeJson, "");

  String buffer;
  unsigned long timeout = millis() + 5000UL;

  while ((httpsClient.connected() || httpsClient.available()) && millis() < timeout) {
    while (httpsClient.available()) {
      char c = httpsClient.read();
      if (c <= 128) buffer += c;
      if (buffer.length() >= 1024) {
        server.sendContent(buffer);
        buffer = "";
        yield();
      }
    }
  }

  // Flush remaining buffer
  if (buffer.length()) {
    server.sendContent(buffer);
  }

  httpsClient.stop();
  server.sendContent("");
  server.client().stop();
}

// Update the current weather message if weather updates are enabled and we have a lat/lon for the selected location
void updateCurrentWeather(float latitude, float longitude) {
  nextWeatherUpdate = millis() + 1200000; // update every 20 mins
  if (!latitude || !longitude) return; // No location co-ordinates
  strcpy(weatherMsg,"");
  bool currentWeatherValid = currentWeather.updateWeather(openWeatherMapApiKey, String(latitude), String(longitude));
  if (currentWeatherValid) {
    currentWeather.currentWeather.toCharArray(weatherMsg,sizeof(weatherMsg));
    weatherMsg[0] = toUpperCase(weatherMsg[0]);
    weatherMsg[sizeof(weatherMsg)-1] = '\0';
  } else {
    nextWeatherUpdate = millis() + 30000; // Try again in 30s
  }
}

/*
 * Setup / Loop functions
*/

//
// The main processing cycle for the National Rail Departures Board
//
void departureBoardLoop() {

  if ((millis() > nextDataUpdate) && (!isScrollingStops) && (!isScrollingService) && (lastUpdateResult != UPD_UNAUTHORISED) && (wifiConnected)) {
    timer = millis() + 2000;
    if (getStationBoard()) {
      if ((lastUpdateResult == UPD_SUCCESS) || (lastUpdateResult == UPD_NO_CHANGE && firstLoad)) drawStationBoard(); // Something changed so redraw the board.
    } else if (lastUpdateResult == UPD_UNAUTHORISED) showTokenErrorScreen();
	  else if (lastUpdateResult == UPD_DATA_ERROR) {
	    if (noDataLoaded) showNoDataScreen();
	    else drawStationBoard();
	  } else if (noDataLoaded) showNoDataScreen();
  } else if (weatherEnabled && (millis()>nextWeatherUpdate) && (!noDataLoaded) && (!isScrollingStops) && (!isScrollingService) && (wifiConnected)) {
    updateCurrentWeather(stationLat,stationLon);
  }

  if (millis()>timer && numMessages && !isScrollingStops && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR && !noScrolling) {
    // Need to start a new scrolling line 2
    prevMessage = currentMessage;
    prevScrollStopsLength = scrollStopsLength;
    currentMessage++;
    if (currentMessage>=numMessages) currentMessage=0;
    scrollStopsXpos=0;
    scrollStopsYpos=10;
    scrollStopsLength = getStringWidth(line2[currentMessage]);
    isScrollingStops=true;
  }

  // Check if there's a via destination
  if (millis()>viaTimer) {
    if (station.numServices && station.service[0].via[0] && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
      isShowingVia = !isShowingVia;
      drawPrimaryService(isShowingVia);
      u8g2.updateDisplayArea(0,0,16,1);
      if (isShowingVia) viaTimer = millis()+3000; else viaTimer = millis()+4000;
    }
  }

  if (millis()>serviceTimer && !isScrollingService && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to change to the next service if there is one
    if (station.numServices <= 1 && !weatherMsg[0]) {
      // There's no other services and no weather so just so static attribution.
      drawServiceLine(1,LINE3); //TODO?
      serviceTimer = millis() + 30000;
      isScrollingService = false;
    } else {
      prevService = line3Service;
      line3Service++;
      if (station.numServices) {
        if ((line3Service>station.numServices && !weatherMsg[0]) || (line3Service>station.numServices+1 && weatherMsg[0])) line3Service=(noScrolling && station.numServices>1) ? 2:1;  // First 'other' service
      } else {
        if (weatherMsg[0] && line3Service>1) line3Service=0;
      }
      scrollServiceYpos=10;
      isScrollingService = true;
    }
  }

  if (isScrollingStops && millis()>timer && !noScrolling) {
    blankArea(0,LINE2,SCREEN_WIDTH,7);
    if (scrollStopsYpos) {
      // we're scrolling up the message initially
      u8g2.setClipWindow(0,LINE2,SCREEN_WIDTH,LINE2+7);
      // if the previous message didn't scroll then we need to scroll it up off the screen
      if (prevScrollStopsLength && prevScrollStopsLength<SCREEN_WIDTH && strncmp("Calling",line2[prevMessage],7)) centreText(line2[prevMessage],scrollStopsYpos+LINE2-10);
      if (scrollStopsLength<SCREEN_WIDTH && strncmp("Calling",line2[currentMessage],7)) centreText(line2[currentMessage],scrollStopsYpos+LINE2); // Centre text if it fits
      else u8g2.drawStr(0,scrollStopsYpos+LINE2-2,line2[currentMessage]);
      u8g2.setMaxClipWindow();
      scrollStopsYpos--;
      if (scrollStopsYpos==0) timer=millis()+1500;
    } else {
      // we're scrolling left
      if (scrollStopsLength<SCREEN_WIDTH && strncmp("Calling",line2[currentMessage],7)) centreText(line2[currentMessage],LINE2); // Centre text if it fits
      else u8g2.drawStr(scrollStopsXpos,LINE2-1,line2[currentMessage]);
      if (scrollStopsLength < SCREEN_WIDTH) {
        // we don't need to scroll this message, it fits so just set a longer timer
        timer=millis()+6000;
        isScrollingStops=false;
      } else {
        scrollStopsXpos--;
        if (scrollStopsXpos < -scrollStopsLength) {
          isScrollingStops=false;
          timer=millis()+500;  // pause before next message
        }
      }
    }
  }

  if (isScrollingService && millis()>serviceTimer) {
    blankArea(0,LINE3,SCREEN_WIDTH,7);
    if (scrollServiceYpos) {
      // we're scrolling the service into view
      u8g2.setClipWindow(0,LINE3,SCREEN_WIDTH,LINE3+7);
      // if the prev service is showing, we need to scroll it up off
      if (prevService>0) drawServiceLine(prevService,scrollServiceYpos+LINE3-10);
      drawServiceLine(line3Service,scrollServiceYpos+LINE3-1);
      u8g2.setMaxClipWindow();
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        serviceTimer=millis()+5000;
        isScrollingService=false;
      }
    }
  }

  // Check if the clock should be updated
  doClockCheck();

  // To ensure a consistent refresh rate (for smooth text scrolling), we update the screen every 25ms (around 40fps)
  // so we need to wait any additional ms not used by processing so far before sending the frame to the display controller
  long delayMs = fpsDelay - (millis()-refreshTimer);
  if (delayMs>0) delay(delayMs);
  u8g2.updateDisplayArea(0,1,16,3);
  refreshTimer=millis();
}

//
// Processing loop for Bus Departures board
//
void busDeparturesLoop() {
  char serviceData[8+MAXLINESIZE+MAXLOCATIONSIZE];
  bool fullRefresh = false;

  if (millis()>nextDataUpdate && !isScrollingService && !isScrollingPrimary && wifiConnected) {
    if (getBusDeparturesBoard()) {
      if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_NO_CHANGE) drawBusDeparturesBoard(); // Something changed so redraw the board.
    } else if (lastUpdateResult == UPD_UNAUTHORISED) showTokenErrorScreen();
	  else if (lastUpdateResult == UPD_DATA_ERROR) {
	    if (noDataLoaded) showNoDataScreen();
	    else drawBusDeparturesBoard();
	  } else if (noDataLoaded) showNoDataScreen();
  } else if (weatherEnabled && millis()>nextWeatherUpdate && !noDataLoaded && !isScrollingService && !isScrollingPrimary && wifiConnected) {
    updateCurrentWeather(busLat,busLon);
    // Update the weather text immediately
    if (weatherMsg[0]) {
      strcpy(line2[1],btAttribution);
      strcpy(line2[0],weatherMsg);
      messages.numMessages=2;
    }
  }

  // Scrolling the additional services
  if (millis()>serviceTimer && !isScrollingPrimary && !isScrollingService && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to change to the next service if there is one
    if (station.numServices<=2 && messages.numMessages==1) {
      // There are no additional services or weather to scroll in so static attribution.
      serviceTimer = millis() + 10000;
      line3Service=station.numServices;
    } else {
      // Need to change to the next service or message
      prevService = line3Service;
      line3Service++;
      scrollServiceYpos=10;
      isScrollingService = true;
      if (line3Service>=station.numServices) {
        // Showing the messages
        prevMessage = currentMessage;
        currentMessage++;
        if (currentMessage>=messages.numMessages) {
          if (station.numServices>2) {
            line3Service = 2;
            currentMessage=-1; // Rollover back to services
          } else {
            line3Service = station.numServices;
            currentMessage=0;
          }
        }
      }
    }
  }

  if (isScrollingService && millis()>serviceTimer) {
    if (scrollServiceYpos) {
      blankArea(0,LINE3,SCREEN_WIDTH,8);
      // we're scrolling up the message
      u8g2.setClipWindow(0,LINE3,SCREEN_WIDTH,LINE3+7);
      // Was the previous display a service?
      if (prevService<station.numServices) {
        drawBusService(prevService,scrollServiceYpos+LINE3-10,busDestX);
      } else {
        // Scrolling up the previous message
        centreText(line2[prevMessage],scrollServiceYpos+LINE3-10);
      }
      // Is this entry a service?
      if (line3Service<station.numServices) {
        drawBusService(line3Service,scrollServiceYpos+LINE3-1,busDestX);
      } else {
        centreText(line2[currentMessage],scrollServiceYpos+LINE3-1);
      }
      u8g2.setMaxClipWindow();
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        serviceTimer = millis()+2800;
        if (station.numServices<=2) serviceTimer+=3000;
      }
    } else isScrollingService=false;
  }

  if (isScrollingPrimary) {
    blankArea(0,LINE1,SCREEN_WIDTH,LINE3-LINE1+8);
    fullRefresh = true;
    // we're scrolling the primary service(s) into view
    u8g2.setClipWindow(0,LINE1,SCREEN_WIDTH,LINE1+7);
    if (station.numServices) drawBusService(0,scrollPrimaryYpos+LINE1-1,busDestX);
    else centreText(F("No scheduled services"),scrollPrimaryYpos+LINE1);
    if (station.numServices>1) {
      u8g2.setClipWindow(0,LINE2,SCREEN_WIDTH,LINE2+7);
      drawBusService(1,scrollPrimaryYpos+LINE2-1,busDestX);
    }
    if (station.numServices>2) {
      u8g2.setClipWindow(0,LINE3,SCREEN_WIDTH,LINE3+7);
      drawBusService(2,scrollPrimaryYpos+LINE3-1,busDestX);
    } else if (station.numServices<3 && messages.numMessages==1) {
      // scroll up the attribution once...
      u8g2.setClipWindow(0,LINE3,SCREEN_WIDTH,LINE3+7);
      centreText(btAttribution,scrollPrimaryYpos+LINE3-1);
    }
    u8g2.setMaxClipWindow();
    scrollPrimaryYpos--;
    if (scrollPrimaryYpos==0) {
      isScrollingPrimary=false;
      serviceTimer = millis()+2800;
    }
  }

  // Check if the clock should be updated
  if (millis()>nextClockUpdate) {
    nextClockUpdate = millis()+500;
    drawCurrentTime(true);    // just use the Tube clock for bus mode
  }

  long delayMs = 40 - (millis()-refreshTimer);
  if (delayMs>0) delay(delayMs);
  if (fullRefresh) u8g2.sendBuffer(); else u8g2.updateDisplayArea(0,1,16,3);
  refreshTimer=millis();
}

//
// Setup code
//
void setup(void) {
  // These are the default wsdl XML SOAP entry points. They can be overridden in the config.json file if necessary
  strncpy(wsdlHost,"lite.realtime.nationalrail.co.uk",sizeof(wsdlHost));
  strncpy(wsdlAPI,"/OpenLDBWS/wsdl.aspx?ver=2021-11-01",sizeof(wsdlAPI));
  u8g2.begin();                       // Start the OLED panel
  u8g2.setContrast(brightness);       // Initial brightness
  u8g2.setDrawColor(1);               // Only a monochrome display, so set the colour to "on"
  u8g2.setFontMode(1);                // Transparent fonts
  u8g2.setFontRefHeightAll();         // Count entire font height
  u8g2.setFontPosTop();               // Reference from top
  u8g2.setFlipMode(1);                // Default is flipped
  u8g2.setFont(NatRailTiny7);
  String buildDate = String(__DATE__);
  String notice = "\x82 " + buildDate.substring(buildDate.length()-4) + F(" Gadec Software");

  bool isFSMounted = LittleFS.begin(true);    // Start the File System, format if necessary
  strcpy(station.location,"");                // No default location
  strcpy(weatherMsg,"");                      // No weather message
  strcpy(nrToken,"");                         // No default National Rail token
  loadApiKeys();                              // Load the API keys from the apiKeys.json
  loadConfig();                               // Load the configuration settings from config.json
  u8g2.setContrast(brightness);               // Set the panel brightness to the user saved level
  if (flipScreen) u8g2.setFlipMode(0);
  u8g2.clearBuffer();
  centreText(F("Tiny Departures Board"),4);
  centreText(notice.c_str(),24);
  u8g2.sendBuffer();
  delay(5000);

  u8g2.clearBuffer();
  drawBuildTime();
  u8g2.sendBuffer();
  progressBar(F("WiFi Connecting"),20);
  WiFi.mode(WIFI_MODE_NULL);        // Reset the WiFi
  WiFi.setSleep(WIFI_PS_NONE);      // Turn off WiFi Powersaving
  WiFi.hostname(hostname);          // Set the hostname
  WiFi.mode(WIFI_STA);              // Enter WiFi station mode
  WiFi.setTxPower(DEFAULT_WIFI_POWER);

  WiFiManager wm;                   // Start WiFiManager
  wm.setAPCallback(wmConfigModeCallback);     // Set the callback for config mode notification
  wm.setWiFiAutoReconnect(true);              // Attempt to auto-reconnect WiFi
  wm.setConnectTimeout(8);
  wm.setConnectRetries(2);

  bool result = wm.autoConnect("Departures Board");    // Attempt to connect to WiFi (or enter interactive configuration mode)
  if (!result) {
      // Failed to connect/configure
      ESP.restart();
  }

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }

  // Get our IP address and store
  updateMyUrl();
  if (MDNS.begin(hostname)) {
    MDNS.addService("http","tcp",80);
  }

  wifiConnected=true;
  WiFi.setAutoReconnect(true);
  u8g2.clearBuffer();                                             // Clear the display
  drawBuildTime();
  char ipBuff[17];
  WiFi.localIP().toString().toCharArray(ipBuff,sizeof(ipBuff));   // Get the IP address of the ESP32
  u8g2.drawStr(SCREEN_WIDTH-u8g2.getStrWidth(ipBuff),24,ipBuff);  // Display the IP address
  u8g2.sendBuffer();
  progressBar(F("WiFi Connected"),30);

  // Configure the local webserver paths
  server.on(F("/"),handleRoot);
  server.on(F("/erasewifi"),handleEraseWiFi);
  server.on(F("/factoryreset"),handleFactoryReset);
  server.on(F("/info"),handleInfo);
  server.on(F("/formatffs"),handleFormatFFS);
  server.on(F("/dir"),handleFileList);
  server.onNotFound(handleNotFound);
  server.on(F("/cat"),handleCat);
  server.on(F("/del"),handleDelete);
  server.on(F("/reboot"),handleReboot);
  server.on(F("/stationpicker"),handleStationPicker);           // Used by the Web GUI to lookup station codes interactively
  server.on(F("/firmware"),handleFirmwareInfo);                 // Used by the Web GUI to display the running firmware version
  server.on(F("/savesettings"),HTTP_POST,handleSaveSettings);   // Used by the Web GUI to save updated configuration settings
  server.on(F("/savekeys"),HTTP_POST,handleSaveKeys);           // Used by the Web GUI to verify/save API keys
  server.on(F("/brightness"),handleBrightness);                 // Used by the Web GUI to interactively set the panel brightness
  server.on(F("/ota"),handleOtaUpdate);                         // Used by the Web GUI to initiate a manual firmware/WebApp update
  server.on(F("/testcard"),handleTestCard);                     // Used by the Web GUI to display screen alignment test card
  server.on(F("/update"), HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, contentTypeHtml, updatePage);
  });
  /*handling uploading firmware file */
  server.on(F("/update"), HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    sendResponse(200,(Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        //Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        //Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
      } else {
        //Update.printError(Serial);
      }
    }
  });

  server.on(F("/upload"), HTTP_GET, []() {
      server.send(200, contentTypeHtml, uploadPage);
  });
  server.on(F("/upload"), HTTP_POST, []() {
  }, handleFileUpload);

  server.on(F("/success"), HTTP_GET, []() {
    server.send(200, contentTypeHtml, successPage);
  });

  server.begin();     // Start the local web server

  // Check for Firmware updates?
  if (firmwareUpdates) {
    progressBar(F("Checking for updates"),40);
    if (ghUpdate.getLatestRelease()) {
      checkForFirmwareUpdate();
    } else {
      for (int i=15;i>=0;i--) {
        showUpdateCompleteScreen("Firmware check failed.","Unable to retrieve latest","release information.",i,false);
        delay(1000);
      }
      u8g2.clearDisplay();
      drawBuildTime();
      u8g2.sendBuffer();
    }
  }
  checkPostWebUpgrade();

  // First time configuration?
  if ((!crsCode[0] && !busAtco[0]) || (!nrToken[0] && boardMode==MODE_RAIL)) {
    if (!apiKeys) showSetupKeysHelpScreen();
    else showSetupCrsHelpScreen();
    // First time setup mode will exit with a reboot, so just loop here forever servicing web requests
    while (true) {
      yield();
      server.handleClient();
    }
  }

  configTime(0,0, ntpServer);                 // Configure NTP server for setting the clock
  setenv("TZ",ukTimezone,1);  // Configure UK TimeZone (default and fallback if custom is invalid)
  tzset();                                    // Set the TimeZone
  if (timezone!="") {
    setenv("TZ",timezone.c_str(),1);
    tzset();
  }

  // Check the clock has been set successfully before continuing
  int p=50;
  int ntpAttempts=0;
  bool ntpResult=true;
  progressBar(F("Setting the clock"),50);
  if(!getLocalTime(&timeinfo)) {              // attempt to set the clock from NTP
    do {
      delay(500);                             // If no NTP response, wait 500ms and retry
      ntpResult = getLocalTime(&timeinfo);
      ntpAttempts++;
      p+=5;
      progressBar(F("Setting the clock"),p);
      if (p>80) p=45;
    } while ((!ntpResult) && (ntpAttempts<10));
  }
  if (!ntpResult) {
    // Sometimes NTP/UDP fails. A reboot usually fixes it.
    progressBar(F("NTP Failed. Will reboot."),0);
    delay(5000);
    ESP.restart();
  }

  station.numServices=0;
  if (boardMode == MODE_RAIL) {
      progressBar(F("Initialising Nat'l Rail"),60);
      raildata = new raildataXmlClient();
      int res = raildata->init(wsdlHost, wsdlAPI, &raildataCallback);
      if (res != UPD_SUCCESS) {
        showWsdlFailureScreen();
        while (true) { server.handleClient(); yield();}
      }
      progressBar(F("Initialising Nat'l Rail"),70);
      raildata->cleanFilter(platformFilter,cleanPlatformFilter,sizeof(platformFilter));
  } else if (boardMode == MODE_BUS) {
      progressBar(F("Initialising BusTimes"),70);
      busdata = new busDataClient();
      // Create a cleaned filter
      busdata->cleanFilter(busFilter,cleanBusFilter,sizeof(busFilter));
      startupProgressPercent=70;
  }
}

void loop(void) {

  // WiFi Status icon
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected=false;
    u8g2.drawStr(0,24,"\x7F");  // No Wifi Icon
    u8g2.updateDisplayArea(0,3,1,1);
  } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    wifiConnected=true;
    blankArea(0,24,5,7);
    u8g2.updateDisplayArea(0,3,1,1);
    updateMyUrl();  // in case our IP changed
  }

  // Force a manual reset if we've been disconnected for more than 10 secs
  if (WiFi.status() != WL_CONNECTED && millis() > lastWiFiReconnect+10000) {
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
    lastWiFiReconnect=millis();
  }

  switch (boardMode) {
    case MODE_RAIL:
      departureBoardLoop();
      break;

    case MODE_BUS:
      busDeparturesLoop();
      break;
  }

  server.handleClient();
}
