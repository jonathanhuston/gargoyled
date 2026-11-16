// Copyright (C) 2026 by Jonathan Huston.
//
// This file is part of Gargoyled (Gargoyle fork with LLM-powered natural language input).
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// gargoyled-translate: translates natural-language player input into IF
// parser commands using the on-device Apple Intelligence model (Foundation
// Models framework). Spawned by garglk/llm.cpp once per translated input.
//
// Usage:
//   gargoyled-translate --check
//       Exit 0 if the on-device model is available, 1 otherwise (reason on stderr).
//   gargoyled-translate
//       Reads a JSON object {"instructions": "...", "prompt": "..."} on stdin,
//       writes the translated command to stdout and exits 0. On any failure,
//       writes a message to stderr and exits non-zero.

import Foundation
import FoundationModels

let timeoutSeconds = 10.0

struct Request: Decodable {
    let instructions: String
    let prompt: String
}

func fail(_ message: String, code: Int32 = 1) -> Never {
    FileHandle.standardError.write(Data("gargoyled-translate: \(message)\n".utf8))
    exit(code)
}

func unavailableReason(_ model: SystemLanguageModel) -> String? {
    switch model.availability {
    case .available:
        return nil
    case .unavailable(.deviceNotEligible):
        return "this device does not support Apple Intelligence"
    case .unavailable(.appleIntelligenceNotEnabled):
        return "Apple Intelligence is not enabled in System Settings"
    case .unavailable(.modelNotReady):
        return "the on-device model is not ready yet (still downloading?)"
    case .unavailable(let other):
        return "the on-device model is unavailable (\(other))"
    }
}

// The small on-device model sometimes wraps its answer in quotes or adds a
// label; reduce the output to the bare command line.
func clean(_ raw: String) -> String {
    let line = raw
        .split(whereSeparator: \.isNewline)
        .map { $0.trimmingCharacters(in: .whitespaces) }
        .first { !$0.isEmpty } ?? ""

    var text = line
    for prefix in ["Output:", "Command:", "Commands:", "Translation:", ">"] {
        if text.lowercased().hasPrefix(prefix.lowercased()) {
            text = String(text.dropFirst(prefix.count)).trimmingCharacters(in: .whitespaces)
        }
    }
    let quotes = CharacterSet(charactersIn: "\"'`“”‘’")
    return text.trimmingCharacters(in: quotes).trimmingCharacters(in: .whitespaces)
}

// Translating player text is a content transformation, so use the permissive
// guardrails; otherwise ordinary IF actions ("kill the troll with the sword")
// can trip the default safety filter.
let model = SystemLanguageModel(guardrails: .permissiveContentTransformations)

if CommandLine.arguments.contains("--check") {
    if let reason = unavailableReason(model) {
        fail(reason)
    }
    exit(0)
}

if let reason = unavailableReason(model) {
    fail(reason)
}

let input = FileHandle.standardInput.readDataToEndOfFile()
guard let request = try? JSONDecoder().decode(Request.self, from: input) else {
    fail("could not parse request JSON on stdin")
}

DispatchQueue.global().asyncAfter(deadline: .now() + timeoutSeconds) {
    fail("timed out after \(Int(timeoutSeconds)) seconds", code: 2)
}

Task {
    do {
        let session = LanguageModelSession(model: model, instructions: request.instructions)
        let options = GenerationOptions(samplingMode: .greedy, maximumResponseTokens: 100)
        let response = try await session.respond(to: request.prompt, options: options)
        let command = clean(response.content)
        if command.isEmpty {
            fail("model returned an empty response")
        }
        print(command)
        exit(0)
    } catch {
        fail("generation failed: \(error)")
    }
}

dispatchMain()
