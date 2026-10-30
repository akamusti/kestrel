# AGENTS.md

Binding rules for anyone (agent or human) working in the Kestrel codebase.
Read this file completely before writing code. Precedence: **this file > README > personal taste**.

## 1. Project

**Kestrel** is a keyboard-first command palette / launcher for KDE Plasma — a Raycast
counterpart for the Linux desktop. The user presses a shortcut, types, hits Enter; the app opens.

Design goal in one sentence: *"Find and open what you meant within 100 ms, without lifting your
hands off the keys."*

- A launcher's fate is decided by **open latency** and **per-keystroke latency**. Those are not
  features; they are the product.
- Primary target: KDE Plasma 6 / Wayland. X11 is secondary.
- License: GPL-2.0-or-later (compatible with the KDE ecosystem).

## 2. Stack

| Layer | Technology | Note |
|---|---|---|
| Language | C++20 | `CMAKE_CXX_EXTENSIONS OFF` |
| UI | Qt 6.11 + QML (Qt Quick, Qt Quick Controls "Basic") | Kirigami optional, not a dependency yet |
| IPC | QtDBus | KDE KGlobalAccel global shortcut |
| Build | CMake ≥ 3.21 + Ninja | uses `qt_add_qml_module` |
| Test | Qt Test | `ctest` |
| Platform | KDE Plasma 6, Wayland | KF6 libraries are a later optional layer |

**Dependency rule:** a new third-party dependency that lands on the hot path (startup, per-keystroke
search) is rejected. Heavy stacks (Electron, WebEngine, React) are not used here.

KF6 development packages (`kf6-kirigami-devel`, `kf6-kservice-devel`, `kf6-kglobalaccel-devel`) are
not installed; the core is built on plain Qt6. When KF6 integration is added it must be conditional
(`find_package(KF6 ... OPTIONAL_COMPONENTS ...)`), never mandatory.

## 3. Commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/kestrel                     # run the app
ctest --test-dir build --output-on-failure
```

Fast headless verification (catches QML errors without a session):

```bash
QT_QPA_PLATFORM=offscreen ./build/kestrel --quit-after-first-frame
```

## 4. Directory map

```
CMakeLists.txt          # single build definition; update when adding a .cpp
AGENTS.md               # this file — the rules
README.md               # user-facing intro and roadmap
docs/re/                # reverse-engineering notes (Raycast behaviour, KRunner contracts)
src/
  main.cpp              # QGuiApplication, style, image provider, QML loading, single instance
  desktopentry.{h,cpp}  # .desktop parsing, XDG scan, Exec field codes
  appindex.{h,cpp}      # application index: background scan, entry list for QML
  ranker.{h,cpp}        # fuzzy matcher, thresholds, frecency, search-term tiers
  usagestore.{h,cpp}    # frecency anchors, learned terms, aliases (QSettings, deferred flush)
  searchmodel.{h,cpp}   # query -> ranked results (sections) + launch()
  launcher.{h,cpp}      # QProcess launcher, terminal wrapping
  iconimageprovider.{h,cpp}  # image://icon/<theme-name> resolver
qml/
  Main.qml              # window, search bar, list, shortcuts, footer
  ResultDelegate.qml    # single result row
  ShortcutHint.qml      # footer key-cap chip
tests/
  tst_core.cpp          # matcher, threshold, frecency and .desktop parsing tests
```

Layer rule: **QML never contains business logic.** Parsing, ranking and process launching live in
C++; QML only binds and paints.

## 5. Performance budget (violation = bug)

| Measurement | Budget |
|---|---|
| Cold open (shortcut → first frame) | ≤ 120 ms |
| Keystroke → updated result list | ≤ 16 ms (one frame) |
| Disk I/O while typing | 0 (index must already be in memory) |
| Idle memory | ≤ 80 MB RSS |
| Typical .desktop scan (300 files) | ≤ 50 ms, off the main thread |

Forbidden:
- `QFile`/`QSettings` reads or writes on the search path.
- Synchronous directory scans on the main thread.
- Rebuilding the whole model on every keystroke when a score update would do.
- Triggering heavy work from `Component.onCompleted` in QML.

## 6. Code style

C++:
- Qt/KDE-adjacent: types `PascalCase`, functions `camelCase`, members `m_camelCase`,
  constants `kPascalCase`, files `kebab-case`.
- 4 spaces, 100 columns, prefer `QStringLiteral`, take `const QString &` parameters.
- No raw `new`/`delete`; rely on QObject ownership or value semantics.
- Connect signals with pointer-to-member syntax (`connect(a, &A::x, b, &B::y)`).
- Prefer early returns; nesting depth stays below 3.
- All user-visible text is wrapped in `qsTr()` in QML (i18n-ready).
- When adding a `.cpp`, add it to `kestrel_core` in `CMakeLists.txt`.

QML:
- Minimal logic; move complex expressions into C++.
- Constants are `readonly property`; no magic numbers.
- One component per file, one responsibility; split QML above 200 lines.
- Animations: 90–160 ms, `Easing.OutCubic`; animate only composite properties (`opacity`,
  `scale`, `transform`). Animating layout-driven `width`/`height` is janky on Wayland.

## 7. Testing rules

- If ranking (`fuzzyScore`, `passesThreshold`, `frecencyScore`) or `.desktop` parsing changes,
  add or update tests.
- Every bug fix starts with a test that reproduces the bug.
- No commit while `ctest` is red.

## 8. Commits and branches

- Conventional Commits: `feat:`, `fix:`, `perf:`, `refactor:`, `test:`, `docs:`, `build:`, `chore:`.
- Commit messages are written in English, always.
- One commit, one logical change. Subjects like "WIP", "fixes", "update" are rejected.
- The body explains *why*; *what* is in the diff.
- `main` is never knowingly left broken: build and tests must be green before you hand back.

## 9. Roadmap (ordered)

1. [x] Skeleton: window, search, application list, Enter to launch
2. [x] Global shortcut (KDE `kglobalaccel` over D-Bus) + single-instance toggle
3. [x] Frecency + learned search terms (persisted, deferred flush)
4. [ ] KRunner bridge: query `org.kde.krunner1` runners over D-Bus (windows, calculator, units)
5. [ ] Action panel (Ctrl+K), filters (Ctrl+P), fallback web search
6. [ ] Calculator / unit / date inline results
7. [ ] File search (Baloo/`kio` or an `fd` subprocess)
8. [ ] Clipboard history and snippets
9. [ ] Extension API (Raycast-shaped manifest, JSON-RPC process runtime)
10. [ ] Settings UI, Breeze palette, i18n

Stay inside this order. If a change jumps the queue, justify it in the commit body.

## 10. Working protocol for agents

1. Read the relevant files first; never edit by guesswork.
2. Progress in small, compilable steps; run `cmake --build build` after each.
3. Behaviour changes come with tests; do not claim "done" without running them.
4. Keep user-visible strings English and `qsTr()`-wrapped; do not mix languages.
5. If a change affects the performance budget, report the measurement command and result.
6. Never add a dependency silently; call out `CMakeLists.txt` changes in the report.
7. Never mark something as done when it is not; say plainly what is unfinished.
8. AppImage packages are built by the repository's GitHub Actions workflow, not by an unverified
   local packaging substitute. When starting an AppImage workflow, check its run to completion;
   report failures and the failing step to the user, and do not claim an AppImage is available until
   the run succeeds and uploads its artifact.
9. Before finishing every code change, review the changed files and final diff for credentials,
   access tokens, API secrets, and personal data. Never commit real secrets or unnecessary personal
   data. Put required local credentials in an ignored `.env` file, provide only placeholder values
   in `.env.example`, and read secrets from the environment at runtime; remove or anonymize personal
   data rather than moving it into `.env`.
10. When asked to commit and push, use an English Conventional Commit message and push the verified
    change to the configured GitHub remote. Include this trailer in every commit unless the user
    explicitly opts out:

    ```text
    Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
    ```
