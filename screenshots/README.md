# Screenshots

These images were produced by `tools/capture_screenshots.cpp`. It drives the real `MainWindow` through a fixed script, so every state shown comes from a real operation on the sample tree `50, 30, 70, 20, 40, 60, 80`.

| File | Shows |
|---|---|
| `01-overview.png` | Start-up state with the sample tree loaded |
| `02-insert-empty-slot.png` | Insert 45: the search path ends at the empty right child of 40 |
| `03-insert-completed.png` | Insert 45 completed |
| `04-search-compare.png` | Search 60: comparison `60 < 70` |
| `05-search-found.png` | Search 60: key found |
| `06-search-not-found.png` | Search 65: reached a null pointer |
| `07-delete-two-children-case.png` | Delete 50: case 3 identified |
| `08-delete-successor-found.png` | Delete 50: inorder successor 60 found |
| `09-delete-completed.png` | Delete 50 completed (60 is the new root key) |
| `10-update-transaction.png` | Update 40 → 65: transaction begins on a working copy |
| `11-update-reinsert.png` | Update 40 → 65: reinsertion path for 65 |
| `12-update-committed.png` | Update committed |
| `13-inorder-midway.png` | Inorder traversal after four outputs |
| `14-postorder-midway.png` | Postorder traversal after four outputs |
| `15-algorithm-reference.png` | Algorithm Reference page |

To regenerate them:

```bat
cmake -S . -B build -DVISUALDSA_BUILD_TOOLS=ON
cmake --build build
build\bin\capture_screenshots.exe screenshots
```
