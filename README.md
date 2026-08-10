# C Weather App (GTK 2)

Порт tray-приложения из `c-weather` / `ts-weather` на C с **GTK 2**.

Показывает текущую температуру в системном трее и обновляет её с заданным интервалом. Собран для работы на современном Linux (Ubuntu и др.) и на **Windows XP** (через MinGW + GTK2).

## Возможности

- Температура и погодная иконка в трее (`GtkStatusIcon`)
- Автообновление по `updateIntervalInSeconds`
- Местоположение: город+страна или широта+долгота
- Провайдеры: `open-meteo` и `openweathermap`
- Диалоги: подробности, настройки, справка, история API-запросов и ошибок

## Зависимости (Ubuntu/Debian)

```bash
sudo apt-get install build-essential pkg-config libgtk2.0-dev libcurl4-openssl-dev \
  libcairo2-dev
```

Опционально для линтера: `cppcheck`.

cJSON вендорится в `third_party/`.

## Сборка и запуск

```bash
make
./c-weather-gtk2
```

Портативная сборка для Windows:

```bash
make STATIC=1
```

`STATIC=1` включает `-static-libgcc` (на Windows также static libstdc++/winpthread и static libcurl, где доступно). GTK остаётся shared и упаковывается в zip вместе с DLL.

Линтер и тесты:

```bash
make lint   # требует cppcheck
make test   # unit-тесты (weather, settings, history, url_encode, icon)
```

При первом запуске создаётся `settings.json` в текущей директории.

## Трей

На **Linux** и **Windows** используется `GtkStatusIcon` (legacy system tray). На GNOME 3+ может понадобиться расширение «AppIndicator» / поддержка legacy tray (Xfce, MATE, KDE работают из коробки).

## Windows XP

Релизный артефакт `c-weather-gtk2-windows-xp-i686.zip` — **32-bit (PE32)** под XP.
Сборка `windows-x86_64` на XP не запускается (`is not a valid Win32 application`).

Локально (Linux cross-compile, как в CI):

```bash
just bundle-win32   # → dist-xp/
```

Или вручную через MSYS2 MinGW32:

```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-gtk2 mingw-w64-i686-curl make pkgconf
make -f Makefile.win32 STATIC=1
# лучше: just bundle-win32 (подставляет XP-совместимые GTK/curl DLL)
```

Скопируйте `dist-xp/` на XP. В каталоге должен быть `curl-ca-bundle.crt` — без него
libcurl на XP даёт `Problem with the SSL CA cert`. Запускайте через
`run-c-weather-gtk2.cmd` (выставляет `CURL_CA_BUNDLE`).

## Настройки

Файл `settings.json` (camelCase, как в `c-weather`):

```json
{
  "city": "New York City",
  "country": "United States",
  "latitude": null,
  "longitude": null,
  "updateIntervalInSeconds": 60,
  "apiProvider": "open-meteo",
  "apiKey": null
}
```

## Взаимодействие

Окно при старте не открывается. Правый клик по иконке температуры в трее:

- Обновить сейчас
- Подробная информация о погоде
- Настройки
- Как пользоваться
- История запросов / ошибок API
- Выйти

## Структура

```
c-weather-gtk2/
├── Makefile
├── README.md
├── packaging/              # desktop, DLL bundler
├── third_party/cJSON.{c,h}
└── src/
    ├── main.c              # GtkStatusIcon, таймер, цикл GTK
    ├── settings.c          # settings.json
    ├── http.c              # libcurl
    ├── weather.c           # геокодинг и погода
    ├── history.c           # кольцевые буферы ошибок/запросов
    ├── icon.c              # PNG-иконки (Cairo)
    ├── ui.c                # GTK2-диалоги
    └── compat.h            # POSIX/GLib/thread (Windows XP)
```

## Лицензия

MIT
