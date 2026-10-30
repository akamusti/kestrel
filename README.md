# Kestrel

A keyboard-first command palette for KDE Plasma — a Raycast counterpart for the Linux desktop.

Press a shortcut, type, hit Enter: the thing you meant opens. Kestrel is built around two numbers,
not around features — how fast the palette appears (**≤ 120 ms** from shortcut to first frame) and
how fast it answers a keystroke (**≤ 16 ms**, one frame). Everything else is negotiable.

![Kestrel showing the suggestions list](docs/screenshot.png)

- C++20 + Qt 6 (Quick/QML), no Electron, no WebEngine.
- 130+ applications indexed in the background at startup; typing never touches the disk.
- Ranking follows Raycast's model: fuzzy match class → score → frecency → collation.
- Frecency and learned search terms persist between launches.
- Installed KRunner D-Bus runners provide asynchronous results for apps, calculations, windows,
  units and other Plasma runner features.
- KDE global shortcut: `Meta+Space` toggles the running palette through KGlobalAccel.
- Single instance: a second launch toggles the running window instead of starting a new one.

Status: early. Applications launch, ranking, frecency and KRunner D-Bus results work; see the
[Roadmap](#roadmap) for what's next.

## Requirements

| | |
|---|---|
| Qt | 6.6 or newer (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Concurrent`, `DBus`, `Test`) |
| CMake | 3.21 or newer |
| Build tool | Ninja (recommended) |
| Compiler | GCC 11+ or Clang 14+ (C++20) |

On Fedora: `sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel cmake ninja-build gcc-c++`.
On Arch: `sudo pacman -S qt6-base qt6-declarative cmake ninja`.
On Debian/Ubuntu the packaged Qt is older than 6.6; use the AppImage below or a newer distribution.

## Build from source

```bash
git clone https://github.com/akamusti/kestrel.git
cd kestrel
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/kestrel
```

Run the tests with:

```bash
ctest --test-dir build --output-on-failure
```

If the window does not appear, check the QML side headlessly first — this catches QML errors without
a desktop session:

```bash
QT_QPA_PLATFORM=offscreen ./build/kestrel --quit-after-first-frame
```

## Installing

There are two ways to get Kestrel:

- **AppImage (recommended when a successful build is available):** download a ready-to-run file
  built by GitHub Actions. You do not need to install Qt or compile the source code.
- **Build from source:** compile Kestrel yourself and optionally install it into your home directory.
  This is useful for development and requires the packages listed under [Requirements](#requirements).

### AppImage: download and run

For each version tag, GitHub Actions builds and tests Kestrel, then creates a GitHub Release and
attaches the AppImage if every step succeeds. To install the latest published version:

1. Open [Kestrel Releases](https://github.com/akamusti/kestrel/releases).
2. Open the latest version and download the file ending in `.AppImage` under **Assets**. If no
   release is listed yet, the first build may still be running or may have failed; check the
   repository's **Actions → AppImage** page for its status.
3. Make an `Applications` folder in your home directory if you do not already have one. In a
   terminal, run:

   ```bash
   mkdir -p ~/Applications
   ```

4. Move the downloaded AppImage into `~/Applications`. In a terminal, make it executable and run
   it. Replace the filename below with the exact name you downloaded:

   ```bash
   chmod +x ~/Applications/Kestrel-v0.2.2-x86_64.AppImage
   ~/Applications/Kestrel-v0.2.2-x86_64.AppImage
   ```

To update, download the AppImage from a newer release and replace the old file. You do not need Qt
or a compiler to use the AppImage.

GitHub shows a failed run with a red X and identifies the failed step in its log. To receive
notifications about failed runs, enable GitHub Actions notifications in your GitHub notification
settings; the workflow does not send email on its own. A manually started workflow run also provides
a temporary **Kestrel-AppImage** artifact, but only a successful version-tag run creates a Release.

### Build from source and install for your user

Follow [Build from source](#build-from-source), then run these commands from the Kestrel source
directory to install it under `~/.local`:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

This installs the program, a KDE application-menu entry, and its icon. Kestrel should then appear in
the application menu. If the `kestrel` command is not found, make sure `~/.local/bin` is in your
`PATH`, or use the full path `~/.local/bin/kestrel`.

To install system-wide instead, configure with `-DCMAKE_INSTALL_PREFIX=/usr` and run
`sudo cmake --install build`.

### Distribution packages

Not available yet. Packaging for Fedora/openSUSE/Arch follows once the feature set stabilises.

## Global shortcut (KDE Plasma 6)

Kestrel registers its built-in `Meta+Space` shortcut while it is running. On first launch, it asks
whether it may add a user-level autostart entry so the app is already running (hidden) after sign-in
and the shortcut works immediately. This only writes to your own `~/.config/autostart` directory;
it does not require administrator permission. Choose **Not now** to skip it, or remove **Kestrel**
from **System Settings → Autostart** later.

If you prefer not to keep Kestrel running in the background, you can instead create a KDE command
shortcut that starts it on demand:

#### Manual alternative: make a KDE shortcut that starts or toggles Kestrel

1. First make sure Kestrel runs by opening it once. For an AppImage, open the file you downloaded;
   for a source install, open **Kestrel** from the application menu.
2. Open **System Settings** and select **Shortcuts** (in some Plasma versions this is under
   **Keyboard**).
3. Select **Add New… → Command or Script…**.
4. Enter a name such as `Kestrel`.
5. In **Command**, enter the full path to the app followed by `--toggle`. For example:

   ```text
   /home/alex/Applications/Kestrel.AppImage --toggle
   ```

   For a source install, use `/home/alex/.local/bin/kestrel --toggle` instead. Replace `alex` with
   your Linux username. If the path contains spaces, put the path to the app in double quotes, for
   example `"/home/alex/My Apps/Kestrel.AppImage" --toggle`.
6. Select the shortcut field, press the key combination you want (for example `Meta+Space`), then
   select **Apply**.
7. Test it: close Kestrel, press the new shortcut to start it, then press the shortcut again to hide
   it. The `--toggle` command starts Kestrel if it is closed and toggles the already-running instance.

If Plasma reports that the key is already in use, choose another combination or change the
conflicting shortcut. Do not assign the same key to both Kestrel's built-in action and a KDE command
shortcut; change one of them under **System Settings → Keyboard → Shortcuts** to avoid a conflict.
`Alt+Space` is commonly used by KRunner.

#### Use Kestrel's built-in shortcut

Start Kestrel from the application menu or by opening the AppImage. While it is running, its
default `Meta+Space` shortcut toggles the palette. You can change this action under **System
Settings → Keyboard → Shortcuts → Kestrel**. If the app is closed, this built-in shortcut cannot
start it; use the KDE command shortcut above for that.

If a shortcut does not work, first check that its command starts Kestrel when run from a terminal.
For example:

```bash
~/Applications/Kestrel-*.AppImage --toggle
```

Notes:

- The first start after login indexes applications; later toggles stay warm while Kestrel remains
  running.
- The command runs as your user in your session; running it from another user or over SSH will not
  reach the running window.

## Keyboard shortcuts

| Key | Action |
|---|---|
| Type | Filter applications |
| `↑` `↓` | Move selection |
| `PageUp` `PageDown` | Jump a page |
| `Enter` | Launch the selected application |
| `Ctrl+1` … `Ctrl+9` | Launch the n-th result directly |
| `Ctrl+Shift+C` | Copy the selected name |
| `Esc` | Clear the query, or close when it is empty |
| `Shift+Esc` | Close and reset |
| `Ctrl+R` | Rescan the application index |

## How ranking works

Each application is scored four times — name, keywords, generic name, comment — using a dynamic
programming fuzzy matcher that rewards word starts and penalises gaps. A match must clear Raycast's
*medium* sensitivity threshold to be shown. Surviving results are sorted by match class first (exact
alias, exact learned term, exact title, prefix, overbounds), then score, then **frecency**: an
exponential score with a 10-day half-life, +100 points per launch. Queries that opened an item stay
associated with it for 17 days, so `ff` keeps meaning Firefox.

The observation notes behind these numbers, including the KRunner D-Bus contracts, live in
[`docs/re/raycast-findings.md`](docs/re/raycast-findings.md).

## Roadmap

1. [x] Skeleton: window, search, application list, Enter to launch
2. [x] Global shortcut (`kglobalaccel` over D-Bus) + single-instance toggle
3. [x] Frecency + learned search terms (persisted, deferred flush)
4. [x] KRunner bridge: query `org.kde.krunner1` runners over D-Bus (windows, calculator, units)
5. [ ] Action panel (`Ctrl+K`), filters (`Ctrl+P`), fallback web search
6. [ ] Calculator / unit / date inline results
7. [ ] File search (Baloo or an `fd` subprocess)
8. [ ] Clipboard history and snippets
9. [ ] Extension API (Raycast-shaped manifest, JSON-RPC process runtime)
10. [ ] Settings UI, Breeze palette, i18n

## Development

Read [`AGENTS.md`](AGENTS.md) before touching the code — it defines the layering, the performance
budget and the commit rules. Short version: business logic in C++, QML only binds and paints, no disk
I/O on the search path, and nothing is "done" until `ctest` is green.

Quick smoke test after a change:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
./build/kestrel --grab /tmp/kestrel.png     # renders one frame to a PNG, then exits
```

## License

GPL-2.0-or-later — see [`LICENSE`](LICENSE).
