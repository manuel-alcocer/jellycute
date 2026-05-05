# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`jellycute` is a Qt6 (Widgets) desktop client for Jellyfin media servers, with playback handled in-process by libmpv via `mpv_render_context` on a `QOpenGLWidget`. C++20, CMake + Ninja, single executable (no library, no install rules).

## Build & run

The build directory is `build/` (in-tree, gitignored). Configure once, then re-build incrementally:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build           # incremental build
./build/jellycute             # run
```

There is no test suite, no linter config, and no install target. Reconfigure (`cmake -S . -B build ...`) is only needed when `CMakeLists.txt` changes — adding a new `.cpp` requires editing the `add_executable` list there.

System dependencies (Arch package names): `qt6-base` (Widgets, Network, OpenGLWidgets ≥ 6.5), `mpv` (libmpv via pkg-config), `cmake`, `ninja`. Qt's AUTOMOC/AUTOUIC/AUTORCC are on, so MOC files are generated under `build/jellycute_autogen/` automatically — no manual `moc` invocations.

## Architecture

The app is structured around a single `MainWindow` that owns a `QStackedWidget` with two pages: a `BrowserWidget` (library navigation) and an `MpvWidget` wrapped in a player page with a custom control bar. `JellyfinClient` is a single shared instance owned by `main()` and passed by pointer to all UI components — every HTTP request flows through it.

### Components

- **`main.cpp`** — bootstraps Qt, requests an OpenGL 3.3 Compatibility surface format *before* `QApplication` (libmpv's render API needs ≥ 3.x), forces `LC_NUMERIC=C` *after* `QApplication` (Qt resets it; libmpv's option parser requires C locale), applies a global Breeze-inspired stylesheet + palette, picks `Oxygen`/`Breeze`/`Fusion` style in that order, and chooses between auto-login (from `QSettings`) and `LoginDialog`.
- **`JellyfinClient`** — pure model/network layer. Uses Jellyfin's `X-Emby-Authorization` header scheme. Persists `deviceId` to `QSettings` (under organization `jellycute`). Emits Qt signals for every async result; never touches widgets. The `JellyfinItem` struct is the canonical media DTO used everywhere.
- **`LoginDialog`** — modal dialog used only when no credentials are stored. On success, writes `server`/`userId`/`token` to `QSettings`.
- **`BrowserWidget`** — the entire browsing UI: sidebar (Home + libraries), Home page (Resume row + per-library Latest rows via `MediaRow`), and a Grid page with breadcrumb navigation, alphabet filter, and pagination. Maintains two stacks: `m_stack` (`Frame` LIFO for the current grid drill-down) and `m_history` (`NavEntry` LIFO of past views, including Home/Grid/Detail snapshots) used by Back. Embeds a `DetailPage` for item detail views. Loads posters lazily through a separate `QNetworkAccessManager` (`m_imgNam`) so image traffic doesn't block API requests.
- **`MediaRow`** — horizontal row widget (Resume, Latest, etc.) with two poster shapes: `Vertical` (2:3) for movies/series, `Backdrop` (16:9) for episodes/featured.
- **`DetailPage`** — single-item view (poster, metadata, Play/Resume/Favorite/Watched/Unwatched actions). Receives an item, then re-fetches full details via `JellyfinClient::fetchItemDetails` and rebuilds when `itemDetailsLoaded` arrives.
- **`MpvWidget`** — `QOpenGLWidget` hosting libmpv's render API. Pumps mpv events on `wakeup` (queued to the GUI thread) and triggers `update()` from the render-update callback. Exposes high-level helpers (`play`, `seekAbsolute`, `togglePause`, track selection, volume, hwdec choice, video aspect). Translates Qt key/mouse events into mpv input commands so mpv's input layer (and the optional native OSC) keeps working inside the embedded surface.
- **`MainWindow`** — wires everything together: stacks Browser + Player, builds the menus/toolbar/player control bar, owns the playback-progress reporting timer, handles fullscreen toggling, drives the slide-up/down audio/subtitle track panels, and bridges Browser's `playRequested(item, startTicks)` into `MpvWidget::play(streamUrl, startSeconds)` + Jellyfin progress reporting.

### Cross-cutting conventions

- **Time units.** Jellyfin uses 100-ns ticks (`TICKS_PER_SECOND = 10'000'000`); libmpv and the UI use seconds. Conversions live at the boundary between `JellyfinClient`/`JellyfinItem` and `MpvWidget`/`MainWindow`.
- **`JellyfinItem.playable()`** decides whether an item gets a Play button — currently `Movie`, `Episode`, `Video`. Anything else is treated as a folder (drill-in) regardless of `isFolder`.
- **Stylesheet is centralized in `main.cpp`.** Object names referenced there (`sidebar`, `contentPanel`, `mediaRow`, `contentCard`, `detailCard`, `letterPanel`, `trackPanel`) are load-bearing — setting `setObjectName(...)` on a widget opts it into the card/panel styling. Don't sprinkle per-widget stylesheets; extend the global one.
- **Toolbar/menu icons** in `MainWindow.cpp` are hand-drawn with `QPainter` via the `makeIcon(size, lambda)` helper rather than relying on the system icon theme — keeps the look consistent across distros.
- **OpenGL context.** The 3.3 Compatibility profile in `main.cpp` is intentional. Don't lower it; the comment there explains why (mpv probes features that Qt's default 2.0 context lacks).
- **Settings keys** (`QSettings`, organization `jellycute`): `server`, `userId`, `token`, `deviceId`. New persistent state should follow the same flat layout.

## Code style

Per the user's global instructions: **all code-related text in English** (identifiers, comments, commit messages, file names, log/error strings) regardless of conversation language.
