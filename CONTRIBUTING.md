# Contributing to BEI

Thanks for helping! Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and requesting features

Use the [issue templates](https://github.com/LiteLDev/bei/issues/new/choose). For bugs, include:

- the BEI version;
- the LeviLamina version;
- the Minecraft version;
- steps to reproduce;
- the relevant part of `logs/latest.log` from your game folder.

## Development setup

See [Building](README.md#building) in the README. [AGENTS.md](AGENTS.md) is the developer guide. It covers the layout of the code, the contracts between modules, threading and lifetime rules, and how to read the LeviLamina SDK headers. Please read it before changing anything under `src/`.

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change. Describe what it does and how you tested it.
3. Follow the existing style: C++20, the provided `.clang-format` and `.editorconfig`, English in code and comments.
4. User-facing text goes through `app::tr` with entries in **both** `assets/lang/en_US/` and `assets/lang/zh_CN/`.
5. Logic in `src/core` must stay free of game headers and come with unit tests in `tests/`:
   `xmake build -y bei-tests; xmake run bei-tests`.
6. If you change something visible in game, run the relevant end-to-end scenario from [tools/e2e](tools/e2e/README.md) and check its screenshots. Better still, add a scenario.
7. CI must pass before a pull request is merged.

Commit messages use the imperative mood ("Add anvil category", not "Added …"). Update [CHANGELOG.md](CHANGELOG.md) under *Unreleased* for user-visible changes.

## License

By contributing you agree that your contributions are licensed under the [LGPL-3.0](COPYING.LESSER), the license of this project.
