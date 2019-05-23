#include <SPI.h> // not necessary if Arduino IDE version >=1.6.6]
#include <TFT_eSPI.h> // Hardware-specific library
#define TFT_GREY 0x5AEB
#define TFT_Z 0xFFD300
#define TFT_GRAY 0x847b62
TFT_eSPI myGLCD = TFT_eSPI();

#include "DHT.h"
#define DHTPIN 12
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#include <ESP8266WiFi.h>
const char* ssid     = "STASSID";
const char* password = "STAPSK";
//--------------------------------------------------------
// The scrolling area must be a integral multiple of TEXT_HEIGHT
#define TEXT_HEIGHT 16 // Height of text to be printed and scrolled
#define BOT_FIXED_AREA 0 // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 16 // Number of lines in top fixed area (lines counted from top of screen)
#define YMAX 240 // Bottom of screen area

// The initial y coordinate of the top of the scrolling area
uint16_t yStart = TOP_FIXED_AREA;
// yArea must be a integral multiple of TEXT_HEIGHT
uint16_t yArea = YMAX-TOP_FIXED_AREA-BOT_FIXED_AREA;
// The initial y coordinate of the top of the bottom text line
uint16_t yDraw = YMAX - BOT_FIXED_AREA - TEXT_HEIGHT;

// Keep track of the drawing x coordinate
uint16_t xPos = 0;

// For the byte we read from the serial port
byte data = 0;

// A few test variables used during debugging
boolean change_colour = 1;
boolean selected = 1;

// We have to blank the top line each time the display is scrolled, but this takes up to 13 milliseconds
// for a full width line, meanwhile the serial buffer may be filling... and overflowing
// We can speed up scrolling of short text lines by just blanking the character we drew
int blank[19]; // We keep all the strings pixel lengths to optimise the speed of the top line blanking
//-------------------------------------------------------------

#define NSTARS 1024
uint8_t sx[NSTARS] = {};
uint8_t sy[NSTARS] = {};
uint8_t sz[NSTARS] = {};

uint8_t za, zb, zc, zx;

// Fast 0-255 random number generator from http://eternityforest.com/Projects/rng.php:
uint8_t __attribute__((always_inline)) rng()
{
  zx++;
  za = (za^zc^zx);
  zb = (zb+za);
  zc = (zc+(zb>>1)^za);
  return zc;
}


unsigned long Timer_Delay = 0;
unsigned long lastMenuTime = 0;
unsigned long lastTimeCalibrate = 0;
unsigned long last_delay = 0;
unsigned long last_delay_co2 = 0;
int flag_off_menu = 0;
int flag_calibration = 0;
int flag_stop_display = 0;
int new_menu = 0;

float last_humidity = 0;
float last_temperature = 0;

  float t = 0;
  float h = 0; 
  int t_int = 0; 
  int h_int = 0;

int ppm = 0;
float Temp1 = 0;
int last_ppm = 0; 
unsigned long previousMillis = 0;
 
byte cmd_ppm_range[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB}; //5000 ppm range
byte cmd_logic_on[9] = {0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6}; // ABC logic on
byte cmd_logic_off[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86}; // ABC logic off
//byte cmd_calibrate[9] = {0xFF, 0x01, 0x88, 0x02, 0x00, 0x00, 0x00, 0x00, 0x74}; // 500 ppm
byte cmd_calibrate[9] = {0xFF, 0x01, 0x88, 0x06, 0x01, 0x00, 0x00, 0x00, 0x70}; // 1000 ppm +1
//byte cmd_calibrate[9] = {0xFF, 0x01, 0x88, 0x01, 0x00, 0x00, 0x00, 0x00, 0x75}; // 256 ppm
byte setrangeA_cmd[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB};
byte cmd_calibrate_400[9] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78}; // 400 ppm
int responseHigh = 0;
int responseLow = 0;

unsigned long sensor_not_redy = 0;  
unsigned long sensor_not_redy_delay = 5000; 
unsigned long sensor_timer_starta = 0;

String incoming;

bool flag_terminal = false;
bool flag_start_terminal = false;
bool stringComplete = false;
 
void setup() 
{ 
  WiFi.mode(WIFI_OFF);
  WiFi.begin(ssid, password);
  
  myGLCD.init();
  myGLCD.setRotation(3);
  myGLCD.setSwapBytes(true);

  za = random(256);
  zb = random(256);
  zc = random(256);
  zx = random(256);

  myGLCD.fillScreen(TFT_BLACK);

  dht.begin();
  Serial.begin(9600);
}

void loop () {
  
   if ((millis() - last_delay_co2) > 10000) { 
   last_delay_co2 = millis();
   ppm = readCO2();
   }
   stars_loop();

   if ((millis() - last_delay) > 10000) { 
    last_delay = millis();
     float t_n = dht.readTemperature();
     float h_n = dht.readHumidity();

     if (isnan(t_n) || isnan(h_n)) {
       Serial.println("ERROR---------------------");
     }
     else {
      t = t_n;
      h = h_n;
      t_int = t*10; 
      h_int = h*10;
      Serial.println("t: "+String(t)+", h: "+String(h));   
     }
   }
}

int readCO2()
{
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  // command to ask for data
  byte response[9]; // for answer

  Serial.write(cmd, 9); //request PPM CO2
  while (Serial.available() > 0 && (unsigned char)Serial.peek() != 0xFF) {
    Serial.read();
  }

  memset(response, 0, 9);
  Serial.readBytes(response, 9);
  Temp1 = response[4] - 40;
 
  byte crc = 0;
  for (int i = 1; i < 8; i++) {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc) {
     responseHigh = (int) response[2];
     responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;
    Temp1 = response[4] - 40;
    return ppm;
  } else {
    return 0;
  }
  
}

void menu_co2_temperature(){
    myGLCD.fillScreen(TFT_BLACK);        
    myGLCD.setTextColor(TFT_WHITE,TFT_BLACK);
    myGLCD.drawCentreString ( String("CLIMATE CONTROL"), 160, 5, 4 );      
    myGLCD.setTextColor(TFT_WHITE,TFT_BLACK);
    myGLCD.drawString ( String("T"), 0, 120, 2 );
    myGLCD.setTextColor(TFT_WHITE,TFT_BLACK);
    myGLCD.drawString ( String("H"), 160, 120, 2 );
}
 
void stars_loop()
{
      myGLCD.setTextColor(TFT_WHITE,TFT_BLACK);
      myGLCD.drawCentreString ( String("CLIMATE CONTROL"), 160, 5, 4 );

      myGLCD.setTextColor(TFT_GREEN,TFT_BLACK); 
      myGLCD.drawNumber(ppm, 28, 31, 8 );
      myGLCD.setTextColor(TFT_GREEN,TFT_BLACK); 
      myGLCD.drawNumber(Temp1, 0, 31, 4 );
               
      myGLCD.setTextColor(TFT_YELLOW,TFT_BLACK);
      myGLCD.drawNumber(t, 8, 120, 8 ); 
      myGLCD.setTextColor(TFT_YELLOW,TFT_BLACK);
      myGLCD.drawString("."+String(t_int%10), 118, 120, 4 );    
  
      myGLCD.setTextColor(TFT_BLUE,TFT_BLACK);
      myGLCD.drawNumber(h, 168, 120, 8 );
      myGLCD.setTextColor(TFT_BLUE,TFT_BLACK);
      myGLCD.drawString("."+String(h_int%10), 278, 120, 4 );      
      
  unsigned long t0 = micros();
  uint8_t spawnDepthVariation = 255;

  for(int i = 0; i < NSTARS; ++i)
  {
    if (sz[i] <= 1)
    {
      sx[i] = 160 - 120 + rng();
      sy[i] = rng();
      sz[i] = spawnDepthVariation--;
    }
    else
    {
      int old_screen_x = ((int)sx[i] - 160) * 256 / sz[i] + 160;
      int old_screen_y = ((int)sy[i] - 120) * 256 / sz[i] + 120;

      // This is a faster pixel drawing function for occassions where many single pixels must be drawn
      myGLCD.drawPixel(old_screen_x, old_screen_y,TFT_BLACK);

      sz[i] -= 2;
      if (sz[i] > 1)
      {
        int screen_x = ((int)sx[i] - 160) * 256 / sz[i] + 160;
        int screen_y = ((int)sy[i] - 120) * 256 / sz[i] + 120;
  
        if (screen_x >= 0 && screen_y >= 0 && screen_x < 320 && screen_y < 240)
        {
          uint8_t r, g, b;
          r = g = b = 255 - sz[i];
          myGLCD.drawPixel(screen_x, screen_y, myGLCD.color565(r,g,b));
        }
        else
          sz[i] = 0; // Out of screen, die.
      }
    }
  }
  unsigned long t1 = micros();
}


// ##############################################################################################
// Call this function to scroll the display one text line
// ##############################################################################################
int scroll_line() {
  int yTemp = yStart; // Store the old yStart, this is where we draw the next line
  // Use the record of line lengths to optimise the rectangle size we need to erase the top line
  myGLCD.fillRect(0,yStart,blank[(yStart-TOP_FIXED_AREA)/TEXT_HEIGHT],TEXT_HEIGHT, TFT_BLACK);

  // Change the top of the scroll area
  yStart+=TEXT_HEIGHT;
  // The value must wrap around as the screen memory is a circular buffer
  if (yStart >= YMAX - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA + (yStart - YMAX + BOT_FIXED_AREA);
  // Now we can scroll the display
  scrollAddress(yStart);
  return  yTemp;
}


void start_terminal_tft() {
  myGLCD.fillScreen(TFT_BLACK);
  
  // Setup baud rate and draw top banner
  
  myGLCD.setTextColor(TFT_WHITE, TFT_BLUE);
  myGLCD.drawCentreString(" Serial Terminal - 9600 baud ",120,0,2);

  // Change colour for scrolling zone text
  myGLCD.setTextColor(TFT_WHITE, TFT_BLACK);

  // Setup scroll area
  setupScrollArea(TOP_FIXED_AREA, BOT_FIXED_AREA);

  // Zero the array
  for (byte i = 0; i<18; i++) blank[i]=0;
}

// ##############################################################################################
// Setup a portion of the screen for vertical scrolling
// ##############################################################################################
// We are using a hardware feature of the display, so we can only scroll in portrait orientation
void setupScrollArea(uint16_t tfa, uint16_t bfa) {
  //myGLCD.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
//  myGLCD.writecommand(ILI9488_VSCRDEF); // Vertical scroll definition
  myGLCD.writedata(tfa >> 8);           // Top Fixed Area line count
  myGLCD.writedata(tfa);
  myGLCD.writedata((YMAX-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  myGLCD.writedata(YMAX-tfa-bfa);
  myGLCD.writedata(bfa >> 8);           // Bottom Fixed Area line count
  myGLCD.writedata(bfa);
}

// ##############################################################################################
// Setup the vertical scrolling start address pointer
// ##############################################################################################
void scrollAddress(uint16_t vsp) {
  //myGLCD.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
 // myGLCD.writecommand(ILI9488_VSCRSADD); // Vertical scrolling pointer
  myGLCD.writedata(vsp>>8);
  myGLCD.writedata(vsp);
}
