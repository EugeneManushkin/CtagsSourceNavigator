# Ctags Source Navigator
[![Build status](https://ci.appveyor.com/api/projects/status/0gnelckc0b62ka35?svg=true)](https://ci.appveyor.com/project/EugeneManushkin/ctagssourcenavigator)
[![Build Status](https://travis-ci.com/EugeneManushkin/CtagsSourceNavigator.svg?branch=master)](https://travis-ci.com/EugeneManushkin/CtagsSourceNavigator)

This is a [Far Manager](https://www.farmanager.com/) plugin for browsing source code indexed by [Ctags](https://en.wikipedia.org/wiki/Ctags) utility.
## Useful links
+ [Plugin Homepage](https://github.com/EugeneManushkin/CtagsSourceNavigator)
+ [Far PlugRing](https://plugring.farmanager.com/plugin.php?pid=478)
+ [Discussion Thread](https://forum.farmanager.com/viewtopic.php?f=5&t=6394)
## Features
+ Jump to name declaration
+ Code completion
+ Searching name in entire repository
+ Searching name in currently edited file
+ Search and open files by name
+ Listing class members
+ Source code browsing history
+ Creating Ctags database for selected repository root
+ Reindexing repository.
+ Hotkeys macro
## What's new 
### 2.1.0.51
+ Supported permanent repositories which are available for navigation and name lookup in any other repository or not indexed source file (#53).
+ Improved repository management: you may navigate to the root of a loaded repository in 'Manage repositories' menu (#57).
+ Improved platform error reporting
### 2.1.0.47
+ Supported filtering search results with in any selecting menu. You may also filter filtered results. Press 'Tab' key to enter filtering mode, 'Esc' to return to previous filter and 'Ctrl+Z' to close menu (#54)
+ Improved parsing string literals for navigation (#46)
+ Supported line numbers in round brackets like ```src\plugin.cpp(1323)``` or ```src\plugin.cpp(1323):``` (#47)
+ Included regular linux build
### 2.1.0.42
+ Ctags utility installation is no longer required! Latest [Universal Ctags](https://ctags.io/) utility is included in plugins archive (#42)
+ Most recently searched names and files are shown in lookup menus. List of recently searched names and files is kept for each repository and synchronized accross multiple Far Manager instances (#33)
+ Open declaration also opens include files in string literals like ```#include <src/include/my_file.h>``` or ```#include "src\my_file.h"```. This option is renamed to "Go to" (#41)
+ You may now search indexed files by full path or partial path. Also you may specify line number in search string using colon symbol like ```src/include/my_file.h:1234```
+ Fixed bugs
## Installation
1. Download latest release from [release page](https://github.com/EugeneManushkin/CtagsSourceNavigator/releases)
2. Extract downloaded archive to FarManager/Plugins folder
3. Install hotkeys macro by copying ctags_hotkeys.lua script to %FARPROFILE%\Macros\scripts folder:
   ```
   copy ctags_hotkeys.lua "%FARPROFILE%\Macros\scripts"
   ```
   After copying hotkeys macro restart Far Manager
### Use custom ctags utility
   You may either install [Universal Ctags](https://ctags.io/), [Exuberant Ctags](http://ctags.sourceforge.net/) or use ctags utility from [Cygwin](http://www.cygwin.com/) project.
   Go to **F9->Options->Plugins configuration->Ctags Source Navigator**, put the full path to ctags.exe in 'Path to ctags.exe' edit box, press OK button.
## Usage
#### 1. Navigate to source code in not indexed files
You may use the plugin in an opened file even if it is not indexed by ctags utility. Plugin will automatically index currently edited file and let you search names in it
#### 2. Index your repository
If you want to search names and files in entire repository you should index it with the ctags utility. Use ctags plugin to do this: goto your repository folder, navigate cursor to folder you want to index by ctags or navigate to top folder ("..") if you want to index entire repository folder. Press **F11->Ctags Source Navigator->Index selected directory**. Tags file will be created inside selected folder and all symbols will be automatically loaded in plugin. If you already have a repository indexed by ctags you may load index file ('tags') to tell the plugin that you want to browse this repository. Do either:
* *Do not do anything*: on attempt to search a name plugin will suggest to load one of the 'tags' files found in parent directories.
* Navigate cursor to the 'tags' file, and just press Enter.
* Navigate cursor to the 'tags' file, press **F11->Ctags Source Navigator->Load tags file** (the most boring way to load tags).
* Load recently opened 'tags' file. Press **F11->Ctags Source Navigator->Load from history** and select your tags file.

Now plugin 'knows' about symbols in your repository and you can browse it's source code and lookup names in it.
#### 3. Permanent repositories
By default plugin does not allow to search names from places that are not belong to a certain repository. However this behaviour may be changed by making a repository permanent.
Navigate to a repository folder and press **F11->Ctags Source Navigator->Add permanent repository**. Plugin will suggest you to select one of the tags files which corresponds to a selected repository.
To unload permanent repository open menu **F11->Ctags Source Navigator->Manage repositories**, select permanent repository and then press Ctrl+Del.
#### 4. Go to declaration/definition (Ctrl+F)
Open a file inside your repository folder, navigate to a name you want to search definition/declaration of. Press **F11->Ctags Source Navigator->Go to** and plugin will automatically move the cursor to declaration of that name. If a name has multiple declarations (e.g. in C++ definition of a name is also a name declaration) plugin will suggest to load one of the declaration
#### 5. Open include file (Ctrl+F)
Open a file inside your repository folder, set cursor to an include file inside angle brackets or to any quoted file path. Press **F11->Ctags Source Navigator->Go to** and plugin will open the file
#### 6. Autocomplete name (Ctrl+Space)
Start typing name in editor and then press **F11->Ctags Source Navigator->Complete symbol**
#### 7. Return cursor to previous position (Ctrl+G)
Press **F11->Ctags Source Navigator->Go back** to return cursor to previous position
#### 8. Return to current position from previous position (Ctrl+D)
If you moved the cursor to a previous position by using "Go Back" option you may return the cursor back. To do this press **F11->Ctags Source Navigator->Go forward**
#### 9. List all class members
Set cursor on class name and then press **F11->Ctags Source Navigator->Class members**
#### 10. Search names defined in current file (Ctrl+Shift+X) 
Press **F11->Ctags Source Navigator->Search name in opened file** and begin typing (or insert by Ctrl+V or Shift+Ins) a name you want to search.
#### 11. Search names defined in entire repository (Ctrl+Shift+T)
This option works in plugin main menu and in editor menu. Press **F11->Ctags Source Navigator->Search name in entire repository** and begin typing (or insert by Ctrl+V or Shift+Ins) a name you want to search.
#### 12. Search indexed file (Ctrl+Shift+R)
This feature is available from main and editor menu: press **F11->Ctags Source Navigator->Search file by name** and begin typing a file name you want to open. You may specify a partial path to file with a filename to filter search result by location: ```tests\tags_cache\main.cpp```.
You may also specify a line number after a colon symbol like ```my_file.h:1234```
#### 14. Filter results in any select menu (Tab - new filter, Esc - return to previous filter, Ctrl+Z - close menu)
You may filter any results and even filter your filtered results in any select menu. Press **Tab** to begin new filter, **Esc** or **Backspace** to return to previous filter and **Ctrl+Z** to close menu.
Regular expressions are supported.
#### 15. Reindex your repository 
You may reindex your repository if current code database is outdated. Press **F11->Ctags Source Navigator->Reindex repository** in main menu or in editor menu and then plugin will suggest you to select one of the tags files which you may reindex. **If something goes wrong while reindexing (e.g. reindexing is canceled) plugin will rollback on old tags file.**
## Default macro hotkeys (available if installed)
### Editor hotkeys:
+ Go to - ```Ctrl+F```
+ Complete name - ```Ctrl+Space```
+ Go back - ```Ctrl+G```
+ Go forward - ```Ctrl+D```
+ Search name in opened file - ```Ctrl+Shift+X```
+ Search name in entire repository - ```Ctrl+Shift+T```
+ Search file by name - ```Ctrl+Shift+R```
### Main menu hotkeys:
+ Search name in entire repository - ```Ctrl+Shift+T```
+ Search file by name - ```Ctrl+Shift+R```
## Contribute
If you have an idea of a cool new feature or you have found a bug please [create an issue](https://github.com/EugeneManushkin/CtagsSourceNavigator/issues/new). Mark this repository with a star if you like the plugin
## Source code
The code of this plugin is based on source code of original [Ctags Source Navigator](https://github.com/trexinc/evil-programmers/tree/master/ctags) plugin. It was planed just to port it for Far3 API however a lot of important bugs were fixed, some functionality reworked, new features were added (and there will be more features!) so I decided to become a maintainer of this project. Thanks to author of the original Ctags Source Navigator plugin whose name is Konstantin Stupnik.
