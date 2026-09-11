## 1. Суть проекта

Это прошивка для платы ESP32, на которой построен прибор для измерения выдержки плёночных фотоаппаратов (Shutter Tester ST-2). Используется ESP-IDF 5.4.0 с компонентом Arduino-ESP32.

Прибор регистрирует импульсы света от двух фотодатчиков при срабатывании затвора, оцифровывает их через continuous ADC, рассчитывает времена срабатывания шторок, их скорости и ширину щели, отображает результаты на TFT-дисплее и позволяет сохранять/просматривать измерения, а также управляться по последовательному JSON API (включая OTA-обновление прошивки).

## 2. Примечание для LLM моделей
* Всегда в чате отвечай на русском
* ESP IDF лежит в C:\Users\Alex\esp\v5.4\esp-idf\
* Команда для сборки проекта: `cmd /c "C:\Users\Alex\esp\v5.4\esp-idf\export.bat && cd /d "d:\Programming\Shutter Tester ST-2\Main Unit" && idf.py build 2>&1"`
* При изменениях затрагивающих функционал, который описан в этом файле, нужно проапдейтить данный файл (.clinerules\ai-rules.md), чтобы он был in sync с проектом.

## 3. Структура и назначение файлов

### Корень проекта
* `CMakeLists.txt` — корневой конфиг сборки: подключает `my_components` через `EXTRA_COMPONENT_DIRS`, объявляет проект `main`.
* `film_cameras_list.txt` — словарь названий плёночных фотоаппаратов (~1160 строк, одна камера на строку) для автодополнения при вводе имени записи. Встраивается в прошивку через `EMBED_TXTFILES` (см. `main/CMakeLists.txt`); обновление списка = правка файла + пересборка.
* `partitions.csv` — таблица разделов: `nvs`, `nvs_keys` (encrypted), `otadata`, `phy_init`, `ota_0`, `ota_1` и `storage` (LittleFS).
* `sdkconfig`, `sdkconfig.defaults`, `sdkconfig.old` — конфигурация ESP-IDF. В `sdkconfig.defaults`: автозапуск Arduino, FreeRTOS 1000 Гц, кастомная таблица разделов (смещение `0xa000`, MD5), размер флеша 4MB.
* `dependencies.lock` — lock-файл зависимостей (managed components).
* `.gitignore` — игнорирует `.vscode/`, `build/`, `managed_components/`.
* `secure workflow.txt` — заметки/команды для защищённой прошивки: генерация ключа шифрования флеша, прожиг eFuse, шифрование bootloader/partition/firmware через `espsecure.py` и запись через `esptool.py`. Файл устарел и не используется.

### Каталоги
* `main/` — основной код приложения (исходники прошивки).
* `my_components/` — локальные сторонние компоненты/библиотеки.
* `managed_components/` — компоненты, подтянутые менеджером зависимостей (не редактировать вручную).
* `build/` — сборочный каталог (генерируется).
* `ai/` — текстовые заметки/задания (`ai task 1.txt` … `ai task 5.txt`), не являются частью прошивки. Папка устарела и не используется.
* `.clinerules/` — правила для ИИ (этот файл).

### main/ (корень компонента)
* `main.cpp` — точка входа. `setup()` инициализирует дисплей, энкодер, таймер опроса кнопки, АЦП, LittleFS-хранилище (создаёт `records.bin` и `seriesRecords.bin`), серийный номер в NVS, мьютекс последовательного вывода, запускает задачу `serialApiTask` на ядре 0 и валидирует прошивку (`esp_ota_mark_app_valid_cancel_rollback`). `loop()` содержит `drawMainMenu()`.
* `CMakeLists.txt` — регистрирует компонент `main` (исходник `main.cpp`, `INCLUDE_DIRS "."`), встраивает `../film_cameras_list.txt` в образ прошивки через `EMBED_TXTFILES` (символы `_binary_film_cameras_list_txt_start/end`).
* `idf_component.yml` — зависимости: `espressif/arduino-esp32` (pre-release) и `joltwallet/littlefs`.

### main/lib/
* `Common.h` — общие константы и пины (`TEST_PIN_1/2`, `ENCODER_B_PIN`, `ENCODER_A_PIN`, `BUTTON_PIN`), пороги уровней сигнала, `RECORD_NAME_MAX_LEN 40` / `RECORD_NAME_BUF_SIZE` (макс. длина имени записи), enum'ы (`SensorType`, `MainMenuItems`, `CurtainMovement`, `MeasurementSaveMenuItems`, `ZeroRecordsMeasurementSaveMenuItems`), глобальные объекты (`button`, `curtainMovement`, `curSensorIndex`), ISR `onTimer()`, функции `formatRecordName()` и `halt()`.
* `Display.h` — инициализация TFT-дисплея ST7735 128×160 через Arduino_GFX (`Arduino_HWSPI`), назначение SPI-пинов.
* `ADC.h` — настройка ADC continuous driver (2 канала `ADC_CHANNEL_6/7`, частота 1 МГц, `READ_LEN 4096`), `initADC()`, callback `s_conv_done_cb()`.
* `AlexButton.h` — класс кнопки с подавлением дребезга, распознаванием клика, удержания и состояний `isDown()` (сырой пин) и `isDownStable()` (дебаунс-устойчивое состояние, используется экраном ввода имени).
* `AlexEncoder.h` — класс энкодера: квадратурное декодирование по таблице переходов, опрос пинов таймером `esp_timer` (1 мс), накопление шагов до детента.
* `About.h` — метаданные устройства: `DEVICE_NAME`, `DEVICE_MODEL`, `HW_VERSION`, `SW_VERSION` (плюс лицензионные комментарии Adafruit/Arduino_GFX).
* `MeasuredResult.h` — структура `MeasuredResult` с результатом измерения (времена датчиков, времена/скорости шторок, ширина щели, тип датчика и движение шторки); константа `MEASUREMENT_SERIES_MAX_SIZE 5`.
* `Images.h` — битмапы/графика для экранов результатов (график импульса датчиков, фоны графиков скоростей).
* `serialAPI.h` — задача `serialApiTask`: чтение JSON-команд из stdin, парсинг через cJSON, обработка команд API (свет, измерение, список/чтение/удаление/сохранение записей, OTA, echo); `safePrint()`/`safePrintJson()` для потокобезопасного вывода.
* `SerialAPICommon.h` — enum `ApiRequestAction`, строковые константы команд и ответов API, статусы качества света, макрос отладки `SERIAL_API_DEBUG_PRINT`, `extern serialPrintMutex`.
* `SerialNumber.h` — генерация и хранение серийного номера устройства (10 символов) в NVS.
* `FirmwareUpdate.h` — OTA-обновление: приём base64, расшифровка AES-256-CBC, запись в OTA-раздел, переключение загрузочного раздела и перезагрузка; содержит `AesKey` и `AesIvInit`.
* `AceSorting/` — сторонняя библиотека сортировки (используется `shellSortKnuth`, в т.ч. с кастомным компаратором).
* `FilmCamerasList.h` — доступ к встроенному словарю камер (символы `_binary_film_cameras_list_txt_start/end`) и префикс-поиск `findFilmCameraHint()` (регистрозависимый, перенос строк `\n`/`\r\n`/`\r`) для автодополнения имени записи.

### main/recordsManagement/
* `RecordsManagementCommon.h` — константы LittleFS (`TOTAL_RECORDS_COUNT 100`, `TOTAL_SERIES_COUNT 100`, пути `/littlefs/records.bin` и `/littlefs/seriesRecords.bin`), глобальные объекты `storage` и `seriesStorage`, хелпер `initRecordsFile()`, `getMeasurementSaveRecordSize()`/`getSeriesSaveRecordSize()`, миграция `ensureRecordsFileUpToDate()`/`migrateRecordsFileFromLegacy()` (легаси-файлы без поля `name` автоматически конвертируются: валидным записям в `name` записывается форматированный `recordNumber`, файлы неожидаемого размера пересоздаются), функция `initStorage()` (монтирование LittleFS, миграция и инициализация обоих файлов записей нулями).
* `RecordsStorageManager.h` — шаблонный класс `RecordsStorageManagerT<RecordT>` управления записями (`getAllValidRecordNumbers()`, `getAllValidRecords()`, `getRecordByNumber()`, `deleteRecordByNumber()`, `saveOrUpdateRecord()`, `getFreeSlotsCount()`; доступ к файлу через мьютекс); алиасы `RecordsStorageManager` (для `StoredMeasuredResult`) и `SeriesStorageManager` (для `StoredMeasuredSeries`).
* `StoredMeasuredResult.h` — структура сохраняемой записи (`recordNumber`, `isDeleted` + поля `MeasuredResult` + `name[RECORD_NAME_BUF_SIZE]`), оператор сравнения.
* `StoredMeasuredSeries.h` — структура сохраняемой серии (`recordNumber`, `isDeleted`, `measurementCount`, массив `results[MEASUREMENT_SERIES_MAX_SIZE]` + `name[RECORD_NAME_BUF_SIZE]`).
* `LegacyRecordFormats.h` — легаси-форматы записей (`OldStoredMeasuredResult`, `OldStoredMeasuredSeries`, без поля `name`); используются только миграцией в `RecordsManagementCommon.h` и обязаны оставаться байт-в-байт идентичными старым форматам.

### main/screens/
* `screensCommon.h` — общие функции отрисовки текста (`getStringRect`, `drawStringHCentered`, `drawStringRightAligned`), `Rect`, `SeriesField` (включая скорость датчиков 1/X) и дескрипторы полей, `MeasurementSeries` (до 5 измерений), функции валидации/получения значений полей серии, `formatCompactDouble`, `formatSpeedFractionLabel`, `formatPreciseTimeValue`, `formatSeriesFieldValue`, `buildActiveSeriesFields`, `SERIES_GRID_LINE_COLOR`, `SERIES_AXIS_COLOR`, forward-декларация `drawNavBar`.
* `MainMenu.h` — главное меню (Measure, Light Setup, Saved Measures, About); `drawMainMenu()`.
* `SensorSelectionScreen.h` — выбор типа датчика (35mm, 6×4.5, 6×6, 6×7), затем переход к выбору движения шторки.
* `CurtainMovementSelectionScreen.h` — выбор типа движения шторки (горизонтальный, вертикальный, лепестковый), переход к выбору количества измерений/измерению.
* `MeasurementsCountSelectionScreen.h` — выбор количества измерений в серии (1, 2, 3, 5).
* `LightCheckScreen.h` — проверка уровня сигнала датчиков («Too dim» / «Too bright» / «OK»), `drawSensorSignalLevelBar()`.
* `MeasuringScreen.h` — одиночное измерение: `captureMeasurement()` + `drawMeasuredScreen()`.
* `MeasuringSeriesScreen.h` — серия измерений; `offerPartialSeriesReport()` для показа частичного отчёта при прерывании; после отчёта (полного или частичного) предлагает сохранение серии через `drawSeriesSaveScreen()`.
* `CaptureMeasurement.h` — конечный автомат захвата АЦП, структура `RawMeasurementCapture`, функция `captureMeasurement()`.
* `computeMeasuredResult.h` — расчёт результата из сырых времён; `sensorsData` (размеры кадра и расстояния между датчиками), `CurtainTimings`, `ComputedMeasurement`.
* `MeasuredScreen.h` — экран результата измерения (выдержка, скорость затвора, график, данные шторок), генерация JSON-ответа для API.
* `MeasuredResult.h` — `renderMeasuredResult()`: многостраничный результат (выдержка/скорость/график, шторки, щель) и `drawNavBar()`.
* `MeasurementSaveScreen.h` — меню сохранения результата (No / Yes / Overwrite oldest / Overwrite newest / Select record / Return); для каждого варианта сохранения сначала показывается страница выбора имени (`drawNameSelectionScreen()`): «Save as '0002'» (имя по умолчанию из номера записи) или «Save as...» (посимвольный ввод); `saveMesurementResult()` принимает имя.
* `SeriesSaveScreen.h` — меню сохранения серии (`saveSeriesResult()`, `drawSeriesSaveScreen()`): сохраняет серию как одну запись `StoredMeasuredSeries` в `seriesRecords.bin`; как и для одиночных измерений, перед сохранением показывается страница выбора имени.
* `NameSelectionScreen.h` — страница выбора имени перед сохранением: «Save as 'NNNN'» / «Save as...» (`drawNameSelectionScreen()`), хелпер `getSaveActionTargetRecordNumber()` (предсказывает номер записи для выбранного варианта сохранения: min/max для перезаписей, max+1 для новой).
* `NameEntryScreen.h` — экран посимвольного ввода имени записи (`drawNameEntryScreen()`): поворот энкодера прокручивает текущий символ через алфавит `A-Z a-z 0-9 пробел -()+<>{}[]/#` с серой подсказкой автодополнения из `film_cameras_list.txt`; клик фиксирует символ и подхватывает следующий из подсказки; удержание+поворот двигает курсор (за концом строки — слоты Ok/Cancel); удержание ≥700 мс без поворота — режим удаления (мигающий курсор «<», клик стирает символ слева); «Ok» при пустом имени мигает строкой фоном. Жесты распознаются вручную через `button.isDownStable()` + `millis()` + `AlexEncoder::counter`.
* `MeasurementResultRecordSelectionScreen.h` — шаблонный `drawRecordSelectionScreen(store)` выбора существующей записи (список имен записей — либо номер, если имя не задано; обрезка до 16 символов с «...»; сортировка через AceSorting с компаратором); обёртки `drawMeasurementResultRecordSelectionScreen()` и `drawSeriesRecordSelectionScreen()`.
* `ViewRecordsScreen.h` — просмотр, переименование и удаление сохранённых записей: выбор «Measurements / Series», `drawViewSingleRecordsScreen()` и `drawViewSeriesRecordsScreen()` (серия открывается через `renderSeriesReport()`; в меню «What's next?» есть «Rename record»/«Rename series» — редактор имени с текущим значением, сохранение через `saveOrUpdateRecord()`).
* `MessageScreen.h` — универсальный экран с заголовком и списком опций, возвращает выбранный индекс.
* `SeriesReport.h` — многостраничный отчёт серии: страницы сводки + график по каждому полю.
* `SeriesSummaryPage.h` — сводная таблица статистики (avg / min / max / count) по полям серии; для датчиков — отдельные строки выдержки (1/X) и времени (мс), серая сетка таблицы.
* `SeriesChartPage.h` — график одного поля серии, `drawTextWithPlaque()`.
* `AboutScreen.h` — экран «About»: версии, серийный номер, возможность отката прошивки (Rollback Firm. Update).

### my_components/
* `Arduino_GFX/` — графическая библиотека для дисплеев (`Arduino_GFX_Library`).
* `U8g2/` — библиотека дисплеев и шрифтов u8g2.
* `InterpolationLib/` — библиотека интерполяции (map/range).
* `mString/` — библиотека строк.
