# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`jellycute` is a Qt6 (Kirigami / Qt Quick) desktop client for Jellyfin media servers, with playback handled in-process by libmpv via `mpv_render_context` on a `QQuickFramebufferObject`. C++20 + QML, CMake + Ninja, single executable (no library, no install rules).

The UI is QML/Kirigami throughout — there are no `QWidget` subclasses. The C++ side is purely model/network/playback plumbing exposed to QML via `Q_PROPERTY` / `Q_INVOKABLE` and a small set of context properties wired in `main.cpp`.

## Build & run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build           # incremental build
./build/jellycute             # run
```

There is no test suite, no linter config, and no install target. Reconfigure (`cmake -S . -B build ...`) is only needed when `CMakeLists.txt` changes — adding a new `.cpp` requires editing the `add_executable` list there. New `.qml` files must also be added to `resources.qrc`.

System dependencies (Arch package names): `qt6-base`, `qt6-declarative` (Qml + Quick), `qt6-quickcontrols2` (provided by qt6-declarative), `kirigami` (KF6, ≥ 6.x), `mpv` (libmpv via pkg-config), `cmake`, `ninja`. Qt's AUTOMOC/AUTOUIC/AUTORCC are on, so MOC files are generated under `build/jellycute_autogen/` automatically — no manual `moc` invocations.

## Architecture

The QML side lives entirely under `qml/` (registered through `resources.qrc`). The C++ side under `src/` is split into a network/auth layer, a set of QML-friendly models, a playback session controller, an mpv host item, and a tiny preferences wrapper. `main.cpp` wires them together and hands them to a single `QQmlApplicationEngine`.

### QML

- **`Main.qml`** — `Kirigami.ApplicationWindow` root. Holds the `GlobalDrawer` (Inicio / Reproductor de prueba / Configuración + dynamic library list) and sets `pageStack.initialPage` to `HomePage.qml`. On `Component.onCompleted`, pushes `LoginPage.qml` on top when `jellyfin.authenticated` is false.
- **`HomePage.qml`** — Flickable column with a `MediaRow` for "Continuar viendo" (driven by `resumeModel`) and a `Repeater` over `viewsModel` that creates a per-library `BrowseModel` + `MediaRow` for the latest items in each library. Click routing: folder-like types push another `GridPage`, playable items push `DetailPage`.
- **`GridPage.qml`** — Library / drill-in browser. Owns its own `BrowseModel`, takes `parentId`, `parentName`, `collectionType`, `pageSize`, `currentPage`, `nameStartsWith` as properties (set by `pageStack.push(url, props)`). Layout: alphabet strip on top, `GridView` of poster cards in the middle, prev/next pagination at the bottom. Reload is gated by an `_ready` flag so initial property assignment + `Component.onCompleted` don't double-fire a fetch.
- **`DetailPage.qml`** — Single-item view. Owns an `ItemDetailModel`, takes `itemId` and an optional `initialItem` `QVariantMap` so it can render name/poster/year before `fetchItemDetails` resolves. Subscribes to `playback.stateChanged`: pushes `PlayerPage.qml` when the session reaches `ready`, surfaces resolve errors via `showPassiveNotification`.
- **`PlayerPage.qml`** — Hosts an `MpvObject`. Two operating modes: *session mode* (auto-plays `playback.streamUrl`, fires `reportStarted` on mpv's `playbackStarted`, runs a 10 s timer for `reportProgress`, calls `reportStopped` on EOF/back-out, pops itself on EOF) and *test mode* (state == `idle`, reachable from the sidebar) which keeps a URL `TextField` visible.
- **`LoginPage.qml`** — `Kirigami.FormLayout` for server / username / password. On submit calls `jellyfin.setServer` + `jellyfin.authenticate`. The `Connections { target: jellyfin }` handler persists the (server, account) pair through `accountStore`, refreshes the home models, and pops itself.
- **`SettingsPage.qml`** — Stacked sections in a single `Kirigami.ScrollablePage`: Cuentas (Repeater of servers as Kirigami.Cards with their accounts as `ItemDelegate`s; click switches active, trash button removes; "+ Añadir cuenta" reuses `LoginPage`), Vídeo (hwdec `ComboBox` bound to `preferences.hwdec`), Apariencia (placeholder).
- **`MediaRow.qml`** — Reusable horizontal scroller. Heading + `ListView` of poster cards; configurable shape (`useBackdropPosters: true` for 16:9, otherwise vertical 2:3). Emits `itemClicked(int index)` so consumers can fetch the full row via `mediaModel.get(index)` and route navigation themselves.

### C++

- **`main.cpp`** — bootstraps Qt, requests an OpenGL 3.3 Compatibility surface format *before* `QGuiApplication` (libmpv's render API needs ≥ 3.x), pins Qt Quick to the OpenGL backend (`QQuickWindow::setGraphicsApi(OpenGL)`) since `QQuickFramebufferObject` doesn't work on RHI/Vulkan, forces `LC_NUMERIC=C` *after* `QGuiApplication` (Qt resets it; libmpv's option parser requires C locale), restores the persisted active account into `JellyfinClient`, registers QML types, exposes the context properties, and loads `qrc:/qml/Main.qml`.
- **`JellyfinClient`** — pure model/network layer. Uses Jellyfin's `X-Emby-Authorization` header scheme. Persists `deviceId` to `QSettings` (organization `jellycute`). Browse/auth/user-data methods are `Q_INVOKABLE`; `server`, `userId`, `accessToken`, `authenticated` are `Q_PROPERTY` so QML reacts. The `JellyfinItem` struct is the canonical media DTO. `JellyfinPlayback` (URL + `playSessionId` + method) is C++-only — `PlaybackSession` wraps it for QML.
- **`AccountStore`** — singleton holding the persisted servers + accounts (`QSettings` arrays) and the current-account id. `Q_INVOKABLE` mutators (`addServer`, `addAccountWith`, `removeServer`, `removeAccount`, `setCurrentAccountId`, `findServerIdByUrl`) plus QVariantMap/QVariantList views (`serverList`, `accountList`, `serverAsMap`, `accountAsMap`) so QML iterates without registering Q_GADGETs.
- **`ItemListModel`** — `QAbstractListModel` over `QList<JellyfinItem>` with named roles (`id`, `name`, `type`, `posterUrl`, `backdropUrl`, `runTimeSeconds`, `resumeSeconds`, …). `posterUrl` falls back to the parent series poster when the item itself has no primary image (typical for episodes). `Q_INVOKABLE get(index)` returns a full `QVariantMap` row, used for routing decisions in QML click handlers.
- **`BrowseModel : ItemListModel`** — facade that connects to a `JellyfinClient` (writable `client` `Q_PROPERTY` so QML can wire instances created inside delegates) and exposes `Q_INVOKABLE loadResume / loadUserViews / loadLatest / loadChildren / loadEpisodes`. Tracks the active source so signals from earlier requests don't bleed in. Surfaces `loading` and `totalCount` for paginated grids.
- **`ItemDetailModel`** — `QObject` wrapping a single `JellyfinItem` with one `Q_PROPERTY` per field. `prefill(map)` lets DetailPage show whatever fields it already has from the click-time row dictionary; `load(itemId)` fires `fetchItemDetails`. `toggleFavorite / togglePlayed` mirror the server's confirmation back into the model.
- **`PlaybackSession`** — singleton FSM (`idle → resolving → ready → playing → ended/error`) over `JellyfinPlayback`. `start(itemId, startSeconds)` resolves a stream URL through `JellyfinClient::resolvePlayback`; `reportStarted / reportProgress / reportStopped / cancel` translate to the matching REST calls and emit the stop-active-encoding teardown for transcodes. State changes drive QML page transitions; the resolved `streamUrl`, `startSeconds`, `itemId`, `errorMessage` are exposed as properties.
- **`MpvObject : QQuickFramebufferObject`** — mpv host item. Owns the `mpv_handle` and (per the canonical `mpv-examples/qt_opengl` pattern) the `mpv_render_context*`; the inner `MpvRenderer` (SG render thread) creates the context lazily inside `createFramebufferObject()` and stores it on the item, so the item's destructor can call `mpv_render_context_free()` *before* `mpv_terminate_destroy()` and avoid the "Broken API use" abort. Drains mpv events on `wakeup` (queued to GUI thread); the render-update callback bridges back to `QQuickItem::update()` via `QMetaObject::invokeMethod`. High-level helpers: `play`, `seekAbsolute`, `togglePause`, track selection, volume, hwdec, video aspect. Qt key/mouse events are translated into mpv input commands so mpv's input layer + native OSC keep working inside the embedded surface.
- **`Preferences`** — thin `QSettings` wrapper exposing `hwdec` as a `Q_PROPERTY` and `availableHwdecChoices()` (probed once via `mpv --hwdec=help`). The hwdec value is read by `MpvObject` at construction, so changes only affect the *next* player instance.

### Cross-cutting conventions

- **Time units.** Jellyfin uses 100-ns ticks (`TICKS_PER_SECOND = 10'000'000`); libmpv and the UI use seconds. Conversions live at the boundary between `JellyfinClient`/`JellyfinItem` and `MpvObject`/`PlaybackSession`. Models like `ItemListModel` already expose seconds-typed roles (`runTimeSeconds`, `resumeSeconds`) so QML never sees ticks.
- **`JellyfinItem.playable()`** decides whether an item gets a Play button — currently `Movie`, `Episode`, `Video`. Folder-like types (`Series`, `Season`, `BoxSet`, `Folder`, `CollectionFolder`, `MusicAlbum`, `MusicArtist`) drill in via `GridPage`; everything else is treated as playable.
- **OpenGL context.** The 3.3 Compatibility profile + `setGraphicsApi(OpenGL)` + `AA_ShareOpenGLContexts` in `main.cpp` are intentional and documented inline. Don't switch the SG to RHI/Vulkan — `QQuickFramebufferObject` only works on the OpenGL scene graph backend.
- **mpv option contract.** `MpvObject` deliberately omits `MPV_RENDER_PARAM_ADVANCED_CONTROL`: that mode imposes a stricter render-driver contract on the consumer (timing, DR buffer allocation, hwdec interop) that the SG render-thread split of `QQuickFramebufferObject` can't satisfy. Default `hwdec` is `no` and `hwdec-codecs` is empty so libavcodec doesn't probe nvdec on systems where Arch's ffmpeg pulls in libnvcuvid but `libcuda.so.1` is missing. `audio-fallback-to-null=yes` keeps the player alive when mp3/etc. decoders error out mid-seek.
- **Settings keys** (`QSettings`, organization `jellycute`): `deviceId` (scalar), `servers`/`accounts` (arrays) + `currentAccountId` managed by `AccountStore`, `hwdec` (managed by `Preferences`). New persistent state should follow the same flat layout, ideally through a small wrapper QObject so QML can bind to it.
- **Context properties** wired in `main.cpp` (consumed all across `qml/`): `jellyfin` (the shared `JellyfinClient`), `viewsModel` + `resumeModel` (top-level `BrowseModel`s for the home page), `playback` (the `PlaybackSession` singleton), `accountStore`, `preferences`. New global state should follow this pattern; per-page model instances should be QML-instantiable types with a writable `client` property (see `BrowseModel`).
- **Diagnostic categories.** `jellycute.mpv` and `jellycute.playback` (`Q_LOGGING_CATEGORY`) are silent by default; enable with `QT_LOGGING_RULES="jellycute.*=true"` for verbose tracing of playback flow.

## Code style

Per the user's global instructions: **all code-related text in English** (identifiers, comments, commit messages, file names, log/error strings) regardless of conversation language. QML user-facing strings remain wrapped in `qsTr(...)`.
