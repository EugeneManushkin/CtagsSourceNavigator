
local function OpenPluginMenu(option)
  Keys('F11') 
  Menu.Select("Ctags Source Navigator")
  Keys('Enter')
  Keys(option)
end

-----------------------------------------------------------------------

Macro {
  description="Open declaration";
  area="Editor";
  key="CtrlF";
  action = function() OpenPluginMenu('1') end;
}

Macro {
  description="Complete name";
  area="Editor";
  key="CtrlSpace";
  action = function() OpenPluginMenu('2') end;
}

Macro {
  description="Go back";
  area="Editor";
  key="CtrlG";
  action = function() OpenPluginMenu('3') end;
}

Macro {
  description="Search name in opened file";
  area="Editor";
  key="CtrlShiftX";
  action = function() OpenPluginMenu('5') end;
}

Macro {
  description="Search name in entire repository";
  area="Editor";
  key="CtrlShiftT";
  action = function() OpenPluginMenu('6') end;
}

Macro {
  description="Search file by name";
  area="Editor";
  key="CtrlShiftR";
  action = function() OpenPluginMenu('7') end;
}

-----------------------------------------------------------------------

Macro {
  description="Search name in entire repository";
  area="Shell";
  key="CtrlShiftT";
  action = function() OpenPluginMenu('1') end;
}

Macro {
  description="Search file by name";
  area="Shell";
  key="CtrlShiftR";
  action = function() OpenPluginMenu('2') end;
}
