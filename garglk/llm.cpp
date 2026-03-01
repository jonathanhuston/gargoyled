// Copyright (C) 2026 by Jonathan Huston.
//
// This file is part of Informed (Gargoyle fork with LLM-powered natural language input).
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "glk.h"
#include "garglk.h"
#include "llm.h"
#include "json.hpp"

using json = nlohmann::json;

namespace garglk { namespace llm {

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static bool s_initialized = false;
static bool s_available = false;
static std::string s_api_key;

static const char *MODEL = "claude-haiku-4-5-20251001";
static const int MAX_TOKENS = 150;
static const int TIMEOUT_SECS = 8;

// ---------------------------------------------------------------------------
// System prompt — tells Claude how to be an IF parser translator
// ---------------------------------------------------------------------------

static const char *SYSTEM_PROMPT = R"(You are a command translator for interactive fiction (text adventure) games. Your ONLY job is to convert natural-language player input into standard IF parser commands.

RULES:
1. Output ONLY the translated command(s). No commentary, no explanations, no quotes.
2. Use standard IF verbs: TAKE, DROP, LOOK, EXAMINE (or X), GO, OPEN, CLOSE, UNLOCK, LOCK, PUT, GIVE, ASK, TELL, SHOW, PUSH, PULL, TURN, WAIT (or Z), INVENTORY (or I), NORTH (or N), SOUTH (or S), EAST (or E), WEST (or W), UP (or U), DOWN (or D), NORTHEAST (or NE), NORTHWEST (or NW), SOUTHEAST (or SE), SOUTHWEST (or SW), IN, OUT, ENTER, EXIT, CLIMB, WEAR, REMOVE, EAT, DRINK, THROW, ATTACK, KISS, BUY, SEARCH, TOUCH, FEEL, SMELL, LISTEN, TASTE, THINK, JUMP, SLEEP, WAKE, SING, SWIM, PRAY, YES, NO, SORRY, AGAIN (or G), SAVE, RESTORE, RESTART, QUIT, SCORE, UNDO, ABOUT.
3. For multiple actions, separate them with a period and a space on the SAME line.
4. Use object names EXACTLY as they appear in the game context text. Do not abbreviate or rename objects.
5. Refer to the game context to identify valid objects, directions, and characters.
6. If the input is already a valid parser command, output it unchanged (in uppercase).
7. If you genuinely cannot interpret the input, output it unchanged in uppercase.
8. NEVER output anything other than the command(s). No prefixes like "Command:" or "Output:".

EXAMPLES:
Input: "go north and take the rusty key from the table"
NORTH. TAKE RUSTY KEY FROM TABLE

Input: "I want to look around and see what's here"
LOOK

Input: "pick up the shiny key and unlock the wooden door with it"
TAKE SHINY KEY. UNLOCK WOODEN DOOR WITH SHINY KEY

Input: "ask the wizard about the crystal"
ASK WIZARD ABOUT CRYSTAL

Input: "put the red book on the shelf"
PUT RED BOOK ON SHELF

Input: "what's in my bag?"
SEARCH BAG

Input: "n"
N)";

// ---------------------------------------------------------------------------
// Bypass heuristic — skip the API for already-valid simple commands
// ---------------------------------------------------------------------------

static bool is_simple_command(const std::string &input)
{
    // Trim and uppercase the input for comparison
    std::string trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty()) {
        return true;
    }

    // Uppercase it
    std::string upper;
    upper.reserve(trimmed.size());
    for (char c : trimmed) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    // Single-letter commands and common abbreviations
    static const char *simple_commands[] = {
        "N", "S", "E", "W", "U", "D",
        "NE", "NW", "SE", "SW",
        "L", "X", "I", "Z", "G", "Q",
        "NORTH", "SOUTH", "EAST", "WEST", "UP", "DOWN",
        "NORTHEAST", "NORTHWEST", "SOUTHEAST", "SOUTHWEST",
        "LOOK", "INVENTORY", "WAIT", "AGAIN", "QUIT",
        "SAVE", "RESTORE", "RESTART", "UNDO", "SCORE",
        "YES", "NO", "ABOUT", "HELP", "VERBOSE", "BRIEF",
        "SUPERBRIEF", "VERSION", "EXITS",
        nullptr
    };

    for (const char **cmd = simple_commands; *cmd != nullptr; ++cmd) {
        if (upper == *cmd) {
            return true;
        }
    }

    // Two-word commands starting with common IF verbs followed by a single word
    // (e.g., "EXAMINE LAMP", "TAKE KEY", "GO NORTH")
    // These are already valid parser input.
    // Heuristic: if input has no articles, prepositions typical of natural language,
    // and is 1-3 words of all-alpha, it's probably already a parser command.
    bool has_natural_language_markers = false;
    static const char *nl_markers[] = {
        " the ", " a ", " an ", " my ", " some ", " that ", " this ",
        " and ", " then ", " also ", " please ", " want ", " would ",
        " could ", " should ", " i'd ", " i'll ", " i'm ", " let's ",
        " try ", " can ",
        nullptr
    };

    std::string padded = " " + upper + " ";
    for (const char **marker = nl_markers; *marker != nullptr; ++marker) {
        std::string upper_marker(*marker);
        for (char &c : upper_marker) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (padded.find(upper_marker) != std::string::npos) {
            has_natural_language_markers = true;
            break;
        }
    }

    // If no NL markers and 3 words or fewer, treat as already-valid
    if (!has_natural_language_markers) {
        int word_count = 0;
        bool in_word = false;
        for (char c : trimmed) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                word_count++;
            }
        }
        if (word_count <= 3) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Shell-escape helper for safe popen() invocation
// ---------------------------------------------------------------------------

static std::string shell_escape(const std::string &s)
{
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

// ---------------------------------------------------------------------------
// Execute curl and return response body
// ---------------------------------------------------------------------------

static std::string call_anthropic_api(const std::string &user_message)
{
    // Build the JSON request body
    json request_body;
    request_body["model"] = MODEL;
    request_body["max_tokens"] = MAX_TOKENS;
    request_body["system"] = SYSTEM_PROMPT;
    request_body["messages"] = json::array({
        {{"role", "user"}, {"content", user_message}}
    });

    std::string body_str = request_body.dump();

    // Build curl command
    // -s: silent, -S: show errors, --max-time: timeout
    std::ostringstream cmd;
    cmd << "curl -sS --max-time " << TIMEOUT_SECS
        << " -X POST https://api.anthropic.com/v1/messages"
        << " -H 'Content-Type: application/json'"
        << " -H " << shell_escape("x-api-key: " + s_api_key)
        << " -H 'anthropic-version: 2023-06-01'"
        << " -d " << shell_escape(body_str)
        << " 2>/dev/null";

    std::string command = cmd.str();

    // Execute via popen
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        std::cerr << "[Informed] Failed to execute curl" << std::endl;
        return "";
    }

    std::string response;
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        response += buffer.data();
    }

    int status = pclose(pipe);
    if (status != 0) {
        std::cerr << "[Informed] curl exited with status " << status << std::endl;
        return "";
    }

    return response;
}

// ---------------------------------------------------------------------------
// Parse the Anthropic API response
// ---------------------------------------------------------------------------

static std::string parse_response(const std::string &response_str)
{
    if (response_str.empty()) {
        return "";
    }

    try {
        json response = json::parse(response_str);

        // Check for API errors
        if (response.contains("error")) {
            std::string err_msg = response["error"].value("message", "unknown error");
            std::cerr << "[Informed] API error: " << err_msg << std::endl;
            return "";
        }

        // Extract text from the content array
        if (response.contains("content") && response["content"].is_array()) {
            for (const auto &block : response["content"]) {
                if (block.value("type", "") == "text") {
                    std::string text = block.value("text", "");
                    // Trim whitespace
                    text.erase(0, text.find_first_not_of(" \t\r\n"));
                    text.erase(text.find_last_not_of(" \t\r\n") + 1);
                    return text;
                }
            }
        }
    } catch (const json::exception &e) {
        std::cerr << "[Informed] JSON parse error: " << e.what() << std::endl;
    }

    return "";
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void init()
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    const char *key = std::getenv("ANTHROPIC_API_KEY");
    if (key != nullptr && key[0] != '\0') {
        s_api_key = key;
        s_available = true;
        std::cerr << "[Informed] LLM translation enabled (model: " << MODEL << ")" << std::endl;
    } else {
        s_available = false;
        std::cerr << "[Informed] No ANTHROPIC_API_KEY found; LLM translation disabled." << std::endl;
        std::cerr << "[Informed] Set ANTHROPIC_API_KEY environment variable to enable natural language input." << std::endl;
    }
}

bool is_available()
{
    if (!s_initialized) {
        init();
    }
    return s_available;
}

std::string get_game_context(int max_chars)
{
    auto scrollback = gli_get_scrollback();
    if (!scrollback.has_value() || scrollback->empty()) {
        return "";
    }

    std::string full_text(scrollback->begin(), scrollback->end());

    // Truncate to last max_chars characters (most recent output)
    if (static_cast<int>(full_text.size()) > max_chars) {
        // Find a newline near the cut point to avoid splitting mid-line
        size_t start = full_text.size() - max_chars;
        size_t nl = full_text.find('\n', start);
        if (nl != std::string::npos && nl < start + 200) {
            start = nl + 1;
        }
        full_text = full_text.substr(start);
    }

    return full_text;
}

std::string translate_input(const std::string &player_input,
                            const std::string &game_context)
{
    // If LLM not available, pass through
    if (!s_available) {
        return player_input;
    }

    // If input is already a simple/valid parser command, skip the API call
    if (is_simple_command(player_input)) {
        return player_input;
    }

    // Build the user message with game context
    std::ostringstream user_msg;

    if (!game_context.empty()) {
        user_msg << "Recent game output:\n"
                 << "---\n"
                 << game_context << "\n"
                 << "---\n\n";
    }

    user_msg << "Player input: " << player_input;

    // Call the API
    std::string response_str = call_anthropic_api(user_msg.str());
    std::string translated = parse_response(response_str);

    if (translated.empty()) {
        // API failed; fall back to original input
        return player_input;
    }

    // Log the translation for debugging (to stderr, not game output)
    std::cerr << "[Informed] \"" << player_input << "\" -> \"" << translated << "\"" << std::endl;

    return translated;
}

}} // namespace garglk::llm
