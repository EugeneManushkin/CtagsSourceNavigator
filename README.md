# Ctags Source Navigator
This is a Far Manager plugin for browsing source code indexed by [Ctags](https://en.wikipedia.org/wiki/Ctags) utility.
## Installation
1. Download latest release
2. Extract downloaded archive to FarManager/Plugins folder
3. Install ctags utility. You may either download standalone [Exuberant Ctags](http://ctags.sourceforge.net/) utility for Windows or 
   use ctags utility from [Cygwin](http://www.cygwin.com/) project.
4. If ctags utility is available from command line (is written in %PATH% environment variable) you are not required to do anything. Otherwise configure installed plugin: 
   go to F9-&gt;Options-&gt;Plugins configuration-&gt;Ctags Source Navigator, put the full path to ctags.exe in 'Path to ctags.exe' edit box, press OK button.
## Usage
1. First you need to create a 'tags' file for a repository. You may use ctags plugin to do this: goto your repository folder, navigate cursor to folder you want to index by ctags or
   navigate to top folder ("..") if you want to index entire repository folder. Press F11-&gt;Ctags Source Navigator-&gt;Tag selected directory. Tags file will be created inside
   selected folder and all symbols will be automatically loaded in plugin. If you already have repository indexed by ctags you may add index file ('tags') to tell the plugin that 
   you want browse this repository. Navigate cursor to the 'tags' file, press F11-&gt;Ctags Source Navigator-&gt;Load tags file. Now plugin 'knows' about symbols in this repository 
   and you can browse it source code and lookup names in it. If you previously browsed some repositories and restarted Far Manager you may load tags files from history. Just press 
   F11-&gt;Ctags Source Navigator-&gt;Load from history and select your tags file
2. Open a file inside your repository folder, navigate to a name you want to search definition/declaration of. Press F11-&gt;Ctags Source Navigator-&gt;Find symbol and
   plugin will automatically move the cursor to definition of that name.
3. You can lookup all names defined in current file. Press F11-&gt;Ctags Source Navigator-&gt;Browse symbols in file
4. For autocomplete names press F11-&gt;Ctags Source Navigator-&gt;Complete symbol
## Source code
The code of this plugin is based on source code of original [Ctags Source Navigator](https://github.com/trexinc/evil-programmers.git) plugin. It was planed just to port it for Far3
API however a lot of important bugs were fixed, some functionality reworked, new features were added (and there will be more features!) so I decided to become a maintainer of
this project. Thanks to author of the original Ctags Source Navigator plugin whose name is Konstantin Stupnik.