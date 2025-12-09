# Ctags Source Navigator
Это [Far Manager](https://www.farmanager.com/) плагин для навигации по исходному коду, проиндексированному с помощью утилиты [Ctags](https://en.wikipedia.org/wiki/Ctags).
## Полезные ссылки
+ [Домашняя страница плагина](https://github.com/EugeneManushkin/CtagsSourceNavigator)
+ [Far PlugRing](https://plugring.farmanager.com/plugin.php?pid=478)
+ [Обсуждение плагина](https://forum.farmanager.com/viewtopic.php?f=5&t=6394)
## Фичи
+ Переход к объявлению имени
+ Автодополнение (code completion)
+ Поиск имени по всему репозиторию
+ Поиск имени в текущем файле
+ Поиск и открытие файла по имени
+ Топ посещаемых имен и файлов
+ Список членов класса
+ История навигации по исходникам
+ Создание базы данных Ctags для выбранного корня репозитория
+ Переиндексация редактируемого файла и репозитория
+ Макрос с горячими клавишами
## Что нового
### 2.1.106.x
+ Изменилась схема версионирования. Последнее число в версии переместилось на предпоследнее место и теперь означает номер ревизии.
  Последнее число версии теперь соответствует инкрементальному счетчику релизного пайплайна
  [#111](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/111)
+ Поддержан поиск на русском языке! Это эксперементальная фича и может подтормаживать на маломощных компьютерах. Вы всегда можете выключить
  эту опцию в меню конфигурации плагина (см. "Enable platform language in search menu"). Поддержаны только однобайтовые кодировки
  типа cp1251. Русские буквы включены только для меню в которых отображаются имена файлов или пути к файлам
  [#123](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/123)
+ Улучшено определение номеров строк для шаблонов поиска на подобие ворнингов или ошибок популярных компиляторов. Откройте меню поиска
  файлов ("Search name in entire repository") и вставьте в него строку с текстом ошибки компилятора, например такую:
  `\src\tags.cpp(1831,10): error C2143: syntax error: missing ';' before 'return'`. Плагин определит файл tags.cpp с номером строки 1831
  [#139](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/139)
+ Пробелы при вставки в меню из буфера обмена теперь обрезаются с обоих сторон
  [#140](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/140)
+ Пофикшено:
  [#122](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/122),
  [#129](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/129),
  [#143](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/143),
  [#145](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/145)
### 2.1.100.599
+ Команда 'Go back' перемещает курсор едитора на последний посещенный тег [#98](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/98)
+ Нажатие Ctrl+Enter в меню поиска по тегам перемещает текущую панель на файл в котором расположен тег [#99](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/99)
+ Для коротких фильтров выдача тегов ограничивается до 1500 (можно сконфигурировать либо отключить в конфиге плагина). [#103](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/103)
+ Пофикшены баги: [#101](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/101), [#107](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/107)
### 2.1.0.92
+ При открытии репозитория в меню "Manage Repositories" плагин переместит вас в ту папку где вы работали над проектом [#93](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/93)
+ Навигация полностью переработана. Опции "Go back" и "Go forward" стали более удобными и предсказуемыми [#94](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/94)
+ Плагин стал стабильнее. Пофикшено: [#95](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/95), [#96](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/96), [#97](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/97)
## Установка
1. Загрузите последний релиз со [страницы релизов](https://github.com/EugeneManushkin/CtagsSourceNavigator/releases)
2. Извлеките содержимое архива в папку FarManager/Plugins/ctags
3. Установите макрос с горячими клавишами. Скопируйте скрипт ctags_hotkeys.lua в папку %FARPROFILE%\Macros\scripts
   ```
   copy ctags_hotkeys.lua "%FARPROFILE%\Macros\scripts"
   ```
   После копирования скрипта перезапустите Far Manager
### Используйте кастомный ctags индексатор
   Плагин поддерживает [Universal Ctags](https://ctags.io/), [Exuberant Ctags](http://ctags.sourceforge.net/) и ctags из проекта [Cygwin](http://www.cygwin.com/).
   Откройте **F9->Options->Plugins configuration->Ctags Source Navigator**, поместите путь до ctags.exe в едитбокс 'Path to ctags.exe' и нажмите OK.
### Если вы Lua разработчик, вам понравится [Yet Another Lua TAgs](https://github.com/EugeneManushkin/Yalta)
   Эту штуку легко интегрировать в плагин как кастомный ctags индексатор. Скачайте [свежий релиз](https://github.com/EugeneManushkin/Yalta/releases), распакуйте его и
   пропишите полный путь до скрипта ctags_wrapper.bat расположенного в \<распакованный_релиз_yalta\>\yalta\ctags_wrapper.bat в 'Path to ctags.exe' меню конфигурации плагина.
## Использование
1. Используйте плагин в непроиндексированных файлах. Плагин автоматически проиндексирует текущий открытый файл при вызове меню плагина
2. Проиндексируйте репозиторий. Для этого вы можете использовать ctags плагин: перейдите в папку репозитория, переместите курсор на папку, которую вы хотите индексировать,
   или на папку ```'..'```, если вы хотите индексировать весь репозиторий. Нажмите **F11->Ctags Source Navigator->Index selected directory**. Файл тегов будет создан внутри
   выбранной папки, и все символы будут автоматически загружены в плагин. Если у вас уже есть репозиторий, индексированный утилитой ctags, вы можете загрузить индексный файл (tags),
   чтобы сообщить плагину, что вы намерены работать с этим репозиторием. Это можно сделать несколькими способами:
    * *Ничего не делать*: при попытке поиска имени плагин сам предложит загрузить один из файлов тегов, который он нашел в родительских каталогах.
    * Переместите курсор на файл тегов и просто нажмите Enter.
    * Переместите курсор на файл тегов, нажмите **F11->Ctags Source Navigator->Load tags file**.
    * Загрузите недавно открытый файл тегов. Нажмите **F11->Ctags Source Navigator->Load from history** и выберите файл тэгов.

    Теперь плагин «знает» о символах в вашем репозитории, и вы можете использовать плагин для поиска имен.
3. По умолчанию плагин не выполняет поиск имен для репозиториев, в котором вы не находитесь в данный момент. Однако это поведение можно поменять для некоторых репозиториев, сделав их библиотечными (permanent).
   Чтобы сделать репозиторий библиотечным, перейдите в нужный репозиторий и наберите меню **F11->Ctags Source Navigator->Add permanent repository**. После этого плагин предложит вам на выбор tags файл, который соответствует репозиторию.
   Чтобы репозиторий перестал быть библиотечным, вам необходимо зайти в меню **F11->Ctags Source Navigator->Manage repositories**, выбрать соответствующий репозиторий и нажать комбинацию клавиш Ctrl+Del.
4. Откройте файл с исходным кодом, перейдите к имени, объявление которого вы хотели бы найти. Нажмите **F11->Ctags Source Navigator->Go to**, и плагин автоматически переместит курсор к
   объявлению этого имени. Если имя имеет несколько объявлений (например, в C++ определение имени также является объявлением, поэтому в этом случае у вас будет два объявления), плагин
   предложит загрузить одно из них.
5. Для автодополнения имени нажмите **F11->Ctags Source Navigator->Complete symbol**
6. Вы можете вернуться на то место откуда перешли используя планин нажав **F11->Ctags Source Navigator->Go back**
7. Перемещайтесь между текущей и предыдущей позиции используя опцию **Go back** вместе с **F11->Ctags Source Navigator->Go forward**
8. Вы можете просмотреть список членов класса. Для этого установите курсор на имя класса, а затем нажмите **F11->Ctags Source Navigator->Class members**
9. Вы можете просмотреть имена, объявленные в текущем файле. Нажмите **F11->Ctags Source Navigator->Search name in opened file** и начните печатать (или вставьте из буфера обмена) имя, которое вы хотите найти.
10. Вы можете искать имена по всему репозиторию. Эта опция работает как в главном меню плагина так и в меню редактора. Нажмите **F11->Ctags Source Navigator->Search name in entire repository** и начните печатать (или вставьте из буфера обмена) имя, которое вы хотите найти.
11. Вы можете искать и открывать файлы, которые были проиндексированы. Эта опция доступна в главном меню и в меню редактора, нажмите **F11->Ctags Source Navigator->Search file by name** и начните печатать (или вставьте из буфера обмена) имя файла, который хотите открыть.
12. Вы можете отфильтровать любую выдачу, а так же результаты фильтрации. Нажмите **Tab** в любом меню с выбором имен или файлов чтобы начать фильтровать результат, **Esc** - чтобы перейти к предыдущему фильтру и **Ctrl+Z** чтобы закрыть меню.
13. Если кодовая база устарела, вы можете переиндексировать репозиторий. Нажмите **F11->Ctags Source Navigator->Reindex repository** в главном меню или в меню editor и плагин предложит вам переиндексировать один из tags файлов, которые он нашел. **Если во время индексирования что-то пойдет не так (например, вы отмените операцию) плагин откатится к старому tags файлу.**
## Горячие клавиши (доступно только если установлено)
### Редактор:
+ Go to - ```Ctrl+F```
+ Complete name - ```Ctrl+Space```
+ Go back - ```Ctrl+G```
+ Go forward - ```Ctrl+D```
+ Search name in opened file - ```Ctrl+Shift+X```
+ Search name in entire repository - ```Ctrl+Shift+T```
+ Search file by name - ```Ctrl+Shift+R```
### Главное меню:
+ Search name in entire repository - ```Ctrl+Shift+T```
+ Search file by name - ```Ctrl+Shift+R```
## Сделайте вклад в проект
Если у вас есть крутая идея новой фичи или вы обнаружили ошибку, пожалуйста, создайте [ишью](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/new). Нажмите на звездочку в правом верхнем углу страницы, если вам нравится этот плагин
## Исходный код
Код этого плагина основан на исходном коде оригинального [Ctags Source Navigator](https://github.com/trexinc/evil-programmers/tree/master/ctags) плагина. Изначально я планировал просто портировать его под третий Far. Однако чтобы плагин можно было использовать пришлось все серьезно переделать и добавить фичей (и фичей будет еще больше!). Поэтому я решил стать мантейнером плагина. Спасибо Константину Ступнику, автору оригинального плагина Ctags Source Navigator.
