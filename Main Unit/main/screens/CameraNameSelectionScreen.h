#pragma once

#include <stdio.h>
#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/FilmCameraBrandsList.h"
#include "lib/FilmCamerasList.h"

// Camera picker for the record name editor: the brand list (FilmCameraBrandsList)
// opens the model list of that brand (FilmCamerasList filtered by the brand),
// and the chosen camera name goes back to the editor.
//
// Both lists are paged like the record selection list: the encoder moves the
// arrow (one item per detent), the page follows it and a click selects the
// item. "Back" is the first item of both lists: the model list returns to the
// brand list, the brand list cancels the pick.

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

// Paged brand list with the Back item on top. Returns the brand index in
// FilmCameraBrandsList, or -1 on Back / API abort.
int16_t drawCameraBrandSelectionScreen()
{
  static const int16_t kBackIndex = 0;
  static const int16_t kItemsCount = FILM_CAMERA_BRANDS_COUNT + 1;
  static const int16_t kX = 20;
  static const int16_t kArrowX = 8;
  static const int16_t kY = 30;
  static const int16_t kItemH = 13;
  static const int16_t kPageSize = 7;
  static const int16_t kPagerY = 122;

  int16_t totalPagesCount = (kItemsCount + kPageSize - 1) / kPageSize;
  int16_t startEncoderVal = AlexEncoder::counter;
  int16_t prevIndex = 0;
  int16_t prevPage = 1;

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);
  drawStringHCentered("Select brand", 16);

  auto drawPage = [&](int16_t page)
  {
    display->fillRect(0, kY - 10, display->width(), kPageSize * kItemH + 4, BLACK);
    display->setFont();
    display->setTextColor(WHITE);

    for (int16_t slot = 0; slot < kPageSize; slot++)
    {
      int16_t idx = (page - 1) * kPageSize + slot;

      if (idx >= kItemsCount)
      {
        break;
      }

      display->setCursor(kX, kY + slot * kItemH);
      display->print((idx == kBackIndex) ? "Back" : FilmCameraBrandsList[idx - 1]);
    }

    display->fillRect(0, 112, display->width(), 16, BLACK);
    display->setFont(u8g2_font_6x13_tf);
    display->setTextColor(WHITE);
    char pageBuf[24];
    snprintf(pageBuf, sizeof(pageBuf), "Page %d of %d", page, totalPagesCount);
    drawStringHCentered(pageBuf, kPagerY);
    display->setFont();
    display->setTextColor(WHITE);
  };

  auto eraseArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(BLACK);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
    display->setTextColor(WHITE);
  };

  auto drawArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(WHITE);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
  };

  drawPage(1);
  drawArrowAt(0);

  while (true)
  {
    if (isApiRequestReceived)
    {
      return -1; // ВЫХОД ДЛЯ API
    }

    int16_t curIndex = AlexEncoder::counter - startEncoderVal;

    if (curIndex > kItemsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (kItemsCount - 1);
      curIndex = kItemsCount - 1;
    }

    if (curIndex < 0)
    {
      curIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (prevIndex != curIndex)
    {
      int16_t curPage = curIndex / kPageSize + 1;

      if (curPage != prevPage)
      {
        drawPage(curPage);
        drawArrowAt(curIndex % kPageSize);
        prevPage = curPage;
      }
      else
      {
        eraseArrowAt(prevIndex % kPageSize);
        drawArrowAt(curIndex % kPageSize);
      }

      prevIndex = curIndex;
    }

    if (button.isClicked())
    {
      return (curIndex == kBackIndex) ? -1 : (curIndex - 1);
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// Paged model list of one brand, the Back item on top. Returns the index in
// FilmCamerasList, or -1 on Back / API abort.
int16_t drawCameraModelSelectionScreen(const char *brand)
{
  // FilmCamerasList indices of the models of this brand; rebuilt on every
  // screen entry (the biggest brands hold up to ~90 models).
  static int16_t cameraIndices[FILM_CAMERAS_COUNT];
  int16_t camerasCount = 0;

  for (int16_t i = 0; i < FILM_CAMERAS_COUNT; i++)
  {
    if (cameraNameMatchesBrand(FilmCamerasList[i], brand))
    {
      cameraIndices[camerasCount] = i;
      camerasCount++;
    }
  }

  int16_t itemsCount = camerasCount + 1; // + Back
  static const int16_t kX = 20;
  static const int16_t kArrowX = 8;
  static const int16_t kY = 30;
  static const int16_t kLineH = 9;
  static const int16_t kItemH = 17;
  static const uint8_t kCharsPerRow = 23;
  static const int16_t kPageSize = 5;
  static const int16_t kPagerY = 122;

  int16_t totalPagesCount = (itemsCount + kPageSize - 1) / kPageSize;
  int16_t startEncoderVal = AlexEncoder::counter;
  int16_t prevIndex = 0;
  int16_t prevPage = 1;

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);
  drawStringHCentered(brand, 16);

  auto drawPage = [&](int16_t page)
  {
    display->fillRect(0, kY - 10, display->width(), kPageSize * kItemH + 6, BLACK);
    display->setFont();
    display->setTextColor(WHITE);

    for (int16_t slot = 0; slot < kPageSize; slot++)
    {
      int16_t idx = (page - 1) * kPageSize + slot;

      if (idx >= itemsCount)
      {
        break;
      }

      const char *name = (idx == 0) ? "Back" : FilmCamerasList[cameraIndices[idx - 1]];

      char line1[kCharsPerRow + 1];
      char line2[kCharsPerRow + 1];
      splitNameTwoLines(name, line1, line2, kCharsPerRow);

      int16_t y0 = kY + slot * kItemH;
      display->setCursor(kX, y0);
      display->print(line1);

      if (line2[0] != '\0')
      {
        display->setCursor(kX, y0 + kLineH);
        display->print(line2);
      }
    }

    display->fillRect(0, 112, display->width(), 16, BLACK);
    display->setFont(u8g2_font_6x13_tf);
    display->setTextColor(WHITE);
    char pageBuf[24];
    snprintf(pageBuf, sizeof(pageBuf), "Page %d of %d", page, totalPagesCount);
    drawStringHCentered(pageBuf, kPagerY);
    display->setFont();
    display->setTextColor(WHITE);
  };

  auto eraseArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(BLACK);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
    display->setTextColor(WHITE);
  };

  auto drawArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(WHITE);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
  };

  drawPage(1);
  drawArrowAt(0);

  while (true)
  {
    if (isApiRequestReceived)
    {
      return -1; // ВЫХОД ДЛЯ API
    }

    int16_t curIndex = AlexEncoder::counter - startEncoderVal;

    if (curIndex > itemsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (itemsCount - 1);
      curIndex = itemsCount - 1;
    }

    if (curIndex < 0)
    {
      curIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (prevIndex != curIndex)
    {
      int16_t curPage = curIndex / kPageSize + 1;

      if (curPage != prevPage)
      {
        drawPage(curPage);
        drawArrowAt(curIndex % kPageSize);
        prevPage = curPage;
      }
      else
      {
        eraseArrowAt(prevIndex % kPageSize);
        drawArrowAt(curIndex % kPageSize);
      }

      prevIndex = curIndex;
    }

    if (button.isClicked())
    {
      return (curIndex == 0) ? -1 : cameraIndices[curIndex - 1];
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
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
