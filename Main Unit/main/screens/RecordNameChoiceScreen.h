#pragma once

#include <cstdio>
#include "MessageScreen.h"

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