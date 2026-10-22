#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace vdsa {

// Keys are limited to a range that fits legibly inside a drawn node.
inline constexpr int kMinKey = -9999;
inline constexpr int kMaxKey = 9999;
// Upper bound on tree size for the visualizer (keeps snapshots cheap and
// every node readable on screen).
inline constexpr std::size_t kMaxTreeSize = 64;

enum class InputError {
    None,
    Empty,
    InvalidInteger,
    Overflow,     // outside the 32-bit signed integer range
    OutOfRange,   // a valid int, but outside [kMinKey, kMaxKey]
    EmptyEntry,   // e.g. "10,,20"
    DuplicateKeys,
    TooManyKeys,
};

struct KeyParseResult {
    bool ok = false;
    int value = 0;
    InputError error = InputError::None;
    std::string message;
};

struct KeyListParseResult {
    bool ok = false;
    std::vector<int> keys;
    InputError error = InputError::None;
    std::string message;
};

// Parses a single key such as " -42 ". Leading '+' or '-' is allowed;
// anything else besides digits is rejected.
KeyParseResult parseKey(const std::string& text);

// Parses a comma- and/or whitespace-separated list such as "50, 30, 70".
// A single trailing comma is tolerated. Duplicates are rejected with an
// explanation of the duplicate-key policy.
KeyListParseResult parseKeyList(const std::string& text, std::size_t maxKeys = kMaxTreeSize);

} // namespace vdsa
