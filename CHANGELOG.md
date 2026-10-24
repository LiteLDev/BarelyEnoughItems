# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A logo (`docs/logo.png`), shown on [LeviMC Mods](https://levimc.org/mods/) through `avatar_url` in `tooth.json`.

## [0.1.1] - 2026-10-21

### Fixed

- `lip install github.com/LiteLDev/BarelyEnoughItems#client` failed after the repository rename, because `tooth.json` still declared the old path `github.com/LiteLDev/bei`.

## [0.1.0] - 2026-10-21

First release.

### Added

- **Item list** next to every container screen, with paging, mouse-wheel scrolling and native item icons (including enchantment glint).
  - It covers every creative inventory item, plus registry items that are missing from it.
  - Items carry their localized names, creative groups and item tags.
- **Search** with JEI syntax: `@namespace`, `#tag`, `$id`, `-exclude`, `a|b` and `"phrase"`.
  - Matching ignores case and accents.
  - Chinese names also match by pinyin, full or initials.
  - `↑` / `↓` recall the last 20 searches.
- **Recipe screen**:
  - category tabs and paging;
  - workstation shortcuts;
  - cycling ingredient alternatives;
  - left click an ingredient for its recipes and right click for its uses;
  - back navigation.
- **Recipe categories**:
  - crafting, including special recipes shown as examples;
  - smelting, blasting, smoking, campfire and soul campfire cooking;
  - fuel;
  - stonecutting;
  - smithing;
  - cartography;
  - brewing;
  - anvil repair;
  - composting;
  - a generic fallback for any other recipe type.
- **Hotkeys**:
  - `R` recipes, `U` uses, `A` bookmark, `O` show/hide the item list, `Backspace` back, `Ctrl` + `F` focus search.
  - They work over BEI panels, over vanilla slots and inside the recipe screen.
  - They can be remapped in *Settings → Keyboard & Mouse*.
- **Recipe transfer** with the `+` button (shift for as many sets as possible) into:
  - the inventory and crafting-table grids;
  - furnaces, blast furnaces and smokers, including fuel;
  - the smithing table.

  Missing ingredients are highlighted.
- **Bookmarks** panel on the left.
- **Cheat mode** for players who may use commands:
  - take items from the list;
  - in creative mode, delete the held item by clicking the list.
- **In-game settings** popup (wrench button next to the search bar). `Ctrl`-click it to toggle cheat mode.
- **Translations**: English and Simplified Chinese.

[Unreleased]: https://github.com/LiteLDev/BarelyEnoughItems/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/LiteLDev/BarelyEnoughItems/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/LiteLDev/BarelyEnoughItems/releases/tag/v0.1.0
