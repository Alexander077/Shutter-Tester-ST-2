#pragma once

#include <stdio.h>
#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/FilmCameraBrandsList.h"
#include "lib/FilmCamerasList.h"
#include "screens/SelectItemScreen.h"

// Camera picker for the record name editor: the brand list (FilmCameraBrandsList)
// opens the model list of that brand (FilmCamerasList filtered by the brand),
// and the chosen camera name goes back to the editor.
//
// Both lists are drawn by drawSelectItemScreen() (generic paged list with the
// "Go Back" item on top): the encoder moves the arrow, the page follows it and
// a click selects the item. "Go Back" of the model list returns to the brand
// list, "Go Back" of the brand list cancels the pick.

// Model lines that FilmCameraBrandsList folds into a brand: the camera names
// of these lines start with the model line instead of the brand itself
// (Canonet -> Canon, Rolleiflex/Rolleicord -> Rollei and so on).
struct CameraBrandModelLine
{
  const char *brand;
  const char *modelLine;
};

static const CameraBrandModelLine kCameraBrandModelLines[] =
{
    {"Beauty", "Beautyflex"},
    {"Canon", "Canonet"},
    {"Fuji", "Fujica"},
    {"Leica", "Leicaflex"},
    {"Ricoh", "Ricohflex"},
    {"Ricoh", "Ricohmatic"},
    {"Rollei", "Rolleicord"},
    {"Rollei", "Rolleiflex"},
};

static const uint8_t kCameraBrandModelLinesCount =
    sizeof(kCameraBrandModelLines) / sizeof(kCameraBrandModelLines[0]);

// True when 'camera' starts with 'prefix' followed by the end of the name or a
// space: a plain prefix comparison would let the brand "Exa" match "Exakta".
static bool cameraNameHasPrefix(const char *camera, const char *prefix)
{
  uint8_t prefixLen = strlen(prefix);

  if (!recordNameStartsWithIgnoreCase(camera, prefix, prefixLen))
  {
    return false;
  }

  return camera[prefixLen] == '\0' || camera[prefixLen] == ' ';
}

// True when the camera belongs to the brand: the name starts with the brand
// itself or with one of the folded model lines of that brand.
static bool cameraNameMatchesBrand(const char *camera, const char *brand)
{
  if (cameraNameHasPrefix(camera, brand))
  {
    return true;
  }

  for (uint8_t i = 0; i < kCameraBrandModelLinesCount; i++)
  {
    if (strcmp(kCameraBrandModelLines[i].brand, brand) == 0 &&
        cameraNameHasPrefix(camera, kCameraBrandModelLines[i].modelLine))
    {
      return true;
    }
  }

  return false;
}

// Brand list (drawSelectItemScreen with the Go Back item on top). Returns the
// brand index in FilmCameraBrandsList, or -1 on Go Back / API abort.
int16_t drawCameraBrandSelectionScreen()
{
  return drawSelectItemScreen("Select brand", FilmCameraBrandsList, (int16_t)FILM_CAMERA_BRANDS_COUNT);
}

// Model list of one brand (drawSelectItemScreen with the Go Back item on top).
// Returns the index in FilmCamerasList, or -1 on Go Back / API abort.
int16_t drawCameraModelSelectionScreen(const char *brand)
{
  // Names and FilmCamerasList indices of the models of this brand; rebuilt on
  // every screen entry (the biggest brands hold up to ~90 models).
  static const char *modelNames[FILM_CAMERAS_COUNT];
  static int16_t cameraIndices[FILM_CAMERAS_COUNT];
  int16_t camerasCount = 0;

  for (int16_t i = 0; i < FILM_CAMERAS_COUNT; i++)
  {
    if (cameraNameMatchesBrand(FilmCamerasList[i], brand))
    {
      modelNames[camerasCount] = FilmCamerasList[i];
      cameraIndices[camerasCount] = i;
      camerasCount++;
    }
  }

  int16_t selectedIndex = drawSelectItemScreen(brand, modelNames, camerasCount);

  if (selectedIndex < 0)
  {
    return -1;
  }

  return cameraIndices[selectedIndex];
}

// Camera picker: brand list -> model list -> the chosen camera name.
// Returns false when the user backs out (or the API takes over); 'out' gets
// the full FilmCamerasList entry otherwise.
bool pickCameraName(char out[RECORD_NAME_BUF_SIZE])
{
  while (true)
  {
    if (isApiRequestReceived)
    {
      return false;
    }

    int16_t brandIndex = drawCameraBrandSelectionScreen();

    if (brandIndex < 0)
    {
      return false; // Back from the brand list / API abort
    }

    int16_t cameraIndex = drawCameraModelSelectionScreen(FilmCameraBrandsList[brandIndex]);

    if (cameraIndex >= 0)
    {
      copyRecordName(out, FilmCamerasList[cameraIndex]);
      return true;
    }

    if (isApiRequestReceived)
    {
      return false;
    }
    // Back from the model list: choose the brand again.
  }
}
