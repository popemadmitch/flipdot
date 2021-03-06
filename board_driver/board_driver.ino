/*
 * Swansea Hackspace FlipDot Driver system
 * 
 * Library Requires:
 * - Adafruit GFX Library
 * - CopyThreads
 */
 
 #include <Cth.h>
#include <Adafruit_GFX.h>
#include "FlipDot_GFX.h"


FlipDot_GFX display;

/* Driver address pins (w/ pullups) */
#define PANELADDR_0         2
#define PANELADDR_1         3
#define PANELADDR_2         4
#define PANELADDR_3         5

uint8_t read_paneladdr(void)
{
    uint8_t num = 0;

    if (digitalRead(PANELADDR_0) == HIGH) num |= 0x01;
    if (digitalRead(PANELADDR_1) == HIGH) num |= 0x02;
    if (digitalRead(PANELADDR_2) == HIGH) num |= 0x04;
    if (digitalRead(PANELADDR_3) == HIGH) num |= 0x08;
    return num;
}

/* falling rain effect */
void rain(int count, int speed)
{
    uint8_t const h = display.height();
    while (count > 0) {
        display.scrollright();
        uint8_t row = random(h);
        display.drawPixel(0, row, 1);
        display.display();
        count--;
        delay(speed);
    }
}

/* scroll some text
 * call with frame==-1 to obtain number of frames required
 * then call with frame == 0..N
 */
int scrolltext(String msg, int frame)
{
  uint8_t w = display.width();
  int twidth = (msg.length() * 6) + w;

  if (frame == -1) return twidth;

  display.clearDisplay();
  display.setCursor(w-frame,4);
  display.print(msg);
  display.display();
}

/* just plain draw text */
void show_text(String text)
{
  display.clearDisplay();
  display.setCursor(0,4);
  display.print(text);
  display.display();
#ifdef DEBUG
  Serial.print("Display text: ");
  Serial.println(text);
#endif
}

int8_t hextoi(char h)
{
    if (h >= '0' && h <= '9')
        return h - '0';
    if (h >= 'A' && h < 'F')
        return 10 + (h - 'A');
    if (h >= 'a' && h < 'f')
        return 10 + (h - 'a');
    return -1;
}

unsigned int readHex(String hex)
{
    unsigned int val = 0;
    for (uint8_t i=0; i<hex.length(); i++) {
        char h = hextoi( hex.charAt(i) );
        if (h == -1) break;
        val <<= 4;
        val |= h;
    }
    return val;
}

/* unit number of this panel driver, and its ascii hex char */
uint8_t unit = 0;
char unitc = '0';

bool ignoreLine = false;
String inbuff;

void setup() {
  /* these pins give the unit address */
  pinMode(PANELADDR_0, INPUT_PULLUP);
  pinMode(PANELADDR_1, INPUT_PULLUP);
  pinMode(PANELADDR_2, INPUT_PULLUP);
  pinMode(PANELADDR_3, INPUT_PULLUP);

  /* Serial port to receive commands */
  Serial.begin(115200);
#ifdef DEBUG
  Serial.print("\x1B""[2J");
  Serial.print("\x1B""[1;1H");
  Serial.println("FlipDot Driver");
#endif

  /* initialise the flipdot panel */
  display.begin();
  display.setRotation(0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.clearDisplay();
  display.refresh();
  delay(200);
#ifdef DEBUG
  Serial.println("Initial clear");
  delay(5000);
#endif

  /* read the unit id number and display it */
  unit = read_paneladdr();
  if (unit < 10)
      unitc = '0' + unit;
  else
      unitc = 'A' + (unit - 10);

  show_text("#" + (String)unit);
#ifdef DEBUG
  Serial.println("Flipdot Unit #" + (String)unit + " = '"+(String)unitc+"'");
#endif
  delay(200);

  Scheduler.startLoop(loopSerial);
}

/* unpack a bitmap into flipdot pixels */
void write_bitmap(const char *buff)
{
    int x=0, y=0;
    uint8_t const h = display.height();
    uint8_t const w = display.width();

    if (buff[0] == 0) return;
    if (buff[1] != 'B') return;
    buff += 2;

    while (*buff != 0) {
        int8_t v = hextoi(*buff);
        /* bad character, not hex, stop */
        if (v == -1) break;
        if (x >= w) {
            x = 0;
            y++;
        }
        /* guard against overruns */
        if (y >= h) break;

        display.drawPixel(x  , y, (v&8)?WHITE:BLACK);
        display.drawPixel(x+1, y, (v&4)?WHITE:BLACK);
        display.drawPixel(x+2, y, (v&2)?WHITE:BLACK);
        display.drawPixel(x+3, y, (v&1)?WHITE:BLACK);

        x += 4;
        buff++;
    }
}

void processCommand()
{
    String incoming;
    char cmd;
    int x1, y1, x2, y2, c;
#ifdef DEBUG
    String debug;
#endif
    /* signal that we have copied the command */
    incoming.concat(inbuff);
    inbuff = "";
    Scheduler.yield();


    cmd = incoming.charAt(1);

    switch (cmd) {
        case 'R':
            display.refresh();
#ifdef DEBUG
            debug += "Forced Refresh";
#endif
            break;
        case 'C':
            display.display();
#ifdef DEBUG
            debug += "Commit";
#endif
            break;
        case 'W':
            display.clearDisplay();
#ifdef DEBUG
            debug += "Wipe Display";
#endif
            break;
        case 'D':
            x1 = readHex(incoming.substring(2,4));
            y1 = readHex(incoming.substring(4,6));
            c = readHex(incoming.substring(6,7));
            display.drawPixel(x1, y1, c);
#ifdef DEBUG
            debug += "Pixel ("+String(x1)+","+String(y1)+") = "+String(c);
#endif
            break;
        case 'X':
            x1 = readHex(incoming.substring(2,4));
            y1 = readHex(incoming.substring(4,6));
            x2 = readHex(incoming.substring(6,8));
            y2 = readHex(incoming.substring(8,10));
            c = readHex(incoming.substring(10,11));
            if (c > 1) {
                display.drawRect(x1, y1, x2, y2, c-2);
#ifdef DEBUG
                debug += "Rect ("+String(x1)+","+String(y1)+"),("+String(x2)+","+String(y2)+") = "+String(c-2);
#endif
            } else {
                display.fillRect(x1, y1, x2, y2, c);
#ifdef DEBUG
                debug += "FillRect ("+String(x1)+","+String(y1)+"),("+String(x2)+","+String(y2)+") = "+String(c);
#endif
            }
            break;
        case 'L':
            x1 = readHex(incoming.substring(2,4));
            y1 = readHex(incoming.substring(4,6));
            x2 = readHex(incoming.substring(6,8));
            y2 = readHex(incoming.substring(8,10));
            c = readHex(incoming.substring(10,11));
            display.drawLine(x1, y1, x2, y2, c);
#ifdef DEBUG
            debug += "Line ("+String(x1)+","+String(y1)+"),("+String(x2)+","+String(y2)+") = "+String(c);
#endif
            break;
        case 'S':
            x1 = incoming.charAt(2);
            c = readHex(incoming.substring(3,4));
            for (y1=0; y1<c; y1++) {
                switch (x1) {
                    case 'a': display.scrollleft(); break; 
                    case 'w': display.scrollup(); break; 
                    case 's': display.scrolldown(); break; 
                    case 'd': display.scrollright(); break; 
                }
            }
#ifdef DEBUG
            debug += "Scroll dir="+String(x1)+" dist="+String(c);
#endif
            break;
        case 'T':
            // FIXME
            show_text(incoming.substring(7));
#ifdef DEBUG
            debug += "Text: " + incoming.substring(7);
#endif
            break;
        case 'B':
            write_bitmap(incoming.c_str());
#ifdef DEBUG
            debug += "Bitmap";
#endif
            break;
        case 'M':
            // FIXME
#ifdef DEBUG
            debug += "Marquee: " + incoming.substring(2);
#endif
            break;
    }
#ifdef DEBUG
  Serial.print("\x1B""[18;1H");
  Serial.print("\x1B""[0K");
  Serial.print("Command: ");
  Serial.print(cmd);
  Serial.println(" - "+debug);
#endif
}

int wait_strlen(int len)
{
  return inbuff.length() == len;
}

/* thread to read serial port */
void loopSerial()
{
  Scheduler.wait_available(Serial);
  
  /* if there is text available, read it */
  while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
          char target = inbuff.charAt(0);
          if (target == '*' || target == unitc) {
              /* Start a new thread to interpret the command */
              Scheduler.start(processCommand);
              Scheduler.wait(wait_strlen, 0);
          } else {
#ifdef DEBUG
              Serial.print("\x1B""[18;1H");
              Serial.print("\x1B""[0K");
              Serial.println("Ignore Command "+inbuff.substring(1,2)+" for unit "+String(target));
#endif
              inbuff = "";
          }
      } else
          inbuff += c;
  }
}

