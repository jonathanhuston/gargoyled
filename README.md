# Gargoyled

An interactive fiction player with LLM-powered natural language input.

**Gargoyled** is a fork of [Gargoyle](https://github.com/garglk/garglk) that adds natural language input translation. Type plain English prose and an LLM translates it into standard IF parser commands before delivering them to the game interpreter. On supported Macs, translation runs entirely on-device with **Apple Intelligence** — no API key, no cost, works offline. [Claude](https://www.anthropic.com/claude) is available as an alternative or fallback.

For example, instead of typing `TAKE LAMP. GO NORTH. OPEN DOOR`, you can type "grab the lamp, head north, and open the door" and Gargoyled will translate it for you.

When input is already a valid parser command (like `LOOK` or `TAKE KEY`), it passes through unchanged — no translation needed.

## Setup

### Apple Intelligence (default on supported Macs)

Requires an Apple Silicon Mac running macOS 26 or later, with Apple Intelligence turned on in **System Settings → Apple Intelligence & Siri**. If those are met, Gargoyled uses the on-device model automatically; there is nothing to configure.

### Claude (optional)

On Macs without Apple Intelligence, or if you prefer Claude, provide an [Anthropic API key](https://console.anthropic.com/).

**macOS:** If Apple Intelligence isn't available, Gargoyled prompts for your API key on first launch and stores it securely in the macOS Keychain.

Alternatively, set the `ANTHROPIC_API_KEY` environment variable:

```bash
export ANTHROPIC_API_KEY="sk-ant-..."
```

If both are available, Apple Intelligence is used and Claude serves as a fallback when an on-device translation fails. With neither, Gargoyled behaves identically to standard Gargoyle.

### Choosing a backend

Set `GARGOYLED_BACKEND` to override the automatic choice:

| Value | Behavior |
|---|---|
| `auto` (default) | Apple Intelligence if available, Claude as fallback |
| `apple` | On-device Apple Intelligence only |
| `claude` | Claude only |
| `off` | Disable translation |

To see translations as they happen, launch from Terminal; each one is logged to stderr with a `[Gargoyled]` prefix:

```bash
./Gargoyled.app/Contents/MacOS/Gargoyled path/to/game.z5
```

## Building (macOS)

Requires Homebrew (or MacPorts) with CMake and standard build tools. Building the Apple Intelligence helper requires Xcode 26 or later on Apple Silicon; otherwise it is skipped automatically (disable explicitly with `-DWITH_APPLE_INTELLIGENCE=OFF`).

```bash
# Full build + DMG packaging:
./gargoyle_osx.sh

# Build without creating DMG:
./gargoyle_osx.sh -n

# Clean build:
./gargoyle_osx.sh -c
```

The resulting `Gargoyled.app` is self-contained and can be moved to `/Applications`.

## Supported Formats

Gargoyled includes interpreters for all major IF formats: Z-machine (Infocom), Glulx, Hugo, TADS 2/3, Alan 2/3, ADRIFT, AdvSys, JACL, Level 9, Magnetic, Scott Adams, and more.

## License

GPLv2+. Original Gargoyle by Tor Andersson and [contributors](https://github.com/garglk/garglk). LLM integration by Jonathan Huston.
