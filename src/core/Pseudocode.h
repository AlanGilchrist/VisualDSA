#pragma once

#include "AlgorithmEvent.h"

#include <vector>

namespace vdsa {

struct PseudoLineSpec {
    PseudoLine id; // PseudoLine::None for lines no event points at
    int indent;    // indentation level (2 spaces per level when displayed)
    const char* text;
};

// The pseudocode listing for an operation. Every event emitted by that
// operation refers to a line id that appears in its listing.
const std::vector<PseudoLineSpec>& pseudocodeFor(OperationType op);

// Row index of a line id within an operation's listing, or -1.
int pseudocodeRow(OperationType op, PseudoLine line);

struct ComplexityInfo {
    const char* best;
    const char* average;
    const char* worst;
    const char* space;
    const char* note;
};

// Theoretical complexity for an ordinary (non-self-balancing) BST.
const ComplexityInfo& complexityFor(OperationType op);

} // namespace vdsa
