# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Composting category: every compostable item with its chance to raise the compost level.

### Fixed

- Data-driven fuels such as coal and charcoal were missing from the Fuel category and from the smelting fuel slots.

## [0.1.0] - 2026-10-21

### Added

- Item list next to container screens, with paging, mouse-wheel scrolling and native item icons.
- Search with JEI syntax (`@namespace`, `#tag`, `$id`, `-exclude`, `a|b`, `"phrase"`), Unicode folding, Chinese pinyin matching and search history.
- Recipe screen with category tabs, paging, workstation shortcuts, cycling ingredient alternatives and back navigation.
- Recipe categories:
  - crafting, including special recipes shown as examples;
  - smelting, blasting, smoking, campfire and soul campfire cooking;
  - fuel;
  - stonecutting, smithing and cartography;
  - brewing;
  - anvil repair;
  - a generic fallback for any other recipe type.
- `R` / `U` / `A` / `O` / `Backspace` / `Ctrl+F` hotkeys. They work on BEI panels, on vanilla slots and inside the recipe screen, and can be remapped in the game's controls menu.
- Recipe transfer (`+`, shift for as many sets as possible) into the inventory and crafting table grids, furnaces and the smithing table, with missing ingredients highlighted.
- Bookmarks panel.
- Cheat mode:
  - take items from the list;
  - delete the held item in creative mode.
- In-game settings popup.
- English and Simplified Chinese translations.

[Unreleased]: https://github.com/LiteLDev/bei/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LiteLDev/bei/releases/tag/v0.1.0
