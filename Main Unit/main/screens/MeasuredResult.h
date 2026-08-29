#include "../lib/MeasuredResult.h"
#include "screensCommon.h"

#define SHUTTR_SPEEDS_COUNT 14
#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

void renderMeasuredResult(MeasuredResult &res)
{
  int16_t startEncoderVal = AlexEncoder::counter;
  int8_t prevPageIndex = -1;
  display->fillScreen(BLACK);

  while (true)
  {
    if (isApiRequestReceived)
      return;

    int16_t resultPageIndex = AlexEncoder::counter - startEncoderVal;

    if (resultPageIndex > RESULT_PAGES_COUNT - 1)
    {
      startEncoderVal = AlexEncoder::counter - (RESULT_PAGES_COUNT - 1);
      resultPageIndex = RESULT_PAGES_COUNT - 1;
    }

    if (resultPageIndex < 0)
    {
      resultPageIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

// displayManager.drawMeasuredScreen(resultPageIndex, res);
#pragma region // display interaction
    // int8_t resultPageIndex = getInputParamsArrayInt(1);
    bool isReadingsValid = res.sensor0Time != MEASUREMENTS_IS_INVALID_VAL;

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

        // double sensorTime = getInputParamsArrayFloat(2);
        double sensorTime = resultPageIndex == 0 ? res.sensor0Time : res.sensor1Time;

        drawStringHCentered("Sensor " + String(resultPageIndex + 1) + " summary", 15);

        if (sensorTime > 0) // if any data taken
        {
          display->setCursor(5, 35);

          if (sensorTime < 1.0) // less than a millisecond
          {
            display->printf("Time: %.3f ms", sensorTime);
          }
          else if (sensorTime > 1000.0) // More than a second
          {
            display->printf("Time: %.2f s", sensorTime / 1000.0);
          }
          else // between 1ms and 1s
          {
            display->printf("Time: %.2f ms", sensorTime);
          }

          if (sensorTime <= 1000.0) // show speed and graph for sensor time only under 1 second
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

        if (res.curtain1spanAtime == BOTH_SENSORS_MEASUREMENTS_REQUIRED) // no curtain data
        {
          drawStringHCentered("No curtain data", 30);
          drawStringHCentered("Measurements from both", 55);
          drawStringHCentered("sensors are required", 70);
        }
        else if (res.curtain1spanAtime == NOT_AVAILABLE_FOR_LEAF_SHUTERS) // no curtain data
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
          display->printf(" Speed b/w sen.: %1.2f m/s", res.curtain1spanAspeed);
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf("  Time b/w sen.: %1.2f ms", res.curtain1spanAtime);
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf(" Est. tot. time: %1.2f ms", res.curtain1TotalTime);
          curPos += lineSpacing + 15;

          display->setCursor(0, curPos);
          display->setFont(u8g2_font_6x13_tf);

          drawStringHCentered("2nd curtain", curPos);

          curPos += 5;

          display->setFont();

          display->setCursor(0, curPos);
          display->printf(" Speed b/w sen.: %1.2f m/s", res.curtain2spanAspeed);
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf("  Time b/w sen.: %1.2f ms", res.curtain2spanAtime);
          curPos += lineSpacing;

          display->setCursor(0, curPos);
          display->printf(" Est. tot. time: %1.2f ms", res.curtain2TotalTime);
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

        if (res.slitWidthSensor0 == BOTH_SENSORS_MEASUREMENTS_REQUIRED)
        {
          drawStringHCentered("Measurements from both", 27);
          drawStringHCentered("sensors are required", 37);
          curPos += lineSpacing * 2;
        }
        else if (res.slitWidthSensor0 == NOT_AVAILABLE_FOR_LEAF_SHUTERS)
        {
          drawStringHCentered("Not available", 27);
          drawStringHCentered("for leaf shutters", 37);
          curPos += lineSpacing * 2;
        }
        else
        {
          display->printf(" By sensor 1: %1.2f mm", res.slitWidthSensor0);
          curPos += lineSpacing;
          display->setCursor(0, curPos);
          display->printf(" By sensor 2: %1.2f mm", res.slitWidthSensor1);
          curPos += lineSpacing;
          display->setCursor(0, curPos);
          display->printf(" On average: %1.2f mm", res.slitWidthAverage);
        }

        curPos += lineSpacing + 10;

        title = "Other data";
        display->setFont(u8g2_font_6x13_tf);
        drawStringHCentered(title, curPos + 5);
        display->setFont();

        curPos += lineSpacing + 3;
        display->setCursor(0, curPos);
        display->printf(" Used sensor.: %s", SensorTypeStr[(uint8_t)res.usedSensorType]);

        curPos += lineSpacing;
        display->setCursor(0, curPos);
        display->printf(" Sel.curt.mov.: %s", CurtainMovementItemsStr[(int8_t)res.selectedCurtainMovement]);

        display->setFont(u8g2_font_6x13_tf);

        prevPageIndex = resultPageIndex;
        drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
      }
    }
#pragma endregion // end of display interaction

    if (button.isClicked())
    {
      return;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}