#pragma once

#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/FilmCamerasList.h"
#include "screens/MessageScreen.h"

// One symbol of the final suggested name line.
struct SuggestStringSymbol
{
  char symbol;          // the character at this position
  bool isPartOfSuggest; // true = grey suggestion tail, false = user-typed symbol
};

// Builds the suggested line for the currently typed string.
//
// currentString : the text typed so far ('\0'-terminated, <= RECORD_NAME_MAX_LEN).
// out           : receives the resulting line: every typed symbol first
//                 (isPartOfSuggest == false), then the grey suggestion tail
//                 (isPartOfSuggest == true), and a terminator with symbol='\0'.
//                 Must be at least RECORD_NAME_BUF_SIZE (41) entries.
//
// This is the autocomplete hook: the renderer/click handler below only rely on
// the symbol/isPartOfSuggest contract, so the matching algorithm inside can be
// freely changed. Default: pick the camera whose beginning best matches the
// end of the typed text (case-insensitive), then append its remainder.
void buildSuggestString(const char *currentString, SuggestStringSymbol *out)
{
  uint8_t typedLen = 0;
  while (typedLen < RECORD_NAME_MAX_LEN && currentString[typedLen] != '\0')
  {
    typedLen++;
  }

  // User-typed symbols come first and are never part of the suggestion.
  for (uint8_t i = 0; i < typedLen; i++)
  {
    out[i].symbol = currentString[i];
    out[i].isPartOfSuggest = false;
  }

  // Match against the typed text with trailing spaces trimmed (the camera name
  // does not continue from trailing spaces).
  uint8_t lookLen = typedLen;
  while (lookLen > 0 && currentString[lookLen - 1] == ' ')
  {
    lookLen--;
  }

  uint8_t tailLen = 0;
  if (lookLen > 0)
  {
    const char *best = nullptr;
    uint8_t bestLen = 0;

    for (int i = 0; i < FILM_CAMERAS_COUNT; i++)
    {
      const char *cam = FilmCamerasList[i];
      uint8_t camLen = 0;
      while (cam[camLen] != '\0' && camLen < lookLen)
      {
        camLen++;
      }

      // Longest k so that the last k typed chars equal cam[0..k-1].
      uint8_t k = 0;
      for (uint8_t cand = camLen; cand > bestLen; cand--)
      {
        bool ok = true;
        for (uint8_t j = 0; j < cand; j++)
        {
          if (recordNameToLowerChar(currentString[lookLen - cand + j]) != recordNameToLowerChar(cam[j]))
          {
            ok = false;
            break;
          }
        }
        if (ok)
        {
          k = cand;
          break;
        }
      }

      if (k > bestLen)
      {
        bestLen = k;
        best = cam;
      }
      if (bestLen == lookLen)
      {
        break;
      }
    }

    if (best != nullptr && bestLen > 0)
    {
      uint8_t freeRoom = RECORD_NAME_MAX_LEN - typedLen;
      uint8_t i = 0;
      while (best[bestLen + i] != '\0' && i < freeRoom)
      {
        out[typedLen + i].symbol = best[bestLen + i];
        out[typedLen + i].isPartOfSuggest = true;
        i++;
      }
      tailLen = i;
    }
  }

  out[typedLen + tailLen].symbol = '\0';
  out[typedLen + tailLen].isPartOfSuggest = false;
}

// Record name editor: character-by-character entry with
// Record name editor: character-by-character entry with
// camera-name autocomplete, delete mode and Ok/Cancel/Delete Mode menu.
// Redraws only dirty areas (name cell, message line, menu arrow).
// Both cursors stay visible: white = active, grey = inactive. Footer menu
// position is preserved across focus switches; the long hold toggles focus
// both ways (no "Back To Edit" item).
//
// Returns true on "Ok" (nameOut holds the new name, '\0'-terminated),
// false on "Cancel" or API abort. isDuplicate() is storage-specific
// (measurements vs series) and must already exclude the overwritten record.
bool editRecordName(const char *title,
                    char nameOut[RECORD_NAME_BUF_SIZE],
                    const char *initialName,
                    bool (*isDuplicate)(const char *))
{
  // Charset in encoder order (space first = visible empty slot, Q4).
  static const char kCharset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-()+<>{}[]/#.*&";
  static const uint8_t kCharsetLen = sizeof(kCharset) - 1; // without '\0'

  enum class EditorFocus : uint8_t
  {
    EDIT,
    MENU
  };

  enum class EditMode : uint8_t
  {
    INSERT,
    DELETE_MODE
  };

  static const char *kMenuItems[] = {"Ok", "Cancel", "Delete Mode"};
  static const uint8_t kMenuCount = 3;

  // Layout: u8g2_font_6x13_tf is 6px wide, display is 160px -> 26 chars/row.
  // 40 chars always fit into exactly 2 rows, no scrolling needed.
  static const uint8_t kNameCharsPerRow = 26;
  static const uint8_t kNameRows = 2;
  static const uint8_t kNameX = 2;
  static const int16_t kTitleY = 12;
  static const int16_t kNameY0 = 28;
  static const int16_t kNameY1 = 43;
  static const int16_t kMsgY = 60;
  static const int16_t kMsgH = 12;
  static const int16_t kMenuY = 76;
  static const int16_t kMenuRowH = 12;
  static const uint8_t kMenuX = 20;
  static const uint8_t kArrowX = 8;
  static const uint8_t kNameW = 160 - kNameX * 2;
  static const uint16_t kHoldToMenuMs = 700;

  char name[RECORD_NAME_BUF_SIZE];
  uint8_t nameLen = 0;
  uint8_t cursor = 0;
  EditorFocus focus = EditorFocus::EDIT;
  EditMode mode = EditMode::INSERT;
  uint8_t menuIndex = 0;
  char message[32] = {0};
  bool confirmed = false;

  if (initialName != nullptr)
  {
    copyRecordName(name, initialName);
  }
  else
  {
    name[0] = '\0';
  }

  nameLen = 0;
  while (nameLen < RECORD_NAME_MAX_LEN && name[nameLen] != '\0')
  {
    nameLen++;
  }
  // Prefill is shown trailing-trimmed, like the record list shows it.
  while (nameLen > 0 && name[nameLen - 1] == ' ')
  {
    nameLen--;
  }
  name[nameLen] = '\0';
  cursor = nameLen;

  auto charsetIndexOf = [&](char c) -> int8_t
  {
    for (uint8_t i = 0; i < kCharsetLen; i++)
    {
      if (kCharset[i] == c)
      {
        return i;
      }
    }
    return -1;
  };

  // Sanitizes a prefilled name: unknown chars become spaces so the encoder
  // can always cycle them (charsetIndexOf never returns -1 afterwards).
  auto sanitizeName = [&]()
  {
    for (uint8_t i = 0; i < nameLen; i++)
    {
      if (charsetIndexOf(name[i]) < 0)
      {
        name[i] = ' ';
      }
    }
  };

  sanitizeName();

  // (suggestion matching lives in buildSuggestString() above)

  // Resulting suggested line, rebuilt via the buildSuggestString hook on every
  // user edit. Row rendering and click handling read suggestResult directly:
  // entries with isPartOfSuggest == true are the grey suggestion tail.
  SuggestStringSymbol suggestResult[RECORD_NAME_BUF_SIZE] = {};
  uint8_t suggestResultLen = 0;

  auto rebuildSuggestion = [&]()
  {
    buildSuggestString(name, suggestResult);

    suggestResultLen = 0;
    while (suggestResultLen < RECORD_NAME_BUF_SIZE &&
           suggestResult[suggestResultLen].symbol != '\0')
    {
      suggestResultLen++;
    }
  };

  // Visible rows: committed text + current suggestion tail (grey).
  char rowText[kNameRows][kNameCharsPerRow + 1];
  uint8_t rowSuggFrom[kNameRows]; // first grey col in row, 255 = none

  auto rebuildRows = [&]()
  {
    for (uint8_t r = 0; r < kNameRows; r++)
    {
      rowText[r][0] = '\0';
      rowSuggFrom[r] = 255;
    }

    rebuildSuggestion();

    for (uint8_t pos = 0; pos < kNameCharsPerRow * kNameRows; pos++)
    {
      if (pos >= suggestResultLen)
      {
        break; // input exhausted (typed text + suggestion tail)
      }

      uint8_t r = pos / kNameCharsPerRow;
      uint8_t c = pos % kNameCharsPerRow;

      rowText[r][c] = suggestResult[pos].symbol;
      rowText[r][c + 1] = '\0';

      if (suggestResult[pos].isPartOfSuggest && rowSuggFrom[r] == 255)
      {
        rowSuggFrom[r] = c;
      }
    }
  };

  // --- Static layout (drawn once) ---
  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);
  drawStringHCentered(title, kTitleY);
  // All items except "Delete Mode" (its On/Off flag is drawn by
  // drawDeleteFlag() below so it can be updated without a full redraw).
  for (uint8_t i = 0; i < kMenuCount - 1; i++)
  {
    display->setCursor(kMenuX, kMenuY + i * kMenuRowH);
    display->print(kMenuItems[i]);
  }

  // --- Dirty-state caches ---
  char drawnRows[kNameRows][kNameCharsPerRow + 1] = {{0}};
  uint8_t drawnSuggFrom[kNameRows] = {255, 255};
  uint8_t drawnCursor = 255;
  EditorFocus drawnFocus = EditorFocus::MENU;
  EditMode drawnMode = EditMode::DELETE_MODE;
  uint8_t drawnMenuIndex = 255;
  EditorFocus drawnMenuActive = EditorFocus::MENU; // focus used for arrow color
  bool drawnDeleteFlag = false;                    // last drawn On/Off state
  bool drawnDeleteFlagOnce = false;
  char drawnMessage[32] = {0};
  bool drawnMessageOnce = false;

  auto drawNameArea = [&]()
  {
    display->setFont(u8g2_font_6x13_tf);

    for (uint8_t r = 0; r < kNameRows; r++)
    {
      int16_t rowY = (r == 0) ? kNameY0 : kNameY1;

      if (strcmp(drawnRows[r], rowText[r]) != 0 || drawnSuggFrom[r] != rowSuggFrom[r])
      {
        display->fillRect(kNameX, rowY - 10, kNameW, 12, BLACK);
        display->setTextColor(WHITE);
        display->setCursor(kNameX, rowY);

        uint8_t rowStart = r * kNameCharsPerRow;
        uint8_t whiteLen = 0;
        while (whiteLen < kNameCharsPerRow && rowText[r][whiteLen] != '\0' &&
               (rowStart + whiteLen) < nameLen)
        {
          whiteLen++;
        }
        for (uint8_t i = 0; i < whiteLen; i++)
        {
          display->print(rowText[r][i]);
        }

        if (rowSuggFrom[r] != 255)
        {
          display->setCursor(kNameX + (int16_t)rowSuggFrom[r] * 6, rowY);
          display->setTextColor(DARKGREY);
          for (uint8_t i = rowSuggFrom[r]; rowText[r][i] != '\0'; i++)
          {
            display->print(rowText[r][i]);
          }
          display->setTextColor(WHITE);
        }

        strcpy(drawnRows[r], rowText[r]);
        drawnSuggFrom[r] = rowSuggFrom[r];
        drawnCursor = 255;
      }
    }
  };

  auto drawCursor = [&]()
  {
    display->setFont(u8g2_font_6x13_tf);

    // Name cursor is never hidden: white when editing, grey while the
    // footer menu owns the focus.
    uint16_t cursorColor = (focus == EditorFocus::EDIT) ? WHITE : DARKGREY;

    if (drawnCursor != 255 && (drawnCursor != cursor || drawnFocus != focus || drawnMode != mode))
    {
      uint8_t r = drawnCursor / kNameCharsPerRow;
      uint8_t c = drawnCursor % kNameCharsPerRow;
      if (r < kNameRows)
      {
        int16_t rowY = (r == 0) ? kNameY0 : kNameY1;
        display->fillRect(kNameX + c * 6, rowY - 10, 6, 12, BLACK);
        display->setCursor(kNameX + c * 6, rowY);
        // Restore the underlying cell: committed char (white) or the live
        // suggestion tail char (grey) from the current rowText.
        char ch = ' ';
        bool isSuggCell = true;
        if (drawnCursor < nameLen)
        {
          ch = name[drawnCursor];
          isSuggCell = false;
        }
        else if (r < kNameRows && c < kNameCharsPerRow && rowText[r][c] != '\0')
        {
          ch = rowText[r][c];
        }
        display->setTextColor(isSuggCell ? DARKGREY : WHITE);
        display->print(ch);
        display->setTextColor(WHITE);
      }
      drawnCursor = 255;
    }

    if (drawnCursor != cursor || drawnFocus != focus || drawnMode != mode)
    {
      uint8_t r = cursor / kNameCharsPerRow;
      uint8_t c = cursor % kNameCharsPerRow;
      if (r < kNameRows)
      {
        int16_t rowY = (r == 0) ? kNameY0 : kNameY1;
        display->fillRect(kNameX + c * 6, rowY - 10, 6, 12, BLACK);
        display->setCursor(kNameX + c * 6, rowY);
        if (mode == EditMode::INSERT)
        {
          char ch = (cursor < nameLen) ? name[cursor] : ' ';
          display->setTextColor(cursorColor);
          display->print(ch);
          display->drawLine(kNameX + c * 6, rowY + 1, kNameX + c * 6 + 5, rowY + 1, cursorColor);
        }
        else
        {
          display->setTextColor(cursorColor);
          display->print('<');
        }
        drawnCursor = cursor;
        drawnFocus = focus;
        drawnMode = mode;
      }
    }
  };

  auto drawDeleteFlag = [&]()
  {
    // "Delete Mode [On]/[Off]" flag right of the label. Redrawn only when
    // the mode actually changes; label + flag share one dirty region so no
    // other row flickers.
    bool isOn = (mode == EditMode::DELETE_MODE);

    if (!drawnDeleteFlagOnce || drawnDeleteFlag != isOn)
    {
      static const uint8_t kDeleteItem = kMenuCount - 1;
      int16_t rowY = kMenuY + kDeleteItem * kMenuRowH;
      display->setFont(u8g2_font_6x13_tf);
      display->fillRect(kMenuX, rowY - 10, display->width() - kMenuX, 12, BLACK);
      display->setTextColor(WHITE);
      display->setCursor(kMenuX, rowY);
      display->print(kMenuItems[kDeleteItem]);
      display->print(isOn ? " [On]" : " [Off]");
      drawnDeleteFlag = isOn;
      drawnDeleteFlagOnce = true;
    }
  };

  auto drawMenuArrow = [&]()
  {
    // Footer arrow mirrors the name cursor: grey while inactive, white
    // when the menu owns the focus. Position is preserved across focus
    // switches (no reset to Ok).
    uint16_t arrowColor = (focus == EditorFocus::MENU) ? WHITE : DARKGREY;

    if (drawnMenuIndex != 255 && (drawnMenuIndex != menuIndex || drawnMenuActive != focus))
    {
      display->setTextColor(BLACK);
      display->setCursor(kArrowX, kMenuY + drawnMenuIndex * kMenuRowH);
      display->print("->");
      display->setTextColor(WHITE);
      drawnMenuIndex = 255;
    }
    if (drawnMenuIndex != menuIndex || drawnMenuActive != focus)
    {
      display->setTextColor(arrowColor);
      display->setCursor(kArrowX, kMenuY + menuIndex * kMenuRowH);
      display->print("->");
      display->setTextColor(WHITE);
      drawnMenuIndex = menuIndex;
      drawnMenuActive = focus;
    }
  };

  auto setMessage = [&](const char *msg)
  {
    strncpy(message, msg, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
  };

  auto drawMessageArea = [&]()
  {
    if (!drawnMessageOnce || strcmp(drawnMessage, message) != 0)
    {
      display->fillRect(0, kMsgY - 10, display->width(), kMsgH, BLACK);
      if (message[0] != '\0')
      {
        display->setFont(u8g2_font_6x13_tf);
        display->setTextColor(WHITE);
        drawStringHCentered(message, kMsgY);
      }
      strcpy(drawnMessage, message);
      drawnMessageOnce = true;
    }
  };

  rebuildRows();
  drawNameArea();
  drawCursor();
  drawDeleteFlag();
  drawMenuArrow();
  drawMessageArea();

  int16_t encoderSnap = AlexEncoder::counter;
  bool holdArmed = false;
  uint32_t holdStartMs = 0;
  int16_t holdEncoderSnap = 0;
  bool holdMovedCursor = false;

  while (true)
  {
    if (isApiRequestReceived)
    {
      return false;
    }

    bool down = button.isDown();
    uint32_t now = millis();

    if (down && !holdArmed)
    {
      holdArmed = true;
      holdStartMs = now;
      holdEncoderSnap = AlexEncoder::counter;
      holdMovedCursor = false;
    }
    if (!down)
    {
      holdArmed = false;
    }

    if (holdArmed && focus == EditorFocus::EDIT)
    {
      int16_t heldDelta = AlexEncoder::counter - holdEncoderSnap;
      if (heldDelta != 0)
      {
        int16_t newCursor = (int16_t)cursor + heldDelta;
        if (newCursor < 0)
        {
          newCursor = 0;
        }
        if (newCursor > nameLen)
        {
          newCursor = nameLen;
        }
        if (newCursor != cursor)
        {
          cursor = (uint8_t)newCursor;
          rebuildRows();
          drawNameArea();
          drawCursor();
        }
        holdEncoderSnap = AlexEncoder::counter;
        holdMovedCursor = true;
        encoderSnap = AlexEncoder::counter;
      }
    }

    // Long hold without rotation toggles between the name field and the
    // footer menu (both directions); the menu keeps its position.
    if (holdArmed && !holdMovedCursor && (now - holdStartMs) > kHoldToMenuMs)
    {
      if (focus == EditorFocus::EDIT)
      {
        focus = EditorFocus::MENU;
      }
      else
      {
        focus = EditorFocus::EDIT;
      }
      drawCursor();
      drawMenuArrow();
      holdArmed = false;
      encoderSnap = AlexEncoder::counter;
      button.isClicked();
      button.isHolded();
    }

    int16_t delta = AlexEncoder::counter - encoderSnap;
    if (delta != 0 && !down)
    {
      encoderSnap = AlexEncoder::counter;
      if (focus == EditorFocus::EDIT)
      {
        if (mode == EditMode::INSERT)
        {
          if (cursor == nameLen && nameLen < RECORD_NAME_MAX_LEN)
          {
            name[nameLen] = ' ';
            nameLen++;
            name[nameLen] = '\0';
          }
          if (cursor < nameLen)
          {
            int8_t idx = charsetIndexOf(name[cursor]);
            if (idx < 0)
            {
              idx = 0;
            }
            idx = (idx + delta) % kCharsetLen;
            if (idx < 0)
            {
              idx += kCharsetLen;
            }
            name[cursor] = kCharset[idx];
            name[nameLen] = '\0';
            setMessage("");
            rebuildRows();
            drawNameArea();
            drawCursor();
            drawMessageArea();
          }
        }
      }
      else
      {
        int16_t newMenu = (int16_t)menuIndex + delta;
        if (newMenu < 0)
        {
          newMenu = 0;
        }
        else if (newMenu >= kMenuCount)
        {
          newMenu = kMenuCount - 1;
        }
        if (newMenu != menuIndex)
        {
          menuIndex = (uint8_t)newMenu;
          drawMenuArrow();
        }
      }
    }
    else if (delta != 0)
    {
      encoderSnap = AlexEncoder::counter;
    }

    if (button.isClicked())
    {
      if (holdMovedCursor)
      {
        holdMovedCursor = false;
        encoderSnap = AlexEncoder::counter;
      }
      else if (focus == EditorFocus::EDIT)
      {
        if (mode == EditMode::DELETE_MODE)
        {
          if (cursor > 0 && nameLen > 0)
          {
            uint8_t delPos = cursor - 1;
            if (delPos < nameLen)
            {
              memmove(&name[delPos], &name[delPos + 1], nameLen - delPos);
              nameLen--;
              name[nameLen] = '\0';
            }
            cursor = delPos;
            setMessage("");
            rebuildRows();
            drawNameArea();
            drawCursor();
            drawMessageArea();
          }
        }
        else if (cursor < RECORD_NAME_MAX_LEN)
        {
          // Click accepts the current cell and advances. When the new cursor
          // lands past the typed text, the next char is pulled in: from the
          // suggestion tail when active, otherwise a space (accepts an empty
          // cell, so several spaces in a row can be entered).
          cursor++;
          if (cursor >= nameLen && nameLen < RECORD_NAME_MAX_LEN)
          {
            name[nameLen] = (suggestResultLen > nameLen) ? suggestResult[nameLen].symbol : ' ';
            nameLen++;
            name[nameLen] = '\0';
            cursor = nameLen - 1;
          }
          else if (cursor > nameLen)
          {
            cursor = nameLen;
          }
          rebuildRows();
          drawNameArea();
          drawCursor();
        }
      }
      else
      {
        if (menuIndex == 0)
        {
          if (isRecordNameEmpty(name))
          {
            setMessage("Name is empty");
            drawMessageArea();
          }
          else if (isDuplicate != nullptr && isDuplicate(name))
          {
            setMessage("Name already exists");
            drawMessageArea();
          }
          else
          {
            uint8_t visLen = getRecordNameVisibleLen(name);
            name[visLen] = '\0';
            copyRecordName(nameOut, name);
            confirmed = true;
            break;
          }
        }
        else if (menuIndex == 1)
        {
          break;
        }
        else if (menuIndex == 2)
        {
          // Toggles delete/insert mode and returns to the name field;
          // the menu keeps its position for the next visit. Returning
          // from DELETE happens via the long hold (see above).
          mode = (mode == EditMode::DELETE_MODE) ? EditMode::INSERT : EditMode::DELETE_MODE;
          focus = EditorFocus::EDIT;
          drawCursor();
          drawDeleteFlag();
          drawMenuArrow();
          encoderSnap = AlexEncoder::counter;
        }
      }
    }

    button.isHolded();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  return confirmed;
}

bool drawRecordNameChoiceScreen(const char *defaultName,
                                char chosenName[RECORD_NAME_BUF_SIZE],
                                const char *prefillName,
                                bool (*isDuplicate)(const char *))
{
  char saveAsDefault[RECORD_NAME_BUF_SIZE + 12];
  snprintf(saveAsDefault, sizeof(saveAsDefault), "Save as '%s'", defaultName);

  const char *optsAlive[2] = {saveAsDefault, "Save as..."};

  int16_t choice = drawMessageScreen("Choose name", 2, optsAlive);

  if (isApiRequestReceived)
  {
    return false;
  }

  if (choice == 0)
  {
    copyRecordName(chosenName, defaultName);
    return true;
  }

  if (choice == 1)
  {
    return editRecordName("Enter name:", chosenName, prefillName, isDuplicate);
  }

  return false;
}



