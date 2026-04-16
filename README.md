# QDock 🚀

A macOS-style application dock for Linux, built with C++ and Qt6.

## Features

### 🎯 Core Features (macOS parity)
- **App launching** — click to open, click again to focus running window
- **Running indicators** — dots below/beside icons show open windows
- **Window management** — right-click to see all open windows per app
- **Bounce animation** — icons bounce when launching applications
- **Icon magnification** — smooth zoom effect when hovering (like macOS)
- **App labels** — tooltip-style labels appear on hover
- **Pinned apps** — keep favourite apps in the dock permanently
- **Running apps** — uninstalled/unpinned running apps appear automatically
- **Context menus** — right-click for macOS-style app menus
- **Quit/close windows** — close apps directly from the dock
- **New window** — middle-click or right-click → "New Window"
- **Show/hide** — auto-hide with IntelliHide support

### 📐 Positioning
- **Bottom** (default, macOS-style)
- **Top**
- **Left**
- **Right**
- Multi-monitor support (select which screen)

### 🎨 Appearance
- Translucent frosted glass background
- Adjustable opacity
- Configurable icon size (24–96px)
- Magnification factor control
- Running indicator dots
- App label tooltips
- Drop shadows on icons
- Rounded corners (configurable radius)
- Custom border color/opacity

### ⚙️ Behavior
- **Always Visible** — dock is always shown
- **Auto-Hide** — hides when mouse leaves, slides back in
- **IntelliHide** — hides only when a window overlaps

### 🔧 System Integration
- Reads `.desktop` files from standard XDG paths
- Loads icons from freedesktop icon themes
- X11 EWMH integration (struts, window type, state)
- Sets `_NET_WM_WINDOW_TYPE_DOCK` for proper WM handling
- `_NET_WM_STRUT_PARTIAL` so maximized windows don't overlap
- Polls `_NET_CLIENT_LIST` to track running applications
- Activates windows via `_NET_ACTIVE_WINDOW` messages
- Closes windows via `_NET_CLOSE_WINDOW` messages

## Requirements

### Runtime
- Linux with X11 (Xorg)
- Qt6 (Core, Gui, Widgets, DBus)
- freedesktop-compliant desktop environment

### Build
- CMake 3.16+
- GCC or Clang with C++17
- Qt6 development headers
- X11 development libraries

## Installation

### Ubuntu / Debian
```bash
sudo apt install qt6-base-dev libx11-dev cmake build-essential
./install.sh
```

### Fedora / RHEL
```bash
sudo dnf install qt6-qtbase-devel libX11-devel cmake gcc-c++
./install.sh
```

### Arch Linux
```bash
sudo pacman -S qt6-base libx11 cmake base-devel
./install.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

## Configuration

QDock saves its configuration to:
```
~/.config/qdock/qdock.conf
```

### Right-click the dock background for quick settings:
- Change position (Bottom/Top/Left/Right)
- Open full Preferences dialog

### Preferences Dialog (right-click → Dock Preferences)

**Appearance Tab:**
- Position & display selection
- Icon size slider
- Magnification toggle + strength
- Background opacity
- Show/hide indicators, labels, shadows
- Corner radius

**Behavior Tab:**
- Always visible / Auto-hide / IntelliHide
- Bounce on launch
- Recent apps

**Apps Tab:**
- Manage pinned applications
- Browse all installed apps
- Pin/unpin with one click

## Usage

### Launching
```bash
qdock
```

### Autostart
The installer adds `qdock.desktop` to `~/.config/autostart/`.

To remove autostart:
```bash
rm ~/.config/autostart/qdock.desktop
```

### Mouse Controls
| Action | Result |
|--------|--------|
| Left click (not running) | Launch app |
| Left click (running) | Focus/raise window |
| Middle click | Open new instance |
| Right click on app | Context menu |
| Right click on background | Dock settings |
| Hover | Show label + magnification |

### Context Menu (right-click app)
- App name (header)
- Keep in Dock / Remove from Dock
- Open / New Window
- Window list (if multiple windows open)
- Quit

## Architecture

```
src/
├── main.cpp              — Entry point
├── DockConfig.h          — Configuration structures
├── DockWindow.h/.cpp     — Main dock window, layout, WM integration
├── DockItem.h/.cpp       — Individual icon widget (animations, events)
├── ApplicationScanner.h/.cpp — Scans .desktop files, loads icons
├── SettingsDialog.h/.cpp — Preferences UI
└── TrashItem.h           — Trash/special items (placeholder)
```

## Known Limitations

- X11 only (Wayland support planned)
- Some DE-specific features (like Dash-to-Dock integration) not available
- Application matching relies on `StartupWMClass` in .desktop files; some apps may not be recognized automatically

## License

MIT License — free to use, modify, and distribute.
