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
* `main.cpp` — точка входа. `setup()` инициализирует дисплей, энкодер, таймер опроса кнопки, АЦП, LittleFS-хранилище (создаёт `records.bin` и `seriesRecords.bin`, мигрирует старые файлы, backfill пустых имён через `fillEmptyRecordNames()`), серийный номер в NVS, мьютекс последовательного вывода, запускает задачу `serialApiTask` на ядре 0 и валидирует прошивку (`esp_ota_mark_app_valid_cancel_rollback`). `loop()` содержит `drawMainMenu()`.
* `CMakeLists.txt` — регистрирует компонент `main` (исходник `main.cpp`, `INCLUDE_DIRS "."`).
* `idf_component.yml` — зависимости: `espressif/arduino-esp32` (pre-release) и `joltwallet/littlefs`.

### main/lib/
* `Common.h` — общие константы и пины (`TEST_PIN_1/2`, `ENCODER_B_PIN`, `ENCODER_A_PIN`, `BUTTON_PIN`), пороги уровней сигнала, enum'ы (`SensorType`, `MainMenuItems`, `CurtainMovement`, `MeasurementSaveMenuItems`, `ZeroRecordsMeasurementSaveMenuItems`), глобальные объекты (`button`, `curtainMovement`, `curSensorIndex`), ISR `onTimer()`, функции `formatRecordName()`/`formatDefaultRecordName()`/`copyRecordName()`/`isRecordNameEmpty()`/`getRecordNameVisibleLen()`/`recordNameStartsWithIgnoreCase()`/`recordNamesEqualIgnoreCase()` и `halt()`.
* `Display.h` — инициализация TFT-дисплея ST7735 128×160 через Arduino_GFX (`Arduino_HWSPI`), назначение SPI-пинов.
* `ADC.h` — настройка ADC continuous driver (2 канала `ADC_CHANNEL_6/7`, частота 1 МГц, `READ_LEN 4096`), `initADC()`, callback `s_conv_done_cb()`.
* `AlexButton.h` — класс кнопки с подавлением дребезга, распознаванием клика, удержания и состояния `isDown()`.
* `AlexEncoder.h` — класс энкодера: квадратурное декодирование по таблице переходов, опрос пинов таймером `esp_timer` (1 мс), детент фиксируется по возврату последовательности в состояние покоя (считанное на старте, `_restState`), накопленный знак даёт направление и `counter` (детент = 1; «фаза» частичных шагов и дребезга сбрасывается на каждом детенте, поэтому неверный/пропущенный шаг не сдвигает сетку детентов и не требует второго поворота; резервный порог `QUAD_STEPS_PER_DETENT` = 4 срабатывает, если опрос пропустил состояние покоя).
* `About.h` — метаданные устройства: `DEVICE_NAME`, `DEVICE_MODEL`, `HW_VERSION`, `SW_VERSION` (плюс лицензионные комментарии Adafruit/Arduino_GFX).
* `MeasuredResult.h` — структура `MeasuredResult` с результатом измерения (времена датчиков, времена/скорости шторок, ширина щели, тип датчика и движение шторки); константа `MEASUREMENT_SERIES_MAX_SIZE 5`.
* `Images.h` — битмапы/графика для экранов результатов (график импульса датчиков, фоны графиков скоростей).
* `serialAPI.h` — задача `serialApiTask`: чтение JSON-команд из stdin, парсинг через cJSON, обработка команд API (свет, измерение, список/чтение/удаление/сохранение записей, OTA, echo); `safePrint()`/`safePrintJson()` для потокобезопасного вывода.
* `SerialAPICommon.h` — enum `ApiRequestAction`, строковые константы команд и ответов API, статусы качества света, макрос отладки `SERIAL_API_DEBUG_PRINT`, `extern serialPrintMutex`.
* `SerialNumber.h` — генерация и хранение серийного номера устройства (10 символов) в NVS.
* `FirmwareUpdate.h` — OTA-обновление: приём base64, расшифровка AES-256-CBC, запись в OTA-раздел, переключение загрузочного раздела и перезагрузка; содержит `AesKey` и `AesIvInit`.
* `FilmCamerasList.h` — массив `FilmCamerasList` (1160 названий плёночных камер) для автоподсказок редактора имён.
* `AceSorting/` — сторонняя библиотека сортировки (используется `shellSortKnuth`).

### main/recordsManagement/
* `RecordsManagementCommon.h` — константы LittleFS (`TOTAL_RECORDS_COUNT 100`, `TOTAL_SERIES_COUNT 100`, пути `/littlefs/records.bin` и `/littlefs/seriesRecords.bin`), глобальные объекты `storage` и `seriesStorage`, хелпер `initRecordsFile()`, `getMeasurementSaveRecordSize()`/`getSeriesSaveRecordSize()`, функция `initStorage()` (монтирование LittleFS, инициализация файлов нулями, миграция старых безимённых файлов в формат с `name` через `migrateRecordsFileToNamed()`), `fillEmptyRecordNames()`, `isMeasurementRecordNameTaken()`/`isSeriesRecordNameTaken()` (проверка дубликатов имён), `getNextRecordNumber()`.
* `RecordsStorageManager.h` — шаблонный класс `RecordsStorageManagerT<RecordT>` управления записями (`getAllValidRecordNumbers()`, `getRecordByNumber()`, `deleteRecordByNumber()`, `saveOrUpdateRecord()`, `getFreeSlotsCount()`; доступ к файлу через мьютекс); алиасы `RecordsStorageManager` (для `StoredMeasuredResult`) и `SeriesStorageManager` (для `StoredMeasuredSeries`).
* `StoredMeasuredResult.h` — структура сохраняемой записи (`recordNumber`, `isDeleted` + поля `MeasuredResult` + `name[41]`), оператор сравнения.
* `StoredMeasuredSeries.h` — структура сохраняемой серии (`recordNumber`, `isDeleted`, `measurementCount`, массив `results[MEASUREMENT_SERIES_MAX_SIZE]`, `name[41]`).

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
* `MeasurementSaveScreen.h` — меню сохранения результата (No / Yes / Overwrite oldest / Overwrite newest / Select record / Return); `saveMesurementResult()` принимает имя, `saveMeasurementWithName()` показывает `Choose name` (`Save as 'xxxx'` / `Save as...`) и редактор имени; имя предзаполняется только при «Sel. rec. to overwrite» (у Yes/Oldest/Newest поле пустое), а перезаписываемая запись исключается из проверки дубликатов (`measurementSaveExcludeRecordNumber`).
* `SeriesSaveScreen.h` — меню сохранения серии (`saveSeriesResult()` с именем, `saveSeriesWithName()` с выбором имени, `drawSeriesSaveScreen()`): сохраняет серию как одну запись `StoredMeasuredSeries` в `seriesRecords.bin`; предзаполнение имени и исключение перезаписываемой серии из проверки дубликатов — как в `MeasurementSaveScreen.h` (`seriesSaveExcludeRecordNumber`).
* `RecordNameEditor.h` — редактор имени записи: `editRecordName()` — посимвольный ввод энкодером (поворот листает символ в позиции курсора, клик переводит курсор к следующему символу и принимает серый символ подсказки), лимит `RECORD_NAME_MAX_LEN 40`; имя всегда видно целиком как 2 центрированные строки по 20 символов (`u8g2_font_6x13_tf`, ширина символа 6 px, поля по 20 px), под ними область сообщений («Name is empty» / «Name already exists»), ниже меню «Ok / Cancel / Delete Mode [On/Off]»; курсор — подчёркивание под символом в режиме INSERT и 1-px вертикальная черта между символами в режиме DELETE, белый у активной области и серый у неактивной; удержание > 700 мс без поворота переключает фокус поле/меню (при уходе в меню Delete Mode всегда [Off]), удержание + поворот двигает курсор по всей показанной строке (включая серый хвост подсказки), причём поворот при зажатой кнопке двигает курсор всегда (состояние кнопки читается напрямую с пина, поэтому детент, пришедший в момент нажатия, не теряется), а короткий дребезг отпускания (~20 мс) не прерывает удержание; курсор конца полного 40-символьного имени рисуется в правом поле за строкой, кэш курсора сбрасывается только при перерисовке той строки, в которой он стоит; поворот в режиме DELETE стирает символ слева от курсора (курсор сдвигается влево) или символ под курсором (курсор на месте), одиночный клик выходит из режима; автоподсказка `buildSuggestString()`/`matchHintAt()` из `FilmCamerasList`: скан набранного текста от границы слова (начало строки или не-буква/не-цифра перед ним), регистронезависимо, пробел в набранном тексте — «пустая ячейка», которую заполняет подсказка (серым), серый хвост за концом набранного текста, после подсказки скан продолжается за ней, свои непробельные символы никогда не заменяются, листание набора символов стартует от показанного серого символа подсказки; `Ok` применяет серые символы внутри набранного текста и проверяет пустоту/дубликаты; перерисовываются только «грязные» области (строки имени, курсор, флаг Delete Mode, сообщение) — без мерцания. Курсор конца полного 40-символьного имени рисуется в правом поле за строкой.
* `RecordNameChoiceScreen.h` — экран выбора имени при сохранении (`drawRecordNameChoiceScreen()`): «Save as 'xxxx'» (форматированный `recordNumber` через `formatDefaultRecordName()`) и «Save as...» (открывает `editRecordName()`).
* `MeasurementResultRecordSelectionScreen.h` — шаблонный `drawRecordSelectionScreen(store)` выбора записи по имени (мелкий шрифт, до 2 строк через `splitNameTwoLines()`, тримминг хвостовых пробелов, сортировка через AceSorting); exclude-id plumbing для проверки дубликатов (`measurementSaveExcludeRecordNumber`, `seriesSaveExcludeRecordNumber`, хелперы `isMeasurementSaveNameDuplicate()`/`isSeriesSaveNameDuplicate()`);
* `ViewRecordsScreen.h` — просмотр, удаление и переименование сохранённых записей: выбор «Measurements / Series», `drawViewSingleRecordsScreen()` и `drawViewSeriesRecordsScreen()` (серия открывается через `renderSeriesReport()`).
* `MessageScreen.h` — универсальный экран с заголовком и списком опций, возвращает выбранный индекс.
* `SeriesReport.h` — многостраничный отчёт серии: страницы сводки + график по каждому полю.
* `SeriesSummaryPage.h` — сводная таблица статистики (avg / min / max / count) по полям серии; для датчиков — отдельные строки выдержки (1/X) и времени (мс).
* `SeriesChartPage.h` — график одного поля серии, `drawTextWithPlaque()`.
* `AboutScreen.h` — экран «About»: версии, серийный номер, возможность отката прошивки (Rollback Firm. Update).

### my_components/
* `Arduino_GFX/` — графическая библиотека для дисплеев (`Arduino_GFX_Library`).
* `U8g2/` — библиотека дисплеев и шрифтов u8g2.
* `InterpolationLib/` — библиотека интерполяции (map/range).
* `mString/` — библиотека строк.
