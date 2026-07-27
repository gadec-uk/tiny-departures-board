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

// Set a safe default - some ESP32 C3 SuperMini boards don't handle high output power WiFi
#define DEFAULT_WIFI_POWER WIFI_POWER_15dBm

#define MAXLINESIZE 20

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <StreamString.h>
#include <Ticker.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <HTTPUpdateGitHub.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <weatherClient.h>
#include <sharedDataStructs.h>
#include <responseCodes.h>
#include <raildataXmlClient.h>
#include <rdmRailClient.h>
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

static AsyncWebServer server(80); // Hosting the Web GUI

// Shorthand for response formats
static const char contentTypeJson[] = "application/json";
static const char contentTypeText[] = "text/plain";
static const char contentTypeHtml[] = "text/html";

// Using NTP to set and maintain the clock
static struct tm timeinfo;
static const char ukTimezone[] = "GMT0BST,M3.5.0/1,M10.5.0";

// Default hostname
static const char defaultHostname[] = "TinyDeparturesBoard";

// Local firmware updates via /update Web GUI
static const char updatePage[] =
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
static const char uploadPage[] =
"<html><body style=\"font-family:Helvetica,Arial,sans-serif\">"
"<h2>Upload a file to the file system</h2><form method='post' enctype='multipart/form-data'><input type='file' name='name'>"
"<input class='button' type='submit' value='Upload'></form></body></html>";

// /success page
static const char successPage[] =
"<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h3>Upload completed successfully.</h3>\n"
"<p><a href=\"/dir\">List file system directory</a></p>\n"
"<h2>Upload another file</h2><form method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form>\n"
"</body></html>";

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE,9,8);

// Vertical line positions on the OLED display
#define LINE1 0
#define LINE2 9
#define LINE3 18
#define LINE4 24

static Ticker restartTimer; // used to schedule reboots

//
// Custom fonts - replicas of those used on the real display boards
//
static const uint8_t NatRailTiny7[1275] U8G2_FONT_SECTION("NatRailTiny7") =
  "\221\0\3\2\4\3\4\4\5\11\7\0\0\7\0\7\0\1\65\2g\4\342 \5\0|\12!\7qD"
  "\211A\11\42\7\63d\212\304\22#\16uD\233R\222\14JePJI\2$\14uD\253l\251m"
  "I\262E\0%\14t\304\212H\221\42)R\244\0&\15uD\33\251$%\211\224D\221\22'\6\61"
  "d\211\1(\10sD\252\244T+)\11sD\212\254T)\1*\12UL\253Jei\212\0+\12"
  "UL\253\60\32\244\60\2,\7\62\304\231D\1-\6\23\134\212\1.\6\21D\211\0/\13sD\252"
  "D\211\22%\212\0\60\12t\304\32%\362\224(\0\61\10\363\304\232D\352\62\62\12t\304\32%\312\242"
  "\266!\63\14t\304\32%\312\22QJ\24\0\64\12t\304\272HI\244A+\65\13t\304\212A\33\63"
  ")Q\0\66\14t\304\32%\322\226HJ\24\0\67\12t\304\212!\213jM\0\70\14t\304\32%\222"
  "\22%\222\22\5\71\14tD\33%\222\222MJ\24\0:\6AL\211(;\7R\304\231T\1<\10"
  "t\304\272\250\261\1=\6\63T\212m>\10t\304\212\260\251\15?\14t\304\32%\312\22)\7\42\0"
  "@\14uD\233%\263$\312\220.\0A\12t\304\32%\222\206\311\24B\14t\304\212%\222\206$\222"
  "\206\4C\12t\304\32%\322\232\22\5D\12t\304\212%\362\64$\0E\13t\304\212A\313\226,\33"
  "\2F\12t\304\212A\313\226\254\6G\14t\304\32%\322\222IJ\24\0H\12t\304\212\310\64L\246"
  "\0I\7qD\211C\0J\11t\304\272nR\242\0K\13t\304\212\310\222HII\12L\10t\304"
  "\212\254\267!M\12uD\213lY\22\315-N\12t\304\212hQ&\247\0O\12t\304\32%\362\224"
  "(\0P\13t\304\212%\222\206$\253\1Q\12t\304\32%\362\22%\1R\13t\304\212%\222\206\244"
  "I\12S\12t\304\32%\22M\211\2T\11uD\213A\12{\2U\11t\304\212\310\247D\1V\12"
  "uD\213\314[R\213\0W\13uD\213\314%Q\222[\0X\13uD\213LKj\225\232\26Y\13"
  "uD\213LKja\23\0Z\12uD\213A\314:\16\2[\10r\304\211\245\213\0\134\11sD\212"
  "H*I\5]\10r\304\14\245\313\0^\6#l\232\6_\6\23D\212\1`\6\42\354\211(a\11"
  "T\304\32\61\31\242db\13t\304\212,[\42\323\220\0c\10T\304\232!+\16d\12t\304\272\312"
  "\20\231\222\1e\11T\304\32%\32\306\1f\12t\304\252Ji\312J\0g\12T\304\232!J\266!"
  "\1h\12t\304\212,[\42\247\0i\7qD\211d\20j\12sD\252\64\212\224\12\0k\13t\304"
  "\212\254\244$RR\12l\7qD\211C\0m\12UD\13\245EI\64-n\11T\304\212$\61\231"
  "\2o\12T\304\32%\62%\12\0p\12T\304\212%\222\206$\3q\11T\304\232!\222\222-r\11"
  "T\304\212$\261\325\0s\11T\304\232!\24\207\4t\12t\304\232,\32\222\254(u\11T\304\212\310"
  ")Q\0v\12UD\213\314\226\324\42\0w\13UD\213LI\224D\351\2x\12UD\213,\251U"
  "j\1y\12T\304\212HJ\266!\1z\11UD\213Ak\33\4{\12sD\252$J\262(\13|"
  "\7qD\211C\0}\13sD\212,\312\222(\211\0~\7%\134\33S\2\15uD\213$\253\24"
  "\243$J\224\2\200\5\0D\10\201\14\345L\274d\320\242lP\62\0\202\7\62\304\231D\1\203\5\0"
  "D\10\204\10\64\304\232\26%\1\205\7\25D\213\244\0\206\13uD\233%\263\15\7\245\2\207\16wD"
  "\254-i\212,j\222e\23\0\210\20x\304\314t\310\242$\231\226R\66\244!\0\211\5\0D\10\212"
  "\5\0D\10\213\5\0D\10\214\5\0D\10\215\15u\304\213\323\222h\312\220\14\203\0\216\15u\304\213"
  "C\244\14\312\240H\303\20\217\13wD\214\267)\262\250\333p\220\6\63\327\217\7\221\7\62\344\211$\12"
  "\222\7\62\344\231D\1\223\10\64\344\212\244)\11\224\10\64\344\232\26%\1\225\6\63T\212\7\226\6\25"
  "\134\213A\227\5\0D\10\230\5\0D\10\231\5\0D\10\232\5\0D\10\233\5\0D\10\234\5\0D"
  "\10\235\5\0D\10\236\5\0D\10\237\5\0D\10\240\5\0D\10\241\5\0D\10\242\5\0D\10\243"
  "\12t\304\252Ji\312\242!\244\5\0D\10\245\5\0D\10\246\5\0D\10\247\5\0D\10\250\5\0"
  "D\10\251\16wD\254\255\22)J&)\265l\2\252\5\0D\10\253\5\0D\10\254\5\0D\10\255"
  "\5\0D\10\256\5\0D\10\257\5\0D\10\260\11D\334\32%\222\22\5\0\0\0";

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
static const char nrAttributionn[] = "National Rail Enquiries";
static const char rdgAttribution[] = "Rail Delivery Group";
static const char btAttribution[] = "Powered by bustimes.org";

#define DATAUPDATEINTERVAL 150000     // How often we fetch data from National Rail (ms - 2.5 mins) - "default" option
#define FASTDATAUPDATEINTERVAL 45000  // How often we fetch data from National Rail (ms - 45 secs) - "fast" option
#define BUSDATAUPDATEINTERVAL 45000   // How often we fetch data from bustimes.org (ms - 45 secs)
#define WEATHERUPDATEINTERVAL 1200000 // How often to update the weather forecast (ms - 20 mins)

// Reusable data transfer structures
rdiStation xfrStation;
stnMessages xfrMessages;
busTubeStation xfrBusTubeStation;
sharedBufferSpace jsonKeyBuffer;

// Station Data (shared)
rdStation station;
// Station Messages (shared)
stnMessages messages;

// Data transfer clients
rdmRailClient rdmRailData(&xfrStation,&xfrMessages,&jsonKeyBuffer);
raildataXmlClient darwinRailData(&xfrStation,&xfrMessages,&jsonKeyBuffer);
busDataClient busdata(&xfrBusTubeStation,&jsonKeyBuffer);
weatherClient currentWeather(&jsonKeyBuffer);
github ghUpdate(&jsonKeyBuffer);

static char weatherMsg[MAXWEATHERSIZE];

// Bit and bobs
static unsigned long timer = 0;
static bool weatherEnabled = false;        // Showing weather at station location. Requires an OpenWeatherMap API key.
static bool enableBus = false;             // Include Bus services on the board?
static bool firmwareUpdates = true;        // Check for and install firmware updates automatically at boot?
static int brightness = 50;                // Initial brightness level of the OLED screen
static unsigned long lastWiFiReconnect=0;  // Last WiFi reconnection time (millis)
static bool firstLoad = true;              // Are we loading for the first time (no station config)?
static int prevProgressBarPosition=0;      // Used for progress bar smooth animation
static int startupProgressPercent;         // Initialisation progress
static bool wifiConnected = false;         // Connected to WiFi?
static unsigned long nextDataUpdate = 0;   // Next National Rail update time (millis)
static int dataLoadSuccess = 0;            // Count of successful data downloads
static int dataLoadFailure = 0;            // Count of failed data downloads
static unsigned long lastLoadFailure = 0;  // When the last failure occurred
static bool noScrolling = false;           // Suppress all horizontal scrolling
static bool flipScreen = false;            // Rotate screen 180deg
static String timezone = "";               // custom (non UK) timezone for the clock
static bool apiKeys = false;               // Does apikeys.json exist?
static bool softResetNeeded = false;       // Is a soft reset pending?
static bool manualUpdateCheck = false;     // Has the GUI requested a firmware update check
static bool useRDMclient = false;          // Use the new Rail Data Marketplace API instead of Darwin Lite
static char hostname[33];                  // Network hostname (mDNS)
static char myUrl[24];                     // Stores the board's own url

// WiFi Manager status
bool wifiConfigured = false;               // Is WiFi configured successfully?

// Station Board Data
char nrToken[37] = "";                    // National Rail Darwin Lite Tokens are in the format nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn, where each 'n' represents a hexadecimal character (0-9 or a-f).
static String rdmDeparturesApiKey = "";   // RDM Consumer key for DeparturesBoard API
char crsCode[4] = "";                     // Station code (3 character)
float stationLat=0;                       // Selected station Latitude/Longitude (used to get weather for the location)
float stationLon=0;
char callingCrsCode[4] = "";              // Station code to filter routes on
char callingStation[45] = "";             // Calling filter station friendly name
char platformFilter[MAXPLATFORMFILTERSIZE]; // CSV list of platforms to filter on
char cleanPlatformFilter[MAXPLATFORMFILTERSIZE]; // Cleaned up platform filter (for performance)
char busAtco[13]="";                      // Bus Stop ATCO location
String busName="";                        // Bus Stop long name
int busDestX;                             // Variable margin for bus destination
char busFilter[25]="";                    // CSV list of services to filter on
char cleanBusFilter[25];                  // Cleaned up bus filter (for performance)
float busLat=0;                           // Bus stop Latitude/Longitude (used to get weather for the location)
float busLon=0;
static bool railIsSet = false;
static bool busIsSet = false;

// tiny board has two possible modes.
enum boardModes {
  MODE_RAIL = 0,
  MODE_BUS = 1
};
boardModes boardMode = MODE_RAIL;

// Coach class availability
static const char firstClassSeating[] = " First class seating only.";
static const char standardClassSeating[] = " Standard class seating only.";
static const char dualClassSeating[] = " First and Standard class seating available.";

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
unsigned long nextWeatherUpdate = 0;            // When the next weather update is due
static char openWeatherMapApiKey[33] = "";      // If no OWM API key is provided, we use Open-Meteo weather data

bool noDataLoaded = true;                       // True if no data received for the station
int lastUpdateResult = 0;                       // Result of last data refresh
unsigned long lastDataLoadTime = 0;             // Timestamp of last data load
long apiRefreshRate = DATAUPDATEINTERVAL;       // User selected refresh rate for National Rail API

#define MAXHOSTSIZE 48                          // Maximum size of the wsdl Host
#define MAXAPIURLSIZE 48                        // Maximum size of the wsdl url

char wsdlHost[MAXHOSTSIZE];                     // wsdl Host name
char wsdlAPI[MAXAPIURLSIZE];                    // wsdl API url

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

void drawTruncatedText(const char *message, int line) {
  char buff[strlen(message)+4];
  int maxWidth = SCREEN_WIDTH - 6;
  strcpy(buff,message);
  int i = strlen(buff);
  while (u8g2.getStrWidth(buff)>maxWidth && i) buff[i--] = '\0';
  strcat(buff,"\x85");
  u8g2.drawStr(0,line-1,buff);
}

void centreText(const char *message, int line) {
  int width = u8g2.getStrWidth(message);
  if (width<=SCREEN_WIDTH) u8g2.drawStr((SCREEN_WIDTH-width)/2,line-1,message);
  else drawTruncatedText(message,line);
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
  centreText("WiFi Setup. Connect to",0);
  centreText("\"Departures Board\"",8);
  centreText("and then go to",16);
  centreText("http://192.168.4.1",24);
  u8g2.sendBuffer();
}

void showNoDataScreen() {
  u8g2.clearBuffer();
  centreText("No data for the selected",0);
  centreText("location is available.",8);
  u8g2.sendBuffer();
}

void showSetupKeysHelpScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailSmall9);
  centreText("Next, enter your",0);
  centreText("API keys at:",10);
  centreText(myUrl,20);
  u8g2.setFont(NatRailTiny7);
  u8g2.sendBuffer();
}

void showSetupCrsHelpScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(NatRailSmall9);
  centreText("Next, select a",0);
  if (nrToken[0] || rdmDeparturesApiKey.length()) centreText("station or bus stop at:",10);
  else centreText("bus stop at:",10);
  centreText(myUrl,20);
  u8g2.setFont(NatRailTiny7);
  u8g2.sendBuffer();
}

void showWsdlFailureScreen() {
  u8g2.clearBuffer();
  centreText("NatRail data feed is",0);
  centreText("unavailable. The board",8);
  centreText("cannot be loaded.",16);
  centreText("Try again later. :(",24);
  u8g2.sendBuffer();
}

void showTokenErrorScreen() {
  char msg[60];
  u8g2.clearBuffer();
  switch (boardMode) {
    case MODE_RAIL:
      if (useRDMclient) {
        centreText("RDG api access denied.",0);
      } else {
        centreText("NatRail access denied.",0);
        strcpy(nrToken,"");
      }
      break;
    case MODE_BUS:
      centreText("Bustimes.org access denied.",0);
      break;
  }
  centreText("Check you have entered your",8);
  centreText("API keys correctly at:",16);
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
  centreText("is not valid. Select a",8);
  centreText("valid code at:",16);
  centreText(myUrl,24);
  u8g2.sendBuffer();
}

void showFirmwareUpdateWarningScreen(int secs) {
  char countdown[60];
  u8g2.clearBuffer();
  centreText("Firmware Update Available",0);
  centreText("The update will begin",8);
  sprintf(countdown,"installing in %d seconds.",secs);
  centreText(countdown,16);
  centreText("*DO NOT REMOVE POWER*",24);
  u8g2.sendBuffer();
}

void showFirmwareUpdateProgress(int percent) {
  u8g2.clearBuffer();
  progressBar("Updating Firmware",percent);
  centreText("*DO NOT REMOVE POWER*",24);
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
  JsonDocument doc;
  char prevFirmware[15] = "B0.0-W0.0";
  char prevGUI[8];
  char currentGUI[8];

  if (LittleFS.exists("/fw.json")) {
    File file = LittleFS.open("/fw.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["fw"].is<const char*>()) {
          strlcpy(prevFirmware,settings["fw"],sizeof(prevFirmware));
        }
      }
      file.close();
    }
  }

  if (prevFirmware[0]) {
    sscanf(prevFirmware,"%*[^ -]-%s",prevGUI);
    sprintf(currentGUI,"W%d.%d",WEBAPPVER_MAJOR,WEBAPPVER_MINOR);
    if (strcmp(prevGUI,currentGUI)) {
      // clean up old/dev files
      progressBar("Cleaning up following upgrade",45);
      LittleFS.remove("/index_d.htm");
      LittleFS.remove("/index.htm");
      LittleFS.remove("/keys.htm");
      LittleFS.remove("/nrelogo.webp");
      LittleFS.remove("/rdglogo.webp");
      LittleFS.remove("/btlogo.webp");
      LittleFS.remove("/nr.webp");
      LittleFS.remove("/favicon.svg");
      LittleFS.remove("/favicon.png");
      LittleFS.remove("/webver");
    }
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

  if (LittleFS.exists("/apikeys.json")) {
    File file = LittleFS.open("/apikeys.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["rdmDepKey"].is<const char*>()) {
          rdmDeparturesApiKey = settings["rdmDepKey"].as<String>();
        }

        if (settings["nrToken"].is<const char*>()) {
          strlcpy(nrToken, settings["nrToken"], sizeof(nrToken));
        }

        if (settings["owmToken"].is<const char*>()) {
          strlcpy(openWeatherMapApiKey, settings["owmToken"], sizeof(openWeatherMapApiKey));
        }

        apiKeys = true;

      } else {
        // JSON deserialization failed - TODO
      }
      file.close();
    }
  }
}

void resetLocationIds() {
  strcpy(crsCode,"");
  strcpy(busAtco,"");
  railIsSet = false;
  busIsSet = false;
}

void saveFirmwareInfo() {
  String fw = "{\"fw\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "-W" + String(WEBAPPVER_MAJOR) + "." + String(WEBAPPVER_MINOR) + "\"}";
  saveFile("/fw.json",fw);
}

// Write a default config file so that the Web GUI works initially (force bus mode if no NR token)
void writeDefaultConfig() {
    String defaultConfig = "{\"crs\":\"\",\"station\":\"\",\"lat\":0,\"lon\":0,\"weather\":true,\"showBus\":false,\"update\":true,\"brightness\":20,\"mode\":" + String((!nrToken[0] && rdmDeparturesApiKey=="")?"1":"0") + "}";
    saveFile("/config.json",defaultConfig);
    resetLocationIds();
    saveFirmwareInfo();
}

// Load the configuration settings (if they exist, if not create a default set for the Web GUI page to read)
void loadConfig() {
  JsonDocument doc;

  // Set defaults
  strcpy(hostname,defaultHostname);
  timezone = String(ukTimezone);
  resetLocationIds();

  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["crs"].is<const char*>())        strlcpy(crsCode, settings["crs"], sizeof(crsCode));
        if (settings["callingCrs"].is<const char*>()) strlcpy(callingCrsCode, settings["callingCrs"], sizeof(callingCrsCode));
        if (settings["callingStation"].is<const char*>()) strlcpy(callingStation, settings["callingStation"], sizeof(callingStation));
        if (settings["platformFilter"].is<const char*>())  strlcpy(platformFilter, settings["platformFilter"], sizeof(platformFilter));
        if (settings["hostname"].is<const char*>())   strlcpy(hostname, settings["hostname"], sizeof(hostname));
        if (settings["wsdlHost"].is<const char*>())   strlcpy(wsdlHost, settings["wsdlHost"], sizeof(wsdlHost));
        if (settings["wsdlAPI"].is<const char*>())    strlcpy(wsdlAPI, settings["wsdlAPI"], sizeof(wsdlAPI));
        if (settings["showBus"].is<bool>())           enableBus = settings["showBus"];
        if (settings["fastRefresh"].is<bool>())       apiRefreshRate = settings["fastRefresh"] ? FASTDATAUPDATEINTERVAL : DATAUPDATEINTERVAL;
        if (settings["weather"].is<bool>())           weatherEnabled = settings["weather"];
        if (settings["update"].is<bool>())            firmwareUpdates = settings["update"];
        if (settings["brightness"].is<int>())         brightness = settings["brightness"];
        if (settings["lat"].is<float>())              stationLat = settings["lat"];
        if (settings["lon"].is<float>())              stationLon = settings["lon"];

        if (settings["mode"].is<int>())               boardMode = settings["mode"];

        if (settings["busId"].is<const char*>())      strlcpy(busAtco, settings["busId"], sizeof(busAtco));
        if (settings["busName"].is<const char*>())    busName = String(settings["busName"]);
        if (settings["busLat"].is<float>())           busLat = settings["busLat"];
        if (settings["busLon"].is<float>())           busLon = settings["busLon"];
        if (settings["busFilter"].is<const char*>())  strlcpy(busFilter, settings["busFilter"], sizeof(busFilter));

        if (settings["noScroll"].is<bool>())          noScrolling = settings["noScroll"];
        if (settings["flip"].is<bool>())              flipScreen = settings["flip"];
        if (settings["TZ"].is<const char*>())         timezone = settings["TZ"].as<String>();

        if (settings["dataSource"].is<int>())         useRDMclient = (settings["dataSource"]?1:0);
        // validate the data source against which api keys are available
        if (nrToken[0] && rdmDeparturesApiKey=="") useRDMclient = false;
        else if (!nrToken[0] && rdmDeparturesApiKey!="") useRDMclient = true;

        if (strlen(crsCode)) railIsSet = true;
        if (strlen(busAtco)) busIsSet = true;

      } else {
        // JSON deserialization failed - TODO
      }
      file.close();
    }
  } else if (apiKeys) writeDefaultConfig();
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

  switch (boardMode) {
    case MODE_RAIL:
      // Create a cleaned platform filter (if any)
      rdmRailData.cleanFilter(platformFilter,cleanPlatformFilter,sizeof(platformFilter));
      progressBar("Initialising Nat'l Rail",70);
      if (!useRDMclient) {
        // Using legacy XML client
        int res = darwinRailData.init(wsdlHost, wsdlAPI);
        if (res != UPD_SUCCESS) {
          showWsdlFailureScreen();
          while (true) { delay(1);}
        }
      }
      break;

    case MODE_BUS:
      //checkWeatherUpdate(prevLat,prevLon);
      progressBar("Initialising BusTimes",70);
      // Create a cleaned filter
      busdata.cleanFilter(busFilter,cleanBusFilter,sizeof(busFilter));
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

  // Check that we found the firmware.bin file in the release assets
  if (ghUpdate.firmwareURL.length()==0) return result;

  for (int i=30;i>=0;i--) {
    showFirmwareUpdateWarningScreen(i);
    delay(1000);
  }
  u8g2.clearDisplay();
  prevProgressBarPosition=0;
  showFirmwareUpdateProgress(0);  // So we don't have a blank screen
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.onProgress(update_progress);
  httpUpdate.rebootOnUpdate(false); // Don't auto reboot, we'll handle it

  HTTPUpdateResult ret = httpUpdate.handleUpdate(client, ghUpdate.firmwareURL);
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

void updateRailDepartures() {
  if (useRDMclient) rdmRailData.loadDepartures(&station,&messages);
  else darwinRailData.loadDepartures(&station,&messages);
}

// Request a data update via the raildataClient
bool getStationBoard() {
  if (!firstLoad) showUpdateIcon(true);
  if (useRDMclient) {
    lastUpdateResult = rdmRailData.fetchDepartures(&station,&messages,crsCode,rdmDeparturesApiKey,"",MAXBOARDSERVICES,enableBus,callingCrsCode,cleanPlatformFilter,0,false,true);
  } else {
    lastUpdateResult = darwinRailData.fetchDepartures(&station,&messages,crsCode,nrToken,MAXBOARDSERVICES,enableBus,callingCrsCode,cleanPlatformFilter,0,false,true);
  }
  //lastUpdateResult = raildata->updateDepartures(&station,&messages,crsCode,nrToken,MAXBOARDSERVICES,enableBus,callingCrsCode,cleanPlatformFilter);
  nextDataUpdate = millis()+apiRefreshRate;
  if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_SEC_CHANGE|| lastUpdateResult == UPD_NO_CHANGE) {
    showUpdateIcon(false);
    updateRailDepartures();
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
    while (true) { delay(100); }
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
    strcat(clipDestination,"\x85");
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
      strcat(clipDestination,"\x85");
    }
    u8g2.drawStr(destPos,y-1,clipDestination);
  } else {
    if (weatherMsg[0] && line==station.numServices) {
      // We're showing the weather
      centreText(weatherMsg,y);
    } else {
      // We're showing the mandatory attribution
      if (!useRDMclient) centreText(nrAttributionn,y); else centreText(rdgAttribution,y);
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
    centreText("No scheduled services.",LINE1);
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

/*
 *
 * Bus Departures Board
 *
 */
bool getBusDeparturesBoard() {
  if (!firstLoad) showUpdateIcon(true);
  //lastUpdateResult = busdata->updateDepartures(&station,busAtco,cleanBusFilter,&busCallback);
  lastUpdateResult = busdata.fetchDepartures(&station,busAtco,cleanBusFilter);
  nextDataUpdate = millis()+BUSDATAUPDATEINTERVAL; // default update freq
  if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_SEC_CHANGE || lastUpdateResult == UPD_NO_CHANGE) {
    showUpdateIcon(false);
    busdata.loadDepartures(&station);
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
    while (true) { delay(200); }
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
      strcat(clipDestination,"\x85");
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
      centreText("No scheduled services",LINE1-1);
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
void sendResponse(int code, String msg, AsyncWebServerRequest *request) {
  request->send(code,contentTypeText,msg);
}

// Return the correct MIME type for a file name
String getContentType(String filename) {
  if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".json")) {
    return "application/json";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  } else if (filename.endsWith(".svg")) {
    return "image/svg+xml";
  } else if (filename.endsWith(".webp")) {
    return "image/webp";
  }
  return "text/plain";
}

// Stream a file from the file system
bool handleStreamFile(String filename, AsyncWebServerRequest *request) {
  if (LittleFS.exists(filename)) {
    String contentType = getContentType(filename);
    request->send(LittleFS,filename,contentType);
    return true;
  } else return false;
}

// Stream a file stored in flash (default graphics are now included in the firmware image)
void handleStreamFlashFile(String filename, const uint8_t *filedata, size_t contentLength, AsyncWebServerRequest *request) {
  String contentType = getContentType(filename);
  AsyncWebServerResponse *response = request->beginResponse(200, contentType, filedata, contentLength);
  response->addHeader("Cache-Control", "public,max-age=3600,s-maxage=3600");
  request->send(response);
}

void handleStreamGzipFlashFile(String filename, const uint8_t *filedata, size_t contentLength, AsyncWebServerRequest *request) {
  String contentType = getContentType(filename);
  AsyncWebServerResponse *response = request->beginResponse(200, contentType, filedata, contentLength);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
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
void handleFileList(AsyncWebServerRequest *request) {
  String path;
  if (!request->hasParam("dir")) path="/"; else path = request->getParam("dir")->value();
  File root = LittleFS.open(path);

  String output="<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h2>Tiny Departures Board File System</h2>";
  if (!root) {
    output+="<p>Failed to open directory</p>";
  } else if (!root.isDirectory()) {
    output+="<p>Not a directory</p>";
  } else {
    output+="<table>";
    File file = root.openNextFile();
    while (file) {
      output+="<tr><td>";
      if (file.isDirectory()) {
        output+="[DIR]</td><td><a href=\"/rmdir?f=" + String(file.path()) + "\" title=\"Delete\">X</a></td><td><a href=\"/dir?dir=" + String(file.path()) + "\">" + String(file.name()) + "</a></td></tr>";
      } else {
        output+=String(file.size()) + "</td><td><a href=\"/del?f="+ String(file.path()) + "\" title=\"Delete\">X</a></td><td><a href=\"/cat?f=" + String(file.path()) + "\">" + String(file.name()) + "</a></td></tr>";
      }
      file = root.openNextFile();
    }
  }

  output += "</table><br>";
  output += getFSInfo() + "<p><a href=\"/upload\">Upload</a> a file</p></body></html>";
  request->send(200,contentTypeHtml,output);
}

// Stream a file to the browser
void handleCat(AsyncWebServerRequest *request) {
  if (request->hasParam("f")) {
    String filename = request->getParam("f")->value();
    handleStreamFile(filename,request);
  } else sendResponse(404,"Not found",request);
}

// Delete a file from the file system
void handleDelete(AsyncWebServerRequest *request) {
  if (request->hasParam("f")) {
    String filename = request->getParam("f")->value();
    if (LittleFS.remove(filename)) {
      // Successfully removed go back to directory listing
      request->redirect("/dir");
    } else sendResponse(400,"Failed to delete file",request);
  } else sendResponse(404,"Not found",request);
}

// Format the file system
void handleFormatFFS(AsyncWebServerRequest *request) {
  String message;

  if (LittleFS.format()) {
    message="File System was successfully formatted\n\n";
    message+=getFSInfo();
  } else message="File System could not be formatted!";
  sendResponse(200,message,request);
}

/*
 * Web GUI handlers
 */

// Fallback function for browser requests
void handleNotFound(AsyncWebServerRequest *request) {
  if ((LittleFS.exists(request->url())) && (request->method() == HTTP_GET)) handleStreamFile(request->url(),request);
  else if (request->url() == "/keys.htm") handleStreamGzipFlashFile(request->url(), keyshtm, sizeof(keyshtm),request);
  else if (request->url() == "/index.htm") handleStreamGzipFlashFile(request->url(), indexhtm, sizeof(indexhtm),request);
  else if (request->url() == "/nrelogo.webp") handleStreamFlashFile(request->url(), nrelogo, sizeof(nrelogo),request);
  else if (request->url() == "/rdglogo.webp") handleStreamFlashFile(request->url(), rdglogo, sizeof(nrelogo),request);
  else if (request->url() == "/btlogo.webp") handleStreamFlashFile(request->url(), btlogo, sizeof(btlogo),request);
  else if (request->url() == "/nr.webp") handleStreamFlashFile(request->url(), nricon, sizeof(nricon),request);
  else if (request->url() == "/ibus.webp") handleStreamFlashFile(request->url(), ibus, sizeof(ibus),request);
  else if (request->url() == "/irail.webp") handleStreamFlashFile(request->url(), irail, sizeof(irail),request);
  else if (request->url() == "/favicon.png") handleStreamFlashFile(request->url(), faviconpng, sizeof(faviconpng),request);
  else sendResponse(404,"Not Found",request);
}

String getResultCodeText(int resultCode) {
  switch (resultCode) {
    case UPD_SUCCESS:
      return "SUCCESS";
      break;
    case UPD_NO_CHANGE:
      return "SUCCESS (NO CHANGES)";
      break;
    case UPD_SEC_CHANGE:
      return "SUCCESS (SECONDARY CHANGES)";
      break;
    case UPD_DATA_ERROR:
      return "DATA ERROR";
      break;
    case UPD_UNAUTHORISED:
      return "UNAUTHORISED";
      break;
    case UPD_HTTP_ERROR:
      return "HTTP ERROR";
      break;
    case UPD_INCOMPLETE:
      return "INCOMPLETE DATA RECEIVED";
      break;
    case UPD_NO_RESPONSE:
      return "NO RESPONSE FROM SERVER";
      break;
    case UPD_TIMEOUT:
      return "TIMEOUT WAITING FOR SERVER";
      break;
    default:
      return "OTHER ERROR";
      break;
  }
}

// Send some useful system & station information to the browser
void handleInfo(AsyncWebServerRequest *request) {
  unsigned long uptime = millis();
  char sysUptime[30];
  int days = uptime / msDay ;
  int hours = (uptime % msDay) / msHour;
  int minutes = ((uptime % msDay) % msHour) / msMin;

  sprintf(sysUptime,"%d days, %d hrs, %d min", days,hours,minutes);

  String message = "Hostname: " + String(hostname) + "\nFirmware version: v"+ String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + " " + getBuildTime() + "\nSystem uptime: "+ String(sysUptime) + "\nFree Heap: "+ String(ESP.getFreeHeap()) + "\nFree LittleFS space: "+ String(LittleFS.totalBytes() - LittleFS.usedBytes());
  message+="\nCore Plaform: " + String(ESP.getCoreVersion()) + "\nCPU speed: "+ String(ESP.getCpuFreqMHz()) + "MHz\nCPU Temperature: "+ String(temperatureRead()) + "\nWiFi network: "+ String(WiFi.SSID()) + "\nWiFi signal strength: "+ String(WiFi.RSSI()) + "dB";
  getLocalTime(&timeinfo);

  sprintf(sysUptime,"%02d:%02d:%02d %02d/%02d/%04d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
  message+="\nSystem clock: " + String(sysUptime);
  message+="\nCRS station code: " + String(crsCode) + "\nSuccessful: "+ String(dataLoadSuccess) + "\nFailures: "+ String(dataLoadFailure) + "\nTime since last data load: "+ String((int)((millis()-lastDataLoadTime)/1000)) + " seconds";
  if (dataLoadFailure) message+="\nTime since last failure: " + String((int)((millis()-lastLoadFailure)/1000)) + " seconds";
  message+="\nLast Result: ";
  switch (boardMode) {
    case MODE_RAIL:
      if (useRDMclient) message+="RDMClient: " + String(jsonKeyBuffer.lastResultMessage);
      else message+="darwinClient: " + String(jsonKeyBuffer.lastResultMessage);
      break;

    case MODE_BUS:
      message+=String(jsonKeyBuffer.lastResultMessage);
      break;
  }
  message+="\nServices: " + String(station.numServices) + "\nMessages: ";
  message+=String(messages.numMessages);
  message+="\n";
  if (boardMode != MODE_BUS) for (int i=0;i<messages.numMessages;i++) message+=String(messages.messages[i]) + "\n";
  message+="\nUpdate result code: ";
  switch (lastUpdateResult) {
    case UPD_SUCCESS:
      message+="SUCCESS";
      break;
    case UPD_NO_CHANGE:
      message+="SUCCESS (NO CHANGES)";
      break;
    case UPD_DATA_ERROR:
      message+="DATA ERROR";
      break;
    case UPD_UNAUTHORISED:
      message+="UNAUTHORISED";
      break;
    case UPD_HTTP_ERROR:
      message+="HTTP ERROR";
      break;
    case UPD_INCOMPLETE:
      message+="INCOMPLETE JSON RECEIVED";
      break;
    case UPD_NO_RESPONSE:
      message+="NO RESPONSE FROM SERVER";
      break;
    case UPD_TIMEOUT:
      message+="TIMEOUT WAITING FOR SERVER";
      break;
    default:
      message+="ERROR CODE (" + String(lastUpdateResult) + ")";
      break;
  }
  sendResponse(200,message,request);
}

// Stream the index.htm page unless we're in first time setup and need the api keys
void handleRoot(AsyncWebServerRequest *request) {
  if (!apiKeys) {
    if (LittleFS.exists("/keys.htm")) handleStreamFile("/keys.htm",request); else handleStreamGzipFlashFile("/keys.htm",keyshtm,sizeof(keyshtm),request);
  } else {
    if (LittleFS.exists("/index_d.htm")) handleStreamFile("/index_d.htm",request); else handleStreamGzipFlashFile("/index.htm",indexhtm,sizeof(indexhtm),request);
  }
}

// Send the firmware version to the client (called from index.htm)
void handleFirmwareInfo(AsyncWebServerRequest *request) {
  String response = "{\"firmware\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "-W" + String(WEBAPPVER_MAJOR) + "." + String(WEBAPPVER_MINOR) + "\"}";
  request->send(200,contentTypeJson,response);
}

// Force a reboot of the ESP32
void handleReboot(AsyncWebServerRequest *request) {
  sendResponse(200,"The Departures Board is restarting...",request);
  restartTimer.once(1, []() { ESP.restart(); });
}

// Erase the stored WiFiManager credentials
void handleEraseWiFi(AsyncWebServerRequest *request) {
  sendResponse(200,"Erasing stored WiFi settings.\n\nYou will need to connect to the \"Departures Board\" network and use WiFi Manager to reconfigure the settings.",request);
  restartTimer.once(1, []() { WiFiManager wm; wm.resetSettings(); ESP.restart();});
}

// "Factory reset" the app - delete WiFi, format file system and reboot
void handleFactoryReset(AsyncWebServerRequest *request) {
  sendResponse(200,"Factory reseting the Departures Board...",request);
  restartTimer.once(1, []() { WiFiManager wm; wm.resetSettings(); LittleFS.format(); ESP.restart();});
}

// Interactively change the brightness of the OLED panel (called from index.htm)
void handleBrightness(AsyncWebServerRequest *request) {
  if (request->hasParam("b")) {
    int level = request->getParam("b")->value().toInt();
    if (level>0 && level<256) {
      u8g2.setContrast(level);
      brightness = level;
      sendResponse(200,"OK",request);
      return;
    }
  }
  sendResponse(200,"invalid request",request);
}

// Web GUI has requested updates be installed
void handleOtaUpdate(AsyncWebServerRequest *request) {
  sendResponse(200,"Update initiated - check the Departure Board display for progress.",request);
  manualUpdateCheck = true;
}

void doManualOtaCheck() {
  u8g2.clearBuffer();
  centreText("Getting latest firmware",LINE2);
  centreText("details from GitHub...",LINE3);
  u8g2.sendBuffer();

  if (ghUpdate.getLatestRelease()==UPD_SUCCESS) {
    checkForFirmwareUpdate();
  } else {
    for (int i=15;i>=0;i--) {
      showUpdateCompleteScreen("Firmware check failed.","Unable to retrieve latest","release information.",i,false);
      delay(1000);
    }
  }
  // Always restart
  ESP.restart();
}

/*
 * External data functions - weather, stationpicker, firmware updates
 */

// Call the National Rail Station Picker (called from index.htm)
void handleStationPicker(AsyncWebServerRequest *request)
{
  if (!request->hasParam("q")) {
    sendResponse(400,"Missing Query",request);
    return;
  }

  String query = request->getParam("q")->value();
  if (query.length() <= 2) {
    sendResponse(400,"Query too short",request);
    return;
  }

  const char* host = "stationpicker.nationalrail.co.uk";
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(4000);

  if (!client.connect(host, 443)) {
    sendResponse(408, "NR Connect Timeout",request);
    return;
  }

  client.print(String("GET /stationPicker/") + query + " HTTP/1.0\r\n"
               "Host: stationpicker.nationalrail.co.uk\r\n"
               "Referer: https://www.nationalrail.co.uk\r\n"
               "Origin: https://www.nationalrail.co.uk\r\n"
               "Connection: close\r\n\r\n");

  int requestTimer = 0;
  while (!client.available() && requestTimer<1000) {
    requestTimer++;
    delay(1);
  }

  if (!client.available()) {
    client.stop();
    sendResponse(408,"NRQ Timeout",request);
  }

  String statusLine = client.readStringUntil('\n');

  if (statusLine.indexOf("200") == -1) {
    client.stop();
    sendResponse(503, statusLine, request);
    return;
  }

  // Skip the remaining headers
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Start sending response
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  uint8_t buffer[512];
  unsigned long timeout = millis() + 5000UL;
  while ((client.connected() || client.available()) && millis() < timeout) {
    int len = client.read(buffer, sizeof(buffer));
    if (len > 0) {
      response->write(buffer, len);
      delay(1);
    }
  }

  client.stop();
  request->send(response);
}

// Update the current weather message if weather updates are enabled and we have a lat/lon for the selected location
void updateCurrentWeather(float latitude, float longitude) {
  nextWeatherUpdate = millis() + 1200000; // update every 20 mins
  if (!latitude || !longitude) return; // No location co-ordinates
  strcpy(weatherMsg,"");
  bool currentWeatherResult = currentWeather.updateWeather(openWeatherMapApiKey, latitude, longitude);
  if (currentWeatherResult == UPD_SUCCESS) {
    strlcpy(weatherMsg,currentWeather.currentWeatherMessage,MAXWEATHERSIZE);
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
      if ((lastUpdateResult == UPD_SUCCESS) || lastUpdateResult == UPD_SEC_CHANGE || (lastUpdateResult == UPD_NO_CHANGE && firstLoad)) drawStationBoard(); // Something changed so redraw the board.
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
      if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_SEC_CHANGE || lastUpdateResult == UPD_NO_CHANGE) drawBusDeparturesBoard(); // Something changed so redraw the board.
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
    else centreText("No scheduled services",scrollPrimaryYpos+LINE1);
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
  String notice = "\xA9 " + buildDate.substring(buildDate.length()-4) + " Gadec Software";

  bool isFSMounted = LittleFS.begin(true);    // Start the File System, format if necessary
  strcpy(station.location,"");                // No default location
  strcpy(weatherMsg,"");                      // No weather message
  strcpy(nrToken,"");                         // No default National Rail token
  loadApiKeys();                              // Load the API keys from the apiKeys.json
  loadConfig();                               // Load the configuration settings from config.json
  u8g2.setContrast(brightness);               // Set the panel brightness to the user saved level
  if (flipScreen) u8g2.setFlipMode(0);
  u8g2.clearBuffer();
  centreText("Tiny Departures Board",4);
  centreText(notice.c_str(),24);
  u8g2.sendBuffer();
  delay(5000);

  u8g2.clearBuffer();
  drawBuildTime();
  u8g2.sendBuffer();
  progressBar("WiFi Connecting",20);
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
  std::vector<const char *> menu = {"wifi","exit"};
  wm.setMenu(menu);

  bool result = wm.autoConnect("Departures Board");    // Attempt to connect to WiFi (or enter interactive configuration mode)
  if (!result || wifiConfigured) {
      // Need to restart after config (cannot reuse port)
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
  progressBar("WiFi Connected",30);

// Configure the local webserver paths
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){handleRoot(request);});
  server.on("/erasewifi", HTTP_GET, [](AsyncWebServerRequest *request){handleEraseWiFi(request);});
  server.on("/factoryreset", HTTP_GET, [](AsyncWebServerRequest *request){handleFactoryReset(request);});
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request){handleInfo(request);});
  server.on("/formatffs", HTTP_GET, [](AsyncWebServerRequest *request){handleFormatFFS(request);});
  server.on("/dir", HTTP_GET, [](AsyncWebServerRequest *request){handleFileList(request);});
  server.onNotFound([](AsyncWebServerRequest *request){handleNotFound(request);});
  server.on("/cat", HTTP_GET, [](AsyncWebServerRequest *request){handleCat(request);});
  server.on("/del", HTTP_GET, [](AsyncWebServerRequest *request){handleDelete(request);});
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){handleReboot(request);});
  server.on("/stationpicker", HTTP_GET, [](AsyncWebServerRequest *request){handleStationPicker(request);});
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request){handleFirmwareInfo(request);});
  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request){handleBrightness(request);});
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){handleOtaUpdate(request);});
  server.on("/success", HTTP_GET, [](AsyncWebServerRequest *request){request->send(200,contentTypeHtml,successPage);});

  //
  // Save settings returned by the Web GUI
  //
  server.on("/savesettings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->_tempObject) {
      String* body = (String*)(request->_tempObject);
      saveFile("/config.json", body->c_str());

      delete body; // Clean up memory
      request->_tempObject = nullptr;

      if ((!railIsSet && !busIsSet) || (!nrToken[0] && rdmDeparturesApiKey=="" && boardMode==MODE_RAIL) || request->hasParam("reboot")) {
        // First time setup or base config change, we need a full reboot
        sendResponse(200,"Configuration saved. The Departures Board will now restart.",request);
        restartTimer.once(1, []() { ESP.restart(); });
      } else {
        sendResponse(200,"Configuration updated. The Departures Board will update shortly.",request);
        softResetNeeded = true;
      }
    } else {
      sendResponse(400,"Empty",request);
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index) {
      // First chunk: Create a String object in RAM
      request->_tempObject = new String("");
    }

    String* body = (String*)(request->_tempObject);
    for (size_t i = 0; i < len; i++) {
      body->concat((char)data[i]);
    }
  });

  //
  // Save the API keys returned from the Web GUI
  //
  server.on("/savekeys", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->_tempObject) {
      String* body = (String*)(request->_tempObject);

      JsonDocument doc;
      bool result = true;
      String msg = "The API keys have been saved successfully.";
      DeserializationError error = deserializeJson(doc, body->c_str());
      if (!error) {
        if (!saveFile("/apikeys.json", body->c_str())) {
          msg = "Failed to save the API keys to the file system (file system corrupt or full?)";
          result = false;
        } else {
          JsonObject settings = doc.as<JsonObject>();
          String nrToken = settings["nrToken"].as<String>();
          String rdmDepToken = settings["rdmDepKey"].as<String>();
          if (!nrToken.length() && !rdmDepToken.length()) msg+="\n\nNote: Only Bus Departures will be available without either Rail Data or National Rail keys.";
        }
      } else {
        msg = "Invalid JSON format. No changes have been saved.";
        result = false;
      }

      delete body; // Clean up memory
      request->_tempObject = nullptr;

      if (result) {
        // Load/Update the API Keys in memory
        loadApiKeys();
        // If all location codes are blank we're in the setup process. If not, the keys have been changed so just reboot.
        if (!railIsSet && !busIsSet) {
          sendResponse(200,msg,request);
          writeDefaultConfig();
          showSetupCrsHelpScreen();
        } else {
          msg += "\n\nThe Departures Board will now restart.";
          sendResponse(200,msg,request);
          restartTimer.once(1, []() { ESP.restart(); });
        }
      } else {
        sendResponse(400,msg,request);
      }
    } else {
      sendResponse(400,"Empty",request);
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index) {
      // First chunk: Create a String object in RAM
      request->_tempObject = new String("");
    }

    String* body = (String*)(request->_tempObject);
    for (size_t i = 0; i < len; i++) {
      body->concat((char)data[i]);
    }
  });

  //
  // Handle uploads to LittleFS
  //
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){request->send(200,contentTypeHtml,uploadPage);});
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->redirect("/success");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      String path = "/" + filename;
      if (LittleFS.exists(path)) LittleFS.remove(path);
      size_t fileSize = request->header("Content-Length").toInt();
      size_t availableSpace = LittleFS.totalBytes() - LittleFS.usedBytes() - 1024;

      if (fileSize > availableSpace) {
          sendResponse(507,"Insufficient storage space in File System",request);
          request->client()->close();
          return;
      }
      // First chunk: Create/Open the file and store the handle in _tempObject
      // We use a pointer to a File object so we can keep it open between chunks
      File *file = new File(LittleFS.open(path, FILE_WRITE));
      if (!*file) {
        sendResponse(500,"File System Error",request);
        request->client()->close();
        return;
      }
      request->_tempObject = file;
    }

    // If we have a valid file handle, write the current chunk
    if (len && request->_tempObject) {
      File *file = reinterpret_cast<File *>(request->_tempObject);
      file->write(data, len);
    }

    if (final && request->_tempObject) {
      // Last chunk: Close the file and clean up the pointer
      File *file = reinterpret_cast<File *>(request->_tempObject);
      file->close();
      delete file;
      request->_tempObject = nullptr;
    }
  });

  //
  // Handle manual firmware updates at /update
  //
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){request->send(200,contentTypeHtml,updatePage);});
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Check if the Update library encountered any errors.
    bool shouldReboot = !Update.hasError();

    // Create a response. The AJAX script is just looking for a successful HTTP status.
    AsyncWebServerResponse *response = request->beginResponse((shouldReboot ? 200 : 500), "text/plain", (shouldReboot ? "OK" : "FAIL"));
    response->addHeader("Connection", "close");
    request->send(response);

    // If successful, restart the ESP32 to boot into the new firmware
    if (shouldReboot) restartTimer.once(0.5, []() { ESP.restart(); });
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      // First chunk: Initialize the OTA Update
      // UPDATE_SIZE_UNKNOWN tells the library to just accept chunks until 'final' is true
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        sendResponse(500,"Update begin failed",request);
      }
    }

    // Write chunk data to the flash memory
    if (!Update.hasError() && len) {
      if (Update.write(data, len) != len) {
        sendResponse(500,"Update write failed",request);
      }
    }

    // Final chunk: Close the OTA process
    if (final) {
      if (!Update.end(true)) {
        sendResponse(500,"Update end failed",request);
      }
    }
  });

  server.begin();     // Start the local web server

  // Check for Firmware updates?
  if (firmwareUpdates) {
    progressBar("Checking for updates",40);
    if (ghUpdate.getLatestRelease()==UPD_SUCCESS) {
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
  if ((!railIsSet && !busIsSet) || (!nrToken[0] && rdmDeparturesApiKey=="" && boardMode==MODE_RAIL)) {
    if (!apiKeys) showSetupKeysHelpScreen();
    else showSetupCrsHelpScreen();
    // First time setup mode will exit with a reboot, so just loop here forever servicing web requests
    while (true) { delay(10); }
  }

  configTzTime(ukTimezone, "uk.pool.ntp.org","time.cloudflare.com","time.windows.com");
  if (timezone!="") {
    setenv("TZ",timezone.c_str(),1);
    tzset();
  }

  // Check the clock has been set successfully before continuing
  int p=50;
  int ntpAttempts=0;
  bool ntpResult=true;
  progressBar("Setting the clock",50);
  if(!getLocalTime(&timeinfo,2000)) {              // attempt to set the clock from NTP
    do {
      ntpResult = getLocalTime(&timeinfo,2000);
      ntpAttempts++;
      p+=5;
      progressBar("Setting the clock",p);
      if (p>80) p=45;
    } while ((!ntpResult) && (ntpAttempts<10));
  }
  if (!ntpResult) {
    // Sometimes NTP/UDP fails. A reboot usually fixes it.
    progressBar("NTP Failed. Will reboot.",0);
    delay(5000);
    ESP.restart();
  }


  station.numServices=0;
  if (boardMode == MODE_RAIL) {
      if (!useRDMclient) {
        // Using legacy darwin XML client
        progressBar("Initialising Nat'l Rail",60);
        int res = darwinRailData.init(wsdlHost, wsdlAPI);
        if (res != UPD_SUCCESS) {
          showWsdlFailureScreen();
          while (true) {delay(1);}
        }
      }
      progressBar("Initialising Nat'l Rail",70);
      rdmRailData.cleanFilter(platformFilter,cleanPlatformFilter,sizeof(platformFilter));
  } else if (boardMode == MODE_BUS) {
      progressBar("Initialising BusTimes",70);
      // Create a cleaned filter
      busdata.cleanFilter(busFilter,cleanBusFilter,sizeof(busFilter));
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

  if (manualUpdateCheck) doManualOtaCheck();

  if (softResetNeeded) {
    softResetNeeded = false;
    softResetBoard();
  }

}
