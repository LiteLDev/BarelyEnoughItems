# BEI end-to-end tests

These tests drive the **real game** on this machine:
- the LeviLauncher instance `1.26.51.01` at `%APPDATA%\levilauncher.exe\versions\1.26.51.01`;
- **real input** (`SendInput`);
- **window screenshots** (`PrintWindow`).

There is one game instance per machine, so `run.ps1` holds a machine-wide lock (`%APPDATA%\levilauncher.exe\bei-e2e.lock`) for the whole run. Parallel worktrees simply queue.

```powershell
xmake -y                                           # build bin\BEI first
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario smoke
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game open afterwards
```

`run.ps1` performs these steps:
1. Takes the lock.
2. Deploys `bin\BEI` to `mods\BEI`.
3. Restores the `BEI-Test` world from `fixtures\BEI-Test.zip` (survival, peaceful, cheats on, always day).
4. Launches the game through LeviLauncher.
5. Waits for `BEI enabled` in the log.
6. Enters the world.
7. Runs `scenarios\<name>.ps1`.
8. Checks the log for `ERR`/`FTL` from BEI, LeviLamina or CrashLogger.
9. Always closes the game and releases the lock.

**Look at the screenshots** in `tools\e2e\out\<scenario>\` (e.g. with the Read tool). They are the evidence that a feature works.

## Writing a scenario

A scenario is a PowerShell file dot-sourced after the player is in the world, with `lib.ps1` loaded. Useful helpers:

| Helper | What it does |
| ------ | ------------ |
| `Open-Inventory` / `Close-Screen` | `E` / `Esc` with waits |
| `Invoke-Chat '/give @s oak_log 16'` | Opens chat, types, presses Enter. Cheats are on. |
| `Move-Mouse x y -Relative`, `Click x y -Relative [-Right]`, `Scroll n` | Coordinates are fractions of the 1600x900 client area |
| `Press-Key R` / `Press-Key ESC` / `Type-Text 'oak'` | Keyboard |
| `Save-Screenshot 'name' $u` | Saves to `out\$u\name.png`, downscaled to 1280x720 |
| `Wait-LogLine 'regex' [timeoutSeconds]` | Waits for a line in this session's log |
| `Get-LeviLog` | This session's log lines |

To find coordinates, take a screenshot. The screenshot is 1280x720, so a point `(px, py)` in it corresponds to `-Relative (px/1280) (py/720)`.

Reference positions at 1600x900 (GUI scale 3, as fractions):
- first cell of the item panel: `0.836 0.083`;
- panel search box: `0.9 0.957`;
- inventory 2x2 crafting grid: top-left cell `0.672 0.31`, output `0.778 0.34`.

## Notes and pitfalls

- **Elevated windows block injected input.** While a UAC prompt or the Windows Firewall "allow Minecraft" dialog is in the foreground, every click and key is silently dropped. Close such dialogs by hand.
- **LeviLamina appends to `logs\latest.log` across sessions.** `Get-LeviLog` only returns lines written after this session's `Start-Game`. BEI flushes every message.
- **The harness is DPI-aware and forces a 1600x900 client area**, so coordinates are stable.
- **Bedrock pauses when it loses focus.** `Focus-Game` only clicks the title bar if the game is not already in the foreground.
