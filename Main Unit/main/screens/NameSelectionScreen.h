#pragma once

#include <vector>
#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "screens/MessageScreen.h"
#include "screens/NameEntryScreen.h"
#include "recordsManagement/RecordsManagementCommon.h"

// Определяет номер записи, который будет использован выбранным вариантом
// сохранения (нужен для показа имени по умолчанию еще до сохранения):
//  - перезапись выбранной записи -> ее номер;
//  - Overwrite oldest/newest -> min/max из существующих номеров;
//  - новая запись -> max номер + 1 (так же нумерует RecordsStorageManager).
template <typename StorageT>
int32_t getSaveActionTargetRecordNumber(StorageT &store, bool overwriteOldest, bool overwriteNewest, int32_t recordNumberToOverwrite)
{
  if (recordNumberToOverwrite > 0)
  {
    return recordNumberToOverwrite;
  }

  std::vector<int32_t> ids = store.getAllValidRecordNumbers();

  if (ids.empty())
  {
    return 1; // первая запись
  }

  int32_t minId = ids[0];
  int32_t maxId = ids[0];

  for (int32_t id : ids)
  {
    if (id < minId)
      minId = id;
    if (id > maxId)
      maxId = id;
  }

  if (overwriteOldest)
    return minId;
  if (overwriteNewest)
    return maxId;
  return maxId + 1;
}

// Страница выбора имени перед сохранением: "Save as '0002'" (имя по умолчанию
// из номера записи) или "Save as..." (посимвольный ввод). Возвращает:
//   0 - выбрано имя по умолчанию (записано в outName);
//   1 - введено свое имя (записано в outName);
//  -1 - поток прерван запросом API (сохранение нужно отменить).
int16_t drawNameSelectionScreen(const char *title, int32_t recordNumber, char outName[RECORD_NAME_BUF_SIZE])
{
  char defaultName[RECORD_NAME_BUF_SIZE];
  formatRecordName(recordNumber, defaultName);

  char firstItem[RECORD_NAME_BUF_SIZE + 14];
  snprintf(firstItem, sizeof(firstItem), "Save as '%s'", defaultName);

  const char *items[] = {firstItem, "Save as..."};

  while (true)
  {
    int16_t selectedIndex = drawMessageScreen(title, 2, items);

    if (isApiRequestReceived)
    {
      return -1;
    }

    if (selectedIndex == 0) // имя по умолчанию из номера записи
    {
      snprintf(outName, RECORD_NAME_BUF_SIZE, "%s", defaultName);
      return 0;
    }

    // "Save as..." - посимвольный ввод имени
    NameEditResult editResult = drawNameEntryScreen("Enter name", "");

    if (isApiRequestReceived)
    {
      return -1;
    }

    if (editResult.confirmed)
    {
      snprintf(outName, RECORD_NAME_BUF_SIZE, "%s", editResult.name);
      return 1;
    }

    // Ввод отменен - возвращаемся к странице выбора (она перерисуется)
  }
}
