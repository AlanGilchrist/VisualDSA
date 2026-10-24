// VisualDSA web — input parsing (port of src/utils/Validation.cpp).
(function (global) {
    'use strict';

    const VDSA = global.VDSA || (global.VDSA = {});

    // Keys are limited to a range that fits legibly inside a drawn node.
    const MIN_KEY = -9999;
    const MAX_KEY = 9999;
    // Upper bound on tree size (keeps snapshots cheap and every node readable).
    const MAX_TREE_SIZE = 64;

    const INT_MIN = -2147483648;
    const INT_MAX = 2147483647;

    const InputError = Object.freeze({
        None: 'None',
        Empty: 'Empty',
        InvalidInteger: 'InvalidInteger',
        Overflow: 'Overflow',     // outside the 32-bit signed integer range
        OutOfRange: 'OutOfRange', // a valid int, but outside [MIN_KEY, MAX_KEY]
        EmptyEntry: 'EmptyEntry', // e.g. "10,,20"
        DuplicateKeys: 'DuplicateKeys',
        TooManyKeys: 'TooManyKeys',
    });

    const failure = (error, message) => ({ ok: false, value: 0, error, message });

    // Parses a single key such as " -42 ". A leading '+' or '-' is allowed;
    // anything else besides digits is rejected.
    function parseKey(rawText) {
        const text = String(rawText).trim();
        if (!text)
            return failure(InputError.Empty, 'Enter a key.');
        if (!/^[+-]?\d+$/.test(text))
            return failure(InputError.InvalidInteger, `"${text}" is not a whole number.`);

        // BigInt keeps arbitrarily long digit strings exact, so nothing overflows.
        const value = BigInt(text);
        if (value < BigInt(INT_MIN) || value > BigInt(INT_MAX))
            return failure(InputError.Overflow, `"${text}" is outside the 32-bit integer range.`);
        if (value < BigInt(MIN_KEY) || value > BigInt(MAX_KEY))
            return failure(InputError.OutOfRange, `Keys must be between ${MIN_KEY} and ${MAX_KEY} so they fit inside a node.`);
        return { ok: true, value: Number(value), error: InputError.None, message: '' };
    }

    // Parses a comma- and/or whitespace-separated list such as "50, 30, 70".
    // A single trailing comma is tolerated. Duplicates are rejected with an
    // explanation of the duplicate-key policy.
    function parseKeyList(text, maxKeys = MAX_TREE_SIZE) {
        const fail = (error, message) => ({ ok: false, keys: [], error, message });
        if (!String(text).trim())
            return fail(InputError.Empty, 'Enter at least one key, e.g. 50, 30, 70.');

        const fields = String(text).split(',');
        if (fields.length > 1 && !fields[fields.length - 1].trim())
            fields.pop(); // tolerate one trailing comma

        const keys = [];
        for (let i = 0; i < fields.length; ++i) {
            const tokens = fields[i].split(/\s+/).filter(Boolean);
            if (!tokens.length)
                return fail(InputError.EmptyEntry, `Entry ${i + 1} is empty (two commas in a row?).`);
            for (const token of tokens) {
                const key = parseKey(token);
                if (!key.ok)
                    return fail(key.error, key.message);
                keys.push(key.value);
            }
        }

        if (keys.length > maxKeys)
            return fail(InputError.TooManyKeys, `${keys.length} keys given; the visualizer supports at most ${maxKeys}.`);

        const seen = new Set();
        for (const key of keys) {
            if (seen.has(key)) {
                return fail(InputError.DuplicateKeys, `Key ${key} appears more than once. This BST stores unique keys only, `
                    + 'so the current tree was not changed.');
            }
            seen.add(key);
        }
        return { ok: true, keys, error: InputError.None, message: '' };
    }

    Object.assign(VDSA, { MIN_KEY, MAX_KEY, MAX_TREE_SIZE, InputError, parseKey, parseKeyList });
})(typeof globalThis !== 'undefined' ? globalThis : this);
