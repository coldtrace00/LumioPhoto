# Lumio Photo

🇺🇦 Українська · [🇬🇧 English](#english)

**Lumio Photo** — швидкий локальний організатор і редактор фото для Windows на **C++ / Qt**.
Відкрита заміна Google Picasa: бібліотека з папок, миттєві мініатюри, перегляд, неруйнівне
редагування, альбоми, слайдшоу, експорт. Може бути програмою за замовчуванням для фото —
подвійний клік у Провіднику відкриває знімок у Lumio Photo.

**[⬇ Завантажити останню версію](https://github.com/coldtrace00/LumioPhoto/releases/latest)** — файл `LumioPhoto-Setup.exe`.

## Приватність і безпека

- **Без телеметрії, аналітики й хмари.** Програма нічого не надсилає. Єдиний мережевий запит —
  перевірка оновлень на GitHub, і лише тоді, коли ви самі натискаєте «Перевірити й оновити».
  Автоматичних перевірок у фоні немає.
- **Метадані у файли не пишуться — ніякі.** Зірочки, теги, підписи, альбоми й правки
  зберігаються лише в локальній базі програми (`%LOCALAPPDATA%\LumioPhoto\library.db`).
  У папках із фото не з'являються службові файли на кшталт `.picasa.ini`.
- **Експорт і «Зберегти у файл» записують лише пікселі.** EXIF, GPS, камера й дата
  в нові файли не потрапляють, тож ними безпечно ділитися.
- **Оригінали захищені.** Правки неруйнівні: файл не змінюється, доки ви самі не натиснете
  «Зберегти у файл». Тоді оригінал копіюється в резервну папку програми,
  і його можна відновити однією кнопкою.
- **Видалення лише в кошик**, з підтвердженням.
- EXIF лише **читається** (для панелі «Інформація») обережним парсером із перевіркою меж,
  тож пошкоджені файли не можуть зламати програму.

## Встановлення

Запустіть **`LumioPhoto-Setup.exe`** — це один файл, у якому вже є вся програма. Вгорі вікна виберіть мову
(Українська / English) і тему (темна / світла) — їх буде передано програмі. Далі можна вибрати:
- **Встановити на цей комп'ютер** у будь-яку папку (без прав адміністратора). Програма з'явиться
  в меню «Пуск» і в «Параметри → Програми», звідки її можна видалити.
- **Портативна версія**: бібліотека, налаштування й мініатюри лежать у папці `data` поруч
  із програмою. Її можна носити на флешці, а в систему нічого не записується.

## Програма за замовчуванням для фото

Меню ⚙ → **«Відкривати фото в Lumio Photo…»**. Програма зареєструється для JPG, PNG, BMP, GIF, WebP,
TIFF, HEIC та інших форматів і відкриє «Параметри Windows → Програми за замовчуванням → Lumio Photo».
Там натисніть кнопку, що встановлює її за замовчуванням (Windows 10/11 дозволяє зробити це лише
самому користувачу). Інший спосіб: правий клік по фото → «Відкрити за допомогою» → Lumio Photo → «Завжди».

Після цього:
- **подвійний клік по фото в Провіднику** відкриває його в Lumio Photo;
- стрілки ← → гортають інші фото з тієї самої папки, як у Picasa Photo Viewer;
- якщо програма вже відкрита, фото відкривається в тому ж вікні, без другої копії;
- фото з папок поза бібліотекою до бібліотеки не додаються, але зірочки й правки для них теж зберігаються.

## Оновлення

**Однією кнопкою.** Меню ⚙ → **«Перевірити й оновити…»**. Програма запитає GitHub про останній реліз.
Якщо є новіша версія, вона покаже список змін і кнопку **«Оновити зараз»**. Далі все відбувається
автоматично: програма завантажить інсталятор, звірить контрольну суму SHA-256 (якщо GitHub її надає),
встановить нову версію в ту саму папку й перезапуститься. Бібліотека, налаштування, ярлики
й портативний режим зберігаються.

**Вручну.** Завантажте `LumioPhoto-Setup.exe` зі сторінки
[Releases](https://github.com/coldtrace00/LumioPhoto/releases/latest), запустіть його і вкажіть ту саму
папку, куди програму встановлено зараз (для звичайного встановлення це папка за замовчуванням).
Нова версія встановиться поверх старої. Бібліотека (зірочки, теги, альбоми, правки) не зачіпається,
бо вона лежить окремо, у `%LOCALAPPDATA%\LumioPhoto` (у портативній версії — у папці `data`).
Мову й тему буде взято з тих, що ви виберете в інсталяторі.

## Можливості

**Бібліотека**
- Додаєте папки, і програма сама знаходить у них фото (з підпапками) у фоновому режимі
- Дерево папок, «Усі фото», «Улюблені», «Нові за 30 днів», «Відредаговані», альбоми
- Швидка сітка мініатюр: кеш у пам'яті й на диску, фонові потоки, плавне прокручування
  десятків тисяч фото; розмір мініатюр регулюється
- Пошук за назвою, тегами, підписом і папкою; сортування за датою зйомки, назвою, розміром
- Зірочки (S), теги, підписи, альбоми; перетягування фото в інші програми
- Пакетні дії: поворот, експорт, копіювання, друк, додавання до альбому, видалення

**Перегляд і редагування** (подвійний клік по фото в бібліотеці або в Провіднику)
- Масштаб колесом, 100% подвійним кліком, перетягування; ← → для гортання
- **Основне**: обрізання (вільно, 1:1, 4:3, 3:2, 16:9, 4:5…), вирівнювання горизонту з сіткою,
  «Магія» (як «Мені пощастить» у Picasa), автоконтраст, автоколір, поворот, дзеркало
- **Тон**: яскравість, контраст, тіні, світлі ділянки, насиченість, теплота
- **Ефекти**: Ч/Б, сепія, різкість, віньєтка
- Скасування (Ctrl+Z), «Скинути все», «Зберегти у файл», «Відновити оригінал»
- Панель «Інформація»: камера, об'єктив, витримка, діафрагма, ISO, GPS (лише показ)

**Ще**
- Слайдшоу на весь екран з плавними переходами
- Експорт зі зміною розміру, формату (JPEG/PNG) та якості
- Друк, встановлення як шпалери, «Показати в папці»
- **Дві теми: темна та світла** — вибір в інсталяторі, змінити можна в меню ⚙ → «Тема»
- HiDPI, кілька моніторів; F11 — повний екран
- **Дві мови інтерфейсу: українська та англійська.** Мова визначається за налаштуваннями Windows,
  її можна вибрати в інсталяторі й змінити в меню ⚙ → «Мова / Language».

## Збирання з вихідного коду

Потрібно: **Qt 6.x** (або Qt 5.15) для **MinGW** + CMake (обидва ставляться через Qt Online Installer;
модулі Widgets, Sql, PrintSupport, Network входять у стандартне встановлення).

Двічі клацніть **`build.bat`**: він сам знайде Qt у `C:\Qt` і створить
- **`LumioPhoto-Setup.exe`** — інсталятор в одному файлі;
- **`dist\LumioPhoto.exe`** — готову програму з усіма DLL.

Якщо Qt встановлено не в `C:\Qt`, відкрийте `build.bat` і вкажіть шляхи `QT_DIR` і `MINGW_DIR` на початку файлу.

Також можна відкрити `CMakeLists.txt` (або `Lumio-Photo.pro`) у Qt Creator, вибрати кит MinGW і режим Release.

## Як випустити нову версію

1. Змініть версію в `CMakeLists.txt` (`project(LumioPhoto VERSION 1.1.0 ...)`) і в `Lumio-Photo.pro` (`VERSION = 1.1.0`).
   Інсталятор бере версію з `CMakeLists.txt` автоматично.
2. Запустіть `build.bat`. З'явиться `LumioPhoto-Setup.exe`.
3. На GitHub: **Releases → Draft a new release**, тег **`v1.1.0`**, опис змін, прикріпіть файл
   **`LumioPhoto-Setup.exe`** (саме з такою назвою) і натисніть **Publish release**.

Програма порівнює тег останнього релізу зі своєю версією. Тож щойно реліз опубліковано,
кнопка «Перевірити й оновити» запропонує його всім користувачам.

## Клавіші

| Де | Клавіша | Дія |
|---|---|---|
| Бібліотека | Enter / подвійний клік | Відкрити фото |
| Бібліотека | S | Улюблене |
| Бібліотека | Ctrl+L / Ctrl+R | Повернути ліворуч / праворуч |
| Бібліотека | Ctrl+C, Ctrl+E, Ctrl+P | Копіювати, експорт, друк |
| Бібліотека | F2, Delete | Перейменувати, у кошик |
| Бібліотека | F5 | Оновити бібліотеку |
| Перегляд | ← →, пробіл | Попереднє / наступне |
| Перегляд | колесо, 0, 1, + − | Масштаб, вмістити, 100%, крок |
| Перегляд | Ctrl+Z | Скасувати правку |
| Перегляд | I | Інформація |
| Перегляд | Esc | Назад до бібліотеки |
| Слайдшоу | пробіл, ← →, Esc | Пауза, гортати, вихід |
| Будь-де | F11 | Повний екран |

## Формати
JPEG, PNG, BMP, GIF, а також WebP і TIFF, якщо в Qt встановлено модуль «Qt Image Formats».
HEIC/AVIF — якщо є відповідний плагін Qt.

## Структура проєкту
```
src/
  main.cpp              запуск, одна копія програми, відкриття файлів з Провідника
  theme.*, i18n.*       темна / світла тема, українська / англійська
  updater.*             перевірка й встановлення оновлень з GitHub Releases
  fileassoc.*           реєстрація як програми для відкриття фото
  mainwindow.*          бібліотека: дерево, сітка, пошук, дії
  viewerpage.*          перегляд і редактор
  imagecanvas.*         полотно: масштаб, перетягування, обрізання
  edits.*               неруйнівні правки (поворот, обрізання, тон, ефекти)
  library.*             локальна база SQLite (зірочки, теги, альбоми, правки)
  scanner.*             фоновий пошук фото в папках
  thumbcache.*          кеш мініатюр (пам'ять + диск, фонові потоки)
  photomodel.*          модель і відмальовування сітки
  exif.*                безпечне читання EXIF
  infopanel.*, slideshow.*, exportdialog.*, util.*, icons.*
installer/              LumioPhoto-Setup.exe і деінсталятор (Win32, без Qt)
third_party/miniz/      розпакування zip в інсталяторі (MIT)
logo/                   лого в PNG до 1024 пікс. та .ico
```

## Ліцензія
MIT — див. [LICENSE](LICENSE). miniz — MIT (`third_party/miniz/LICENSE`).

---

<a id="english"></a>
# Lumio Photo (English)

**Lumio Photo** is a fast, local photo organizer and editor for Windows written in **C++ / Qt** —
an open replacement for Google Picasa: a library built from your folders, instant thumbnails,
a viewer, non-destructive editing, albums, slideshows and export. It can be your default photo app —
double-click a photo in Explorer and it opens in Lumio Photo.

**[⬇ Download the latest version](https://github.com/coldtrace00/LumioPhoto/releases/latest)** — the `LumioPhoto-Setup.exe` file.

## Privacy and safety
- **No telemetry, analytics or cloud.** The program sends nothing. The only network request is the
  update check on GitHub, and only when you click “Check for updates” yourself. There are no background checks.
- **No metadata is ever written to your files.** Stars, tags, captions, albums and edits live only
  in the program's local database (`%LOCALAPPDATA%\LumioPhoto\library.db`). No `.picasa.ini`-style
  files appear in your photo folders.
- **Export and “Save to file” write pixels only** — no EXIF, GPS, camera or date, so the results are safe to share.
- **Originals are protected.** Edits are non-destructive until you click “Save to file”; the original is
  then backed up in the program's folder and can be restored with one click.
- **Deleting goes to the Recycle Bin**, always with a confirmation.
- EXIF is only **read** (for the Information panel) by a careful, bounds-checked parser.

## Installation
Run **`LumioPhoto-Setup.exe`** — a single file containing the whole program. Pick the language
(Українська / English) and the theme (dark / light) at the top of the window — they are passed on to the program. Then choose:
- **Install on this computer** into any folder (no administrator rights). Lumio Photo appears in the Start
  menu and in “Settings → Apps”, where it can be uninstalled.
- **Portable version**: the library, settings and thumbnails live in a `data` folder next to the program —
  carry it on a USB stick; nothing is written to the system.

## Default photo app
Menu ⚙ → **“Open photos with Lumio Photo…”**. The program registers itself for JPG, PNG, BMP, GIF, WebP, TIFF,
HEIC and other formats and opens “Windows Settings → Default apps → Lumio Photo”. Click the button there that
makes it the default (Windows 10/11 only lets the user do this). Alternatively: right-click a photo →
“Open with” → Lumio Photo → “Always”.

After that:
- **double-clicking a photo in Explorer** opens it in Lumio Photo;
- ← → browse the other photos in the same folder, like Picasa Photo Viewer;
- if the program is already open, the photo opens in the same window, without a second copy;
- photos from folders outside the library are not added to it, but their stars and edits are kept too.

## Updates
**One button.** Menu ⚙ → **“Check for updates…”**. The program asks GitHub for the latest release.
If a newer version exists, it shows what's new and an **“Update now”** button. The rest is automatic:
the program downloads the installer, verifies its SHA-256 checksum (when GitHub provides one), installs
the new version into the same folder and restarts. The library, settings, shortcuts and portable mode are kept.

**Manually.** Download `LumioPhoto-Setup.exe` from the
[Releases](https://github.com/coldtrace00/LumioPhoto/releases/latest) page, run it and choose the same folder
the program is installed in now (for a regular install this is the default folder). The new version is installed
over the old one. The library (stars, tags, albums, edits) is not touched, because it is stored separately in
`%LOCALAPPDATA%\LumioPhoto` (the `data` folder in the portable version). The language and theme are taken
from what you choose in the installer.

## Features
- **Library**: add folders and photos are found automatically (with subfolders) in the background;
  folder tree, All photos, Favorites, Last 30 days, Edited, Albums; fast thumbnail grid with memory and
  disk cache; search by name, tags, caption and folder; sort by date taken, name or size;
  stars (S), tags, captions, albums; drag photos into other apps; batch rotate/export/copy/print/delete.
- **Viewer and editor** (double-click a photo in the library or in Explorer): zoom with the wheel, 100% on double-click, pan; ← → to browse.
  - **Basic**: crop (free, 1:1, 4:3, 3:2, 16:9, 4:5…), straighten with a grid, “Magic” (like Picasa's
    “I'm feeling lucky”), auto contrast, auto color, rotate, mirror
  - **Tone**: brightness, contrast, shadows, highlights, saturation, warmth
  - **Effects**: B&W, sepia, sharpness, vignette
  - Undo (Ctrl+Z), Reset all, Save to file, Restore original; Information panel with camera, lens,
    shutter speed, aperture, ISO, GPS (display only)
- Full-screen slideshow with smooth transitions; export with resizing, format (JPEG/PNG) and quality;
  print; set as wallpaper; HiDPI, multiple monitors; F11 — full screen.
- **Two themes: dark and light** — chosen in the installer, switchable in ⚙ → “Theme”.
- **Two interface languages: Ukrainian and English.** Chosen in the installer and switchable in
  ⚙ → “Мова / Language”.

## Building from source
Requires **Qt 6.x** (or Qt 5.15) for **MinGW** + CMake (both come with the Qt Online Installer; the
Widgets, Sql, PrintSupport and Network modules are part of the standard install).

Double-click **`build.bat`** — it finds Qt in `C:\Qt` and produces
- **`LumioPhoto-Setup.exe`** — the one-file installer;
- **`dist\LumioPhoto.exe`** — the ready-to-run program with all DLLs.

If Qt is not in `C:\Qt`, open `build.bat` and set `QT_DIR` and `MINGW_DIR` at the top. You can also open
`CMakeLists.txt` (or `Lumio-Photo.pro`) in Qt Creator, pick the MinGW kit and the Release configuration.

## Publishing a new version
1. Change the version in `CMakeLists.txt` (`project(LumioPhoto VERSION 1.1.0 ...)`) and in `Lumio-Photo.pro`
   (`VERSION = 1.1.0`). The installer picks the version up from `CMakeLists.txt` automatically.
2. Run `build.bat`. It produces `LumioPhoto-Setup.exe`.
3. On GitHub: **Releases → Draft a new release**, tag **`v1.1.0`**, describe the changes, attach
   **`LumioPhoto-Setup.exe`** (exactly this name) and click **Publish release**.

The program compares the latest release tag with its own version, so as soon as the release is published,
“Check for updates” offers it to every user.

## License
MIT — see [LICENSE](LICENSE). miniz — MIT (`third_party/miniz/LICENSE`).
