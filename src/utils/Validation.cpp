#include "Validation.h"

#include <cctype>
#include <climits>
#include <unordered_map>

namespace vdsa {

namespace {

std::string trim(const std::string& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(begin, end - begin);
}

std::vector<std::string> splitWhitespace(const std::string& text)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty())
                parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty())
        parts.push_back(current);
    return parts;
}

KeyParseResult failure(InputError error, std::string message)
{
    KeyParseResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

} // namespace

KeyParseResult parseKey(const std::string& rawText)
{
    const std::string text = trim(rawText);
    if (text.empty())
        return failure(InputError::Empty, "Enter a key.");

    std::size_t pos = 0;
    bool negative = false;
    if (text[0] == '+' || text[0] == '-') {
        negative = text[0] == '-';
        pos = 1;
    }
    if (pos == text.size())
        return failure(InputError::InvalidInteger, "\"" + text + "\" is not a whole number.");

    // Accumulate with an explicit bound so no input can overflow.
    long long magnitude = 0;
    bool overflow = false;
    for (std::size_t i = pos; i < text.size(); ++i) {
        const char c = text[i];
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return failure(InputError::InvalidInteger, "\"" + text + "\" is not a whole number.");
        if (!overflow) {
            magnitude = magnitude * 10 + (c - '0');
            if (magnitude > static_cast<long long>(INT_MAX) + 1)
                overflow = true;
        }
    }
    const long long value = negative ? -magnitude : magnitude;
    if (overflow || value < INT_MIN || value > INT_MAX)
        return failure(InputError::Overflow, "\"" + text + "\" is outside the 32-bit integer range.");
    if (value < kMinKey || value > kMaxKey) {
        return failure(InputError::OutOfRange, "Keys must be between " + std::to_string(kMinKey) + " and "
                                                   + std::to_string(kMaxKey) + " so they fit inside a node.");
    }

    KeyParseResult result;
    result.ok = true;
    result.value = static_cast<int>(value);
    return result;
}

KeyListParseResult parseKeyList(const std::string& text, std::size_t maxKeys)
{
    KeyListParseResult result;
    auto fail = [&result](InputError error, std::string message) {
        result.ok = false;
        result.keys.clear();
        result.error = error;
        result.message = std::move(message);
        return result;
    };

    if (trim(text).empty())
        return fail(InputError::Empty, "Enter at least one key, e.g. 50, 30, 70.");

    // Split on commas; each field may hold one or more whitespace-separated keys.
    std::vector<std::string> fields;
    std::string current;
    for (char c : text) {
        if (c == ',') {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(current);
    if (fields.size() > 1 && trim(fields.back()).empty())
        fields.pop_back(); // tolerate one trailing comma

    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::vector<std::string> tokens = splitWhitespace(fields[i]);
        if (tokens.empty())
            return fail(InputError::EmptyEntry, "Entry " + std::to_string(i + 1) + " is empty (two commas in a row?).");
        for (const std::string& token : tokens) {
            const KeyParseResult key = parseKey(token);
            if (!key.ok)
                return fail(key.error, key.message);
            result.keys.push_back(key.value);
        }
    }

    if (result.keys.size() > maxKeys) {
        return fail(InputError::TooManyKeys, std::to_string(result.keys.size()) + " keys given; the visualizer "
                                                 "supports at most " + std::to_string(maxKeys) + ".");
    }

    std::unordered_map<int, int> counts;
    for (int key : result.keys) {
        if (++counts[key] == 2) {
            return fail(InputError::DuplicateKeys, "Key " + std::to_string(key)
                                                       + " appears more than once. This BST stores unique keys only, "
                                                         "so the current tree was not changed.");
        }
    }

    result.ok = true;
    return result;
}

} // namespace vdsa
