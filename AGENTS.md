# AGENTS.md

BEI (Barely Enough Items) is a JEI-style item list and recipe viewer for Minecraft Bedrock,
built as a **client** mod on LeviLamina. It targets MC **1.26.51.01** and LeviLamina
**26.51.7**, and is built with xmake and clang-cl on Windows x64.

## Layout and ownership

| Path              | What it is                                                                 |
| ----------------- | -------------------------------------------------------------------------- |
| `src/core/`       | Pure C++ logic: model, recipe index, search, layout, state. **No `mc/` or `ll/` includes.** Built as the static lib `bei-core` and unit-tested on the host. |
| `src/game/`       | Adapters from game objects to the core model (`ItemCatalog`, recipe providers, `RecipeManager`). |
| `src/ui/`         | Rendering. `gfx/Painter` wraps the game UI renderer; `UiRoot` hosts `IUiLayer`s; `overlay/`, `recipe/`, `widgets/`. |
| `src/input/`      | `InputRouter`: game input to BEI (mouse, keys, hotkeys, vanilla slot hover). |
| `src/actions/`    | Recipe transfer and cheat-mode give.                                      |
| `src/app/`        | `Controller` (navigation, bookmarks, config, hover), `I18n`.               |
| `src/mod/`        | Entry point. `BEI.cpp` is the only place that calls module `install()`s.   |
| `assets/lang/<locale>/*.json` | Translation fragments, merged into `bin/BEI/lang/<locale>.json` at build time. Each feature owns its own fragment file; a duplicate key fails the build. |
| `tests/`          | GoogleTest suites for `src/core` (target `bei-tests`).                    |
| `tools/e2e/`      | PowerShell harness that drives the real game (see `tools/e2e/README.md`).  |

The public headers in these directories are **contracts** between features:
`core/model/*`, `core/recipe/RecipeIndex.h`, `core/search/SearchEngine.h`, `core/state/*`,
`game/items/ItemCatalog.h`, `game/recipes/IRecipeProvider.h`, `game/recipes/RecipeManager.h`,
`ui/UiLayer.h`, `ui/UiRoot.h`, `ui/gfx/Painter.h`, `ui/overlay/ItemListPanel.h`,
`ui/recipe/RecipeScreen.h`, `ui/widgets/*.h`, `input/InputRouter.h`, `actions/Actions.h`,
`app/Controller.h`.
- Extend them compatibly: add members, don't change or remove existing ones.
- Register new things through the registries: `BEI_REGISTER_RECIPE_PROVIDER`, `BEI_REGISTER_UI_LAYER`.
- Don't edit `src/mod/BEI.cpp` unless you add a new module that needs `install()`.

## Build and test

New tools are on the persistent PATH but not in already-open shells. In a fresh pwsh, refresh first:
`$env:Path=[Environment]::GetEnvironmentVariable('Path','Machine')+';'+[Environment]::GetEnvironmentVariable('Path','User')`

```powershell
xmake f -a x64 -m release -p windows --target_type=client -y   # once (config is sticky)
xmake -y                                                         # builds bin/BEI/{BEI.dll,manifest.json,lang/}
xmake build -y bei-tests; xmake run bei-tests                    # unit tests (src/core only)
pwsh tools/e2e/run.ps1 -Scenario smoke                           # real game, see tools/e2e/README.md
```

`git config --global core.longpaths true` is required, because the LeviLamina package has very long header paths.

## Bedrock / LeviLamina rules (learned the hard way)

**Where the SDK headers are**
- `%LOCALAPPDATA%\.xmake\packages\l\levilamina\26.51.7\<hash>\include\{ll,mc}`.
- The same source is in the lip cache: `%LOCALAPPDATA%\lip\cache\git+https...LeviLamina.git#v26.51.7\{src,src-client}`.
- Check these before guessing any name or signature.

**What you may call**
- `MCAPI`, `MCFOLD` and `MCTAPI` functions and unmarked `virtual` functions are callable.
- **Never call `MCNAPI`.** For example, all of `ItemStackNetManagerClient` is unavailable.
- `::ll::TypedStorage<...> mX` members are readable layout fields: use `mX.get()` or `*mX`, or implicit conversion.
- Hook a virtual function through its `$name` thunk, e.g. `LL_TYPE_INSTANCE_HOOK(H, ll::memory::HookPriority::Normal, Cls, &Cls::$method, Ret, Args...)`. Install with `H::hook()` and remove with `H::unhook()` in your module's install/uninstall.

**Threads**
- UI rendering (`ll::event::AfterUIRenderEvent`) and level ticks run on the client thread.
- `KeyInputEvent` and `MouseInputEvent` arrive on the **window thread**. Decide synchronously whether to `cancel()`, then do the real work via `ll::thread::ClientThreadExecutor::getDefault().execute(...)`.
- Never touch game state from input callbacks.

**Lifetimes**
- Never keep raw game pointers (screens, controllers, items, level) across frames.
- `ItemCatalog` owns cached `ItemStack`s and clears them on level exit.
- Restore vanilla state on screen leave, level exit and focus loss.

**Rendering order**
- In the UI pass the order matters: rectangles → `flushImages`, then item icons, then text → `flushText`. `ui::Painter` does this for you; call `painter.flush()` between z bands.
- Coordinates are GUI units. Mouse pixels × `GuiData::mInvGuiScale` = GUI units.

**Names and screens**
- Item ids are full names (`minecraft:oak_planks`). `ItemStackBase::getTypeName()` returns them.
- Screen names come from `ClientInstance::getScreenName()`: `inventory_screen`, `crafting_screen`, `hud_screen`, …
- Container screens report `ScreenController::_isContainerScreen()`.

**Policy**
- Lamium (github.com/amatouhake/Lamium) has no license. Read it for ideas only and never copy code from it.

## Conventions

- C++20, `.clang-format` (LLVM-based, 4 spaces, 120 columns). Namespaces follow directories: `bei::core`, `bei::game`, `bei::ui`, `bei::input`, `bei::actions`, `bei::app`.
- English in code and comments. User-facing strings go through `app::tr("bei.some.key", "English fallback")` and the lang fragments (`en_US` and `zh_CN` both required).
- Log with `bei::log()` (`mod/Log.h`). Keep per-frame logging out; log on state changes.
- Every `src/core` change comes with gtest coverage in `tests/`.
