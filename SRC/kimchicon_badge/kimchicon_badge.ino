#include "Adafruit_GFX.h"     // https://github.com/adafruit/Adafruit-GFX-Library
#include "Adafruit_ST7735.h"  // https://github.com/adafruit/Adafruit-ST7735-Library
#include <SPI.h>
#include <EEPROM.h>

#include "image_data.h"
#include "flappy_bird.h"  // CREDIT : https://github.com/mrt-prodz/ATmega328-Flappy-Bird-Clone

//#define DEBUGGING
#define ENABLE_FLAPPY_BIRD         

char badgeServerAddr[] = "badge.kimchicon.org";
char command[32] = "CommandNumber";  
char tempBuffer[300] = "";

// initialize 1.8" TFT screen
#define TFT_DC     9     
#define TFT_RST   10    
#define TFT_CS    11     

// initialize screen with pin numbers
Adafruit_ST7735 TFT = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// instead of using TFT.width() and TFT.height() set constant values
#define TFTW            128         // screen width
#define TFTH            160         // screen height
#define TFTW2           TFTW/2      // half screen width
#define TFTH2           TFTH/2      // half screen height

// defines for JOYSTICK keys
#define JOYSTICK_UP     A1
#define JOYSTICK_DOWN   A2
#define JOYSTICK_LEFT   A0
#define JOYSTICK_RIGHT  A3
#define BUTTON_A        A4
#define BUTTON_B        A5

// defines for PrintKeyboard
#define KEY_LEFT_MARGIN                   10
#define KEYS_TOP_MARGIN                   40
#define CHAR_HEIGHT                       11
#define CHAR_WIDTH                        10
#define SELECTED_TEXT_CURSOR_LEFT_MARGIN  12
#define SELECTED_TEXT_CURSOR_WIDTH        8
#define CURSOR_WIDTH                      CHAR_WIDTH+1
#define ROW_KEYS_COUNT                    11

// defines for PrintTable
#define TABLE_TOP_MARGIN      30
#define TABLE_LEFT_MARGIN      7
#define TABLE_ITEM_HEIGHT     12

// defines for menus
#define MENU_TYPE_INTERNET    1   // get data from internet
#define MENU_TYPE_VOTE        2
#define MENU_TYPE_WIFI        3
#define MENU_TYPE_SETTING     4
#define MENU_TYPE_CREDIT      5
#define MENU_TYPE_CHANGE_USER_NAME  6

#define MENU_TOP_MARGIN       21
#define MENU_LEFT_MARGIN      5
#define MENU_HEIGHT           18
#define MAX_MENU_CNT          7
#define MAX_MENU_NAME_LEN     10
#define MENU_LEFT             38
#define MAX_PT_CNT            12
#define CHANGE_MENU_UP        -1
#define CHANGE_MENU_DOWN      1

// defines for PrintTable
#define SHOW_TABLE_TRUE     1
#define SHOW_TABLE_FALSE    0
#define FOR_SETTING_TRUE    1
#define FOR_SETTING_FALSE   0

// defines for PrintMyName
#define NAME_CHAR_WIDTH     11

// defines for LED and Buzzer
#define LED_CLOCK   4
#define LED_DATA    5
#define LED_LATCH   8
#define BUZZER      6

// defines for network
#define TIMEOUT               3000
#define CONNECT_TIMEOUT       3000
#define CONNECT_AP_TIMEOUT    10000
#define LIST_AP_TIMEOUT       15000

// defines for command
#define COMMAND_PRINT_IMAGE     '1'
#define COMMAND_ALERT_MSG       '2'
#define COMMAND_LED             '3'  

// user name and ip address of badge
char userName[20];
char ipAddr[20];

// badge menu list
enum {
  MENU_NOTICE,
  MENU_SCHED,
  MENU_GAME,
  MENU_GALLERY,
  MENU_VOTE,
  MENU_SETTING,
  MENU_CREDIT
};

// defines for EEPROM
#define EEPROM_ICON1              32    // +32
#define EEPROM_ICON2              64    // +32
#define EEPROM_ICON3              96    // +32
#define EEPROM_ICON4              128   // +32
#define EEPROM_ICON5              160   // +32
#define EEPROM_ICON6              192   // +32
#define EEPROM_ICON7              224   // +32
#define EEPROM_LOGO_IMG           256   // +357
#define EEPROM_USER_NAME          613   // +14
#define EEPROM_CREDITS            627   // +165
#define EEPROM_PERMISSION_DENIED  792   // +19
#define EEPROM_AP_NAME            811   // +20
#define EEPROM_AP_PASS            831   // +20
#define EEPROM_DEFAULT_AP_NAME    851   // +20
#define EEPROM_DEFAULT_AP_PASS    871   // +20
#define EEPROM_SETTING            891   // +24
#define EEPROM_TABLE_REQUEST      915   // +54
#define EEPROM_VOTE_REQUEST       969   // +54

// global variables
char menuList[][MAX_MENU_NAME_LEN] = {"Notice", "Schedule", "Game", "Gallery", "Vote", "Setting", "Credit"};
int currentMenuIndex = 0;
bool flappyBirdLock = true;

// Network Functions
int WaitString(char *findString, int timeout, char *saveBuffer)
{
  char *readyChar, ch;
  int count = 0, len;
  
  Serial1.setTimeout(timeout);
  readyChar = findString;

  while (true) {    
    len = Serial1.readBytes(&ch, 1);               
    if (!len || count>300){
      if(saveBuffer) {
        saveBuffer[count-1] = 0; 
        return count;  
      }
      return false; // this mean that it failed to find string 
    }

    if(saveBuffer)
      saveBuffer[count] = ch;
    
    count++;
        
    if (ch == *readyChar) {            
      readyChar++;
      if (*readyChar == 0)
        break;
    } else {
      readyChar = findString;
    }
  }

  if(saveBuffer)
    saveBuffer[count] = 0;
  return count;
}

void ClearESPReadBuffer(void)
{
  while(Serial1.available()) Serial1.read();
}

int SendCommand(char *cmd, int timeout, char *saveBuffer)
{  
  int len, ret;
  //Serial.print("AT command : ");
  //Serial.println(cmd);
  
  // clear buffer
  ClearESPReadBuffer();
  Serial1.println(cmd);  

  if(len = WaitString("OK", timeout, saveBuffer)) 
    ret = len;  
  else 
    ret = false;    
    
  ClearESPReadBuffer();
  return ret;
}

bool Ping(void)
{
  if(SendCommand("AT", TIMEOUT, false)){
    BootMsg("ESP-ping OK!");
    return true;
  }
  else{
    BootMsg("failed..");
    return false;
  } 
}

void ChangeBaudrate(long baudrate)
{
  sprintf(tempBuffer, "AT+CIOBAUD=%ld", baudrate);
  SendCommand(tempBuffer, TIMEOUT, false);   // change baud-rate
  Serial1.begin(baudrate);
  while (!Serial1) ; 
  // wating for re-initializing 
  delay(500);
}

// Scanning nearby wifi APs
// this funcion must be called after AT+CWMODE=1 or 3
/* reply example
 *  AT+CWLAP="",""
 *  +CWLAP:(2,"HGHG",-38,"c5:6e:1f:28:df:8c",10,10)
 *  +CWLAP:(1,"Idiol",-77,"00:26:66:ac:6b:a6",11,6)
 *  +CWLAP:(4,"EY3133",-90,"31:b5:c2:e6:f3:98",11,0)
 *  
 *  0 OPEN
 *  1 WEP
 *  2 WPA_PSK
 *  3 WPA2_PSK
 *  4 WPA_WPA2_PSK
 */

bool StartScan(char *buffer)
{ 
  int i, enc, power, j = 0, len;
  char apName[20], encStr[8];
  
  len = SendCommand("AT+CWLAP=\"\",\"\"", LIST_AP_TIMEOUT, buffer);  
  for(i=0; i<len; i++)
  {
      if(buffer[i] == '\n'){
        if(buffer[i+1] != '+')
          continue;
        sscanf(&buffer[i+1], "+CWLAP:(%d,\"%19[^\"]\",%d,", &enc, apName, &power);
        if(enc == 1)
          strcpy(encStr, "WEP");
        else if(enc > 1)
          strcpy(encStr, "WPA");
        else
          strcpy(encStr, "OPEN");
        sprintf(&buffer[j], "%s(%s)|", apName, encStr);  // no power ver
        j = strlen(buffer);
      }        
  }      
  return true;
}

bool Connect(char * host, unsigned int port)
{  
  ClearESPReadBuffer();    

  // 4 = Connection ID
  if (!Serial1.print("AT+CIPSTART=4,\"TCP\",\""))
    goto out;
  if (!Serial1.print(host))
    goto out;
  if (!Serial1.print("\","))
    goto out; 
  if (!Serial1.println(port))
    goto out;
  
  //if (!waitString("4,CONNECT", false))
  //  goto out;
  
  if (!WaitString("OK", TIMEOUT, false))
    goto out;
  
  //Serial.println("Connection OK!");
  return true;
  
out:
  //Serial.println("Connection failed..");
  ClearESPReadBuffer();
  return false;
}

// TCP socket send
void Send(char * buffer)
{  
  ClearESPReadBuffer();  
  
  Serial1.print("AT+CIPSEND=4,");    
  Serial1.println(strlen(buffer));
  
  if(!WaitString("OK", TIMEOUT, false))
    goto out;
  
  if(!WaitString("> ", TIMEOUT, false))
    goto out;
  
  //if(!WaitString(" ", TIMEOUT, false)) 
  //  goto out;

  // send packet data 
  Serial1.print(buffer);

  // we need this because our firmware outputs unintended "Recv 18 bytes" string 
  if(!WaitString("bytes", TIMEOUT, false))
    goto out;
  
  if(!WaitString("SEND OK", TIMEOUT, false))
    goto out;

  //Serial.println("send succeed!");   
  return;

out:
  //Serial.println("send failed...");   
  ClearESPReadBuffer();
}

bool GetRequest(char *request)
{
  if(!Connect(badgeServerAddr, 80)){
    Alert("Connect failed");
    return false;
  }
  Send(request);
}

// change text property 
void SetLCDText(int size, int color)
{
  TFT.setTextSize(size);
  TFT.setTextColor(color);
}

// print text to the TFT-LCD
void PrintLCD(char size, int color, int x, int y, char *msg)
{
  SetLCDText(size, color);
  TFT.setCursor(x, y);
  TFT.print(msg); 
}

// pring boot-msg to the TFT-LCD
void BootMsg(char *msg)
{
  static int y = 10;  
  PrintLCD(1, ST7735_YELLOW, 5, y, msg);
  y += CHAR_HEIGHT;
}

bool PrintKeyboard(char *subject, char *buffer)
{
  unsigned long currentTime;
  unsigned long prevTime = 0;  
  int i, x = KEY_LEFT_MARGIN, y = KEYS_TOP_MARGIN, key_x = 0, key_y = 0, delOffset_x, delOffset_y;
  unsigned char selectedChar = ' ', cursorIndex = 0;

  TFT.fillScreen(ST7735_BLACK);  
  TFT.drawRect(0, 0, TFTW, TFTH, ST7735_WHITE); 

  PrintLCD(1, ST7735_YELLOW, 35, 10, subject);

  // show selected text area
  for(i=0; i<13; i++)
    TFT.drawFastHLine(SELECTED_TEXT_CURSOR_LEFT_MARGIN+(i*SELECTED_TEXT_CURSOR_WIDTH), KEYS_TOP_MARGIN-5, 5, ST7735_WHITE);

  // print keyboard layout
  for(i=' '; i<='z'; i++, x+=10)
  {
    if((i - ' ') % ROW_KEYS_COUNT == 0){
      x = KEY_LEFT_MARGIN;
      y += CHAR_HEIGHT;
    }
      
    TFT.setCursor(x, y);
    TFT.print((char)i);
  }

  TFT.setTextColor(ST7735_CYAN);
  TFT.print(" <- OK");
  TFT.setTextColor(ST7735_WHITE);

  // process user's typing 
  while(1) {        
    delOffset_x = delOffset_y = 0;

    currentTime = millis();
    if(currentTime - prevTime > 100) {   
      prevTime = currentTime;      
      
      if(digitalRead(JOYSTICK_UP) == LOW) {
        if(key_y > 0) {
          key_y--;      
          delOffset_y = 1;
          selectedChar -= CURSOR_WIDTH;
        }
      }
      else if(digitalRead(JOYSTICK_DOWN) == LOW) {
        if(key_y < 8) {
          key_y++;              
          delOffset_y = -1;
          selectedChar += CURSOR_WIDTH;
        }
      }
      else if(digitalRead(JOYSTICK_LEFT) == LOW) {
        if(key_x > 0) {
          key_x--;
          delOffset_x = 1;      
          selectedChar--;
        }
      }
      else if(digitalRead(JOYSTICK_RIGHT) == LOW) {      
        if(key_x < 10) {
          key_x++;      
          delOffset_x = -1;      
          selectedChar++;
        }
      }
      else if(digitalRead(BUTTON_A) == LOW) {                    
        switch(selectedChar)
        {
          // Back Button
          case 0x7b:
          case 0x7c:
            PrintLCD(1, ST7735_BLACK, 12+(--cursorIndex*SELECTED_TEXT_CURSOR_WIDTH), KEYS_TOP_MARGIN-14, "");
            TFT.print(buffer[cursorIndex]);            
            break;
          // OK Button          
          case 0x7d:
          case 0x7e:
            Alert("saved!");
            buffer[cursorIndex] = 0;
            return true;
          default:
            if(cursorIndex > 12)
              break;
            PrintLCD(1, ST7735_WHITE, 12+(cursorIndex*SELECTED_TEXT_CURSOR_WIDTH), KEYS_TOP_MARGIN-14, "");
            TFT.print((char)selectedChar);
            buffer[cursorIndex++] = selectedChar;                     
            break;
        }
        // preventing double input
        delay(300);   
      }
      else if(digitalRead(BUTTON_B) == LOW)
        return false;
     
      TFT.drawFastHLine(KEY_LEFT_MARGIN-1+CHAR_WIDTH*(key_x + delOffset_x), KEYS_TOP_MARGIN+(CHAR_HEIGHT*(key_y + delOffset_y + 2))-2, CHAR_WIDTH-1, ST7735_BLACK); 
      TFT.drawFastHLine(KEY_LEFT_MARGIN-1+CHAR_WIDTH*(key_x), KEYS_TOP_MARGIN+(CHAR_HEIGHT*(key_y+2))-2, CHAR_WIDTH-1, ST7735_RED);      
    }    
  }  
}


// control 8-leds on the board
void Control8LED(int value)
{
  digitalWrite(LED_LATCH, LOW);
  shiftOut(LED_DATA, LED_CLOCK, LSBFIRST, value);
  digitalWrite(LED_LATCH, HIGH);
}

void PlaySound(int freq)
{
  for(int i=0; i<1000; i++);
  {
    digitalWrite(BUZZER, HIGH);
    delay(freq);
    digitalWrite(BUZZER, LOW);
    delay(freq);
  }
}

// goto sleep to debug
void Stop(void)
{
  while(1){}
}

// GET and print BMP image from HTTP server
bool PrintBMPImage(char *img)
{    
  char bmpData[2];
  unsigned short pixel;    
  unsigned char ch;  
  int packetLen, size;
  bool isFirstPacket = true;
  int i, len, draw_x = 0, draw_y = 0; 

  ChangeBaudrate(115200);
  TFT.fillScreen(ST7735_BLACK); 

  // get request to the server
  sprintf(tempBuffer, "GET /2016/%s HTTP/1.0\r\nHost: %s\r\n\r\n", img, badgeServerAddr);
  if(!GetRequest(tempBuffer))    
    return 0;
  
#ifdef DEBUGGING
  Serial.println(F("Printing response"));
#endif
  
  while(1){
    // format : IPD:4,LENGTH: response
    i = 0;
    memset(tempBuffer, 0, 20);
    while(1) {
      len = Serial1.readBytes(&ch, 1); 
      if(!len) // break when there is no more data
        break;

      // we will be faced 0x0d0a from second loop
      if(ch == 0x0d || ch == 0x0a)
        continue;
              
      tempBuffer[i++] = ch;        
      if(ch == ':')
        break;
    }  

#ifdef DEBUGGING
    Serial.print("IPD RESPONSE : ");
    Serial.println(tempBuffer);
#endif

    // we've finished receiving BMP image
    if(!strcmp(tempBuffer, "4,CLOSED")){
      //Serial.println("Finished!");
      break;
    }

#ifdef DEBUGGING
    for(int y=0; y<strlen(tempBuffer); y++)
      Serial.println((unsigned char)tempBuffer[y], HEX);
#endif

    // the maximun size of one packet is 1460  
    sscanf(tempBuffer, "+IPD,4,%d[^:]", &packetLen);

#ifdef DEBUGGING
    Serial.print("packetLen : ");
    Serial.println(packetLen);
#endif

    // eliminate the HTTP HEADER
    if(isFirstPacket){    
      len = WaitString("\r\n\r\n", TIMEOUT, false);          

#ifdef DEBUGGING
      Serial.print("Header length : ");
      Serial.println(readLen);
#endif
  
      packetLen -=  len;
      isFirstPacket = false;
    }    

    // determine size
    for(i=0; i<packetLen; i+=2)
    {      
      if(packetLen - i < 2)
        size = 1;
      else 
        size = 2;
        
      Serial1.readBytes((char *)&pixel, size);        
      
      // shooting to the LCD
      drawPixel(draw_x, draw_y, pixel);

      // increase Y position
      if(++draw_x == 128) {
        draw_x = 0;
        draw_y++;
      }
    }

    // we already printed all of the image data
    if(draw_y >= 160)
      break;
  }

  // waiting for user interaction
  while(1){
    delay(100);
    
    // process admin's command
    ProcessCommand();
    
    if(digitalRead(BUTTON_A) == LOW)
      return true;
    else if(digitalRead(BUTTON_B) == LOW)
      return false;
  }  
}

// print informations in table format
int PrintTable(char *title, char *data, bool isOutline, int menuType)
{  
  int i, len, packetLen, page = 1, offset, currentSchedule, idx, count, secretKeyIdx = 0; 
  char shortSubject[20], ch, secretKey[4];  
  char description[10][40], buffer[20+1], *p, *p2;

SHOW_TABLE_START:
  currentSchedule = 0, count = 0, idx = 0; 
  
  TFT.fillScreen(ST7735_BLACK);
  
  // print title
  PrintLCD(2, ST7735_WHITE, TFTW2 - strlen(title)*12/2, 7, title);

  // drawing table outline
  if(isOutline){
    for(i=0; i<=10; i++){
      TFT.drawFastHLine(TABLE_LEFT_MARGIN, TABLE_TOP_MARGIN + i*12, TFTW-10, ST7735_WHITE);
      //TFT.drawRect(TABLE_LEFT_MARGIN, TABLE_TOP_MARGIN + i*12, TFTW-13, 12, ST7735_WHITE);  
      //TFT.drawRect(TABLE_LEFT_MARGIN, TABLE_TOP_MARGIN + i*12, 35, 12, ST7735_WHITE);  
    }
  }

  // make information by handed argument string  
  if(menuType > MENU_TYPE_VOTE) {    
    idx = 0, count = 0;    
    for(i=0; i<strlen(data); i++) {      
      buffer[idx] = data[i];
      if(data[i] == '|'){ 
        // print string
        if(idx > 20)
          idx = 20;
        buffer[idx] = 0;

        strcpy(description[count], buffer);
        PrintLCD(1, ST7735_GREEN, 7, TABLE_TOP_MARGIN + count*12+3, buffer);
        idx = 0;
        count++;    
        continue;
      }
      idx++;
    }
  }
  else {      
    // connect to the data server and gethering information
    GetDataFromEEPROM(EEPROM_TABLE_REQUEST, tempBuffer+100, 60);
    sprintf(tempBuffer, tempBuffer+100, data, page, badgeServerAddr);    
    if(!GetRequest(tempBuffer))
      return 0;

    // wating for "+IPD" response    
    count = 0;

    // IPD:4,LENGTH: response
    i = 0;
    memset(tempBuffer, 0, 20);
    while(1) {
      len = Serial1.readBytes(&ch, 1);      
      if(!len) // break when there is no more data
        break;
    
      // warning, we will be faced 0x0d0a from second loop
      if(ch == 0x0d || ch == 0x0a)
        continue;
            
      tempBuffer[i++] = ch;    
    
      // +IPD,4,332:
      if(ch == ':')
        break;
    }  
    
#ifdef DEBUGGING
    Serial.print("IPD RESPONSE : ");
    Serial.println(tempBuffer);
#endif
    
    // the size of one packet is 1460  
    sscanf(tempBuffer, "+IPD,4,%d[^:]", &packetLen);
    
#ifdef DEBUGGING
    Serial.print("packetLen : ");
    Serial.println(packetLen);
#endif
    
    // eliminate the HTTP HEADER
    len = WaitString("\r\n\r\n", TIMEOUT, false); 
    packetLen -=  len;

#ifdef DEBUGGING
    Serial.print("packetLen2 : ");
    Serial.println(packetLen);
#endif

    idx = 0;
    memset(tempBuffer, 0, 20);
    for(i=0; i<packetLen; i++)
    {      
      if(!Serial1.readBytes(&ch, 1))         
        break;
    
      // time 
      if(ch == '|'){      
        // printing time information
        tempBuffer[idx] = 0;
        PrintLCD(1, ST7735_GREEN, 10, TABLE_TOP_MARGIN + count*TABLE_ITEM_HEIGHT+3, tempBuffer); 
        idx = 0;
        continue;
      }
      else if(ch == '\n'){   // description 
         // printing subject informatin
        tempBuffer[idx] = 0;
        strcpy(description[count], tempBuffer);
        tempBuffer[13] = 0;
        PrintLCD(1, ST7735_WHITE, 45, TABLE_TOP_MARGIN + count*TABLE_ITEM_HEIGHT+3, tempBuffer);        
        idx = 0;
        count++;
        continue;
      }
    
      tempBuffer[idx++] = ch;
    }
  }

  // key press processing
  idx = 0; 
  offset = 0;
  while(1){
    delay(100);
    
    // process admin's command
    ProcessCommand();
    
    if(menuType == MENU_TYPE_INTERNET && strlen(description[idx]) > 13){
      TFT.fillRect(43, TABLE_TOP_MARGIN + idx*TABLE_ITEM_HEIGHT+3, 80, 8, ST7735_BLACK);  
      strncpy(tempBuffer, description[idx]+offset++, 13);
      PrintLCD(1, ST7735_WHITE, 45, TABLE_TOP_MARGIN + idx*TABLE_ITEM_HEIGHT+3, tempBuffer);    
      if(offset > strlen(description[idx]))
        offset = 0;
    }

    // menu up
    if(digitalRead(JOYSTICK_UP) == LOW) {          
      if(idx == 0 && page == 1)
          continue;      
      if(--idx < 0){        
        if(page > 1)
          page--;
        goto SHOW_TABLE_START;           
      }
      offset = -1;    
    }
    // menu down
    else if(digitalRead(JOYSTICK_DOWN) == LOW) {  
      if(idx+1 > count-1){      
        if(menuType == MENU_TYPE_INTERNET) {
          page++;
          goto SHOW_TABLE_START;           
        }
        else
          continue;        
      }
      idx++;
      offset = 1;
      if(menuType == MENU_TYPE_CREDIT) {
        secretKey[secretKeyIdx++] = '$';
        PlaySound(40);
      }
    }
    else if(digitalRead(JOYSTICK_RIGHT) == LOW) {   
      if(menuType == MENU_TYPE_CREDIT) {   
        secretKey[secretKeyIdx++] = '*';
        PlaySound(40);
      }
    }
    else if(digitalRead(BUTTON_A) == LOW){
      switch(menuType)
      {
        case MENU_TYPE_SETTING:     
          if(idx == 0)
            return MENU_TYPE_CHANGE_USER_NAME;            
          else
            return MENU_TYPE_WIFI;
        case MENU_TYPE_WIFI:                   
          if(PrintKeyboard("password", tempBuffer)) {
            if(p = strchr(description[idx], '('))             
              *p = 0;
            for(i=0; i<17; i++) {
                EEPROM.write(EEPROM_AP_NAME + i, description[idx][i]);            
                EEPROM.write(EEPROM_AP_PASS + i + 3, tempBuffer[i]);      
            }        
            Alert("reset please");
          }
          return 0;        
        case MENU_TYPE_CREDIT:                    
          if(!strncmp(secretKey, "*$*", 3)){
            Alert("ok!");            
            flappyBirdLock = false;
            return 0;
          }
          secretKeyIdx = 0;
          break;          
        case MENU_TYPE_VOTE:
          GetDataFromEEPROM(EEPROM_VOTE_REQUEST, tempBuffer+100, 60);
          sprintf(tempBuffer, tempBuffer+100, userName, idx+1, badgeServerAddr);
          if(!GetRequest(tempBuffer))
            return 0;
          Alert("voted!");
          return 0;
      }
    }
    else if(digitalRead(BUTTON_B) == LOW) {
      return 0; 
    }

    // erase previous selected item
    if(menuType != MENU_TYPE_CREDIT){
      PrintLCD(1, ST7735_BLACK, 1, 32 + (idx-offset)*TABLE_ITEM_HEIGHT, ">");
      PrintLCD(1, ST7735_WHITE, 1, 32 + idx*TABLE_ITEM_HEIGHT, ">");
    }
  }
}

// Get color image from EEPROM and output to the LCD 
void PrintColorImageFromEEPROM(int eepromOffset, int left_margin, int top_margin, int width, int height)
{    
  int idx = 0;
  unsigned short color, temp;
  for(int y=0; y<height; y++) {
    for(int x=0; x<width; x++) {        
      color = EEPROM.read(eepromOffset + idx++);      
      temp =  EEPROM.read(eepromOffset + idx++);
      color = color | (temp<<8);       
      drawPixel(x+left_margin, y+top_margin, color);
    }  
  }
}

// Get color image from flash memory and output to the LCD 
void PrintColorImageFromFlash(unsigned short *img, int left_margin, int top_margin, int width, int height)
{        
  for(int y=0; y<height; y++) {
    for(int x=0; x<width; x++) {        
      drawPixel(x+left_margin, y+top_margin, pgm_read_word(img + x + y*width));      
    }  
  }
}

// Get White/Blacxk image from flash memory and output to the LCD 
void printWBImageFromFlash(unsigned char *img, int left_margin, int top_margin, int width, int height, short int img_color)
{
  int i, x = 0, y = 0;
  int idx = 0, bit;
  unsigned short color;

  for(idx=0; idx<width*height/8; idx++){    
    for(i=7; i>=0; i--){    
      bit = (pgm_read_byte(img + idx) >> i) & 0x01;
      if(bit)
        color = img_color;
      else
        color = ST7735_BLACK;

      // shooting the image pixel to the LCD
      drawPixel(x+left_margin, y+top_margin, color);
    }
  }
}

// Get White/Blacxk image from EEPROM and output to the LCD 
void printWBImageFromEEPROM(int eepromOffset, int left_margin, int top_margin, int width, int height, short int img_color)
{
  int i, x = 0, y = 0;
  int idx = 0, bit;
  unsigned short color;

  for(idx=0; idx<width*height/8; idx++){    
    for(i=7; i>=0; i--){    
      bit = (EEPROM.read(eepromOffset + idx) >> i) & 0x01;
      if(bit)
        color = ST7735_BLACK;
      else
        color = img_color;

      // shooting the image pixel to the LCD
      drawPixel(x+left_margin, y+top_margin, color);

      if(++x == width){
          x = 0;
          y++;
      }      
    }
  }
}

// print wifi status on the top
void PrintStatus()
{
  // status information 
  sprintf(tempBuffer, "WIFI %s", *ipAddr ? ipAddr : "OFFLINE");
  PrintLCD(1, ST7735_GREEN, 0, 1, tempBuffer);
}

void PrintMainImage()
{
  // show main logo image
  //printColorImage((unsigned short *)LOGO_IMG, 36, 20, 51, 51);
  //printWBImageFromFlash((unsigned char *)LOGO_IMG, 36, 25, 51, 56, ST7735_YELLOW);
  printWBImageFromEEPROM(EEPROM_LOGO_IMG, 36, 25, 56, 51, ST7735_CYAN);

  // top line
  TFT.fillRect(0, 12, TFTW, 1, ST7735_WHITE);

  //PrintLCD(2, TFT.Color565(172,175,246), TFTW2 - (9*6)+1, TFTH2 - 4+17+1, "KIMCHICON");
  PrintLCD(2, ST7735_CYAN, TFTW2 - (9*6), TFTH2 - 4+17, "KIMCHICON");

  // under line
  TFT.fillRect(10, 110, TFTW-20, 1, ST7735_WHITE);  

  // year information
  PrintLCD(1, ST7735_WHITE, TFTW2 - (9*6)+84+1, TFTH2 - 4+10+18+4+7, "2016");
}

void PrintMyName()
{
  unsigned long currentTime;
  static unsigned long prevTime = 0;
  static int x = -198 - (strlen(userName)+2)*NAME_CHAR_WIDTH/2;
  static int direction = -3;  

  // update on every 300 millsecs
  currentTime = millis();
  if(currentTime - prevTime > 300){   
    prevTime = currentTime;  
   
    // erase
    PrintLCD(2, ST7735_BLACK, x-direction, TFTH2 + 53, userName);
    PrintLCD(2, ST7735_GREEN, x, TFTH2 + 53, userName);    

    // change flow direction 
    if(x < -265)
      direction = 3;
    else if(x +  (strlen(userName))*NAME_CHAR_WIDTH > -121)
      direction = -3;
      
    x += direction;   
  
    // bottom line
    TFT.fillRect(0, TFTH-2, TFTW, 1, ST7735_WHITE);  
  }
}

void Alert(char *msg)
{  
  TFT.fillRect(2, TFTH/2-10, TFTW-4, 20, TFT.Color565(59,59,59));
  TFT.drawRect(2, TFTH/2-10, TFTW-4, 20, ST7735_WHITE);
  PrintLCD(1, ST7735_WHITE, 5, TFTH/2-4, msg);  
  delay(1000);
}


// return value : refresh(true) or not(false)
bool ProcessCommand(void)
{
  int i, len;
  char *p, *p2;  
  
  // wating for command from admin
  Serial1.setTimeout(10);
  if(len = Serial1.readBytes(tempBuffer, 60)){         
    tempBuffer[len] = 0;            
    if(strstr(tempBuffer, "+IPD")){          
      if(p = strchr(tempBuffer, ':')){          
        if(p2 = strchr(p+1, '\n'))          
          *p2 = 0;

        for(i=0; i<strlen(p+1); i++)
          command[i] = *(p+1+i)  ^ 0x13;
        command[i] = 0;

        // invalid format
        if(command[1] != '|')
          return false;
          
        switch(command[0])
        {
          case COMMAND_PRINT_IMAGE:              
              PrintBMPImage(&command[2]);
              return true;
          case COMMAND_ALERT_MSG:
              // 2|BREAK TIME IS OVER!
              PlaySound(100);
              Alert(&command[2]);
              return false;
          case COMMAND_LED:
              // light on the LEDs
              Control8LED(command[2]);
              return false;
        }                    
      }        
    }
  } 
  return false;
}

void BadgeMain() {    

START:
  // clear screen
  TFT.fillScreen(ST7735_BLACK);  
  
  // print main display
  PrintMainImage();
  PrintStatus();  
  
  // wating for button push 
  while(1){
    // process admin's command
    if(ProcessCommand())
      goto START;

    // print user naem
    PrintMyName();    

    // go to the menu
    if (digitalRead(BUTTON_A) == LOW){
      BadgeMenu();
      break;
    }    
  }    
}

void GetDataFromEEPROM(int offset, char *buffer, int size)
{
  for(int i=0; i<size; i++)
    buffer[i] = EEPROM.read(offset + i);
}

void ChangeMenu(char direction)
{
  // erase previous box and re-drawing
  TFT.drawRect(5, MENU_TOP_MARGIN+MENU_HEIGHT*currentMenuIndex, TFTW-10, MENU_TOP_MARGIN, ST7735_BLACK);       
  TFT.drawRect(5, MENU_TOP_MARGIN+MENU_HEIGHT*(currentMenuIndex+direction), TFTW-10, MENU_TOP_MARGIN, ST7735_WHITE);

  // change color to white on previous menu 
  PrintLCD(2, ST7735_WHITE, TFTW2-MENU_LEFT, 25+MENU_HEIGHT*currentMenuIndex, menuList[currentMenuIndex]);  
  currentMenuIndex += direction;              
  // change color to green on seleted menu
  PrintLCD(2, ST7735_GREEN , TFTW2-MENU_LEFT, 25+MENU_HEIGHT*currentMenuIndex, menuList[currentMenuIndex]);   
}

void BadgeMenu()
{
  unsigned long currentTime;
  unsigned long prevTime = 0;
  int i, ret;
  int menuImages[] = {EEPROM_ICON1, EEPROM_ICON2, EEPROM_ICON3, EEPROM_ICON4, EEPROM_ICON5, EEPROM_ICON6, EEPROM_ICON7};
  int menuImageColors[] = {ST7735_YELLOW, ST7735_WHITE, ST7735_RED, ST7735_WHITE, ST7735_CYAN, ST7735_GREEN, ST7735_WHITE};  
  
SHOW_MENU_START:
  // clear screen
  TFT.fillScreen(ST7735_BLACK);  

  // draw essential stuffs
  PrintStatus();
  TFT.drawRect(0, 11, TFTW, TFTH-11, ST7735_WHITE);

  // show menu icons
  for(i=0; i<MAX_MENU_CNT; i++)
    printWBImageFromEEPROM(menuImages[i], 7, 24+MENU_HEIGHT*i, 16, 16, menuImageColors[i]);    

  // show menu name  
  for(i=0; i<MAX_MENU_CNT; i++)  
    PrintLCD(2, ST7735_WHITE, TFTW2-MENU_LEFT, 25+MENU_HEIGHT*i, menuList[i]);  

  // indicate selected menu
  TFT.drawRect(MENU_LEFT_MARGIN, MENU_TOP_MARGIN+MENU_HEIGHT*currentMenuIndex, TFTW-10, MENU_TOP_MARGIN, ST7735_WHITE);
  PrintLCD(2, ST7735_YELLOW, TFTW2-MENU_LEFT, 25+MENU_HEIGHT*currentMenuIndex, menuList[currentMenuIndex]);  
      
  while (1) {

    // process admin's command
    ProcessCommand();
    
    // joystick UP
    if (digitalRead(JOYSTICK_UP) == LOW) {    
        if(currentMenuIndex == 0)
        continue;        

        // update on every 300mill secs
        currentTime = millis();
        if(currentTime - prevTime > 300){   
          prevTime = currentTime;                        
          ChangeMenu(CHANGE_MENU_UP); 
        }    
    }
    // joystick DOWN
    else if(digitalRead(JOYSTICK_DOWN) == LOW) {    
      if(currentMenuIndex >= MAX_MENU_CNT-1)
        continue; 

      // update on every 300mill secs
      currentTime = millis();
      if(currentTime - prevTime > 300){   
        prevTime = currentTime;              
        ChangeMenu(CHANGE_MENU_DOWN);        
      }
    }        
    else if(digitalRead(BUTTON_A) == LOW){
      // processing to selected menu 
      switch(currentMenuIndex)
      {
        case MENU_NOTICE:
          PrintTable("NOTICE", "notice", SHOW_TABLE_TRUE, MENU_TYPE_INTERNET);
          break;
        case MENU_SCHED:          
          PrintTable("SCHEDULE", "sched", SHOW_TABLE_TRUE, MENU_TYPE_INTERNET);
          break;
        case MENU_GAME:
          if(flappyBirdLock){
            GetDataFromEEPROM(EEPROM_PERMISSION_DENIED, tempBuffer, 20);
            Alert(tempBuffer);            
          }   
#ifdef ENABLE_FLAPPY_BIRD         
          else
            RunFlappyBird();
#endif
          break;
        case MENU_GALLERY:                    
          while(1){            
            if(!PrintBMPImage("gallery.php")){
              ChangeBaudrate(9600);
              break;
            }
          }          
          break;
        case MENU_VOTE:
          PrintTable("VOTE", "vote", SHOW_TABLE_FALSE, MENU_TYPE_VOTE);
          break;
        case MENU_SETTING:
          GetDataFromEEPROM(EEPROM_SETTING, tempBuffer, 40);
          ret = PrintTable("SETTINGS", tempBuffer, SHOW_TABLE_FALSE, MENU_TYPE_SETTING);
          if(ret == MENU_TYPE_CHANGE_USER_NAME) {
            if(PrintKeyboard("your name", tempBuffer)) {
              strcpy(userName, tempBuffer);
              for(i=0; i<14; i++)
                EEPROM.write(EEPROM_USER_NAME + i, tempBuffer[i]);
            }
          }
          else if(ret == MENU_TYPE_WIFI) {
            Alert("scanning...");
            StartScan(tempBuffer);
            PrintTable("WIFI", tempBuffer, 0, MENU_TYPE_WIFI);  
          } 
          break;
        case MENU_CREDIT:          
          GetDataFromEEPROM(EEPROM_CREDITS, tempBuffer, 170);                    
          PrintTable("CREDIT", tempBuffer, SHOW_TABLE_FALSE, MENU_TYPE_CREDIT);          
          break;
      }         
      goto SHOW_MENU_START; 
    }
    // back to previous page
    else if(digitalRead(BUTTON_B) == LOW){
      break;
    }
  }  
}

// initial setup
void setup() {      
  int i;  
  char inputPins[] = {BUTTON_A, BUTTON_B, JOYSTICK_UP, JOYSTICK_DOWN, JOYSTICK_LEFT, JOYSTICK_RIGHT};
  char outputPins[] = {LED_CLOCK, LED_DATA, LED_LATCH};
  char apName[20], apPass[20], *p, *p2;
  
  // initialize joystick pins
  for(i=0; i<6; i++)
    pinMode(inputPins[i], INPUT_PULLUP); 

  // initialize LED pins
  for(i=0; i<3; i++)
    pinMode(outputPins[i], OUTPUT); 
  
  // initialize Buzzer
  pinMode(BUZZER, OUTPUT);
  
  // turn off all of the LEDs
  Control8LED(0xff);

  // initialize a ST7735S chip, black tab + change rotation
  TFT.initR(INITR_BLACKTAB);
  TFT.fillScreen(ST7735_BLACK);
  TFT.setRotation_m(); 

  // initialize SERIAL
  Serial.begin(115200);
  //while (!Serial) ;   
  
  BootMsg("Power ON!");    

  // get user name from EEPROM
  GetDataFromEEPROM(EEPROM_USER_NAME, userName, 20);  
  if(!userName[0]) 
    strcpy(userName, "YourName");
    
  // initializing UART to ESP8266
  Serial1.begin(9600);
  while (!Serial1) ; 

  // wait for ESP initializing
  delay(500);
    
  if(!Ping()){  // ping test
    BootMsg("change baud-rate");
    // if failed, change baud-rate
    ChangeBaudrate(115200);

    if(Ping()){  // ping test again
      ChangeBaudrate(9600);    
    }      
    else 
       goto out;    
  }    

  // set wifi mode to station 
  SendCommand("AT+CWMODE=1", TIMEOUT,  false);

  // connect to the AP
  BootMsg("connecting to AP..");    
  for(i=0; i<17; i++) {
      apName[i] = EEPROM.read(EEPROM_AP_NAME + i); 
      apPass[i] = EEPROM.read(EEPROM_AP_PASS + i + 3);     
  }
  sprintf(tempBuffer, "AT+CWJAP=\"%s\",\"%s\"", apName, apPass);    

  if(!SendCommand(tempBuffer, CONNECT_AP_TIMEOUT, false)) {
    // restore to default ap name/pass
    for(i=0; i<17; i++) {
        apName[i] = EEPROM.read(EEPROM_DEFAULT_AP_NAME + i);     
        EEPROM.write(EEPROM_AP_NAME + i, apName[i]); 
        apPass[i] = EEPROM.read(EEPROM_DEFAULT_AP_PASS + i + 3);
        EEPROM.write(EEPROM_AP_PASS + i + 3, apPass[i]); 
    }
    goto out;
  }

  BootMsg("OK!");
  
  // check my IP address     
  SendCommand("AT+CIFSR", TIMEOUT, tempBuffer);      
  if(p = strstr(tempBuffer, "IP,")) {    
    if(p2 = strchr(p+4, '\"')) {
      *p2 = 0;
      strcpy(ipAddr, p+4);
    }
  }  

  // set to multiple connection    
  SendCommand("AT+CIPMUX=1", TIMEOUT, false);     
  
  // run server, 1 = open / 90 = port
  if(!SendCommand("AT+CIPSERVER=1,90", TIMEOUT, false))
    goto out;

  // wait for see..
  //delay(2000);  
  return;
out:  
  BootMsg("FAILED..");
  delay(3000);    
}

// main loop
void loop() {  
  BadgeMain();  
}
