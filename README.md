# BEI

[![standard-readme compliant](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg)](https://github.com/RichardLitt/standard-readme)
[![Build](https://github.com/LiteLDev/bei/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/bei/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/LiteLDev/bei)](https://github.com/LiteLDev/bei/releases)
[![License](https://img.shields.io/github/license/LiteLDev/bei)](COPYING.LESSER)

Barely Enough Items: a JEI-style item list and recipe viewer for Minecraft Bedrock Edition, as a LeviLamina client mod.

BEI brings what [Just Enough Items (JEI)](https://github.com/mezz/JustEnoughItems) does on Java Edition to Bedrock: browse every item, look up how to make it and what it is used for, and move a recipe's ingredients into the crafting grid with one click. It runs entirely on the client and reads the recipes the server already sends, so it works in single-player and on any server, and the server does not need to install anything.

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Usage](#usage)
  - [Item list](#item-list)
  - [Recipes and uses](#recipes-and-uses)
  - [Search syntax](#search-syntax)
  - [Bookmarks](#bookmarks)
  - [Cheat mode](#cheat-mode)
  - [Settings](#settings)
- [Building](#building)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Background

Bedrock Edition has a recipe book, but it only shows crafting recipes, it cannot tell you what an item is *used* for, and it cannot be extended. Java players have relied on JEI for this for over a decade. BEI is a port of that experience to Bedrock, built on [LeviLamina](https://github.com/LiteLDev/LeviLamina), which supports Bedrock clients on Windows.

**Recipe categories:**
- crafting, including special recipes such as banners, fireworks and map cloning, which are shown as examples;
- smelting, blasting, smoking, campfire and soul campfire cooking;
- fuel;
- stonecutting, smithing, cartography and brewing;
- anvil repair;
- any data-driven or add-on recipe type, which gets a generic page.

**Item list:** all creative inventory items plus registry items that are missing from it, with localized names, item tags and creative groups.

**Rendering:** the game's own item renderer and UI textures, so icons, enchantment glint and panels look exactly like vanilla.

**Languages:** English and Simplified Chinese, including pinyin search.

## Install

Requirements:

- Minecraft Bedrock Edition **1.26.51.01** for Windows (x64).
- LeviLamina **26.51.x** client, for example installed through [LeviLauncher](https://github.com/LiteLDev/LeviLauncher).

With [lip](https://github.com/futrime/lip):

```sh
lip install github.com/LiteLDev/bei#client
```

Manually: download `bei-client-windows-x64.zip` from the [latest release](https://github.com/LiteLDev/bei/releases/latest) and extract it so that `BEI/` ends up in the game's `mods/` folder, next to `LeviLamina/`.

## Usage

Open any container screen (inventory, crafting table, furnace, chest, …). The item list appears on the right, and your bookmarks on the left.

### Item list

| Action | Effect |
| --- | --- |
| Hover an item | Tooltip with the name, id and mod |
| Left click | Show its recipes (in cheat mode: take a stack) |
| Right click | Show its uses (in cheat mode: take one) |
| Mouse wheel, `<` `>` | Change page |
| Click the search bar | Type to filter; right click clears it, `↑` `↓` recall earlier searches |

### Recipes and uses

These keys work over BEI's panels, over vanilla inventory slots and inside the recipe screen. All of them can be remapped in *Settings → Keyboard & Mouse*.

| Key | Effect |
| --- | --- |
| `R` | Recipes that make the hovered item |
| `U` | Recipes that use the hovered item. For a workstation, such as a furnace, this lists everything it can make. |
| `Backspace` | Back to the previous page |
| `A` | Add or remove a bookmark |
| `O` | Show or hide the item list |
| `Ctrl` + `F` | Focus the search bar |

In the recipe screen, click a tab to switch category and use the mouse wheel to page. Left click an ingredient to see its recipes, right click it to see its uses, and click a workstation on the left to see every recipe of that category.

The **`+`** button next to a recipe moves its ingredients from your inventory into the open crafting grid, furnace or smithing table. Shift-click moves as many sets as you have. Ingredients you are missing are tinted red.

### Search syntax

| Query | Matches |
| --- | --- |
| `oak planks` | Items whose name contains both words |
| `stone\|dirt` | Either word |
| `-slab` | Exclude items matching the term |
| `@minecraft` | Items from a namespace |
| `#planks` | Items with a matching item tag |
| `$oak_` | Items whose id contains the text |
| `"red wool"` | An exact phrase |
| `mb`, `muban`, `木板` | Chinese names by pinyin initials, full pinyin or characters |

### Bookmarks

Press `A` over any item to pin it to the bookmark list on the left. Bookmarks are saved to `mods/BEI/config/bookmarks.json`.

### Cheat mode

When you are allowed to use commands (an operator, or a world with cheats enabled), cheat mode lets you click items in the list to take them. While it is on:

- left click takes a stack;
- right click or shift-click takes one;
- in creative mode, clicking the list while holding an item deletes it.

To turn it on, `Ctrl`-click the settings button next to the search bar.

### Settings

Click the settings button next to the search bar for in-game options: cheat mode, bookmarks, search bar position, item ids in tooltips and more. They are saved to `mods/BEI/config/config.json`.

## Building

You need Windows x64, [xmake](https://xmake.io), Visual Studio Build Tools with MSVC 14.4x or newer, and LLVM (`clang-cl`).

```powershell
git config --global core.longpaths true
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y                                    # produces bin/BEI/
xmake build -y bei-tests; xmake run bei-tests
```

[AGENTS.md](AGENTS.md) describes the architecture, the conventions and the rules for working with Bedrock internals. [tools/e2e](tools/e2e/README.md) is the end-to-end test harness that drives the real game.

## Maintainers

[@futrime (Zijian Zhang)](https://github.com/futrime)

## Contributing

PRs and issues are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and follow the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues as described in [SECURITY.md](SECURITY.md).

## License

LGPL-3.0 © 2026 Zijian Zhang

BEI is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License version 3, the same license as LeviLamina. See [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).

BEI is not affiliated with Mojang Studios, Microsoft, or the JEI project.
