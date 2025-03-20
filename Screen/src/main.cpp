#include <Arduino.h>
// #include <EEPROM.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <mString.h>
// #include <SafeString.h>
#include "../../include/Common.h"
#include "about.h"
#include "utils.h"
#include "images.h"
#include "esp_flash_encrypt.h"


#define DISPLAY_I2C_ADDRESS 4
#define DISPLAY_CS 39
#define DISPLAY_RESET 18
#define DISPLAY_DC 34

#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146


struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

const char *MainMenuItemsStr[MAIN_MENU_ITEMS_COUNT] =
{
  "Measure",
  "Light Setup",
  "Saved Measures",
  "About"
};

const char *CurtainMovementItemsStr[CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT] =
{
  "Horisontal",
  "Vertical",
  "Leaf"
 };

const char *MeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Overwrite oldest",
  "Overwrite newest",
  "Sel. rec. to overwrite",
  "Return to result",
};

const char *ZeroRecordsMeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Return to result",
};

#define SHUTTR_SPEEDS_COUNT 14
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

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
char inputDataArray[100][50];
char curScreen = '\0';
// bool dataReady = false;
volatile bool dataProcessed = true;

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

const char *GetCurtainMovementItemTitle(CurtainMovement menuItem)
{
  return CurtainMovementItemsStr[(uint8_t)menuItem];
}

const char *GetSaveMasurementMenuItemTitle(MeasurementSaveMenuItems menuItem, bool useForZeroRecords)
{
  if (useForZeroRecords)
  {
    return ZeroRecordsMeasurementSaveItemsStr[(uint8_t)menuItem];
  }
  else
  {
    return MeasurementSaveItemsStr[(uint8_t)menuItem];
  }
}

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
  // Serial.println(inputCmdStr.buf);
  char* strings[inputCmdStr.splitAmount(':')];
  int amount = inputCmdStr.split(strings, ':');

  for (size_t i = 0; i < amount; i++)
  {
    strcpy(inputDataArray[i], strings[i]);
  }
  
  inputCmdStr.unsplit();

  // if (amount > 0)
  // {
  //   for (size_t i = 0; i <= amount; i++)
  //   {
  //     Serial.println(inputDataArray[i]);
  //   }
  // }

  // Serial.println("Parsed data end");
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

Rect drawStringHCentered(const String& str, uint16_t y)
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

int32_t getInputParamsArrayInt(int16_t index)
{
  mString<20> bufStr;
  bufStr.add(inputDataArray[index]);
  return bufStr.toInt();
}

void getInputParamsArrayString(uint8_t index, mString<50>& outStr)
{
  outStr += inputDataArray[index];
}

void drawNavBar(int16_t activePageIndex, int16_t itemsCount)
{
  int16_t navBarWidth = 120;
  int16_t navBarY = 120;
  // int16_t navBarItemsCount = RESULT_PAGES_COUNT;
  int16_t navBarLeftMargin = (display->width() - navBarWidth) / 2;
  display->drawLine(navBarLeftMargin, navBarY, navBarLeftMargin + navBarWidth, navBarY, WHITE);

  for (int8_t i = 0, spacing = 0; i < itemsCount; i++, spacing += navBarWidth / (itemsCount - 1))
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

void drawAboutScreen()
{
  display->fillScreen(BLACK);

  drawStringHCentered("About this device", 15);

  while ((Screens)getCurScreen() == Screens::ABOUT)
  {
    display->setCursor(10, 35);
    display->printf("Hardware version: %s", About::HW_VERSION);
    display->setCursor(10, 50);
    display->printf("Firmware version: %s", About::SW_VERSION);

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){} // wait for new data from main unit
    parseInputData();
  }
}

void drawMeasuringScreen()
{
  display->fillScreen(BLACK);
  // int8_t oldBrightness = -1;
  // int16_t oldLightTemp = -1;
  // const char *lightBrightnessLabel = "Light brightness: %i%% ";

  while ((Screens)getCurScreen() == Screens::MEASURING)
  {
    display->setTextSize(2);
    drawStringHCentered("MEASURING", 40);
    display->setTextSize(1);
    drawStringHCentered("Release camera shutter", 60);
    // drawStringHCentered("Light brightness", 70);

    // int8_t newBrightness = getInputParamsArrayInt(1);
    // uint8_t x = 16;
    // uint8_t y = 75;

    // if (oldBrightness != newBrightness)
    // {
    //   display->setCursor(x, y);
    //   display->setTextColor(BLACK);
    //   display->printf(lightBrightnessLabel, oldBrightness); // erase
    //   display->setCursor(x, y);
    //   display->setTextColor(WHITE);
    //   display->printf(lightBrightnessLabel, newBrightness);

    //   uint8_t dacVal = thresholdDacValue + (uint8_t)round((newBrightness / 100.0) * (double)(DAC_MAX - thresholdDacValue));

    //   if (newBrightness == 0)
    //   {
    //     dacVal = 0;//turn off the light on zero brightness
    //   }
      
    //   dacWrite(DAC_CH1, dacVal);
    //   // Serial.println(dacVal);

    //   oldBrightness = newBrightness;
    // }

    // int16_t newTemp = getInputParamsArrayInt(2);

    // if (oldLightTemp != newTemp)
    // {
    //   drawLightStatusBar(y + 20, newTemp);
    //   oldLightTemp = newTemp;
    // }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
  }

  // dacWrite(DAC_CH1, 0); // turn off the light
}

void drawMeasuredScreen()
{
  display->fillScreen(BLACK);
  int8_t prevPageIndex = -1;

  while ((Screens)getCurScreen() == Screens::MEASURED)
  {
    int8_t resultPageIndex = getInputParamsArrayInt(1);
    bool isReadingsValid = getInputParamsArrayInt(2) != MEASUREMENTS_IS_INVALID_VAL;

    if (!isReadingsValid)
    {
      int16_t yMargin = 25;
      int16_t step = 12;
      drawStringHCentered("Measurement result", yMargin);
      yMargin += step;
      drawStringHCentered("is invalid", yMargin);
      yMargin += step + 8;

      drawStringHCentered("Please check if shutter", yMargin);
      yMargin += step;
      drawStringHCentered("movement settings was", yMargin);
      yMargin += step;
      drawStringHCentered("set correctly", yMargin);
    }
    else
    {
      // result by sensor, shutter speed
      if (resultPageIndex >= 0 && resultPageIndex <= 1 && prevPageIndex != resultPageIndex)
      {
        display->fillScreen(BLACK);

        double sensorTime = getInputParamsArrayFloat(2);
        drawStringHCentered("Sensor " + String(resultPageIndex + 1) + " summary", 15);

        if (sensorTime > 0) // if any data taken
        {
          display->setCursor(5, 35);

          if (sensorTime < 1.0)//less than a millisecond
          {
            display->printf("Time: %.3f ms", sensorTime);
          }
          else if (sensorTime > 1000.0) // More than a second
          {
            display->printf("Time: %.2f s", sensorTime / 1000.0);
          }
          else //between 1ms and 1s
          {
            display->printf("Time: %.2f ms", sensorTime);
          }

          if (sensorTime <= 1000.0)//show speed and graph for sensor time only under 1 second
          {
            display->setCursor(5, 50);
            double shutterSpeed = 1000.0 / sensorTime;
            String speedLabel = "Speed: ";
            String speedLabelValue = "1/" + String(shutterSpeed, 2) + " s";
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

            double range = speedA - speedB;
            double sensorSpeedOnRange = shutterSpeed - speedB;

            double resultPercents = 1.0 - sensorSpeedOnRange / range;
            double speedPointLeftMargin = SPEEDS_GRAPH_BAR_WIDTH_PX * resultPercents;

            int16_t circleX = SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX + 2 + (int16_t)speedPointLeftMargin;
            int16_t circleY = 83;
            display->fillCircle(circleX, circleY, 3, GREEN);

            Rect speedLabelRect = getStringRect(speedLabelValue.c_str(), 0, 0);
            display->drawLine(circleX, circleY, 5 + speedLabelRect.width + 40, 52, GREEN);
            display->drawLine(5 + speedLabelRect.width + 40, 52, 45, 52, GREEN);
          }
        }
        else
        {
          if (sensorTime == SENSOR_LIGHT_IS_TOO_DIM)
          {
            drawStringHCentered("Light is too dim", 40);
          }

          if (sensorTime == SENSOR_LIGHT_IS_TOO_BRIGHT)
          {
            drawStringHCentered("Light is too bright", 40);
          }

          if (sensorTime == SENSOR_TIME_IS_TOO_SHORT)
          {
            drawStringHCentered("Sensor time is too short", 40);
          }
        }

        prevPageIndex = resultPageIndex;
        drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
      }

      // 1-st and 2-nd curtain speeds and timings
      if (resultPageIndex == 2 && prevPageIndex != resultPageIndex)
      {
        display->fillScreen(BLACK);
        uint8_t curPos = 22;

        if (getInputParamsArrayFloat(2) == BOTH_SENSORS_MEASUREMENTS_REQUIRED) // no curtain data
        {
          drawStringHCentered("No curtain data", 30);
          drawStringHCentered("Measurements from both", 55);
          drawStringHCentered("sensors are required", 70);
        }
        else if (getInputParamsArrayFloat(2) == NOT_AVAILABLE_FOR_LEAF_SHUTERS) // no curtain data
        {
          drawStringHCentered("Curtain data is not", 40);
          drawStringHCentered("available for", 55);
          drawStringHCentered("leaf shutters", 70);
        }
        else
        {
          drawStringHCentered("1st curtain", 15);
          const uint8_t lineSpacing = 11;

          display->setFont();

          display->setCursor(0, curPos);
          display->printf(" Speed b/w sen.: %1.2f m/s", getInputParamsArrayFloat(2));
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf("  Time b/w sen.: %1.2f ms", getInputParamsArrayFloat(3));
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf(" Est. tot. time: %1.2f ms", getInputParamsArrayFloat(4));
          curPos += lineSpacing + 15;

          display->setCursor(0, curPos);
          display->setFont(u8g2_font_6x13_tf);

          drawStringHCentered("2nd curtain", curPos);

          curPos += 5;

          display->setFont();

          display->setCursor(0, curPos);
          display->printf(" Speed b/w sen.: %1.2f m/s", getInputParamsArrayFloat(5));
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf("  Time b/w sen.: %1.2f ms", getInputParamsArrayFloat(6));
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf(" Est. tot. time: %1.2f ms", getInputParamsArrayFloat(7));
          curPos += lineSpacing + 5;

          display->setFont(u8g2_font_6x13_tf);
        }

        prevPageIndex = resultPageIndex;
        drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
      }

      if (resultPageIndex == 3 && prevPageIndex != resultPageIndex)
      {
        display->fillScreen(BLACK);

        mString<30> title;
        title += "Estimated slit width";

        drawStringHCentered(title, 15);
        const uint8_t lineSpacing = 11;
        uint8_t curPos = 22;

        display->setFont();
        display->setCursor(0, curPos);

        if (getInputParamsArrayFloat(2) == BOTH_SENSORS_MEASUREMENTS_REQUIRED)
        {
          drawStringHCentered("Measurements from both", 27);
          drawStringHCentered("sensors are required", 37);
          curPos += lineSpacing * 2;
        }
        else if (getInputParamsArrayFloat(2) == NOT_AVAILABLE_FOR_LEAF_SHUTERS)
        {
          drawStringHCentered("Not available", 27);
          drawStringHCentered("for leaf shutters", 37);
          curPos += lineSpacing * 2;
        }
        else
        {
          display->printf(" By sensor 1: %1.2f mm", getInputParamsArrayFloat(2));
          curPos += lineSpacing;
          display->setCursor(0, curPos);
          display->printf(" By sensor 2: %1.2f mm", getInputParamsArrayFloat(3));
          curPos += lineSpacing;
          display->setCursor(0, curPos);
          display->printf(" On average: %1.2f mm", getInputParamsArrayFloat(4));
        }

        curPos += lineSpacing + 10;

        title = "Other data";
        display->setFont(u8g2_font_6x13_tf);
        drawStringHCentered(title, curPos + 5);
        display->setFont();

        curPos += lineSpacing + 3;
        display->setCursor(0, curPos);
        display->printf(" Used sensor.: %s", SensorTypeStr[getInputParamsArrayInt(5)]);

        curPos += lineSpacing;
        display->setCursor(0, curPos);
        display->printf(" Sel.curt.mov.: %s", CurtainMovementItemsStr[getInputParamsArrayInt(6)]);

        display->setFont(u8g2_font_6x13_tf);

        prevPageIndex = resultPageIndex;
        drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
      }
    }
    
    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){}//wait for new data from main unit
    parseInputData();
  }
}

void drawSensorSignalLevelBar(uint8_t &x, uint8_t &y, uint8_t sensorNumber, uint8_t curSensorValue)
{
  static int8_t prevSensor1SignalStatus = -1;
  static int8_t prevSensor2SignalStatus = -1;
  static int8_t prevSensor1ActualBarWidth = 0;
  static int8_t prevSensor2ActualBarWidth = 0;
  static const char *sensorStatuses[3]  = {
    "Too dim",
    "Too bright",
    "OK"
  };
  const uint8_t barWidthPx = 80;
  const uint8_t barX = 10;
  double sensorValueRate = curSensorValue / (double)MAX_SIGNAL_LEVEL;
  sensorValueRate = sensorValueRate > 1.0 ? 1.0 : sensorValueRate;
  x = 15;
  y += 20;
  display->setCursor(x, y);
  display->printf("Sensor %i signal level", sensorNumber);
  y += 5;
  display->drawRect(barX, y, barWidthPx, 10, WHITE);
  int16_t newBarWidth = (barWidthPx - 2) * sensorValueRate;

  if (sensorNumber == 1 && newBarWidth != prevSensor1ActualBarWidth)
  {
    if (newBarWidth > prevSensor1ActualBarWidth)//add to what we have
    {
      display->fillRect(barX + 1 + prevSensor1ActualBarWidth, y + 1, newBarWidth - prevSensor1ActualBarWidth, 8, WHITE);
    }
    else//draw black upon what we have
    {
      display->fillRect(barX + 1 + prevSensor1ActualBarWidth - (prevSensor1ActualBarWidth - newBarWidth),
                        y + 1, prevSensor1ActualBarWidth - newBarWidth, 8, BLACK);
    }

    prevSensor1ActualBarWidth = newBarWidth;
  }

  if (sensorNumber == 2 && newBarWidth != prevSensor2ActualBarWidth)
  {
    if (newBarWidth > prevSensor2ActualBarWidth) // add to what we have
    {
      display->fillRect(barX + 1 + prevSensor2ActualBarWidth, y + 1, newBarWidth - prevSensor2ActualBarWidth, 8, WHITE);
    }
    else // draw black upon what we have
    {
      display->fillRect(barX + 1 + prevSensor2ActualBarWidth - (prevSensor2ActualBarWidth - newBarWidth),
                        y + 1, prevSensor2ActualBarWidth - newBarWidth, 8, BLACK);
    }

    prevSensor2ActualBarWidth = newBarWidth;
  }

  x += barWidthPx;
  y += 9;

  int8_t sensorStatus = 0;

  if (curSensorValue <= MIN_ALLOWED_SIGNAL_LEVEL)
  {
    sensorStatus = 0;
  }
  else if (curSensorValue > MAX_ALLOWED_SIGNAL_LEVEL)
  {
    sensorStatus = 1;
  }
  else
  {
    sensorStatus = 2;
  }

  if (sensorNumber == 1 && sensorStatus != prevSensor1SignalStatus)
  {
    display->setTextColor(BLACK); // erase old text
    display->setCursor(x, y);
    display->print(sensorStatuses[prevSensor1SignalStatus == -1 ? 0 : prevSensor1SignalStatus]);

    prevSensor1SignalStatus = sensorStatus;
  }

  if (sensorNumber == 2 && sensorStatus != prevSensor2SignalStatus)
  {
    display->setTextColor(BLACK); // erase old text
    display->setCursor(x, y);
    display->print(sensorStatuses[prevSensor2SignalStatus == -1 ? 0 : prevSensor2SignalStatus]);

    prevSensor2SignalStatus = sensorStatus;
  }

  display->setCursor(x, y);
  display->setTextColor(WHITE);
  display->print(sensorStatuses[sensorStatus]);
}

void drawLightCheckScreen()
{
  display->fillScreen(BLACK);
  bool oldIsLightQualityOkVal = false;
  const char *lightQualityBrightnessLabel = "Light quality";

  while ((Screens)getCurScreen() == Screens::LIGHT_CHECK)
  {
    bool isLightQualityOk = (bool)getInputParamsArrayInt(1);
    uint8_t x = 16;
    uint8_t y = 20;

    drawStringHCentered("Light quality", y);
    y += 30;

    display->setTextSize(2);

    if(oldIsLightQualityOkVal != isLightQualityOk)
    {
      display->setTextColor(BLACK);
      drawStringHCentered(oldIsLightQualityOkVal ? "OK" : "Bad", y);

      oldIsLightQualityOkVal = isLightQualityOk;
    }

    display->setCursor(x, y);
    display->setTextColor(isLightQualityOk ? GREEN : RED);
    drawStringHCentered(isLightQualityOk ? "OK" : "Bad", y);

    display->setTextColor(WHITE);
    display->setTextSize(1);

    x = 65;
    y = 52;

    display->setTextSize(1);
    uint8_t sensor0Val = getInputParamsArrayInt(2);
    drawSensorSignalLevelBar(x, y, 1, sensor0Val);
    uint8_t sensor1Val = getInputParamsArrayInt(3);
    drawSensorSignalLevelBar(x, y, 2, sensor1Val);

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){} // wait for new data from main unit
    parseInputData();
  }
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

void drawMeasurementResultRecordSelectionScreen()
{
  display->fillScreen(BLACK);

  uint8_t xMargin = 30;
  uint8_t yMargin = 40;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  uint8_t curPageLabelY = 120;
  mString<30> pageLabel;
  const int16_t pageSize = 5;
  int16_t prevPage = 1;
  int16_t curPage = 1;
  const int16_t recTitleBufSize = 30;
  int16_t totalRecordsCount = getInputParamsArrayInt(2);
  int16_t totalPagesCount = ceil((double)totalRecordsCount / (double)pageSize);
  drawStringHCentered("Select record", 20);
  int8_t itemsCountToPrint = min(pageSize, totalRecordsCount);

  for (uint8_t i = 0; i < itemsCountToPrint; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));

    int32_t recordNumber = getInputParamsArrayInt(i + 3);

    if (recordNumber == 0)//"Back" option
    {
      display->print("Back");
      continue;
    }

    char recTitle[recTitleBufSize];
    formatRecordName(recordNumber, recTitle);
    display->print(recTitle);

    pageLabel.clear();
    pageLabel += "Page ";
    pageLabel.add(curPage);
    pageLabel += " of ";
    pageLabel.add(totalPagesCount);
    drawStringHCentered(pageLabel, curPageLabelY);
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t selectedRecordIndex = 0;
  int8_t prevSelectedRecordIndex = 0;

  int8_t topRecordIndex = 0;
  int8_t prevTopRecordIndex = 0;

  while ((Screens)getCurScreen() == Screens::MEASUREMENT_RECORD_SELECTION)
  {
    selectedRecordIndex = getInputParamsArrayInt(1);

    if (prevSelectedRecordIndex != selectedRecordIndex)
    {
      // Serial.printf("Sel. index: %i\n", selectedRecordIndex);

      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (prevSelectedRecordIndex % pageSize)));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (selectedRecordIndex % pageSize)));
      display->print("->");

      prevSelectedRecordIndex = selectedRecordIndex;

      curPage = ceil((selectedRecordIndex + 1) / (double)pageSize);

      if (prevPage != curPage)
      {
        display->setTextColor(BLACK);

        int16_t prevPageMaxIndex = pageSize * (prevPage - 1) + pageSize;
        int16_t prevPageLimit = min(prevPageMaxIndex, totalRecordsCount);

        // erase cur page
        for (uint8_t i = pageSize * (prevPage - 1), rowCounter = 0; i < prevPageLimit; i++, rowCounter++) 
        {
          display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));
          int32_t recordNumber = getInputParamsArrayInt(i + 3);
          char recTitle[recTitleBufSize];
          formatRecordName(recordNumber, recTitle);

          if (recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          display->print(recTitle);
        }

        pageLabel.clear();
        pageLabel += "Page ";
        pageLabel.add(prevPage);
        pageLabel += " of ";
        pageLabel.add(totalPagesCount);
        drawStringHCentered(pageLabel, curPageLabelY);

        display->setTextColor(WHITE);
        int16_t nextPageMaxIndex = pageSize * (curPage - 1) + pageSize;
        int16_t limit = min(nextPageMaxIndex, totalRecordsCount);

        // draw new page records
        for (int16_t i = pageSize * (curPage - 1), rowCounter = 0; i < limit; i++, rowCounter++) 
        {
          display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));
          int32_t recordNumber = getInputParamsArrayInt(i + 3);

          if (recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          char recTitle[recTitleBufSize];
          formatRecordName(recordNumber, recTitle);
          display->print(recTitle);
        }

        pageLabel.clear();
        pageLabel += "Page ";
        pageLabel.add(curPage);
        pageLabel += " of ";
        pageLabel.add(totalPagesCount);
        drawStringHCentered(pageLabel, curPageLabelY);

        prevPage = curPage;
      }
    }

    inputCmdStr.clear();
    dataProcessed = true;
    while (dataProcessed){} // wait for new data from main unit
    parseInputData();
  }
}

void drawMeasurementSaveScreen()
{
  display->fillScreen(BLACK);

  uint8_t xMargin = 20;
  uint8_t yMargin = 40;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  drawStringHCentered("Save measurement result?", 20);
  int16_t freeSlotsLeft = getInputParamsArrayInt(2);
  bool drawMoreThanZeroRecordsMenuItems = getInputParamsArrayInt(3);
  int16_t menuItemsCount = drawMoreThanZeroRecordsMenuItems ? SAVE_MEASUREMENT_MENU_ITEMS_COUNT : SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT;

  for (uint8_t i = 0; i < menuItemsCount; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));

    if (i == (uint8_t)MeasurementSaveMenuItems::YES)
    {
      if (freeSlotsLeft == 0)
      {
        display->setTextColor(DARKGREY);
        display->printf("%s(no free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
        display->setTextColor(WHITE);
      }
      else
      {
        display->printf("%s(%i free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems), freeSlotsLeft);
      }
    }
    else
    {
      display->println(GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
    }
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t selectedMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = 0;

  while ((Screens)getCurScreen() == Screens::MEASUREMENT_SAVE_SCREEN)
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
    while (dataProcessed){} // wait for new data from main unit
    parseInputData();
  }
}

void drawMessageScreen()
{
  display->fillScreen(BLACK);

  uint8_t xMargin = 35;
  uint8_t yMargin = 50;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  mString<50> message;
  int32_t messageScreenId = getInputParamsArrayInt(1);
  int16_t optionsCount = getInputParamsArrayInt(2);
  getInputParamsArrayString(4, message);
  drawStringHCentered(message, 25);

  for (int16_t i = 0; i < optionsCount; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));
    mString<50> optionStr;
    getInputParamsArrayString(i + 5, optionStr);
    display->print(optionStr.c_str());
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t selectedMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = 0;

  while ((Screens)getCurScreen() == Screens::MESSAGE && messageScreenId == getInputParamsArrayInt(1))
  {
    selectedMenuItemIndex = getInputParamsArrayInt(3);

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
    while (dataProcessed);// wait for new data from main unit
    parseInputData();
  }
}

void setup(void)
{
  // Serial.begin(115200);

  display->begin();
  display->fillScreen(BLACK);
  
  Wire.onReceive(i2cReceiveEvent); 
  Wire.begin(DISPLAY_I2C_ADDRESS); // join i2c bus with address #4


  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);
  // display->setTextSize(1,1,10);
}

void loop()
{
  // while (1)
  // {
  //   if (esp_flash_encryption_enabled())
  //     Serial.println("Encryption Enabled");
  //   else
  //     Serial.println("Encryption not Enabled");
  //   delay(1000);
  // }
  
  Serial.println("Starting main loop"); 

  while (true)
  {
    while (dataProcessed)//Wait until new data is available
    {
      Serial.println("No new data. Skipping");
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
      case Screens::ABOUT:
      {
        drawAboutScreen();
        break;
      }
      case Screens::CURTAN_MOVEMENT_SELECTION:
      {
        drawCurtainMovementSelectionScreen();
        break;
      }
      case Screens::MEASUREMENT_SAVE_SCREEN:
      {
        drawMeasurementSaveScreen();
        break;
      }
      case Screens::MEASUREMENT_RECORD_SELECTION:
      {
        drawMeasurementResultRecordSelectionScreen();
        break;
      }
      case Screens::MESSAGE:
      {
        drawMessageScreen();
        break;
      }

      default:
        break;
    }

    inputCmdStr.clear();
    dataProcessed = true;
  }
}