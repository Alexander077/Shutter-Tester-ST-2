#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
#include "screens/MeasuredResult.h"
#include "screens/MessageScreen.h"
#include "lib/serialAPI.h"

inline void drawViewRecordsScreen()
{
  while (true)
  {
    if (isApiRequestReceived)
    {
      return; // ВЫХОД ДЛЯ API
    }

    int32_t selectedRecordNumber = drawMeasurementResultRecordSelectionScreen();

    if (isApiRequestReceived)
    {
      return; // ВЫХОД ДЛЯ API
    }

    if (selectedRecordNumber == 0) //"Back" option selected
    {
      return;
    }

    StoredMeasuredResult selectedResult = getMeasurementStoredResultByNumber(selectedRecordNumber);

    if (selectedResult.recordNumber == -1)
    {
      halt("Halted. selectedRecordNumber is not found");
    }

    MeasuredResult resultToDisplay = {};
    resultToDisplay.sensor0Time = selectedResult.sensor0Time;
    resultToDisplay.sensor1Time = selectedResult.sensor1Time;
    resultToDisplay.curtain1spanAspeed = selectedResult.curtain1spanAspeed;
    resultToDisplay.curtain1spanAtime = selectedResult.curtain1spanAtime;
    resultToDisplay.curtain1TotalTime = selectedResult.curtain1TotalTime;
    resultToDisplay.curtain2spanAspeed = selectedResult.curtain2spanAspeed;
    resultToDisplay.curtain2spanAtime = selectedResult.curtain2spanAtime;
    resultToDisplay.curtain2TotalTime = selectedResult.curtain2TotalTime;
    resultToDisplay.slitWidthSensor0 = selectedResult.slitWidthSensor0;
    resultToDisplay.slitWidthSensor1 = selectedResult.slitWidthSensor1;
    resultToDisplay.slitWidthAverage = selectedResult.slitWidthAverage;
    resultToDisplay.usedSensorType = selectedResult.usedSensorType;
    resultToDisplay.selectedCurtainMovement = selectedResult.selectedCurtainMovement;

    renderMeasuredResult(resultToDisplay);

    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    const char *options[] = {"Go back to list", "Go to main menu", "Delete record"};
    int16_t selectedOptionIndex = drawMessageScreen("What's next?", 3, options);

    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    if (selectedOptionIndex == 0) // Go back to list
    {
      continue;
    }

    if (selectedOptionIndex == 1) // Go to main menu
    {
      return;
    }

    if (selectedOptionIndex == 2) // Delete record
    {
      const char *options[] = {"Yes", "No"};
      int16_t deleteOptionIndex = drawMessageScreen("Really delete?", 2, options);

      if (isApiRequestReceived)
        return; // ВЫХОД ДЛЯ API

      if (deleteOptionIndex == 0) // Delete record
      {
        bool deleteRes = deleteMeasurementStoredResultByNumber(selectedResult.recordNumber);
        char *msg;

        if (!deleteRes)
        {
          msg = "Failed to delete record";
        }
        else
        {
          msg = "Record deleted";
        }

        const char *deleteResOptions[] = {"OK"};
        drawMessageScreen(msg, 1, deleteResOptions);
      }
      continue; // go to records list selection
    }

    return;
  }

  vTaskDelay(1 / portTICK_PERIOD_MS);
}