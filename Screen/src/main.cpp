#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include "credits.h"
#include "utils.h"
#include "images.h"

#define DISPLAY_CS 18
#define DISPLAY_RESET 16
#define DISPLAY_DC 21

#define DISPLAY_I2C_ADDRESS 4

#define MAIN_MENU_ITEMS_COUNT 4

#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146

enum class MainMenuItems
{
  MEASURE,
  CHECK_LIGHT,
  MEASURMENT_HISTORY,
  CREDITS,
};

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'r',
  CREDITS = 'c',
  TEST = 't',
};

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

const char *MainMenuItemsStr[] =
    {
        "Measure",
        "Check Light",
        "View Last Measures",
        "Credits"};

#define SHUTTR_SPEEDS_COUNT 14
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
// Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_DataBus *bus = new Arduino_HWSPI(DISPLAY_DC, DISPLAY_CS, SCK, MOSI);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *display = new Arduino_ST7735(
    bus, DISPLAY_RESET, 3 /* rotation */, false /* IPS */,
    128 /* width */, 160 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */,
    0 /* col offset 2 */, 0 /* row offset 2 */,
    false /* BGR */);

String inputCmdStr = "m:1";
char curScreen = '\0';
char prevMode = '\0';

void receiveEvent(int howMany)
{
  char inputCmdStrBuf[100];
  inputCmdStrBuf[0] = '\0';

  while (Wire.available()) 
  {
    char c = Wire.read(); // receive byte as a character
    char tmpBuf[2] = "\0";
    tmpBuf[0] = c;
    strcat(inputCmdStrBuf, tmpBuf);
  }

  inputCmdStr = String(inputCmdStrBuf);
}

Rect getStringRect(const char *str, int16_t x, int16_t y)
{
  // Variables to hold the bounding box dimensions
  int16_t x1, y1;
  uint16_t w, h;

  // Get the bounds of the text
  display->getTextBounds(str, x, y, &x1, &y1, &w, &h);

  Rect r;
  r.x = x1;
  r.y = y1;
  r.width = w;
  r.height = h;
  return r;
}

Rect drawStringHCentered(const char *str, uint16_t y)
{
  // // Variables to hold the bounding box dimensions
  // int16_t x1, y1;
  // uint16_t w, h;

  Rect r = getStringRect(str, 0, y);
  // Serial.println(r.y);

  // Calculate the position to center the text on the screen
  int16_t x = (display->width() - r.width) / 2;
  // int16_t y = (display->height() - h) / 2;
  r.x = x;

  // Draw the text at the calculated position
  display->setCursor(x, y);
  display->print(str);

  // Serial.print(r.x);
  // Serial.print("|");
  // Serial.print(r.y);
  // Serial.print("|");
  // Serial.print(r.width);
  // Serial.print("|");
  // Serial.println(r.height);

  // Optional: Draw the bounding box around the text for visualization
  // display->drawRect(x, y - r.height, r.width, r.height, RED);
  return r;
}

Rect drawStringHCentered(String str, uint16_t y)
{
  #define CHAR_BUF_SIZE 50
  char buf[CHAR_BUF_SIZE];
  str.toCharArray(buf, CHAR_BUF_SIZE );
  return drawStringHCentered(buf, y);
}

char getCurScreen()
{
  return inputCmdStr[0];
}

void drawMainMenu()
{
  Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT];
  int16_t menuItemsY[MAIN_MENU_ITEMS_COUNT];

  display->fillScreen(BLACK);

  for (uint8_t i = 0, y = 45; i < MAIN_MENU_ITEMS_COUNT; i++, y += 15)
  {
    menuItemsRects[i] = drawStringHCentered(GetMenuItemTitle((MainMenuItems)i), y);
    menuItemsY[i] = y;
  }

  int8_t selectedMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = -1;

  while ((Screens)getCurScreen() == Screens::MAIN_MENU)
  {
    String selIndex = inputCmdStr.substring(2);
    selectedMenuItemIndex = selIndex.toInt();

    // if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    // {
    //   Serial.print(prevSelectedMenuItemIndex);
    //   Serial.print(":");
    //   Serial.println(selectedMenuItemIndex);
    // }

    if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    {
      // Serial.println(selectedMenuItemIndex);
      // Serial.print(prevSelectedMenuItemIndex);
      // Serial.print(":");
      // Serial.println(selectedMenuItemIndex);

      // Serial.println(selectedMenuItemIndex);

      if (prevSelectedMenuItemIndex != -1)
      {
        Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
        display->fillRect(rOld.x, rOld.y /* - rOld.height */, rOld.width, rOld.height, BLACK);
        menuItemsRects[prevSelectedMenuItemIndex] =
            drawStringHCentered(GetMenuItemTitle((MainMenuItems)prevSelectedMenuItemIndex), menuItemsY[prevSelectedMenuItemIndex]);
      }

      String newSelMenuItem = "-> " + String(GetMenuItemTitle((MainMenuItems)selectedMenuItemIndex)) + " <-";
      Rect rNew = menuItemsRects[selectedMenuItemIndex];
      // Serial.println(rNew.y);
      display->fillRect(rNew.x, rNew.y /* - rNew.height */, rNew.width, rNew.height, BLACK);

      menuItemsRects[selectedMenuItemIndex] = drawStringHCentered(newSelMenuItem, menuItemsY[selectedMenuItemIndex]);
      prevSelectedMenuItemIndex = selectedMenuItemIndex;
      // Serial.println(newSelMenuItem);
      // display->setCursor(10, 10);
      // display->drawChar(10, 10, selectedMenuItemIndexStrBuf[0], WHITE, BLACK);
      // display->print(selectedMenuItemIndex);
      // display->println(Credits::ADAFRUIT_GFX_LICENSE_HEADER);
    }
  }
}

void drawCreditsScreen()
{
  display->fillScreen(BLACK);

  while ((Screens)getCurScreen() == Screens::CREDITS)
  {
    // display->setFont();
    display->setCursor(0, 13);
    display->print(Credits::CREDITS_HEADER);
  }
}

void drawMeasuringScreen()
{
  display->fillScreen(BLACK);

  while ((Screens)getCurScreen() == Screens::MEASURING)
  {
    // display->setFont();
    display->setTextSize(2);
    drawStringHCentered("MEASURING", 40);
    display->setTextSize(1);
    drawStringHCentered("Release camera shutter", 60);
    // display->setCursor(0, 13);
  }
}

void drawNavBar(int8_t activePageIndex)
{
  int16_t navBarLeftMargin = 40;
  display->drawLine(navBarLeftMargin, 120, 120, 120, WHITE);

  for (int8_t i = 0, margin = 0; i < 5; i++, margin += 20)
  {
    display->fillCircle(navBarLeftMargin + margin, 120, i == activePageIndex ? 4 : 2, i == activePageIndex ? RGB565(0, 102, 153) : WHITE);
  }
}

void drawMeasuredScreen()
{
  int8_t prevPageIndex = -1;

  while ((Screens)getCurScreen() == Screens::MEASURED)
  {
    int8_t resultPageIndex = inputCmdStr.substring(2, 3).toInt();
    double resultPageParam = inputCmdStr.substring(4).toDouble();

    if (resultPageIndex >= 0 && resultPageIndex <= 2 && prevPageIndex != resultPageIndex)
    {
      display->fillScreen(BLACK);
      // display->fillRect(0,0, display->width(), 115,BLACK);

      double sensorTime = resultPageParam;
      drawStringHCentered("Sensor " + String(resultPageIndex + 1) + " summary", 15);
      display->setCursor(5, 35);
      display->printf("Time: %.2f ms", sensorTime);
      display->setCursor(5, 50);
      double shutterSpeed = 1000.0 / sensorTime;
      String speedLabel = "Speed: ";
      String speedLabelValue = "1/" + String(shutterSpeed, 1) + " s";
      display->print(speedLabel + speedLabelValue);
      uint16_t speedA = -1;
      uint16_t speedB = -1;

      for (size_t i = 0; i < SHUTTR_SPEEDS_COUNT; i++)
      {
        if (shutterSpeeds[i] <= shutterSpeed)
        {
          speedA = shutterSpeeds[i - 1];
          speedB = shutterSpeeds[i];
          break;
        }
      }

      display->drawXBitmap(SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX, 80, SensorResultGraphImagBits, SENSOR_RESULT_GRAPH_IMAGE_WIDTH, SENSOR_RESULT_GRAPH_IMAGE_HEIGHT, WHITE);
      display->setCursor(5, 105);
      display->printf("1/%d", speedA);
      String speedBstr = speedB == 1 ? String(speedB) : "1/" + String(speedB);
      Rect speedBRect = getStringRect(speedBstr.c_str(), 0, 0);
      display->setCursor(160 - speedBRect.width - 5, 105);
      display->printf(speedBstr.c_str());

      double resultPercents = (sensorTime / (1000.0 / speedA)) - 1;
      double speedPointLeftMargin = SPEEDS_GRAPH_BAR_WIDTH_PX * resultPercents;

      int16_t circleX = SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX + 2 + (int16_t)speedPointLeftMargin;
      int16_t circleY = 83;
      display->fillCircle(circleX, circleY, 3, GREEN);

      Rect speedLabelRect = getStringRect(speedLabelValue.c_str(), 0, 0);
      display->drawLine(circleX, circleY, 5 + speedLabelRect.width + 40, 52, GREEN);
      display->drawLine(5 + speedLabelRect.width + 40, 52, 45, 52, GREEN);

      prevPageIndex = resultPageIndex;
      drawNavBar(resultPageIndex);
    }

    if (resultPageIndex == 3 && prevPageIndex != resultPageIndex)
    {

      display->fillScreen(BLACK);
      drawStringHCentered("Spans definition", 15);
      int8_t imageX = 4;
      int8_t imageY = 23;
      #define SPAN_A_COLOR BLUE
      #define SPAN_B_COLOR GREEN
      #define SPAN_C_COLOR YELLOW
      display->drawXBitmap(imageX, imageY, SpeedsHintGraphBgHeightBits, SpeedsHintGraphBgWidth, SpeedsHintGraphBgHeight, WHITE);
      display->drawXBitmap(imageX, imageY, SpeedsHintGraphSpanAbits, SpeedsHintGraphSpanAwidth, SpeedsHintGraphSpanAheight, SPAN_A_COLOR);
      display->drawXBitmap(imageX, imageY, SpeedsHintGraphSpanBbits, SpeedsHintGraphSpanBwidth, SpeedsHintGraphSpanBheight, SPAN_B_COLOR);
      display->drawXBitmap(imageX, imageY, SpeedsHintGraphSpanCbits, SpeedsHintGraphSpanCwidth, SpeedsHintGraphSpanCheight, SPAN_C_COLOR);

      display->setCursor(30, 65);
      // display->setFont();
      display->print("Span ");
      display->setTextColor(SPAN_C_COLOR);
      display->print("C");
      display->setTextColor(WHITE);

      display->setCursor(90, 53);
      display->print("Span ");
      display->setTextColor(SPAN_A_COLOR);
      display->print("A");
      display->setTextColor(WHITE);

      display->setCursor(90, 78);
      display->print("Span ");
      display->setTextColor(SPAN_B_COLOR);
      display->print("B");
      display->setTextColor(WHITE);

      prevPageIndex = resultPageIndex;
      drawNavBar(resultPageIndex);
    }
  }
}

void setup(void)
{
  Serial.begin(115200);

  Wire.begin(DISPLAY_I2C_ADDRESS); // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event

  display->begin();
  display->fillScreen(BLACK);

  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);
  // display->setTextSize(1,1,10);
}

void loop()
{

  while (true)
  {
    curScreen = getCurScreen();
    // Serial.println(inputCmdStr); // print the character

    switch ((Screens)curScreen)
    {
      case Screens::MAIN_MENU:
      {
        drawMainMenu();
        break;
      }
      case Screens::MEASURING:
      {
        drawMeasuringScreen();
        break;
      }
      case Screens::MEASURED:
      {
        drawMeasuredScreen();
        break;
      }
      case Screens::CREDITS:
      {
        drawCreditsScreen();
        break;
      }

      default:
        break;
    }
  }
}