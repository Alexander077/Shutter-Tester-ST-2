#pragma once

#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/FilmCamerasList.h"

// --- Настройки жестов ---
#define NAME_ENTRY_CLICK_MS 300       // нажатие короче этого = клик
#define NAME_ENTRY_HOLD_TOGGLE_MS 700 // нажатие без поворота не короче этого = вход/выход из режима удаления
// Нажатия без поворота длиной между этими порогами игнорируются (мертвая зона)

// Виртуальные позиции курсора за концом строки: слоты подтверждения/отмены,
// в которые курсор уводится удержанием кнопки + поворотом энкодера вправо.
#define NAME_ENTRY_CURSOR_OK_SLOT (-1)
#define NAME_ENTRY_CURSOR_CANCEL_SLOT (-2)

// Алфавит колеса символов (порядок = порядок прокрутки энкодером)
const char NAME_ENTRY_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -()+<>{}[]/#";

// --- Геометрия экрана (шрифт u8g2_font_6x13_tf, 6 px на символ) ---
const int16_t NAME_EDIT_BOX_X = 2;
const int16_t NAME_EDIT_BOX_Y = 34;
const int16_t NAME_EDIT_BOX_W = 124;
const int16_t NAME_EDIT_BOX_H = 24;
const int16_t NAME_EDIT_TEXT_X = 5;   // x первого символа
const int16_t NAME_EDIT_TEXT_Y = 48;  // базовая линия текста (глифы рисуются над ней)
const int16_t NAME_EDIT_VISIBLE_CHARS = 19;
const int16_t NAME_EDIT_SLOTS_Y = 100;
const int16_t NAME_EDIT_MODE_Y = 140;
const uint16_t NAME_EDIT_HINT_COLOR = 0x7BEF; // серый цвет подсказки автодополнения
const uint16_t NAME_EDIT_FRAME_COLOR = DARKGREY;

// Результат работы экрана ввода имени
struct NameEditResult
{
  bool confirmed;                   // true = имя подтверждено (Ok), false = отмена
  char name[RECORD_NAME_BUF_SIZE];  // подтвержденное имя (не пустое при confirmed)
};

/*
 * Экран посимвольного ввода имени записи.
 *
 * Управление (один энкодер + одна кнопка):
 *  - поворот энкодера: прокрутка текущего символа в позиции курсора через алфавит
 *    (подсказка автодополнения из film_cameras_list.txt пересчитывается на лету,
 *    серым текстом после введенной части);
 *  - клик: зафиксировать символ и перейти к следующему (следующий символ
 *    подхватывается из подсказки, но пользователь может его изменить);
 *  - удержание кнопки + поворот: перемещение курсора влево/вправо;
 *    за концом строки располагаются слоты "Ok" и "Cancel";
 *  - удержание без поворота (>=700 мс) с последующим отпусканием: вход в режим
 *    удаления / выход из него. В режиме удаления курсор отображается мигающей
 *    скобкой "<", каждый клик стирает символ слева от курсора; удержание +
 *    поворот так же перемещают курсор;
 *  - "Ok" при пустом имени не подтверждает ввод: строка имени мигает фоном.
 */
NameEditResult drawNameEntryScreen(const char *title, const char *initialName)
{
  NameEditResult result;
  result.confirmed = false;
  result.name[0] = '\0';

  // --- Состояние редактора ---
  char name[RECORD_NAME_BUF_SIZE];
  uint8_t nameLen = 0;

  while (initialName != NULL && initialName[nameLen] != '\0' && nameLen < RECORD_NAME_MAX_LEN)
  {
    name[nameLen] = initialName[nameLen];
    nameLen++;
  }
  name[nameLen] = '\0';

  int16_t cursor = 0;  // 0..nameLen либо NAME_ENTRY_CURSOR_*_SLOT
  bool deleteMode = false;
  char curChar = (nameLen > 0) ? name[0] : NAME_ENTRY_CHARS[0];
  bool curCharEdited = false; // пользователь прокрутил текущий символ энкодером
  int16_t scrollX = 0;

  const char *hint = NULL; // активная подсказка (первое совпадение из словаря)
  uint16_t hintLen = 0;

  const int16_t charsCount = strlen(NAME_ENTRY_CHARS);

  // --- Вспомогательные операции над состоянием ---

  // Подсказка показывается только в режиме ввода на позиции добавления:
  // префикс = зафиксированные символы + текущий (еще не зафиксированный) символ
  auto recomputeHint = [&]()
  {
    hint = NULL;
    hintLen = 0;

    if (deleteMode || cursor != nameLen)
    {
      return;
    }

    char prefix[RECORD_NAME_BUF_SIZE + 1];
    memcpy(prefix, name, nameLen);
    prefix[nameLen] = curChar;

    FilmCamerasHint h;

    if (findFilmCameraHint(prefix, nameLen + 1, &h))
    {
      hint = h.line;
      hintLen = h.len;
    }
  };

  // После перехода на позицию добавления подставляем следующий символ из подсказки
  auto pickCharFromHint = [&]()
  {
    if (deleteMode || cursor != nameLen)
    {
      return;
    }

    FilmCamerasHint h;

    if (findFilmCameraHint(name, nameLen, &h) && h.len > nameLen)
    {
      curChar = h.line[nameLen];
      curCharEdited = false;
    }
  };

  // Мигание строкой имени (фон белеет/чернеет несколько раз)
  auto blinkNameArea = [&]()
  {
    for (uint8_t i = 0; i < 3; i++)
    {
      display->fillRect(NAME_EDIT_BOX_X, NAME_EDIT_BOX_Y, NAME_EDIT_BOX_W, NAME_EDIT_BOX_H, WHITE);
      vTaskDelay(120 / portTICK_PERIOD_MS);
      display->fillRect(NAME_EDIT_BOX_X, NAME_EDIT_BOX_Y, NAME_EDIT_BOX_W, NAME_EDIT_BOX_H, BLACK);
      vTaskDelay(120 / portTICK_PERIOD_MS);
    }
  };

  // Обработка клика. Возвращает true, если редактор завершен.
  auto handleClick = [&]() -> bool
  {
    if (cursor == NAME_ENTRY_CURSOR_OK_SLOT)
    {
      if (nameLen == 0)
      {
        blinkNameArea(); // пустое имя не подтверждается - мигаем строкой имени
        return false;
      }

      memcpy(result.name, name, RECORD_NAME_BUF_SIZE);
      result.confirmed = true;
      return true;
    }

    if (cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT)
    {
      result.confirmed = false;
      return true;
    }

    if (deleteMode)
    {
      // Стираем символ слева от курсора и сдвигаемся на позицию влево
      if (cursor > 0)
      {
        memmove(&name[cursor - 1], &name[cursor], nameLen - cursor);
        nameLen--;
        name[nameLen] = '\0';
        cursor--;
      }
      return false;
    }

    if (cursor < nameLen) // замена существующего символа
    {
      name[cursor] = curChar;
      cursor++;
      curCharEdited = false;

      if (cursor < nameLen)
      {
        curChar = name[cursor];
      }
      else
      {
        pickCharFromHint();
      }
    }
    else // позиция добавления
    {
      if (nameLen < RECORD_NAME_MAX_LEN)
      {
        name[nameLen++] = curChar;
        name[nameLen] = '\0';
        cursor++;
        curCharEdited = false;
        pickCharFromHint();
      }
      else
      {
        cursor = NAME_ENTRY_CURSOR_OK_SLOT; // строка полна - клик завершает ввод
      }
    }

    recomputeHint();
    return false;
  };

  // Шаг курсора вправо (удержание кнопки + поворот энкодера)
  auto moveCursorRight = [&]()
  {
    if (deleteMode)
    {
      if (cursor < nameLen)
      {
        cursor++;
      }
      return;
    }

    if (cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT)
    {
      return;
    }

    if (cursor == NAME_ENTRY_CURSOR_OK_SLOT)
    {
      cursor = NAME_ENTRY_CURSOR_CANCEL_SLOT;
      return;
    }

    if (cursor < nameLen)
    {
      name[cursor] = curChar; // зафиксировать правку символа
      cursor++;
      curCharEdited = false;

      if (cursor < nameLen)
      {
        curChar = name[cursor];
      }
      else
      {
        pickCharFromHint();
      }
    }
    else // из позиции добавления уходим в слот Ok; явно прокрученный
    {    // пользователем символ фиксируем (см. сценарий "Norita 1")
      if (curCharEdited && nameLen < RECORD_NAME_MAX_LEN)
      {
        name[nameLen++] = curChar;
        name[nameLen] = '\0';
        curCharEdited = false;
      }
      cursor = NAME_ENTRY_CURSOR_OK_SLOT;
    }

    recomputeHint();
  };

  // Шаг курсора влево (удержание кнопки + поворот энкодера)
  auto moveCursorLeft = [&]()
  {
    if (deleteMode)
    {
      if (cursor > 0)
      {
        cursor--;
      }
      return;
    }

    if (cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT)
    {
      cursor = NAME_ENTRY_CURSOR_OK_SLOT;
      return;
    }

    if (cursor == NAME_ENTRY_CURSOR_OK_SLOT)
    {
      cursor = nameLen;
      pickCharFromHint();
      recomputeHint();
      return;
    }

    if (cursor == nameLen) // с позиции добавления неподтвержденный символ отбрасываем
    {
      if (nameLen > 0)
      {
        cursor = nameLen - 1;
        curChar = name[cursor];
        curCharEdited = false;
      }
      return;
    }

    if (cursor > 0)
    {
      name[cursor] = curChar; // зафиксировать правку символа
      cursor--;
      curChar = name[cursor];
      curCharEdited = false;
    }

    recomputeHint();
  };

  auto moveCursor = [&](int16_t delta)
  {
    while (delta > 0)
    {
      moveCursorRight();
      delta--;
    }

    while (delta < 0)
    {
      moveCursorLeft();
      delta++;
    }
  };

  // Прокрутка текущего символа через алфавит
  auto cycleChar = [&](int16_t delta)
  {
    int16_t idx = 0;

    for (int16_t i = 0; i < charsCount; i++)
    {
      if (NAME_ENTRY_CHARS[i] == curChar)
      {
        idx = i;
        break;
      }
    }

    idx += delta;

    while (idx < 0)
    {
      idx += charsCount;
    }

    idx %= charsCount;
    curChar = NAME_ENTRY_CHARS[idx];
    curCharEdited = true;
    recomputeHint();
  };

  // Поворот энкодера без нажатой кнопки
  auto handleRotate = [&](int16_t delta)
  {
    if (cursor == NAME_ENTRY_CURSOR_OK_SLOT || cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT)
    {
      // Простым поворотом можно переключаться между слотами Ok/Cancel
      if (delta > 0 && cursor == NAME_ENTRY_CURSOR_OK_SLOT)
      {
        cursor = NAME_ENTRY_CURSOR_CANCEL_SLOT;
      }
      else if (delta < 0 && cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT)
      {
        cursor = NAME_ENTRY_CURSOR_OK_SLOT;
      }
      return;
    }

    if (deleteMode)
    {
      return; // в режиме удаления поворот без удержания ничего не меняет
    }

    cycleChar(delta);
  };

  // Переключение режима удаления (удержание без поворота >=700 мс)
  auto toggleDeleteMode = [&]()
  {
    deleteMode = !deleteMode;

    if (cursor < 0)
    {
      cursor = nameLen; // из слотов Ok/Cancel возвращаемся в конец строки
    }

    if (!deleteMode)
    {
      if (cursor < nameLen)
      {
        curChar = name[cursor];
      }
      else
      {
        pickCharFromHint();
      }

      curCharEdited = false;
      recomputeHint();
    }
  };

  // --- Отрисовка ---

  auto drawCharCell = [&](int16_t pos, char c, uint16_t color, bool inverted)
  {
    if (pos < scrollX || pos >= scrollX + NAME_EDIT_VISIBLE_CHARS)
    {
      return;
    }

    int16_t x = NAME_EDIT_TEXT_X + (pos - scrollX) * 6;

    if (inverted)
    {
      display->fillRect(x - 1, NAME_EDIT_TEXT_Y - 12, 8, 15, WHITE);
      display->setTextColor(BLACK);
    }
    else
    {
      display->setTextColor(color);
    }

    display->setCursor(x, NAME_EDIT_TEXT_Y);
    display->print(c);
    display->setTextColor(WHITE);
  };

  auto drawSlot = [&](const char *text, int16_t x, bool active)
  {
    int16_t w = strlen(text) * 6;

    if (active)
    {
      display->fillRect(x - 2, NAME_EDIT_SLOTS_Y - 12, w + 4, 15, WHITE);
      display->setTextColor(BLACK);
    }
    else
    {
      display->setTextColor(WHITE);
    }

    display->setCursor(x, NAME_EDIT_SLOTS_Y);
    display->print(text);
    display->setTextColor(WHITE);
  };

  auto render = [&](bool blinkOn, bool holdArmed)
  {
    // --- прокрутка окна строки ---
    if (cursor >= 0)
    {
      if (cursor < scrollX)
      {
        scrollX = cursor;
      }

      if (cursor >= scrollX + NAME_EDIT_VISIBLE_CHARS)
      {
        scrollX = cursor - NAME_EDIT_VISIBLE_CHARS + 1;
      }
    }
    else // в слотах Ok/Cancel показываем конец строки
    {
      scrollX = (nameLen > NAME_EDIT_VISIBLE_CHARS) ? (nameLen - NAME_EDIT_VISIBLE_CHARS) : 0;
    }

    if (scrollX < 0)
    {
      scrollX = 0;
    }

    // --- строка имени ---
    display->fillRect(NAME_EDIT_BOX_X, NAME_EDIT_BOX_Y, NAME_EDIT_BOX_W, NAME_EDIT_BOX_H, BLACK);
    display->drawRect(NAME_EDIT_BOX_X, NAME_EDIT_BOX_Y, NAME_EDIT_BOX_W, NAME_EDIT_BOX_H, NAME_EDIT_FRAME_COLOR);

    for (int16_t i = scrollX; i < nameLen && i < scrollX + NAME_EDIT_VISIBLE_CHARS; i++)
    {
      drawCharCell(i, name[i], WHITE, false);
    }

    if (deleteMode)
    {
      // Курсор режима удаления - мигающая скобка "<"
      if (blinkOn && cursor >= 0)
      {
        drawCharCell(cursor, '<', WHITE, true);
      }
    }
    else if (cursor >= 0)
    {
      // Редактируемый символ - инверсией
      drawCharCell(cursor, curChar, WHITE, true);

      // Подсказка автодополнения - серым продолжением (только на позиции добавления)
      if (cursor == nameLen && hint != NULL)
      {
        for (uint16_t j = cursor + 1; j < hintLen; j++)
        {
          drawCharCell((int16_t)j, hint[j], NAME_EDIT_HINT_COLOR, false);
        }
      }
    }

    // --- слоты Ok / Cancel ---
    drawSlot("Ok", 24, cursor == NAME_ENTRY_CURSOR_OK_SLOT);
    drawSlot("Cancel", 70, cursor == NAME_ENTRY_CURSOR_CANCEL_SLOT);

    // --- строка режима и счетчик длины ---
    char counterBuf[12];
    snprintf(counterBuf, sizeof(counterBuf), "%u/%u", (unsigned)nameLen, (unsigned)RECORD_NAME_MAX_LEN);
    display->setTextColor(NAME_EDIT_HINT_COLOR);
    drawStringRightAligned(counterBuf, 126, NAME_EDIT_MODE_Y);

    const char *modeStr = deleteMode ? "Del" : "Type";
    int16_t modeW = strlen(modeStr) * 6;

    if (holdArmed && blinkOn)
    {
      // Обратная связь: удержание без поворота уже достаточно длинное для
      // переключения режима удаления - мигаем индикатором режима
      display->fillRect(3, NAME_EDIT_MODE_Y - 12, modeW + 2, 15, WHITE);
      display->setTextColor(BLACK);
    }
    else
    {
      display->setTextColor(deleteMode ? WHITE : NAME_EDIT_HINT_COLOR);
    }

    display->setCursor(4, NAME_EDIT_MODE_Y);
    display->print(modeStr);
    display->setTextColor(WHITE);
  };

  // --- Отрисовка статичных элементов ---
  display->fillScreen(BLACK);
  drawStringHCentered(title, 20);

  // --- Машина жестов ---
  bool prevDown = false;
  uint32_t pressStartMs = 0;
  int16_t plainEncoderVal = AlexEncoder::counter;
  int16_t holdEncoderVal = AlexEncoder::counter;
  bool rotatedDuringPress = false;

  uint8_t lastBlinkPhase = 255;
  bool dirty = true;
  bool running = true;

  recomputeHint();

  while (running)
  {
    if (isApiRequestReceived)
    {
      result.confirmed = false;
      result.name[0] = '\0';
      break;
    }

    // Сбрасываем накопленные события AlexButton: жесты распознаются ниже вручную
    while (button.isClicked()) {}
    if (button.isHolded()) {}

    bool down = button.isDownStable();
    uint32_t now = millis();

    if (down && !prevDown) // начало нажатия
    {
      pressStartMs = now;
      holdEncoderVal = AlexEncoder::counter;
      plainEncoderVal = AlexEncoder::counter;
      rotatedDuringPress = false;
    }

    if (!down && prevDown) // отпускание кнопки
    {
      plainEncoderVal = AlexEncoder::counter;
      uint32_t pressDuration = now - pressStartMs;

      if (!rotatedDuringPress)
      {
        if (pressDuration < NAME_ENTRY_CLICK_MS)
        {
          if (handleClick())
          {
            running = false;
          }

          // После возможного мигания строки сбрасываем накопленные повороты
          plainEncoderVal = AlexEncoder::counter;
        }
        else if (pressDuration >= NAME_ENTRY_HOLD_TOGGLE_MS)
        {
          toggleDeleteMode(); // вход/выход из режима удаления
        }
        // Нажатия 300..700 мс без поворота игнорируются (мертвая зона)
      }

      dirty = true;
    }

    if (down)
    {
      if (AlexEncoder::counter != holdEncoderVal) // удержание + поворот = движение курсора
      {
        int16_t delta = AlexEncoder::counter - holdEncoderVal;
        holdEncoderVal = AlexEncoder::counter;
        rotatedDuringPress = true;
        moveCursor(delta);
        dirty = true;
      }
    }
    else
    {
      if (AlexEncoder::counter != plainEncoderVal) // обычный поворот
      {
        int16_t delta = AlexEncoder::counter - plainEncoderVal;
        plainEncoderVal = AlexEncoder::counter;
        handleRotate(delta);
        dirty = true;
      }
    }

    prevDown = down;

    bool holdArmed = down && !rotatedDuringPress && (now - pressStartMs >= NAME_ENTRY_HOLD_TOGGLE_MS);
    uint8_t blinkPhase = (now / 350) % 2;

    if (dirty || blinkPhase != lastBlinkPhase)
    {
      render(blinkPhase == 0, holdArmed);
      dirty = false;
      lastBlinkPhase = blinkPhase;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  // Чтобы отложенные клики/удержания не "протекли" в следующие экраны
  while (button.isClicked()) {}
  if (button.isHolded()) {}

  return result;
}
