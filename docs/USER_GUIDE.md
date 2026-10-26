# User Guide

## 1. Installing and launching

1. Build the project as described in the [README](../README.md#build-and-run-on-windows).
2. Run `build\bin\VisualDSA.exe` on Windows, or `./build/bin/VisualDSA` on Linux.

The window opens with the sample tree `50, 30, 70, 20, 40, 60, 80` already loaded.

## 2. The screen

```
┌ Top bar: VisualDSA wordmark ······································· Reset  Help  About ┐
├ Sidebar ─────┬ Workspace ──────────────────────────────────┬ Right panel ─────────────┤
│ Overview     │ Module title and description                 │ OPERATION   inputs       │
│ Insert       │ ROOT · NODES · HEIGHT · NODE      [status]   │ CURRENT STEP explanation │
│ Delete       │ ┌ canvas ─────────────────── − + □ Fit ┐     │ PSEUDOCODE  synced line  │
│ Search       │ │                 tree                  │     │ COMPLEXITY  theory +     │
│ TRAVERSALS   │ │                                        │     │             measured     │
│ Depth (DFS)  │ └────────────────────────────────────────┘     │ HISTORY     replayable   │
│ Breadth (BFS)│                                              │                          │
├──────────────┴──────────────────────────────────────────────┴──────────────────────────┤
│ Playback:  ⏮  ▶  ⏭  ↻   SPEED [1×]   ───────●────────────────   STEP 03 / 09          │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

- **ROOT / NODES / HEIGHT** describe the tree *as drawn at the current step*.
- **NODE** describes the highlighted node, or the node you clicked: its depth, number of children, and parent.
- The **status** badge on the right shows progress (amber), success (green) or a failure such as a rejected or not-found key (red).

## 3. Running operations

| Module | Input | Notes |
|---|---|---|
| Overview | Comma- or space-separated keys, e.g. `50, 30, 70` | **Create tree** inserts the keys in the order given. **Load sample** restores the demonstration tree. **Clear** empties the tree. |
| Insert | One key | Duplicates are rejected and the tree stays unchanged. |
| Delete | One key | Try 20 (leaf), 30 or 50 (two children). For one child: delete 20, then delete 30. |
| Search | One key | Try 60 (found) and 65 (not found). |
| Depth-first (DFS) | Inorder, Preorder or Postorder button | |
| Breadth-first (BFS) | **Run BFS** button | Level-order traversal with a queue. The QUEUE strip and the right panel show the nodes waiting, front first. |

Press **Enter** in an input field, or click the amber button. Each operation runs at once on the C++ tree, and its recorded steps then start playing automatically.

### Input rules

- Keys must be whole numbers between **−9999 and 9999**, so they fit inside a node.
- A tree can hold at most **64 nodes**.

| Input | Message |
|---|---|
| empty | "Enter a key." |
| `abc`, `4.5` | "… is not a whole number." |
| `99999999999` | "… is outside the 32-bit integer range." |
| `10000` | "Keys must be between -9999 and 9999 …" |
| `10, 20, 10` | "Key 10 appears more than once …"; the current tree is **not** changed |
| `1,,2` | "Entry 2 is empty (two commas in a row?)." |

Invalid input never changes the tree. The field gets a red border and the message appears below it.

**Replacing the whole tree** (Create tree, Load sample, Clear, Reset) asks for confirmation first, unless the result would be the same tree.

## 4. Playback

| Control | Action | Shortcut |
|---|---|---|
| Previous | Go back one step (pauses playback) | Ctrl+Left; Left when the canvas has focus |
| Play / Pause | Start or stop automatic playback | Ctrl+Space; Space when the canvas has focus |
| Next | Go forward one step (pauses playback) | Ctrl+Right; Right when the canvas has focus |
| Restart | Jump to step 1 and play | |
| Speed | 0.5×, 1×, 2×, 4× | |
| Timeline | Drag to any step | |

Pressing Play at the last step plays the operation again from the start. Starting a new operation always cancels the playback in progress.

## 5. Reading the canvas

| Visual | Meaning |
|---|---|
| Amber node with a halo | The node being examined |
| Amber edges | The path from the root to the current node |
| `45 > 30` badge | The comparison made at this step |
| Dashed amber ring + label | A second node involved in the step: *next*, *only child*, *successor*, *parent* or *enqueued* |
| Dashed amber ring, no label (BFS) | A node waiting in the queue |
| Dashed red ring, labelled *target* | The node being deleted while its successor is searched for |
| Dashed circle `+45` | The empty slot where the key will be inserted |
| Dashed circle `null` (red) | The empty pointer where an unsuccessful search ended |
| Green node | Found, inserted, or already output by a traversal. The small number shows its output position. |
| Red node fading out | A node being removed |
| OUTPUT strip | The traversal sequence produced so far |
| QUEUE strip (BFS) | The nodes waiting in the queue, front first |

Nodes are placed in inorder order from left to right, so reading the tree horizontally gives the keys sorted.

### Navigating the canvas

- **Zoom:** mouse wheel, or the − and + buttons.
- **Pan:** drag.
- **Fit:** double-click empty space, use the **Fit** button, or press Ctrl+0.
- **Actual size:** the □ button.
- **Inspect a node:** click it; its details appear under NODE.

## 6. History

The **HISTORY** list keeps the 20 most recent operations, each with its status. Click an entry to replay its recorded steps. Each recording is a self-contained snapshot, so this works even after the tree has changed. The status badge shows "Replay ·" during a replay.

## 7. Troubleshooting

| Symptom | Fix |
|---|---|
| App does not start: missing `Qt6Core.dll` | Run the copy in `build\bin`, which has the Qt DLLs deployed, or add `<Qt>\bin` to PATH. |
| "no Qt platform plugin could be initialized" | Run from `build\bin`, which contains `platforms\qwindows.dll`, or set `QT_PLUGIN_PATH=<Qt>\plugins`. |
| Text looks too small or too large | The app follows Windows display scaling. Use Ctrl+= / Ctrl+- to zoom the canvas. |
| The tree runs off the canvas | Press **Fit** or Ctrl+0. Auto-fit turns back on until you zoom or pan manually. |
