#pragma once

#include "AlgorithmEvent.h"
#include "TreeSnapshot.h"

#include <optional>
#include <string>
#include <vector>

namespace vdsa {

enum class ResultStatus {
    Success,  // operation did what was asked
    NotFound, // key absent; tree unchanged
    NoChange, // request was valid but nothing needed to change
    Rejected, // request violated a rule (duplicate, missing key, ...); tree unchanged
    Failed,   // an internal check failed and the change was rolled back
};

enum class ResultCode {
    Ok,
    KeyFound,
    KeyNotFound,
    DuplicateKey,
    KeyMissing,
    SameKey,
    NewKeyExists,
    CapacityReached,
    InvariantViolation,
};

struct OperationResult {
    OperationType operation = OperationType::Insert;
    ResultStatus status = ResultStatus::Success;
    ResultCode code = ResultCode::Ok;
    int key = 0;
    std::optional<int> secondaryKey; // update: the new key
    std::string message;             // short status line, e.g. "Insertion completed."
    OperationMetrics metrics;        // final counter values
    std::vector<int> sequence;       // traversal output
    DeletionCase deletionCase = DeletionCase::None;
    bool treeChanged = false;

    bool succeeded() const { return status == ResultStatus::Success; }
};

// Complete, self-contained recording of one operation: its result, every
// step, and every distinct tree state those steps refer to. Records never
// point into the live tree, so they can be replayed at any time.
struct OperationRecord {
    OperationResult result;
    std::vector<TreeSnapshot> snapshots; // [0] = state before the operation
    std::vector<AlgorithmEvent> events;

    const TreeSnapshot& before() const { return snapshots.front(); }
    const TreeSnapshot& after() const { return snapshots.back(); }
    const TreeSnapshot& snapshotFor(const AlgorithmEvent& e) const { return snapshots.at(e.snapshotIndex); }
};

} // namespace vdsa
