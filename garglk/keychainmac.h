// Copyright (C) 2026 by Jonathan Huston.
//
// This file is part of Gargoyled (Gargoyle fork with LLM-powered natural language input).
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef GARGLK_KEYCHAINMAC_H
#define GARGLK_KEYCHAINMAC_H

#include <string>

namespace garglk { namespace keychain {

// Retrieve the Anthropic API key from the macOS Keychain.
// Returns the key string, or empty string if not found.
std::string get_api_key();

// Store the Anthropic API key in the macOS Keychain.
// Returns true on success, false on failure.
bool set_api_key(const std::string &key);

// Show a dialog prompting the user to enter their Anthropic API key.
// If the user enters a key and clicks Save, it is stored in the Keychain.
// Returns the entered key, or empty string if the user clicked Skip.
std::string prompt_for_api_key();

}} // namespace garglk::keychain

#endif // GARGLK_KEYCHAINMAC_H
