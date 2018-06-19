# Ctags Source Navigator
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
+ Search and open files by name (equivalent to "Open resource" command in Eclipse CDT)
+ Supported tags with absolute paths (ctags.exe ... -R C:\full\path\to\repository)
+ Provided hotkey macro
+ Enhanced user interface
+ Fixed bugs
## Requirements
+ Far3
+ Ctags utility
## Installation
1. Download latest release
2. Extract downloaded archive to FarManager/Plugins folder
3. Install ctags utility. You may either install [Universal Ctags](https://ctags.io/), [Exuberant Ctags](http://ctags.sourceforge.net/) or use ctags utility from [Cygwin](http://www.cygwin.com/) project.
4. *[Optional: skip this step if you don't want to create index files by the plugin or if ctags utility is available from the command line (is written in %PATH% environment variable).]*
   Go to **F9->Options->Plugins configuration->Ctags Source Navigator**, put the full path to ctags.exe in 'Path to ctags.exe' edit box, press OK button.
5. *[Optional: skip this step if you don't want to use plugin default hotkeys macro.]* Copy ctags_hotkeys.lua script to %FARPROFILE%\Macros\scripts folder. You may do this in Far Manager: goto extracted plugin folder and type
   ```
   copy ctags_hotkeys.lua "%FARPROFILE%\Macros\scripts"
   ```
   After copying hotkeys macro restart Far Manager
## Usage
1. First you need to create a 'tags' file for a repository. You may use ctags plugin to do this: goto your repository folder, navigate cursor to folder you want to index by ctags or navigate to top folder ("..") if you want to index entire repository folder. Press **F11->Ctags Source Navigator->Tag selected directory**. Tags file will be created inside selected folder and all symbols will be automatically loaded in plugin.
2. If you already have a repository indexed by ctags you may load index file ('tags') to tell the plugin that you want to browse this repository. Do either:
    * *Do not do anything*: on attempt to search a name plugin will suggest to load one of the 'tags' files found in parent directories.
    * Navigate cursor to the 'tags' file, and just press Enter.
    * Navigate cursor to the 'tags' file, press **F11->Ctags Source Navigator->Load tags file** (the most boring way to load tags).
    * Load recently opened 'tags' file. Press **F11->Ctags Source Navigator->Load from history** and select your tags file.

   Now plugin 'knows' about symbols in your repository and you can browse it's source code and lookup names in it.
3. Open a file inside your repository folder, navigate to a name you want to search definition/declaration of. Press **F11->Ctags Source Navigator->Open declaration** and plugin will automatically move the cursor to declaration of that name. If a name has multiple declarations (e.g. in C++ definition of a name is also a name declaration) plugin will suggest to load one of the declaration
4. For autocomplete names press **F11->Ctags Source Navigator->Complete symbol**
5. Return cursor to previous position by pressing **F11->Ctags Source Navigator->Go back**
6. List all class members. Set cursor on class name and then press **F11->Ctags Source Navigator->Class members**
7. Search names defined in current file. Press **F11->Ctags Source Navigator->Search name in opened file** and begin typing (or insert by Ctrl+V or Shift+Ins) a name you want to search.
8. Search names defined in entire repository. This option works in plugin main menu and in editor menu. Press **F11->Ctags Source Navigator->Search name in entire repository** and begin typing (or insert by Ctrl+V or Shift+Ins) a name you want to search.
9. Search and open indexed file. This feature is available from main and editor menu: press **F11->Ctags Source Navigator->Search file by name** and begin typing a file name you want to open.
10. You may reindex your repository if current code database is outdated. Press **F11->Ctags Source Navigator->Reindex repository** in main menu or in editor menu and then plugin will suggest you to select one of the tags files which you may reindex. **If something goes wrong while reindexing (e.g. reindexing is canceled) plugin will rollback on old tags file.**
## Default macro hotkeys (available if installed)
### Editor hotkeys:
+ Open declaration - ```Ctrl+F```
+ Complete name - ```Ctrl+Space```
+ Go back - ```Ctrl+G```
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
