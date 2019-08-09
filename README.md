# Win32-GUI-Windows-Framework
Basic C++ Windows GUI Framework

## DISCLAIMER: This is unfinished code. (It will be refactored soon)

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

- Drag and Drop for interactive in-window objects...
- Persistant storage for GUI Drag and Drop layout (You can save the layout of a GUI created using the Drag and Drop feature).
- Add Keyboard event support
- Support for custom Window Themes.
- High performance font rendering (Direct2D).
- Video player & Audio player class will support for playing media from memory.

## TODO:

- Refactor the Video Player class to use Microsoft Media Foundation (MMF) API instead of Windows Media Player COM / ActiveX plugin.
- Migrate the event system from EventProxy class into the Window class.
- Remove the EventProxy class.
