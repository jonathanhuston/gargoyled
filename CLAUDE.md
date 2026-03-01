# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Informed** is a fork of [Gargoyle](https://github.com/garglk/garglk), an open-source interactive fiction (IF) player. The fork adds LLM-powered natural language input translation: players type natural English prose and it gets translated into standard IF parser commands via the Anthropic Claude API before delivery to the game interpreter.

## Build Commands

### macOS (Cocoa backend, primary development platform)

```bash
# Full build + DMG packaging:
./gargoyle_osx.sh          # builds in build-osx/, installs to build/dist/
./gargoyle_osx.sh -c       # clean build
./gargoyle_osx.sh -n       # build without creating DMG

# Manual CMake build (for development iteration):
mkdir -p build-osx && cd build-osx
cmake .. -DBUILD_SHARED_LIBS=ON -DDIST_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_FRAMEWORK=LAST
make -j$(sysctl -n hw.ncpu)
```

### Linux (Qt backend)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### Running from build directory

The build copies fonts into the CMake build directory so Gargoyle can find them without system installation. Run `./gargoyle <gamefile>` from the build directory.

### Key CMake Options

- `INTERFACE`: `COCOA` (macOS default) or `QT` (all others)
- `SOUND`: `SDL` (default on Linux), `QT`, or `NONE`
- `WITH_TTS`: `ON`/`OFF`/`AUTO`/`DYNAMIC`
- `WITH_INTERPRETERS`: build bundled terps (default ON)
- `WITH_BABEL`: Treaty of Babel metadata support (default ON)
- `QT_VERSION`: `5` or `6` (default 6)
- `DIST_INSTALL`: install to `build/dist/` for packaging

## Architecture

### Multi-process plugin model

```
gargoyle (launcher) → detects game format → spawns interpreter subprocess
interpreter (e.g. bocfel, git, glulxe) → links against libgarglk → calls Glk API
libgarglk → implements Glk 0.7.6 API, renders text/graphics/sound, platform UI
```

The launcher (`garglk/launcher.cpp`) identifies game files by magic bytes/extension and spawns the appropriate interpreter from `terps/`. Each interpreter is a separate executable that links against `libgarglk` (shared library).

### Dual-license library split

Two versions of libgarglk are built:
- **libgarglk** (GPLv3) — includes xBRZ image scaler
- **libgarglk-gpl2** (GPLv2+) — uses null xBRZ stub; linked by GPLv2 interpreters (agility, jacl, scare, plus, taylor, tadsr)

### Core library (`garglk/`)

| File | Purpose |
|---|---|
| `garglk.h` | Master header: all data structures, config globals, function prototypes |
| `config.cpp` | INI config file parsing (system/user/per-game paths) |
| `draw.cpp` | FreeType font rendering with subpixel LCD, gamma correction, kerning |
| `window.cpp` | Glk window tree management, scrollback buffer |
| `wintext.cpp` | Text buffer window; **contains the LLM integration hook** |
| `wingrid.cpp` | Text grid window |
| `wingfx.cpp` | Graphics window |
| `event.cpp` | Glk event queue (`glk_select`, `glk_select_poll`) |
| `sysqt.cpp` | Qt platform backend |
| `sysmac.mm` | Cocoa platform backend |
| `launcher.cpp` | Game format detection, interpreter subprocess spawning |
| `theme.cpp` | JSON color theme loading |
| `imgload.cpp` / `imgscale.cpp` | Image loading and scaling (HQX, xBRZ) |
| `sndsdl.cpp` / `sndqt.cpp` | Sound backends |

### Informed LLM subsystem (`garglk/llm.h`, `garglk/llm.cpp`)

Integration point: `wintext.cpp` at the line-input completion handler. When the player presses Enter:
1. `llm::is_available()` checks if `ANTHROPIC_API_KEY` env var is set
2. `llm::is_simple_command()` heuristically bypasses API for already-valid parser commands (single-word, common abbreviations, or ≤3 words with no natural language markers)
3. `llm::get_game_context()` extracts up to 2000 chars from the scrollback buffer
4. `llm::translate_input()` calls Anthropic Messages API via `popen(curl ...)` using claude-haiku-4-5
5. Translated command replaces the Glk input buffer contents

The LLM module uses nlohmann/json (`json.hpp`) for request/response serialization. On any failure, it falls back to the original input unchanged.

### Interpreters (`terps/`)

16+ bundled IF interpreters, each in its own subdirectory. Built via a `terp()` CMake function defined in `terps/CMakeLists.txt`. Major ones: bocfel (Z-machine/Infocom), git/glulxe (Glulx), hugo, tads (TADS 2/3), alan2/alan3, scott, scare (ADRIFT).

### Platform-specific code pattern

Font discovery, TTS, and system UI each have platform variants:
- `font{fc,mac,win}.cpp` — Fontconfig / Core Text / GDI
- `tts{speechd,mac,win,null}.cpp` — speech-dispatcher / NSSpeechSynthesizer / SAPI / stub
- `sys{qt,mac}.cpp` + `launch{qt,mac}.cpp` — Qt Widgets / Cocoa

### No test suite

There are no unit tests. CI validates builds across platform/feature matrix combinations (see `.github/workflows/`).

## LLM Configuration

Set `ANTHROPIC_API_KEY` environment variable to enable natural language translation. Without it, Informed behaves identically to standard Gargoyle. Debug output goes to stderr with `[Informed]` prefix.
