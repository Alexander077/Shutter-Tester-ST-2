#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <mString.h>
// #include <SafeString.h>
#include "../../include/Common.h"
#include "credits.h"
#include "utils.h"
#include "images.h"

// #define DISPLAY_CS 18
#define DISPLAY_CS 39
// #define DISPLAY_RESET 16
#define DISPLAY_RESET 18
// #define DISPLAY_DC 21
#define DISPLAY_DC 34

#define DISPLAY_I2C_ADDRESS 4

#define MAIN_MENU_ITEMS_COUNT 4
#define CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT 3
#define RESULT_PAGES_COUNT 7

#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146

#define SPAN_A_COLOR BLUE
#define SPAN_B_COLOR GREEN
#define SPAN_C_COLOR YELLOW

#define DAC_CH1 17
#define DAC_CH2 18
#define DAC_MAX 255

enum class MainMenuItems
{
  MEASURE,
  CHECK_LIGHT,
  MEASURMENT_HISTORY,
  CREDITS,
};

enum class CurtainMovement
{
  HORISONTAL,
  VERTICAL,
  LEAF,
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
  "Credits"
};

const char *CurtainMovementItemsStr[] =
{
  "Horisontal",
  "Vertical",
  "Leaf"
 };

#define SHUTTR_SPEEDS_COUNT 14
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

const char *GetCurtainMovementItemTitle(CurtainMovement menuItem)
{
  return CurtainMovementItemsStr[(uint8_t)menuItem];
}

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
// Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_DataBus *bus = new Arduino_HWSPI(DISPLAY_DC, DISPLAY_CS, SCK, MOSI);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *display = new Arduino_ST7735(
    bus, DISPLAY_RESET, 1 /* rotation */, false /* IPS */,
    128 /* width */, 160 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */,
    0 /* col offset 2 */, 0 /* row offset 2 */,
    false /* BGR */);

mString<300> inputCmdStr;
char inputDataArray[20][20];
char curScreen = '\0';
// char prevMode = '\0';
// bool dataReady = false;
volatile bool dataProcessed = true;

void i2cReceiveEvent(int howMany)
{
  // Serial.println(howMany);
  
  if (!dataProcessed)
  {
    return;
  }
  
  mString<33> tmpBuf;

  while (Wire.available()) 
  {
    tmpBuf += (char)Wire.read();
  }

  if (tmpBuf.startsWith("StArT"))
  {
    inputCmdStr.clear();
    // Serial.println("Message start");
    return;
  }

  if (tmpBuf.startsWith("EnD"))
  {
    uint16_t resStrLength = tmpBuf.toInt(3);
    // Serial.print("Message end. Contents: '");
    // Serial.print(inputCmdStr.c_str());
    // Serial.println("'");

    if (resStrLength != inputCmdStr.length())
    {
      // Serial.println("Bad message");
      inputCmdStr.clear();
      dataProcessed = true;
    }
    else
    {
      dataProcessed = false;
    }

    return;
  }

  inputCmdStr.add(tmpBuf.c_str());
}

void parseInputData(){
  // uint16_t splitCount = inputCmdStr.split(inputDataArray, ':');
  char* buf = inputCmdStr.buf;
  uint16_t strLength = strlen(buf);

  if (strLength > 0)
  {
    // Serial.println("Parsed data");
    uint16_t dataArrIndex = 0;
    int16_t startIndex = -1;

    for (size_t i = 0; i < strLength; i++)
    {
      if (buf[i] == ':' && i != strLength - 1 && i > 0)//do not process delimeter on first and last indeces.
      {
        if (startIndex == -1)
        {
          strncpy(inputDataArray[dataArrIndex], &buf[0], i);
        }
        else 
        {
          strncpy(inputDataArray[dataArrIndex], &buf[startIndex + 1], i - startIndex - 1);
        }

        startIndex = i;
        dataArrIndex++;
      }
    }

    if (startIndex != -1)
    {
      strcpy(inputDataArray[dataArrIndex], &buf[startIndex + 1]);
    }
    else
    {
      strcpy(inputDataArray[dataArrIndex], &buf[0]);
      dataArrIndex++;
    }

    // if (dataArrIndex > 0)
    // {
    //   for (size_t i = 0; i <= dataArrIndex; i++)
    //   {
    //     Serial.println(inputDataArray[i]);
    //   }
    // }

    // Serial.println("Parsed data end");
  }
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
  return inputDataArray[0][0];
}

double getInputParamsArrayFloat(uint8_t index)
{
  mString<20> bufStr;
  bufStr.add(inputDataArray[index]);
  return bufStr.toFloat();
}

int32_t getInputParamsArrayInt(uint8_t index)
{
  mString<20> bufStr;
  bufStr.add(inputDataArray[index]);
  return bufStr.toInt();
}

void drawNavBar(int8_t activePageIndex)
{
  int16_t navBarWidth = 120;
  int16_t navBarY = 120;
  int16_t navBarItemsCount = RESULT_PAGES_COUNT;
  int16_t navBarLeftMargin = (display->width() - navBarWidth) / 2;
  display->drawLine(navBarLeftMargin, navBarY, navBarLeftMargin + navBarWidth, navBarY, WHITE);

  for (int8_t i = 0, spacing = 0; i < navBarItemsCount; i++, spacing += navBarWidth / (navBarItemsCount - 1))
  {
    display->fillCircle(navBarLeftMargin + spacing, navBarY, i == activePageIndex ? 4 : 2, i == activePageIndex ? RGB565(0, 102, 153) : WHITE);
  }
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
    selectedMenuItemIndex = getInputParamsArrayInt(1);

    if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    {
      if (prevSelectedMenuItemIndex != -1)
      {
        Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
        display->fillRect(rOld.x, rOld.y /* - rOld.height */, rOld.width, rOld.height, BLACK);
        menuItemsRects[prevSelectedMenuItemIndex] =
            drawStringHCentered(GetMenuItemTitle((MainMenuItems)prevSelectedMenuItemIndex), menuItemsY[prevSelectedMenuItemIndex]);
      }

      String newSelMenuItem = "-> " + String(GetMenuItemTitle((MainMenuItems)selectedMenuItemIndex)) + " <-";
      Rect rNew = menuItemsRects[selectedMenuItemIndex];
      display->fillRect(rNew.x, rNew.y /* - rNew.height */, rNew.width, rNew.height, BLACK);

      menuItemsRects[selectedMenuItemIndex] = drawStringHCentered(newSelMenuItem, menuItemsY[selectedMenuItemIndex]);
      prevSelectedMenuItemIndex = selectedMenuItemIndex;

    }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
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
    display->setTextSize(2);
    drawStringHCentered("MEASURING", 40);
    display->setTextSize(1);
    drawStringHCentered("Release camera shutter", 60);
    
    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
  }
}

void drawMeasuredScreen()
{
  int8_t prevPageIndex = -1;

  while ((Screens)getCurScreen() == Screens::MEASURED)
  {
    Serial.println("Drawing measured screen");
    mString<20> bufStr;
    bufStr.add(inputDataArray[1]);
    int8_t resultPageIndex = bufStr.toInt();

    //result by sensor, shutter speed
    if (resultPageIndex >= 0 && resultPageIndex <= 1 && prevPageIndex != resultPageIndex)
    {
      display->fillScreen(BLACK);
      // display->fillRect(0,0, display->width(), 115,BLACK);
      bufStr.clear();
      bufStr.add(inputDataArray[2]);

      double sensorTime = bufStr.toFloat();
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

    //1-st or 2-nd curtain speeds and timings
    if ((resultPageIndex == 2 || resultPageIndex == 3) && prevPageIndex != resultPageIndex)
    {
      mString<30> curtainNumber;

      if (resultPageIndex == 4)
      {
        curtainNumber += "1st";
      }

      if (resultPageIndex == 5)
      {
        curtainNumber += "2nd";
      }

      curtainNumber += " curtain summary";

      display->fillScreen(BLACK);
      drawStringHCentered(curtainNumber, 15);
      const uint8_t lineSpacing = 11;
      uint8_t curPos = 27;

      display->setFont();

      display->setCursor(0, curPos);
      display->printf("  Span A speed: %1.2f m/s", getInputParamsArrayFloat(2));
      curPos += lineSpacing; 
      // display->setCursor(0, curPos);
      // display->printf("  Span B speed: %1.2f m/s", getInputParamsArrayFloat(3));
      // curPos += lineSpacing; 
      // display->setCursor(0, curPos);
      // display->printf("  Span C speed: %1.2f m/s", getInputParamsArrayFloat(4));
      // curPos += lineSpacing; 
      display->setCursor(0, curPos);
      display->printf("   Span A time: %1.2f ms", getInputParamsArrayFloat(5));
      curPos += lineSpacing; 
      // display->setCursor(0, curPos);
      // display->printf("   Span B time: %1.2f ms", getInputParamsArrayFloat(6));
      // curPos += lineSpacing; 
      // display->setCursor(0, curPos);
      // display->printf("   Span C time: %1.2f ms", getInputParamsArrayFloat(7));
      // curPos += lineSpacing; 
      // // display->setCursor (0, curPos);
      // // display->printf ("    Avg speed: %1.2f m/s", getInputParamsArrayFloat(8));
      // // curPos += lineSpacing ;
      display->setCursor(0 , curPos);
      display->printf("   Travel time: %1.2f ms", getInputParamsArrayFloat(9));
      curPos += lineSpacing;
      display->setCursor(0, curPos);
      display->setFont(u8g2_font_6x13_tf);

      prevPageIndex = resultPageIndex;
      drawNavBar(resultPageIndex);
    }

    if (resultPageIndex == 4 && prevPageIndex != resultPageIndex)
    {
      mString<30> title;
      title += "Estimated slit width";

      display->fillScreen(BLACK);
      drawStringHCentered(title, 15);
      const uint8_t lineSpacing = 11;
      uint8_t curPos = 27;

      display->setFont();

      display->setCursor(0, curPos);
      display->printf("   Span A: %1.2f mm", getInputParamsArrayFloat(2));
      curPos += lineSpacing;
      display->setCursor(0, curPos);
      display->printf("   Span B: %1.2f mm", getInputParamsArrayFloat(3));

      display->setFont(u8g2_font_6x13_tf);

      prevPageIndex = resultPageIndex;
      drawNavBar(resultPageIndex);
    }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
  }
}

void drawSensorSignalLevelBar(uint8_t &x, uint8_t &y, uint8_t sensorNumber, double curSensorValue)
{
  const uint8_t barWidthPx = 80;
  x = 15;
  y += 20;
  display->setCursor(x, y);
  display->printf("Sensor %i signal level", sensorNumber);
  y += 5;
  display->fillRect(11, y + 1, barWidthPx, 8, BLACK);
  display->drawRect(10, y, barWidthPx, 10, WHITE);
  display->fillRect(11, y + 1, (barWidthPx - 2) * curSensorValue, 8, GREEN);
  x += barWidthPx;
  y += 9;
  display->setCursor(x, y);
  display->print("Too bright");
}

void drawLightCheckScreen()
{
  display->fillScreen(BLACK);
  int8_t oldBrightness = -1;
  drawStringHCentered("Light brightness", 15);
  const uint8_t thresholdDacValue = 100;
  const uint16_t sensorValueBarUpodateIntervalMs = 100;
  uint64_t lastSensorValueUpdateTime = millis();

  while ((Screens)getCurScreen() == Screens::LIGHT_CHECK)
  {
    int8_t newBrightness = getInputParamsArrayInt(1);
    uint8_t x = 60;
    uint8_t y = 45;

    if (oldBrightness != newBrightness)
    {
      display->setTextSize(2);
      display->setCursor(x, y);
      display->setTextColor(BLACK);
      display->printf("%i%% ", oldBrightness);//erase
      display->setCursor(x, y);
      display->setTextColor(WHITE);
      display->printf("%i%% ", newBrightness);
      display->setTextSize(1);

      uint8_t dacVal = thresholdDacValue + (uint8_t)round((newBrightness / 100.0) * (double)(DAC_MAX - thresholdDacValue));
      dacWrite(DAC_CH1, dacVal);
      // Serial.println(dacVal);

      oldBrightness = newBrightness;
    }

    if (millis() - lastSensorValueUpdateTime > sensorValueBarUpodateIntervalMs)
    {
      display->setTextSize(1);
      double sensor0Val = getInputParamsArrayFloat(2);
      drawSensorSignalLevelBar(x, y, 1, sensor0Val);
      double sensor1Val = getInputParamsArrayFloat(3);
      drawSensorSignalLevelBar(x, y, 2, sensor1Val);
      lastSensorValueUpdateTime = millis();
    }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){} // wait for new data from main unit
    parseInputData();
  }

  dacWrite(DAC_CH1, 0); // turn off the light
}

void drawCurtainMovementSelectionScreen()
{
  display->fillScreen(BLACK);

  uint8_t xMargin = 50;
  uint8_t yMargin = 55;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  drawStringHCentered("Select curtain movement", 20);

  for (uint8_t i = 0; i < CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));
    display->println(GetCurtainMovementItemTitle((CurtainMovement)i));
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t selectedMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = 0;

  while ((Screens)getCurScreen() == Screens::CURTAN_MOVEMENT_SELECTION)
  {
    selectedMenuItemIndex = getInputParamsArrayInt(1);

    if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    {
      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * selectedMenuItemIndex));
      display->print("->");

      prevSelectedMenuItemIndex = selectedMenuItemIndex;
    }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
  }
}

void setup(void)
{
  Serial.begin(115200);

  display->begin();
  display->fillScreen(BLACK);
  
  Wire.begin(DISPLAY_I2C_ADDRESS); // join i2c bus with address #4
  Wire.onReceive(i2cReceiveEvent); 


  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);
  // display->setTextSize(1,1,10);
}

void loop()
{
  Serial.println("Starting main loop");

  while (true)
  {
    while (dataProcessed)//Wait until new data is available
    {
      // Serial.println("No new data. Skipping");
    }

    parseInputData();
    curScreen = getCurScreen();

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
      case Screens::LIGHT_CHECK:
      {
        drawLightCheckScreen();
        break;
      }
      case Screens::CREDITS:
      {
        drawCreditsScreen();
        break;
      }
      case Screens::CURTAN_MOVEMENT_SELECTION:
      {
        drawCurtainMovementSelectionScreen();
        break;
      }

      default:
        break;
    }

    inputCmdStr.clear();
    dataProcessed = true;
  }
}