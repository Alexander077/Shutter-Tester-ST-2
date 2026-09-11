#pragma once

#include <stdint.h>
#include <string.h>

// Словарь названий фотокамер для автодополнения при вводе имени записи.
// Встраивается в образ прошивки из файла film_cameras_list.txt в корне
// проекта (см. EMBED_TXTFILES в main/CMakeLists.txt): одна камера на строку.
extern "C" const char _binary_film_cameras_list_txt_start[];
extern "C" const char _binary_film_cameras_list_txt_end[];

struct FilmCamerasHint
{
  const char *line; // первая совпавшая строка внутри словаря
  uint16_t len;     // длина строки без терминатора
};

// Ищет первую строку словаря, начинающуюся с заданного префикса
// (сравнение регистрозависимое). Возвращает false, если совпадений нет.
bool findFilmCameraHint(const char *prefix, uint16_t prefixLen, FilmCamerasHint *out)
{
  if (out == NULL || prefix == NULL || prefixLen == 0)
  {
    return false;
  }

  const char *p = _binary_film_cameras_list_txt_start;
  const char *end = _binary_film_cameras_list_txt_end;

  while (p < end)
  {
    // Границы текущей строки (перенос может быть \n, \r\n или \r)
    const char *lineEnd = p;

    while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r')
    {
      lineEnd++;
    }

    uint16_t lineLen = (uint16_t)(lineEnd - p);

    if (lineLen >= prefixLen && memcmp(p, prefix, prefixLen) == 0)
    {
      out->line = p;
      out->len = lineLen;
      return true;
    }

    // Пропускаем терминаторы строк
    p = lineEnd;

    while (p < end && (*p == '\n' || *p == '\r'))
    {
      p++;
    }
  }

  return false;
}