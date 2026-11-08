# Web-эмулятор PurrGO (WebAssembly + GitHub Pages)

PC-эмулятор (`apps/emulator`) собирается в WebAssembly через Emscripten и запускается в браузере,
в том числе на смартфоне. Ядро (`src/core`) и платформенный слой (`src/platform/pc`) те же, что
в десктопной сборке; отличается только обвязка:

| Что | Десктоп | Браузер |
|-----|---------|---------|
| Окно, рендер | SDL2 (системный) | SDL2 из портов Emscripten, `<canvas>` |
| Основной цикл | `while` + `SDL_Delay` | `emscripten_set_main_loop` (requestAnimationFrame) |
| Кнопки K1-K4 | нарисованы в окне SDL, клавиши 1-4 | HTML-кнопки страницы (`web/shell.html`), клавиши 1-4, 5 = K1+K4 |
| Файлы | диск, пути `../../../../../tests/data/...` | MEMFS: `/maps`, `/books`, `/routes`, `/waypts`, `/tracks`, `/PURRGO.CFG` |
| GNSS | mock или COM-порт | только mock (старт задаётся при сборке) |

Настройки (`PURRGO.CFG`) и треки живут в памяти вкладки и теряются при перезагрузке страницы.

## Публикация на GitHub Pages

1. Один раз: **Settings → Pages → Build and deployment → Source: GitHub Actions**.
2. Workflow `.github/workflows/pages.yml` запускается при push в `main` (если менялись эмулятор,
   ядро, компилятор карт и т. п.) или вручную: **Actions → Web emulator → Run workflow**.
3. Сайт появится по адресу `https://<владелец>.github.io/<репозиторий>/`.

Что делает workflow:

1. Скачивает PBF (Geofabrik), вырезает область `bbox` (`osmium extract`), прогоняет
   `purrgo_osm_optimizer.py` и `purrgo_map_compiler.py`. Результат кэшируется
   (ключ: PBF URL + bbox + код компилятора), поэтому PBF скачивается не при каждой сборке.
2. Кладёт карту в `web-root/maps`, примеры книг (`tests/host/data/sample*.pgb`) в `web-root/books`.
3. Собирает эмулятор пресетом `web-release` с `-DPURRGO_WEB_PRELOAD_DIR=web-root`:
   содержимое `web-root` упаковывается в `index.data` и монтируется в корень виртуальной ФС.
4. Публикует `index.html`, `index.js`, `index.wasm`, `index.data`.

Параметры ручного запуска: `pbf_url`, `bbox` (`lon_min,lat_min,lon_max,lat_max`) и стартовая точка
имитации GNSS (`mock_lat`, `mock_lon`; она должна лежать внутри bbox). Размер образа `index.data`
растёт вместе с областью карты и скачивается браузером целиком при открытии страницы, поэтому
bbox лучше держать в пределах города.

## Локальная сборка

Нужен [emsdk](https://emscripten.org/docs/getting_started/downloads.html) (проверено на 4.0.9)
и переменная окружения `EMSDK`:

```sh
# каталог с содержимым виртуальной ФС: maps/ (файлы карты прямо в maps/), books/ ...
cmake --preset web-release -DPURRGO_WEB_PRELOAD_DIR=/путь/к/web-root \
      -DPURRGO_MOCK_LAT_1E7=539006000 -DPURRGO_MOCK_LON_1E7=275590000
cmake --build out/build/web-release
cd out/build/web-release/apps/emulator && python3 -m http.server 8000
# открыть http://localhost:8000/index.html
```

`file://` не подходит: браузер не даёт загрузить `.wasm` и `.data` с локального диска.
Без `PURRGO_WEB_PRELOAD_DIR` эмулятор запустится, но карты не найдёт.

## Ограничения

- Только имитация GNSS: реальная геопозиция браузера не подключена.
- Своя карта сейчас подключается только пересборкой (через `bbox` в workflow).
- Эмуляция E-Ink упрощённая: частота обновления и серые уровни как в десктопном эмуляторе,
  но без физики ghosting-а.
