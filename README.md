# Gargoyled

An interactive fiction player with LLM-powered natural language input.

**Gargoyled** is a fork of [Gargoyle](https://github.com/garglk/garglk) that adds natural language input translation. Type plain English prose and [Claude](https://www.anthropic.com/claude) translates it into standard IF parser commands before delivering them to the game interpreter.

For example, instead of typing `TAKE LAMP. GO NORTH. OPEN DOOR`, you can type "grab the lamp, head north, and open the door" and Gargoyled will translate it for you.

When input is already a valid parser command (like `LOOK` or `TAKE KEY`), it passes through unchanged — no API call needed.

## Setup

### API Key

Gargoyled requires an [Anthropic API key](https://console.anthropic.com/) to enable natural language translation.

**macOS:** On first launch, Gargoyled will prompt you to enter your API key. It is stored securely in the macOS Keychain.

Alternatively, set the `ANTHROPIC_API_KEY` environment variable:

```bash
export ANTHROPIC_API_KEY="sk-ant-..."
```

Without an API key, Gargoyled behaves identically to standard Gargoyle.

## Building (macOS)

Requires Homebrew (or MacPorts) with CMake and standard build tools.

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
