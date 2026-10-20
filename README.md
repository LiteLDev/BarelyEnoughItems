# BEI — Barely Enough Items

A [JEI](https://github.com/mezz/JustEnoughItems)-style item and recipe viewer for **Minecraft
Bedrock Edition**, as a client mod for [LeviLamina](https://github.com/LiteLDev/LeviLamina).

- **Item list** next to every container screen, with search (`@namespace`, `#tag`, `$id`, `-not`, `a|b`; Chinese pinyin also works) and paging.
- **Recipes (R)** and **uses (U)** for the hovered item, in any screen, across all recipe categories: crafting, smelting, blasting, smoking, campfire, brewing, smithing, stonecutting, and more.
- **Back (Backspace)** through your lookup history.
- **Bookmarks (A)** in a panel on the left.
- **"+" button** that moves a recipe's ingredients into the crafting grid.
- **Cheat mode** to take items directly (operators and creative only).

Works in single-player and on servers, because BEI only reads the recipes the server already sends to the client.

## Requirements

- Minecraft Bedrock **1.26.51.01** (Windows).
- LeviLamina **26.51.x** (client), e.g. installed through LeviLauncher.

## Install

Copy `BEI/` from a release (or from `bin/BEI/` after building) into `<game>/mods/`.

## Build

See [AGENTS.md](AGENTS.md#build-and-test). Short version:

```powershell
git config --global core.longpaths true
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y
```

## License

Not chosen yet.
