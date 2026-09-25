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

// True when a hint is allowed to start at 'index': only at the beginning of a
// word (string start or right after a non-alphanumeric symbol). Without this
// rule an empty cell in the typed text lets a hint match in the middle of a
// word (e.g. "for C" would match "Rec(taflex)").
static bool isSuggestStartAllowed(const char *currentString, uint8_t index)
{
  if (index == 0)
  {
    return true;
  }

  char prev = currentString[index - 1];
  bool isPrevAlnum = (prev >= 'A' && prev <= 'Z') ||
                     (prev >= 'a' && prev <= 'z') ||
                     (prev >= '0' && prev <= '9');

  return !isPrevAlnum;
}

// Number of hint symbols that are written into the line when the hint matches
// at 'index' (0 = the hint does not match here). A space in the typed text is
// an empty cell that any hint symbol fills, the symbols behind the typed text
// are the grey suggestion tail; the part of the hint that does not fit into
// the name limit is cut off.
static uint8_t matchHintAt(const char *hint, const char *currentString,
                           uint8_t typedLen, uint8_t index)
{
  if (index >= typedLen || currentString[index] == ' ')
  {
    return 0; // a hint never starts on an empty cell
  }

  // Full hint length that still fits into the name limit (the tail behind the
  // typed text is written as well, so it is not compared).
  uint8_t room = RECORD_NAME_MAX_LEN - index;
  uint8_t hintLen = 0;
  while (hint[hintLen] != '\0' && hintLen < room)
  {
    hintLen++;
  }

  // Only the symbols covered by the typed text have to agree.
  uint8_t typedRoom = typedLen - index;
  uint8_t checkLen = (hintLen < typedRoom) ? hintLen : typedRoom;

  for (uint8_t j = 0; j < checkLen; j++)
  {
    if (currentString[index + j] != ' ' &&
        recordNameToLowerChar(currentString[index + j]) != recordNameToLowerChar(hint[j]))
    {
      return 0; // does not match, the caller tries the next position
    }
  }

  return hintLen;
}

// Builds the suggested line for the currently typed string.
void buildSuggestString(const char *currentString, SuggestStringSymbol *out)
{
  uint8_t typedLen = 0;
  while (typedLen < RECORD_NAME_MAX_LEN && currentString[typedLen] != '\0')
  {
    typedLen++;
  }

  uint8_t i = 0;
  while (i < typedLen)
  {
    const char *hint = nullptr;
    uint8_t hintLen = 0;

    if (isSuggestStartAllowed(currentString, i))
    {
      for (int camera = 0; camera < FILM_CAMERAS_COUNT; camera++)
      {
        uint8_t len = matchHintAt(FilmCamerasList[camera], currentString, typedLen, i);
        if (len > 0)
        {
          hint = FilmCamerasList[camera];
          hintLen = len;
          break; // the first matching camera wins
        }
      }
    }

    if (hint == nullptr)
    {
      // No suggestion here: the typed symbol stays as it is.
      out[i].symbol = currentString[i];
      out[i].isPartOfSuggest = false;
      i++;
      continue;
    }

    for (uint8_t j = 0; j < hintLen; j++)
    {
      uint8_t pos = i + j;

      if (pos < typedLen && currentString[pos] != ' ')
      {
        // A symbol typed by the user is never replaced (only its case may
        // differ from the camera name).
        out[pos].symbol = currentString[pos];
        out[pos].isPartOfSuggest = false;
      }
      else if (pos < typedLen && hint[j] == ' ')
      {
        // An empty cell that the hint leaves empty as well.
        out[pos].symbol = ' ';
        out[pos].isPartOfSuggest = false;
      }
      else
      {
        // An empty cell filled by the hint or the tail behind the typed text.
        out[pos].symbol = hint[j];
        out[pos].isPartOfSuggest = true;
      }
    }

    i += hintLen;
  }

  out[i].symbol = '\0';
  out[i].isPartOfSuggest = false;
}

// Record name editor: character-by-character entry with
// camera-name autocomplete, delete mode and Ok/Cancel/Delete Mode menu.
// Redraws only dirty areas (name cell, message line, menu arrow).
// Both cursors stay visible: white = active, grey = inactive. Footer menu
// position is preserved across focus switches; the long hold toggles focus
// both ways (no "Back To Edit" item).
// Delete mode erases on the encoder rotation (to the left - symbols left of
// the cursor, to the right - the symbol at the cursor, one per detent)
// The cursor (hold + rotation) travels over the whole suggested line, the
// grey suggestion tail included
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

  // Layout: the whole 40-symbol name is always visible as two centred rows of
  // 20 symbols (u8g2_font_6x13_tf is 6px wide -> a 120px field with 20px
  // margins on the 160px wide display), no scrolling needed.
  static const uint8_t kNameCharsPerRow = 20;
  static const uint8_t kNameRows = 2;
  static const uint8_t kNameCharW = 6; // u8g2_font_6x13_tf advance
  static const uint8_t kNameX = (160 - kNameCharsPerRow * kNameCharW) / 2;
  static const int16_t kTitleY = 12;
  static const int16_t kNameY0 = 28;
  static const int16_t kNameY1 = 43;
  static const int16_t kNameCellH = 12; // height of one symbol cell
  static const int16_t kMsgY = 60;
  static const int16_t kMsgH = 12;
  static const int16_t kMenuY = 76;
  static const int16_t kMenuRowH = 12;
  static const uint8_t kMenuX = 20;
  static const uint8_t kArrowX = 8;
  static const uint8_t kNameW = kNameCharsPerRow * kNameCharW;
  static const uint16_t kHoldToMenuMs = 700;
  // A released pin read shorter than this is treated as contact bouncing: the
  // hold continues, so a rotation in the middle of a press never turns into a
  // symbol change.
  static const uint32_t kHoldReleaseGraceMs = 20;

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

  // Visible rows: committed text + suggestion symbols, with a bit per column
  // telling whether the symbol came from a hint (grey) or from the user
  // (white). A hint may fill single empty cells inside the typed text, so the
  // colour cannot be described by a single "grey tail starts here" column.
  char rowText[kNameRows][kNameCharsPerRow + 1];
  uint32_t rowSuggMask[kNameRows];

  auto rebuildRows = [&]()
  {
    for (uint8_t r = 0; r < kNameRows; r++)
    {
      rowText[r][0] = '\0';
      rowSuggMask[r] = 0;
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

      if (suggestResult[pos].isPartOfSuggest)
      {
        rowSuggMask[r] |= (1UL << c);
      }
    }
  };

  // Takes the live suggestion into the typed text up to and including the
  // cursor: the cells behind the typed text are filled with the symbols shown
  // in the suggestion line (a space when the suggestion is over), so a cursor
  // placed inside the grey suggestion tail becomes an editable position of the
  // name. The cursor is clamped when the name limit is reached.
  auto acceptSuggestUpToCursor = [&]()
  {
    while (nameLen <= cursor && nameLen < RECORD_NAME_MAX_LEN)
    {
      name[nameLen] = (nameLen < suggestResultLen) ? suggestResult[nameLen].symbol : ' ';
      nameLen++;
    }

    name[nameLen] = '\0';

    if (cursor > nameLen)
    {
      cursor = nameLen;
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
  uint32_t drawnSuggMask[kNameRows] = {0};
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

      if (strcmp(drawnRows[r], rowText[r]) != 0 || drawnSuggMask[r] != rowSuggMask[r])
      {
        display->fillRect(kNameX, rowY - 10, kNameW, kNameCellH, BLACK);

        // Cursors of the edge positions live in the margins beside the field,
        // out of the row rect: wipe them as well, otherwise they would stay
        // behind. (The delete-mode bar of the first symbol sits left of the
        // field, the end cursor of a full 40-symbol name right of it.)
        display->fillRect(kNameX - 1, rowY - 10, 1, kNameCellH, BLACK);

        if (r == kNameRows - 1)
        {
          display->fillRect(kNameX + kNameW, rowY - 10, kNameCharW, kNameCellH, BLACK);
        }

        display->setCursor(kNameX, rowY);

        // Colour is chosen per cell: symbols the user typed (including his own
        // spaces) are white, symbols that came from a hint - an empty cell the
        // hint filled or the grey tail behind the typed text - are grey.
        for (uint8_t i = 0; i < kNameCharsPerRow && rowText[r][i] != '\0'; i++)
        {
          bool isSugg = (rowSuggMask[r] & (1UL << i)) != 0;
          display->setTextColor(isSugg ? DARKGREY : WHITE);
          display->print(rowText[r][i]);
        }

        display->setTextColor(WHITE);
        strcpy(drawnRows[r], rowText[r]);
        drawnSuggMask[r] = rowSuggMask[r];

        // The drawn cursor mark is wiped only when this redraw touched the row
        // it stands in (the end cursor of a full name lives in the right margin
        // of the last row). A redraw of the other row leaves the mark on screen,
        // so the cursor cache must stay valid: the erase step then repaints that
        // cell identically (no flicker).
        uint8_t markRow = (drawnCursor >= kNameCharsPerRow * kNameRows)
                              ? (uint8_t)(kNameRows - 1)
                              : (uint8_t)(drawnCursor / kNameCharsPerRow);

        if (drawnCursor != 255 && markRow == r)
        {
          drawnCursor = 255;
        }
      }
    }
  };

  // Cell geometry of a name cursor position. Positions 0..39 sit inside the two
  // name rows; the end position of a full 40-symbol name is drawn in the right
  // margin behind the field, so the cursor never stays invisible.
  struct CursorPos
  {
    int16_t rowY;  // baseline of the row the cursor belongs to
    int16_t cellX; // left edge of the cell (or of the margin position)
    int16_t barX;  // 1px column of the delete-mode bar, in the gap left of the cell
    uint8_t row;   // name row (kNameRows = the margin position)
    uint8_t col;   // name column
    bool inRow;    // false = the margin position behind the field
  };

  auto getCursorPos = [&](uint8_t pos) -> CursorPos
  {
    CursorPos p;

    if (pos < kNameCharsPerRow * kNameRows)
    {
      p.row = pos / kNameCharsPerRow;
      p.col = pos % kNameCharsPerRow;
      p.inRow = true;
    }
    else
    {
      p.row = kNameRows;
      p.col = kNameCharsPerRow;
      p.inRow = false;
    }

    p.rowY = (p.row == 0) ? kNameY0 : kNameY1;
    p.cellX = kNameX + p.col * kNameCharW;
    // The bar stands between the symbols: in the 1px gap left of its cell (in
    // the left margin for the very first symbol, so no glyph gets clipped).
    p.barX = (p.col == 0) ? (int16_t)(kNameX - 1) : (int16_t)(p.cellX - 1);
    return p;
  };

  // Draws the symbol of one cell again (grey when it comes from a hint); the
  // caller blanks the cell first.
  auto drawCellSymbol = [&](uint8_t row, uint8_t col)
  {
    if (row >= kNameRows || col >= kNameCharsPerRow || rowText[row][col] == '\0')
    {
      return; // the suggested line ends before this cell
    }

    int16_t rowY = (row == 0) ? kNameY0 : kNameY1;

    display->setFont(u8g2_font_6x13_tf);
    display->setCursor(kNameX + col * kNameCharW, rowY);
    bool isSugg = (rowSuggMask[row] & (1UL << col)) != 0;
    display->setTextColor(isSugg ? DARKGREY : WHITE);
    display->print(rowText[row][col]);
    display->setTextColor(WHITE);
  };

  // Blanks a cell and restores the symbol shown in it; used to erase the cursor
  // without redrawing the whole row (no flicker).
  auto restoreCell = [&](const CursorPos &p)
  {
    display->fillRect(p.cellX, p.rowY - 10, kNameCharW, kNameCellH, BLACK);

    if (p.inRow)
    {
      drawCellSymbol(p.row, p.col);
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
      CursorPos prev = getCursorPos(drawnCursor);

      if (drawnMode == EditMode::DELETE_MODE)
      {
        // The bar stands in the 1px gap left of its cell (in the left margin
        // for the first cell): erase the bar and redraw the clipped cell.
        display->fillRect(prev.barX, prev.rowY - 10, 1, kNameCellH, BLACK);

        if (prev.col > 0 && prev.col - 1 < kNameCharsPerRow)
        {
          uint8_t clippedCol = (uint8_t)(prev.col - 1);
          uint8_t clippedRow = (prev.row < kNameRows) ? prev.row : (uint8_t)(kNameRows - 1);

          display->fillRect(kNameX + clippedCol * kNameCharW, prev.rowY - 10,
                            kNameCharW, kNameCellH, BLACK);
          drawCellSymbol(clippedRow, clippedCol);
        }
      }
      else
      {
        restoreCell(prev);
      }

      drawnCursor = 255;
    }

    if (drawnCursor != cursor || drawnFocus != focus || drawnMode != mode)
    {
      CursorPos cur = getCursorPos(cursor);

      if (mode == EditMode::INSERT)
      {
        // The cell shows the symbol actually drawn in it: a suggestion symbol
        // stays grey (it is not the user's own symbol yet, a click accepts
        // it), symbols the user typed are drawn in the cursor colour. The
        // cursor itself is the line under the symbol.
        display->fillRect(cur.cellX, cur.rowY - 10, kNameCharW, kNameCellH, BLACK);
        display->setCursor(cur.cellX, cur.rowY);

        char ch = ' ';
        bool isSuggCell = false;

        if (cur.inRow && rowText[cur.row][cur.col] != '\0')
        {
          ch = rowText[cur.row][cur.col];
          isSuggCell = (cursor < suggestResultLen) && suggestResult[cursor].isPartOfSuggest;
        }
        else if (cursor < nameLen)
        {
          ch = name[cursor];
        }

        display->setTextColor(isSuggCell ? DARKGREY : cursorColor);
        display->print(ch);
        display->drawLine(cur.cellX, cur.rowY + 1, cur.cellX + kNameCharW - 1,
                          cur.rowY + 1, cursorColor);
      }
      else
      {
        // Delete mode: the symbols keep their own colour and the cursor is a
        // 1px vertical bar standing between the symbols.
        display->fillRect(cur.barX, cur.rowY - 10, 1, kNameCellH, BLACK);
        display->fillRect(cur.barX, cur.rowY - 10, 1, kNameCellH, cursorColor);
      }

      drawnCursor = cursor;
      drawnFocus = focus;
      drawnMode = mode;
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

  // Moves the cursor by the encoder delta (in detents). The cursor travels over
  // the whole suggested line - the typed text and the grey suggestion tail
  // behind it - so a space inside a hint does not stop the movement. In DELETE
  // mode only the typed text can be reached: the tail holds no symbols of the
  // name which could be erased. The cursor always stays inside [0, limit].
  auto moveCursorByDelta = [&](int16_t delta)
  {
    int16_t moveLimit = (mode == EditMode::INSERT) ? (int16_t)suggestResultLen
                                                   : (int16_t)nameLen;
    int16_t newCursor = (int16_t)cursor + delta;

    if (newCursor < 0)
    {
      newCursor = 0;
    }
    if (newCursor > moveLimit)
    {
      newCursor = moveLimit;
    }

    if (newCursor != cursor)
    {
      cursor = (uint8_t)newCursor;
      rebuildRows();
      drawNameArea();
      drawCursor();
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
  uint32_t lastDownMs = 0;
  bool holdMovedCursor = false;

  while (true)
  {
    if (isApiRequestReceived)
    {
      return false;
    }

    bool down = button.isDown();
    uint32_t now = millis();

    if (down)
    {
      lastDownMs = now;

      if (!holdArmed)
      {
        // Press edge: the long hold starts here and a click is still possible.
        holdArmed = true;
        holdStartMs = now;
        holdMovedCursor = false;
      }
    }
    else if (holdArmed && (now - lastDownMs) > kHoldReleaseGraceMs)
    {
      holdArmed = false;
    }

    // Long hold without rotation toggles between the name field and the
    // footer menu (both directions); the menu keeps its position. Moving to the
    // menu always switches delete mode off, so "Delete Mode [Off]" is shown
    // while the menu owns the focus.
    if (holdArmed && !holdMovedCursor && (now - holdStartMs) > kHoldToMenuMs)
    {
      if (focus == EditorFocus::EDIT)
      {
        focus = EditorFocus::MENU;

        if (mode == EditMode::DELETE_MODE)
        {
          mode = EditMode::INSERT;
          drawDeleteFlag();
        }
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
    if (delta != 0)
    {
      // The detent is consumed here: a clamped cursor movement must not leave a
      // pending delta behind for the next click.
      encoderSnap = AlexEncoder::counter;

      if (focus == EditorFocus::EDIT)
      {
        if (down || holdArmed)
        {
          // While the button is held a detent always moves the cursor and never
          // changes a symbol. The pin state is read directly, so a detent which
          // the encoder committed just before the press was noticed is still
          // applied instead of being dropped. The movement is remembered so the
          // release is not mistaken for a click.
          moveCursorByDelta(delta);
          holdMovedCursor = true;
        }
        else if (mode == EditMode::INSERT)
        {
          // A cell which is shown as a grey suggestion symbol is materialised
          // first: the symbols the cursor passed and the symbol under it become
          // the user's own, so the charset goes on from the symbol he actually
          // sees instead of jumping to the start of the charset.
          bool suggestionTaken = false;

          if (cursor >= nameLen && cursor < suggestResultLen &&
              suggestResult[cursor].isPartOfSuggest)
          {
            acceptSuggestUpToCursor();
            suggestionTaken = true;
          }
          else if (cursor == nameLen && nameLen < RECORD_NAME_MAX_LEN)
          {
            // No suggestion here: a fresh empty cell, so a space can be typed.
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
          else if (suggestionTaken)
          {
            // The name limit was reached while taking the suggestion: there is
            // no cell to cycle, only the taken symbols have to be shown.
            setMessage("");
            rebuildRows();
            drawNameArea();
            drawCursor();
            drawMessageArea();
          }
        }
        else
        {
          // DELETE mode: the encoder rotation erases symbols - to the left
          // erases one symbol left of the cursor per detent (the cursor
          // follows to the left), to the right - the symbol at the cursor per
          // detent (the next symbol slides under the cursor). A click leaves
          // the mode; moving the cursor stays on hold + rotation, which
          // consumes the rotation before this point.
          if (delta > 0)
          {
            for (int32_t k = 0; k < (int32_t)delta; k++)
            {
              if (cursor >= nameLen || nameLen == 0)
              {
                break;
              }

              memmove(&name[cursor], &name[cursor + 1], nameLen - cursor);
              nameLen--;
              name[nameLen] = '\0';
            }
          }
          else
          {
            for (int32_t k = 0; k < -(int32_t)delta; k++)
            {
              if (cursor == 0 || nameLen == 0)
              {
                break;
              }

              uint8_t delPos = cursor - 1;
              if (delPos >= nameLen)
              {
                break;
              }

              memmove(&name[delPos], &name[delPos + 1], nameLen - delPos);
              nameLen--;
              name[nameLen] = '\0';
              cursor = delPos;
            }
          }

          setMessage("");
          rebuildRows();
          drawNameArea();
          drawCursor();
          drawMessageArea();
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

    if (button.isClicked())
    {
      if (holdMovedCursor)
      {
        holdMovedCursor = false;
        encoderSnap = AlexEncoder::counter;
      }
      // In DELETE mode erasing is done with the encoder rotation (see above)
      // and a single click leaves the mode back to INSERT (Delete Mode [Off]);
      // moving the cursor stays on the long hold.
      else if (focus == EditorFocus::EDIT && mode == EditMode::DELETE_MODE)
      {
        mode = EditMode::INSERT;
        drawCursor();
        drawDeleteFlag();
        drawMenuArrow();
        encoderSnap = AlexEncoder::counter;
      }
      else if (focus == EditorFocus::EDIT)
      {
        if (cursor < RECORD_NAME_MAX_LEN)
        {
          // Click accepts the suggestion under the cursor: a grey symbol in an
          // empty cell inside the typed text, or the whole grey tail up to the
          // cursor when it was moved into it by hold + rotation (so the saved
          // name matches the line shown on the display). When there is nothing
          // to accept, the cursor moves on; when it lands past the typed text,
          // the next char is pulled in: from the suggestion tail when active,
          // otherwise a space (accepts an empty cell, so several spaces in a
          // row can be entered).
          if (cursor > nameLen)
          {
            acceptSuggestUpToCursor();
          }
          else if (cursor < nameLen && suggestResult[cursor].isPartOfSuggest &&
                   suggestResult[cursor].symbol != name[cursor])
          {
            name[cursor] = suggestResult[cursor].symbol;
          }
          else
          {
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

            // The cell the cursor landed on may hold a suggestion symbol as
            // well (the hint filled an empty cell inside the typed text).
            if (cursor < nameLen && suggestResult[cursor].isPartOfSuggest &&
                suggestResult[cursor].symbol != name[cursor])
            {
              name[cursor] = suggestResult[cursor].symbol;
            }
          }
          // A click on the field accepts the suggestion and clears a pending
          // "Name is empty" / "Name already exists" message.
          setMessage("");
          rebuildRows();
          drawNameArea();
          drawCursor();
          drawMessageArea();
        }
      }
      else
      {
        if (menuIndex == 0)
        {
          // Empty cells inside the typed text that the suggestion filled are
          // applied now, so the saved name matches the line the user saw (the
          // grey tail behind the typed text stays a hint only).
          for (uint8_t pos = 0; pos < nameLen; pos++)
          {
            if (suggestResult[pos].isPartOfSuggest && suggestResult[pos].symbol != name[pos])
            {
              name[pos] = suggestResult[pos].symbol;
            }
          }

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
          // Toggles delete/insert mode and returns to the name field
          // ("Delete Mode [Off]" -> "[On]" and back); the menu keeps its
          // position for the next visit. A pending message is dropped as soon
          // as editing is resumed.
          setMessage("");
          mode = (mode == EditMode::DELETE_MODE) ? EditMode::INSERT : EditMode::DELETE_MODE;
          focus = EditorFocus::EDIT;
          // Erasing works on the typed text only: a cursor which stood inside
          // the grey suggestion tail steps back to the end of the name.
          if (cursor > nameLen)
          {
            cursor = nameLen;
          }
          drawCursor();
          drawDeleteFlag();
          drawMenuArrow();
          drawMessageArea();
          encoderSnap = AlexEncoder::counter;
        }
      }
    }

    button.isHolded();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }

  return confirmed;
}