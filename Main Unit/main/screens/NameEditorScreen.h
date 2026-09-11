#pragma once

// Character-by-character record name editor with camera-list autocomplete.
//
// Interaction model (all confirmed by the user):
// * Encoder rotation edits the character at the cursor. The charset order is:
//   space, A-Z, a-z, 0-9, then - ( ) + < > { } [ ] / #
// * The first autocomplete match for the current text is shown in DARKGREY
//   right after the typed (WHITE) part. Clicking advances the cursor and the
//   next position inherits the suggested character; typing a different
//   character re-filters the suggestion immediately.
// * Hold + rotation moves the cursor (0..len = text, len = Ok slot, len+1 =
//   Cancel slot). "[Ok] [Cancel]" are drawn below the name.
// * Hold (>700 ms) without rotation toggles delete mode: each click erases the
//   character left of the cursor. In delete mode the cursor blinks as "<",
//   otherwise it blinks as an underscore. Exit delete mode with another hold.
// * Ok with an empty name blinks the name area instead of accepting.
// * "Name already exists" (red, above Ok) blocks Ok while the typed name is a
//   duplicate. excludeRecordNumber lets rename keep its own name.

#include <string.h>
#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/FilmCamerasList.h"
#include "lib/serialAPI.h"

#define NAME_EDITOR_MAX_VISIBLE_ROWS 3

// Charset order: space, A-Z, a-z, 0-9, then symbols from the spec.
static const char NAME_EDITOR_CHARSET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-()+<>{}[]/#";
static const uint8_t NAME_EDITOR_CHARSET_LEN = (uint8_t)(sizeof(NAME_EDITOR_CHARSET) - 1);

#define NAME_EDITOR_HOLD_MS 700
#define NAME_EDITOR_BLINK_MS 500

// Finds the first camera whose prefix matches 'text' (case-sensitive).
// Returns NULL when there is no match or text is empty.
static const char *nameEditorFindSuggestion(const char *text)
{
  if (text == NULL || text[0] == '\0')
  {
    return (FILM_CAMERAS_COUNT > 0) ? FILM_CAMERAS_LIST[0] : NULL;
  }

  size_t len = strlen(text);

  for (int i = 0; i < FILM_CAMERAS_COUNT; i++)
  {
    if (strncmp(FILM_CAMERAS_LIST[i], text, len) == 0)
    {
      return FILM_CAMERAS_LIST[i];
    }
  }

  return NULL;
}

// Index of c inside the editor charset, -1 when absent.
static int8_t nameEditorCharsetIndex(char c)
{
  for (uint8_t i = 0; i < NAME_EDITOR_CHARSET_LEN; i++)
  {
    if (NAME_EDITOR_CHARSET[i] == c)
    {
      return (int8_t)i;
    }
  }
  return -1;
}

struct NameEditorResult
{
  bool confirmed; // true = Ok, false = Cancel (or API abort)
  char name[RECORD_NAME_BUF_SIZE];
};

// Screen cell of the i-th character of the wrapped name area.
static void nameEditorCellPos(int16_t index, int16_t yTop, int16_t *outX, int16_t *outY)
{
  const int16_t xMargin = 8;
  const int16_t rowH = 13;
  int16_t x = xMargin;
  int16_t y = yTop;

  for (int16_t i = 0; i < index; i++)
  {
    x += 6;
    if (x > display->width() - 14)
    {
      x = xMargin;
      y += rowH;
    }
  }

  *outX = x;
  *outY = y;
}

// Draws the name area: typed part in WHITE, suggestion tail in DARKGREY,
// wrapped into rows. Returns the screen Y of the first free row after text.
static int16_t nameEditorDrawText(const char *text, const char *suggestion, size_t suggTailLen, int16_t yTop)
{
  size_t len = strlen(text);
  int16_t x = 0;
  int16_t y = 0;

  display->setTextColor(WHITE);

  for (size_t i = 0; text[i] != '\0'; i++)
  {
    nameEditorCellPos((int16_t)i, yTop, &x, &y);
    display->setCursor(x, y);
    display->print(text[i]);
  }

  display->setTextColor(DARKGREY);

  for (size_t i = 0; i < suggTailLen && suggestion != NULL; i++)
  {
    nameEditorCellPos((int16_t)(len + i), yTop, &x, &y);
    display->setCursor(x, y);
    display->print(suggestion[len + i]);
  }

  display->setTextColor(WHITE);
  return y + 13;
}

// Blinks the whole name area (used for empty-name Ok).
static void nameEditorBlinkNameArea(int16_t yTop, int16_t rows)
{
  for (uint8_t b = 0; b < 3; b++)
  {
    display->fillRect(0, (int16_t)(yTop - 10), display->width(), (int16_t)(rows * 13 + 4), WHITE);
    delay(200);
    display->fillRect(0, (int16_t)(yTop - 10), display->width(), (int16_t)(rows * 13 + 4), BLACK);
    delay(200);
  }
}

// Editor state kept in one struct so the loop body stays readable.
struct NameEditorState
{
  char text[RECORD_NAME_BUF_SIZE];
  int16_t cursor; // 0..len = text slots, len+1 = Ok, len+2 = Cancel
  bool deleteMode;
  bool holdToggleArmed;
  unsigned long holdStartMs;
  int16_t encoderAnchor;
  unsigned long lastBlinkMs;
  bool blinkOn;
};

// Moves the cursor by delta, clamped to 0..len+2.
static void nameEditorMoveCursor(NameEditorState &st, int16_t delta)
{
  int16_t len = (int16_t)strlen(st.text);
  st.cursor += delta;
  if (st.cursor < 0)
  {
    st.cursor = 0;
  }
  if (st.cursor > len + 2)
  {
    st.cursor = len + 2;
  }
}

// Rotates the character at the cursor (or the append slot at len).
static void nameEditorRotateChar(NameEditorState &st, int16_t steps, const char *suggestion, size_t suggTailLen)
{
  size_t len = strlen(st.text);
  if (st.cursor < 0 || st.cursor > (int16_t)len)
  {
    return;
  }
  if (len >= RECORD_NAME_MAX_LEN && st.cursor == (int16_t)len)
  {
    return;
  }

  char *slot = NULL;
  char appendTmp = '\0';
  if (st.cursor == (int16_t)len)
  {
    appendTmp = (suggTailLen > 0) ? suggestion[len] : ' ';
    slot = &appendTmp;
  }
  else
  {
    slot = &st.text[(uint8_t)st.cursor];
  }

  int8_t idx = nameEditorCharsetIndex(*slot);
  if (idx < 0)
  {
    idx = 0;
  }
  int16_t next = (int16_t)idx + steps;
  next %= (int16_t)NAME_EDITOR_CHARSET_LEN;
  if (next < 0)
  {
    next += NAME_EDITOR_CHARSET_LEN;
  }
  *slot = NAME_EDITOR_CHARSET[(uint8_t)next];

  if (st.cursor == (int16_t)len)
  {
    st.text[len] = appendTmp;
    st.text[len + 1] = '\0';
  }
}

// Deletes the character left of the cursor (delete mode click).
static void nameEditorDeleteLeft(NameEditorState &st)
{
  size_t len = strlen(st.text);
  if (st.cursor <= 0 || len == 0)
  {
    return;
  }
  int16_t delPos = (st.cursor > (int16_t)len) ? (int16_t)len - 1 : st.cursor - 1;
  if (delPos < 0)
  {
    return;
  }
  memmove(&st.text[(uint8_t)delPos], &st.text[(uint8_t)delPos + 1], len - (size_t)delPos);
  st.cursor = delPos;
}


// Full editor loop. initialName may be "" (fresh name) or the current name
// (rename flow). isDuplicate() must return true when 'name' is already taken
// (NULL = no duplicate check).
static NameEditorResult drawNameEditorScreen(const char *title, const char *initialName, bool (*isDuplicate)(const char *))
{
  NameEditorResult res;
  memset(&res, 0, sizeof(res));
  res.confirmed = false;

  NameEditorState st;
  memset(&st, 0, sizeof(st));
  if (initialName != NULL)
  {
    strncpy(st.text, initialName, RECORD_NAME_MAX_LEN);
    st.text[RECORD_NAME_MAX_LEN] = '\0';
  }
  st.cursor = (int16_t)strlen(st.text);
  st.deleteMode = false;
  st.holdToggleArmed = false;
  st.holdStartMs = millis();
  st.encoderAnchor = AlexEncoder::counter;
  st.lastBlinkMs = millis();
  st.blinkOn = true;

  // Flush stale clicks from the previous screen.
  while (button.isClicked())
  {
  }

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);

  bool needRedraw = true;
  bool holdWasDown = false;

  while (true)
  {
    if (isApiRequestReceived)
    {
      return res;
    }

    size_t len = strlen(st.text);
    if (st.cursor < 0)
    {
      st.cursor = 0;
    }
    if (st.cursor > (int16_t)len + 2)
    {
      st.cursor = (int16_t)len + 2;
    }

    const char *suggestion = nameEditorFindSuggestion(st.text);
    size_t suggTailLen = 0;
    if (suggestion != NULL)
    {
      size_t suggLen = strlen(suggestion);
      if (suggLen > len)
      {
        suggTailLen = suggLen - len;
      }
    }

    bool onOk = (st.cursor == (int16_t)len + 1);
    bool onCancel = (st.cursor == (int16_t)len + 2);
    bool duplicate = (isDuplicate != NULL) && (len > 0) && isDuplicate(st.text);

    // Hold detection (own timer: AlexButton hold threshold is 500 ms).
    bool isDown = button.isDown();
    unsigned long nowMs = millis();
    if (isDown && !holdWasDown)
    {
      st.holdStartMs = nowMs;
      st.encoderAnchor = AlexEncoder::counter;
      st.holdToggleArmed = false;
    }

    // While physically holding, encoder steps move the cursor.
    if (isDown)
    {
      int16_t heldSteps = AlexEncoder::counter - st.encoderAnchor;
      if (heldSteps != 0)
      {
        nameEditorMoveCursor(st, heldSteps);
        st.encoderAnchor = AlexEncoder::counter;
        // Hold+rotate is cursor movement, not a mode toggle: restart timer.
        st.holdStartMs = nowMs;
        st.holdToggleArmed = false;
        needRedraw = true;
      }
      else if (nowMs - st.holdStartMs >= NAME_EDITOR_HOLD_MS)
      {
        st.holdToggleArmed = true;
      }
    }

    // Release after a pure hold toggles delete mode (consume the release).
    if (!isDown && holdWasDown && st.holdToggleArmed)
    {
      st.deleteMode = !st.deleteMode;
      st.holdToggleArmed = false;
      while (button.isClicked())
      {
      }
      needRedraw = true;
    }
    holdWasDown = isDown;

    // Rotation without hold: edit character at cursor.
    if (!isDown)
    {
      int16_t editSteps = AlexEncoder::counter - st.encoderAnchor;
      if (editSteps != 0)
      {
        if (st.cursor >= 0 && st.cursor <= (int16_t)strlen(st.text))
        {
          nameEditorRotateChar(st, editSteps, suggestion, suggTailLen);
          needRedraw = true;
        }
        st.encoderAnchor = AlexEncoder::counter;
      }
    }

    // Click handling.
    if (button.isClicked())
    {
      if (st.deleteMode)
      {
        nameEditorDeleteLeft(st);
        needRedraw = true;
      }
      else if (onOk)
      {
        if (len == 0)
        {
          nameEditorBlinkNameArea(32, NAME_EDITOR_MAX_VISIBLE_ROWS);
          needRedraw = true;
        }
        else if (!duplicate)
        {
          strncpy(res.name, st.text, RECORD_NAME_MAX_LEN);
          res.name[RECORD_NAME_MAX_LEN] = '\0';
          res.confirmed = true;
          return res;
        }
        else
        {
          // Blocked by duplicate: warning is already visible.
          needRedraw = true;
        }
      }
      else if (onCancel)
      {
        res.confirmed = false;
        return res;
      }
      else
      {
        // Accept current char (typed or suggested) and step right.
        len = strlen(st.text);
        if (st.cursor == (int16_t)len)
        {
          if (len < RECORD_NAME_MAX_LEN)
          {
            st.text[len] = (suggTailLen > 0) ? suggestion[len] : ' ';
            st.text[len + 1] = '\0';
          }
        }
        nameEditorMoveCursor(st, 1);
        needRedraw = true;
      }
    }

    // Cursor blink.
    if (nowMs - st.lastBlinkMs >= NAME_EDITOR_BLINK_MS)
    {
      st.lastBlinkMs = nowMs;
      st.blinkOn = !st.blinkOn;
      needRedraw = true;
    }

    if (needRedraw)
    {
      needRedraw = false;
      len = strlen(st.text);
      onOk = (st.cursor == (int16_t)len + 1);
      onCancel = (st.cursor == (int16_t)len + 2);
      suggestion = nameEditorFindSuggestion(st.text);
      suggTailLen = 0;
      if (suggestion != NULL)
      {
        size_t suggLen = strlen(suggestion);
        if (suggLen > len)
        {
          suggTailLen = suggLen - len;
        }
      }
      duplicate = (isDuplicate != NULL) && (len > 0) && isDuplicate(st.text);

      display->fillScreen(BLACK);
      display->setFont(u8g2_font_6x13_tf);
      drawStringHCentered(title, 12);

      int16_t nameY = 32;
      if (st.deleteMode)
      {
        display->setTextColor(RED);
        drawStringHCentered("DELETE MODE (hold to exit)", 26);
        display->setTextColor(WHITE);
        nameY = 40;
      }

      int16_t afterY = nameEditorDrawText(st.text, suggestion, suggTailLen, nameY);

      // Cursor: underscore in edit mode, "<" in delete mode.
      if (st.blinkOn && !onOk && !onCancel)
      {
        int16_t cx = 0;
        int16_t cy = 0;
        nameEditorCellPos(st.cursor, nameY, &cx, &cy);
        display->setTextColor(st.deleteMode ? RED : WHITE);
        display->setCursor(cx, cy);
        display->print(st.deleteMode ? "<" : "_");
        display->setTextColor(WHITE);
      }

      if (duplicate)
      {
        display->setTextColor(RED);
        drawStringHCentered("Name already exists", afterY + 2);
        display->setTextColor(WHITE);
        afterY += 14;
      }

      int16_t okY = afterY + 4;
      int16_t cancelY = okY + 14;
      display->setCursor(20, okY);
      if (onOk && st.blinkOn)
      {
        display->setTextColor(BLACK, WHITE);
      }
      display->print("Ok");
      display->setTextColor(WHITE, BLACK);

      display->setCursor(20, cancelY);
      if (onCancel && st.blinkOn)
      {
        display->setTextColor(BLACK, WHITE);
      }
      display->print("Cancel");
      display->setTextColor(WHITE, BLACK);
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

