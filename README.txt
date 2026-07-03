Alt+Tab Plus
============

ATP is a tiny alternative to classic Windows Alt+Tab.

Click the tray icon to open configuration file (created if doesn't exist).
Right-click to terminate.

Features:

  * Window filtering by title (Alt+Backtick); Ctrl+Backspace deletes the word
  * List navigation using mouse, arrows, [Alt/Shift+]Tab, Home/End, Page Up/Down
  * Portable (an optional INI file is read, never writes)
  * Lightweight (barebone C++/WinAPI/GDI)

Customizations:

  * Number of rows and columns
  * Colors, margins, sizes and fonts
  * Hiding minimized windows and/or ones centered on other monitors
  * Various window activation methods:
    * SwitchToThisWindow (0): https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-switchtothiswindow
    * AttachThreadInput (1): https://stackoverflow.com/questions/19136365/win32-setforegroundwindow-not-working-all-the-time
    * SendInput (2): https://github.com/microsoft/PowerToys/blob/4cb72ee126caf1f720c507f6a1dbe658cd515366/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L191

Limitations:

  * No window thumbnails
  * Generic icons for Modern/Metro apps
  * Not handling VMware Workstation windows with a running VM (VMware intercepts
    keyboard at a very low level preventing global hooks from triggering)
    * Run ATP under Administrator to handle regular elevated windows (MMC, Task 
      Manager, Command Prompt, etc.)

Inspired by https://github.com/sigoden/window-switcher
