# hanote

A lightweight sticky notes application for Linux, built with GTK4 and C.

## Features

- Multiple colored sticky notes (yellow, green, blue, pink, purple, orange, white)
- Drag and drop / copy and paste images and files
- Custom emoji picker with search
- Per-note font selection
- Markdown formatting (bold, italic, strikethrough, code, headings, lists, images)
- Clickable URLs (Ctrl+Click)
- Image resize (Ctrl+Scroll)
- User-editable configuration (`~/.config/hanote/config.uzon`)

## Dependencies

- GTK4 (>= 4.0)
- json-glib-1.0

## Build

```
make
```

## Install / Uninstall

```
sudo make install
sudo make uninstall
```

Default prefix is `/usr/local`. Override with `PREFIX=/usr`.

## Compositor setup

hanote works best as a floating window. Add a window rule for your compositor:

**Hyprland** (`~/.config/hypr/hyprland.conf`):
```
windowrulev2 = float, class:com.suhokang.hanote
```

**Sway** (`~/.config/sway/config`):
```
for_window [app_id="com.suhokang.hanote"] floating enable
```

**KDE Plasma**: Settings > Window Management > Window Rules > Add > Window class = `com.suhokang.hanote`, Force "Floating"
