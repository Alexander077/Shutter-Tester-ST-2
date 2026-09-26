#pragma once

#include "screensCommon.h"
#include "lib/AlexEncoder.h"

// Generic paged item selection screen: a centered title, a list of items and
// a "Page N of M" footer. A name that does not fit into one row is shown on
// two rows; a short name takes a single row. "Go Back" is always drawn as the
// first item - the caller passes only the real items.
//
// Returns -1 when "Go Back" is chosen (or the API takes over) and the 0-based
// index in 'items' when a regular item is chosen.
//
// Layout (160x128 display, rotation 1):
//   title   u8g2_font_6x13_tf centered at kTitleY
//   items   classic font (6x8 cell, y is the top of the cell) at kX, the "->"
//           marker at kArrowX; a one-line item is kOneLineItemH tall, a
//           two-line item (second line at +kLineH) is kTwoLineItemH tall
//   footer  "Page N of M" with u8g2_font_6x13_tf centered at kPagerY
// The two-line split reuses splitNameTwoLines() (declared in screensCommon.h).
int16_t drawSelectItemScreen(const char *title, const char *items[], int16_t itemsCount)
{
  static const int16_t kX = 20;
  static const int16_t kArrowX = 8;
  static const int16_t kTitleY = 16;
  static const int16_t kListTopY = 25;
  static const int16_t kListBottomY = 110; // items must end above the pager
  static const int16_t kLineH = 9;         // second line offset inside a two-line item
  static const int16_t kOneLineItemH = 12;
  static const int16_t kTwoLineItemH = 21;
  static const int16_t kTwoLineArrowOffsetY = 4; // puts "->" between the two lines
  static const uint8_t kCharsPerRow = 23;        // classic font: 6 px per char at kX
  static const int16_t kPagerY = 122;
  // The biggest list the screen has to page through (FilmCamerasList holds
  // 1161 entries). Guards the page-start table against a bad itemsCount.
  static const int16_t kMaxItemsCount = 1200;

  if (itemsCount < 0)
  {
    itemsCount = 0;
  }

  if (itemsCount > kMaxItemsCount)
  {
    itemsCount = kMaxItemsCount;
  }

  // Display position 0 is the always-present "Go Back" item, so the display
  // list is one item longer than the caller's array.
  const int16_t displayItemsCount = itemsCount + 1;

  auto getItemText = [&](int16_t displayIndex) -> const char *
  {
    return (displayIndex == 0) ? "Go Back" : items[displayIndex - 1];
  };

  auto itemIsTwoLines = [&](int16_t displayIndex)
  {
    return strlen(getItemText(displayIndex)) > kCharsPerRow;
  };

  auto getItemHeight = [&](int16_t displayIndex)
  {
    return itemIsTwoLines(displayIndex) ? kTwoLineItemH : kOneLineItemH;
  };

  // Pages are packed by height: pageStarts[p] is the display index of the
  // first item of page p, pageStarts[pagesCount] == displayItemsCount.
  static int16_t pageStarts[kMaxItemsCount + 2];
  int16_t pagesCount = 0;

  pageStarts[0] = 0;

  while (pageStarts[pagesCount] < displayItemsCount)
  {
    int16_t idx = pageStarts[pagesCount];
    int16_t usedHeight = 0;

    while (idx < displayItemsCount &&
           usedHeight + getItemHeight(idx) <= kListBottomY - kListTopY)
    {
      usedHeight += getItemHeight(idx);
      idx++;
    }

    pagesCount++;
    pageStarts[pagesCount] = idx;
  }

  auto getPageOfIndex = [&](int16_t displayIndex)
  {
    int16_t page = 0;

    while (pageStarts[page + 1] <= displayIndex)
    {
      page++;
    }

    return page;
  };

  // Top y of the item's first line (classic font: y is the top of the cell).
  auto getItemTopY = [&](int16_t displayIndex)
  {
    int16_t y = kListTopY;

    for (int16_t idx = pageStarts[getPageOfIndex(displayIndex)]; idx < displayIndex; idx++)
    {
      y += getItemHeight(idx);
    }

    return y;
  };

  auto drawPage = [&](int16_t page)
  {
    display->fillRect(0, kListTopY - 4, display->width(), kListBottomY - kListTopY + 5, BLACK);
    display->setFont();
    display->setTextColor(WHITE);

    int16_t y = kListTopY;

    for (int16_t idx = pageStarts[page]; idx < pageStarts[page + 1]; idx++)
    {
      char line1[kCharsPerRow + 1];
      char line2[kCharsPerRow + 1];
      splitNameTwoLines(getItemText(idx), line1, line2, kCharsPerRow);

      display->setCursor(kX, y);
      display->print(line1);

      if (line2[0] != '\0')
      {
        display->setCursor(kX, y + kLineH);
        display->print(line2);
      }

      y += getItemHeight(idx);
    }

    display->fillRect(0, 112, display->width(), 16, BLACK);
    display->setFont(u8g2_font_6x13_tf);
    display->setTextColor(WHITE);
    char pageBuf[24];
    snprintf(pageBuf, sizeof(pageBuf), "Page %d of %d", page + 1, pagesCount);
    drawStringHCentered(pageBuf, kPagerY);
    display->setFont();
    display->setTextColor(WHITE);
  };

  auto drawArrowAt = [&](int16_t displayIndex, uint16_t color)
  {
    int16_t y = getItemTopY(displayIndex);

    if (itemIsTwoLines(displayIndex))
    {
      y += kTwoLineArrowOffsetY;
    }

    display->setFont();
    display->setTextColor(color);
    display->setCursor(kArrowX, y);
    display->print("->");
    display->setTextColor(WHITE);
  };

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);
  drawStringHCentered(title, kTitleY);

  int16_t startEncoderVal = AlexEncoder::counter;
  int16_t prevIndex = 0;
  int16_t prevPage = 0;

  drawPage(0);
  drawArrowAt(0, WHITE);

  while (true)
  {
    if (isApiRequestReceived)
    {
      return -1; // ВЫХОД ДЛЯ API
    }

    int16_t curIndex = AlexEncoder::counter - startEncoderVal;

    if (curIndex > displayItemsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (displayItemsCount - 1);
      curIndex = displayItemsCount - 1;
    }

    if (curIndex < 0)
    {
      curIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (prevIndex != curIndex)
    {
      int16_t curPage = getPageOfIndex(curIndex);

      if (curPage != prevPage)
      {
        drawPage(curPage);
        drawArrowAt(curIndex, WHITE);
        prevPage = curPage;
      }
      else
      {
        drawArrowAt(prevIndex, BLACK);
        drawArrowAt(curIndex, WHITE);
      }

      prevIndex = curIndex;
    }

    if (button.isClicked())
    {
      return (curIndex == 0) ? -1 : (curIndex - 1);
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
