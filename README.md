# Win32-GUI-Windows-Framework
Basic C++ Windows GUI Framework

**NOTE: _No code is available yet._ The code is too buggy and messy to put into repository right now.** 

### Relatively small GUI framework for Windows created by ME!

## Supported Features

- Mouse Events are working (Mouse Tracking & Mouse Clicks)
- Buttons
- Video Player
- Audio Player
- Text Rendering
- Drag Drop for files.
- Scrollbar (partly working)
- Images
- Fonts (Some fonts)
- Progress Bar
- Text Boxes

## Upcoming features

- Drag and Drop for interactive in-window objects.
- Persistant storage for GUI Drag and Drop layout (You can save the layout of a GUI created using the Drag and Drop feature).
- Add Keyboard event support.
- Add support for Menus (including drop-down menus).
- Add support for coping data to the clipboard.
- Support for custom Window Themes.
- Support for high performance font rendering (DirectWrite).
- Support for high performance 2D Graphics output (Direct2D). 
- Video player & Audio player class will support playing media directly from memory.

## TODO List (before uploading the code to the repository):

- [x] Remove C-style casts.
- [] Refactor the NativeWindow class for easier creation of varied type of Windows and improved handling of events.
- [] Refactor the Window class for better support with Themeing.
- [x] Add a "Service" class for easier handling of API's. 
- [] Refactor the Video Player & Audio player class to use Microsoft Media Foundation (MMF) API instead of Windows Media Player COM / ActiveX objects.
- [] Migrate the event system from EventProxy class into the NativeWindow class. (effectively destroying the EventProxy class)
- [] Refactor the event handling system.
