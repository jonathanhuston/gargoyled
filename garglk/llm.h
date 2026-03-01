// Copyright (C) 2026 by Jonathan Huston.
//
// This file is part of Informed (Gargoyle fork with LLM-powered natural language input).
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef GARGLK_LLM_H
#define GARGLK_LLM_H

#include <string>
#include <vector>

#include "glk.h"

namespace garglk { namespace llm {

// Initialize the LLM subsystem. Reads ANTHROPIC_API_KEY from environment.
// Safe to call multiple times; only the first call has effect.
void init();

// Returns true if an API key is configured and translation is available.
bool is_available();

// Translate a natural-language player input into parser-friendly IF command(s).
//
// player_input: the raw text the player typed (UTF-8)
// game_context: recent game output for context (UTF-8, may be empty)
//
// Returns: translated command string (may contain multiple lines separated by \n).
//          On any failure (network, auth, timeout), returns player_input unchanged.
std::string translate_input(const std::string &player_input,
                            const std::string &game_context);

// Extract recent game output text from the scrollback buffer of the focused
// text buffer window. Returns UTF-8 string, truncated to approximately
// max_chars characters from the most recent output.
std::string get_game_context(int max_chars = 2000);

}} // namespace garglk::llm

#endif // GARGLK_LLM_H
