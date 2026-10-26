// VisualDSA web — Layer C: the application window (port of src/ui/MainWindow
// and its panels). Every operation goes through the tree's public API; the
// UI only replays the OperationRecord that comes back.
(function (global) {
    'use strict';

    const VDSA = global.VDSA;
    const {
        NO_NODE, OperationType, TraversalOrder, DeletionCase, ResultStatus, BinarySearchTree, AnimationController,
        TreeCanvas, operationName, isTraversal, comparisonText, pseudocodeFor, pseudocodeRow, complexityFor,
        parseKey, parseKeyList, MAX_TREE_SIZE,
    } = VDSA;

    const MAX_HISTORY = 20;
    const $ = (id) => document.getElementById(id);

    const Module = Object.freeze({
        Overview: 'Overview', Insert: 'Insert', Delete: 'Delete', Search: 'Search', Traversals: 'Traversals', Bfs: 'Bfs',
    });

    const MODULES = [
        { module: Module.Overview, name: 'Overview', group: 'WORKSPACE',
            description: 'Build a tree from your own keys or load the sample tree, then choose an operation.' },
        { module: Module.Insert, name: 'Insert', group: 'OPERATIONS',
            description: 'Follow the search path from the root and attach the new key at the first empty position.' },
        { module: Module.Delete, name: 'Delete', group: 'OPERATIONS',
            description: 'Locate the key, identify which of the three deletion cases applies, and restructure the tree.' },
        { module: Module.Search, name: 'Search', group: 'OPERATIONS',
            description: 'Compare with each node and discard one subtree per step until the key is found or a null pointer is reached.' },
        { module: Module.Traversals, name: 'Depth-first (DFS)', group: 'TRAVERSALS',
            description: 'Visit every node depth-first in inorder, preorder or postorder and watch the output sequence form.' },
        { module: Module.Bfs, name: 'Breadth-first (BFS)', group: 'TRAVERSALS',
            description: 'Visit every node level by level with a queue: the level-order traversal.' },
    ];

    function defaultOperation(module) {
        switch (module) {
        case Module.Delete: return OperationType.Delete;
        case Module.Search: return OperationType.Search;
        case Module.Traversals: return OperationType.InorderTraversal;
        case Module.Bfs: return OperationType.LevelOrderTraversal;
        case Module.Overview: return OperationType.Build;
        }
        return OperationType.Insert;
    }

    const toneFor = (status) => (status === ResultStatus.Success ? 'success' : 'error');

    function progressText(r) {
        switch (r.operation) {
        case OperationType.Insert: return `Inserting ${r.key}…`;
        case OperationType.Delete: return `Deleting ${r.key}…`;
        case OperationType.Search: return `Searching for ${r.key}…`;
        case OperationType.InorderTraversal: return 'Inorder traversal…';
        case OperationType.PreorderTraversal: return 'Preorder traversal…';
        case OperationType.PostorderTraversal: return 'Postorder traversal…';
        case OperationType.LevelOrderTraversal: return 'Level-order traversal (BFS)…';
        case OperationType.Build: return 'Building tree…';
        case OperationType.Clear: return 'Clearing tree…';
        }
        return '';
    }

    function describe(r) {
        switch (r.operation) {
        case OperationType.Insert: return `Insert ${r.key}`;
        case OperationType.Delete: return `Delete ${r.key}`;
        case OperationType.Search: return `Search ${r.key}`;
        }
        return operationName(r.operation);
    }

    function statusWord(status) {
        switch (status) {
        case ResultStatus.NotFound: return 'not found';
        case ResultStatus.Rejected: return 'rejected';
        }
        return 'ok';
    }

    function caseName(c) {
        switch (c) {
        case DeletionCase.Leaf: return 'CASE 1 · LEAF';
        case DeletionCase.OneChild: return 'CASE 2 · ONE CHILD';
        case DeletionCase.TwoChildren: return 'CASE 3 · TWO CHILDREN';
        }
        return '';
    }

    function preorderOf(insertionOrder) {
        const tree = new BinarySearchTree();
        tree.rebuild(insertionOrder);
        return tree.snapshot().preorderKeys();
    }

    const sameKeys = (a, b) => a.length === b.length && a.every((k, i) => k === b[i]);

    const escapeHtml = (text) => String(text).replace(/[&<>"']/g, (c) => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
    })[c]);

    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

    // -----------------------------------------------------------------------

    class App {
        constructor() {
            this.tree = new BinarySearchTree();
            this.tree.capacity = MAX_TREE_SIZE;
            this.module = Module.Overview;
            this.replaying = false;
            this.history = []; // newest first: { record, button }

            this.canvas = new TreeCanvas($('tree-canvas'), { onNodeSelected: () => this.refreshNodeInfo() });
            this.controller = new AnimationController({
                onRecordChanged: () => this.onRecordChanged(),
                onStepChanged: (step, count) => this.onStepChanged(step, count),
                onPlayingChanged: () => this.refreshPlayback(),
                onSpeedChanged: (speed) => { $('pb-speed').value = String(speed); },
            });

            this.buildSidebar();
            this.connectOperations();
            this.connectPlayback();
            this.installShortcuts();

            // Start with the deterministic demonstration tree.
            const initial = this.tree.rebuild(BinarySearchTree.sampleKeys());
            this.addHistory(initial);
            this.canvas.showTree(this.tree.snapshot());
            this.refreshStats(this.tree.snapshot());
            this.refreshNodeInfo();
            this.refreshPlayback();
            this.setMeasurements(null, this.tree.size, this.tree.height());
            this.selectModule(Module.Overview);
            this.setStatus('Sample tree loaded: 50, 30, 70, 20, 40, 60, 80.', 'neutral');
        }

        // --- Layout --------------------------------------------------------

        buildSidebar() {
            const nav = $('nav');
            let group = '';
            this.navButtons = new Map();
            for (const info of MODULES) {
                if (info.group !== group) {
                    group = info.group;
                    const label = document.createElement('div');
                    label.className = 'section nav-group-label';
                    label.textContent = group;
                    nav.append(label);
                }
                const button = document.createElement('button');
                button.className = 'nav-button';
                button.textContent = info.name;
                button.addEventListener('click', () => this.selectModule(info.module));
                this.navButtons.set(info.module, button);
                nav.append(button);
            }
        }

        // --- Module selection ---------------------------------------------

        selectModule(module) {
            this.module = module;
            const info = MODULES.find((m) => m.module === module);
            for (const [m, button] of this.navButtons) {
                if (m === module)
                    button.setAttribute('aria-current', 'page');
                else
                    button.removeAttribute('aria-current');
            }
            $('module-title').textContent = info.name;
            $('module-description').textContent = info.description;
            for (const page of document.querySelectorAll('.op-page'))
                page.classList.toggle('active', page.dataset.module === module);
            this.clearError();

            // With nothing recorded yet, preview the selected operation's pseudocode.
            if (!this.controller.hasRecord())
                this.showOperation(defaultOperation(module));
        }

        // --- Operations: every one goes through the tree's public API -----

        connectOperations() {
            for (const form of document.querySelectorAll('form[data-op]')) {
                const input = form.querySelector('input');
                form.addEventListener('submit', (e) => {
                    e.preventDefault();
                    this.clearError();
                    const key = this.readKey(input, '');
                    if (key === null)
                        return;
                    input.select();
                    const op = form.dataset.op;
                    this.runOperation(() => (op === 'insert' ? this.tree.insert(key)
                        : op === 'delete' ? this.tree.remove(key) : this.tree.search(key)));
                });
            }

            // The three depth-first orders and the BFS button all carry a data-order.
            for (const button of document.querySelectorAll('.op-page button[data-order]')) {
                button.addEventListener('click', () => {
                    this.clearError();
                    this.runOperation(() => this.tree.traverse(TraversalOrder[button.dataset.order]));
                });
            }

            $('form-build').addEventListener('submit', async (e) => {
                e.preventDefault();
                this.clearError();
                const input = $('tree-input');
                const parsed = parseKeyList(input.value);
                if (!parsed.ok) {
                    input.setAttribute('aria-invalid', 'true');
                    this.showError(parsed.message);
                    return;
                }
                if (await this.confirmReplace('Creating a tree from your keys', parsed.keys))
                    this.runOperation(() => this.tree.rebuild(parsed.keys));
            });
            $('sample-button').addEventListener('click', () => this.loadSample('Loading the sample tree'));
            $('clear-button').addEventListener('click', async () => {
                this.clearError();
                if (await this.confirmReplace('Clearing', []))
                    this.runOperation(() => this.tree.clearTree());
            });

            $('reset-button').addEventListener('click', async () => {
                if (await this.loadSample('Reset'))
                    this.canvas.fitToView();
            });
            $('help-button').addEventListener('click', () => this.showHelp());
            $('about-button').addEventListener('click', () => this.showAbout());

            $('zoom-out').addEventListener('click', () => this.canvas.zoomOut());
            $('zoom-in').addEventListener('click', () => this.canvas.zoomIn());
            $('zoom-reset').addEventListener('click', () => this.canvas.resetZoom());
            $('zoom-fit').addEventListener('click', () => this.canvas.fitToView());
        }

        async loadSample(action) {
            this.clearError();
            if (!(await this.confirmReplace(action, BinarySearchTree.sampleKeys())))
                return false;
            this.runOperation(() => this.tree.rebuild(BinarySearchTree.sampleKeys()));
            return true;
        }

        readKey(input, fieldName) {
            const parsed = parseKey(input.value);
            if (!parsed.ok) {
                input.setAttribute('aria-invalid', 'true');
                this.showError(fieldName ? `${fieldName}: ${parsed.message}` : parsed.message);
                input.focus();
                input.select();
                return null;
            }
            return parsed.value;
        }

        showError(message) {
            const el = $('input-error');
            el.textContent = message;
            el.hidden = false;
        }

        clearError() {
            $('input-error').hidden = true;
            $('input-error').textContent = '';
            for (const input of document.querySelectorAll('.panel input[aria-invalid]'))
                input.removeAttribute('aria-invalid');
        }

        runOperation(operation) {
            let record;
            try {
                record = operation();
            } catch (error) {
                // Only raised when an internal invariant check fails, i.e. a bug.
                this.setStatus('Internal error: operation aborted.', 'error');
                this.showDialog('Internal error', `<p>An internal consistency check failed:</p><p>${escapeHtml(error.message)}</p>`);
                return;
            }
            this.addHistory(record);
            this.present(record, false);
        }

        present(record, replay) {
            this.replaying = replay;
            this.controller.load(record, true); // cancels any playback in progress
        }

        async confirmReplace(action, newKeys) {
            if (this.tree.empty())
                return true;
            if (sameKeys(this.tree.snapshot().preorderKeys(), preorderOf(newKeys)))
                return true; // the result would be identical; nothing is lost
            return this.showDialog('Replace the current tree?',
                `<p>${escapeHtml(action)} will replace the current tree (${this.tree.size} nodes). Continue?</p>`,
                [{ label: 'Cancel', value: 'cancel' }, { label: 'Yes', value: 'yes', primary: true }]).then((v) => v === 'yes');
        }

        // --- History -------------------------------------------------------

        addHistory(record) {
            const r = record.result;
            const li = document.createElement('li');
            const button = document.createElement('button');
            button.type = 'button';
            button.dataset.tone = toneFor(r.status);
            button.textContent = `${describe(r)}   ·   ${statusWord(r.status)}`;
            button.title = r.message;
            const entry = { record, button };
            button.addEventListener('click', () => {
                for (const h of this.history)
                    h.button.removeAttribute('aria-current');
                button.setAttribute('aria-current', 'true');
                this.present(entry.record, true);
            });
            li.append(button);
            $('history').prepend(li);
            this.history.unshift(entry);
            while (this.history.length > MAX_HISTORY) {
                this.history.pop();
                $('history').lastElementChild.remove();
            }
        }

        // --- Playback ------------------------------------------------------

        connectPlayback() {
            const c = this.controller;
            $('pb-previous').addEventListener('click', () => c.previous());
            $('pb-next').addEventListener('click', () => c.next());
            $('pb-play').addEventListener('click', () => c.togglePlayPause());
            $('pb-restart').addEventListener('click', () => c.restart());
            $('pb-timeline').addEventListener('input', (e) => c.jumpTo(Number(e.target.value)));
            $('pb-speed').addEventListener('change', (e) => c.setSpeed(Number(e.target.value)));
        }

        installShortcuts() {
            const c = this.controller;
            document.addEventListener('keydown', (e) => {
                if ($('dialog').open)
                    return;
                const ctrl = e.ctrlKey || e.metaKey;
                let handled = true;
                if (e.key === 'F1')
                    this.showHelp();
                else if (ctrl && e.key === ' ')
                    c.togglePlayPause();
                else if (ctrl && e.key === 'ArrowRight')
                    c.next();
                else if (ctrl && e.key === 'ArrowLeft')
                    c.previous();
                else if (ctrl && e.key === '0')
                    this.canvas.fitToView();
                else if (ctrl && (e.key === '=' || e.key === '+'))
                    this.canvas.zoomIn();
                else if (ctrl && e.key === '-')
                    this.canvas.zoomOut();
                else
                    handled = false;
                if (handled)
                    e.preventDefault();
            });
            // Bare keys only while the canvas has focus, so typing in inputs is unaffected.
            $('tree-canvas').addEventListener('keydown', (e) => {
                if (e.ctrlKey || e.metaKey || e.altKey)
                    return;
                if (e.key === ' ')
                    c.togglePlayPause();
                else if (e.key === 'ArrowRight')
                    c.next();
                else if (e.key === 'ArrowLeft')
                    c.previous();
                else
                    return;
                e.preventDefault();
            });
        }

        transitionDuration() {
            return reducedMotion.matches ? 0 : Math.min(460, Math.floor(this.controller.stepIntervalMs() * 0.6));
        }

        onRecordChanged() {
            if (this.controller.record)
                this.showOperation(this.controller.record.result.operation);
        }

        onStepChanged(step, count) {
            const event = this.controller.currentEvent();
            const snapshot = this.controller.currentSnapshot();
            if (!event || !snapshot) {
                const live = this.tree.snapshot();
                this.canvas.showTree(live, this.transitionDuration());
                this.clearExplanation();
                this.setActiveLine(VDSA.PseudoLine.None);
                this.setMeasurements(null, this.tree.size, this.tree.height());
                this.refreshPlayback();
                this.refreshStats(live);
                this.refreshNodeInfo();
                return;
            }

            this.canvas.showStep(snapshot, event, this.transitionDuration());
            this.setActiveLine(event.line);
            this.setExplanation(event, snapshot, step, count);
            this.setMeasurements(event.metrics, snapshot.size, snapshot.height());
            this.refreshPlayback();
            this.refreshStats(snapshot);
            this.refreshNodeInfo();

            const result = this.controller.record.result;
            const prefix = this.replaying ? 'Replay · ' : '';
            if (step === count - 1)
                this.setStatus(prefix + result.message, toneFor(result.status));
            else
                this.setStatus(prefix + progressText(result), 'active');
        }

        refreshPlayback() {
            const c = this.controller;
            const step = c.currentStep;
            const count = c.stepCount;
            const has = count > 0;
            $('pb-previous').disabled = !(has && step > 0);
            $('pb-next').disabled = !(has && step < count - 1);
            $('pb-play').disabled = count <= 1;
            $('pb-restart').disabled = count <= 1;
            $('pb-play').classList.toggle('playing', c.playing);
            $('pb-play').setAttribute('aria-label', c.playing ? 'Pause' : 'Play');

            const timeline = $('pb-timeline');
            timeline.disabled = count <= 1;
            timeline.max = String(Math.max(0, count - 1));
            timeline.value = String(step);
            timeline.style.setProperty('--pct', count > 1 ? `${(step / (count - 1)) * 100}%` : '0%');

            const pad = (n) => String(n).padStart(2, '0');
            $('pb-step').textContent = has ? `STEP ${pad(step + 1)} / ${pad(count)}` : 'NO OPERATION YET';
        }

        // --- Panels ----------------------------------------------------------

        showOperation(op) {
            $('pseudocode-title').textContent = `PSEUDOCODE  ·  ${operationName(op).toUpperCase()}`;
            const list = $('pseudocode');
            list.replaceChildren(...pseudocodeFor(op).map((l) => {
                const li = document.createElement('li');
                li.textContent = '  '.repeat(l.indent) + l.text;
                return li;
            }));
            this.pseudocodeOp = op;

            const c = complexityFor(op);
            $('cx-best').textContent = c.best;
            $('cx-average').textContent = c.average;
            $('cx-worst').textContent = c.worst;
            $('cx-space').textContent = c.space;
            $('cx-note').textContent = c.note;
        }

        setActiveLine(line) {
            const list = $('pseudocode');
            const row = pseudocodeRow(this.pseudocodeOp, line);
            [...list.children].forEach((li, i) => li.classList.toggle('active', i === row));
            const active = list.children[row];
            if (active) {
                // Scroll only the listing itself, never the page.
                const top = active.offsetTop;
                const bottom = top + active.offsetHeight;
                if (top < list.scrollTop)
                    list.scrollTop = top - 6;
                else if (bottom > list.scrollTop + list.clientHeight)
                    list.scrollTop = bottom - list.clientHeight + 6;
            }
        }

        setExplanation(event, snapshot, step, count) {
            let header = `STEP ${step + 1} / ${count}  ·  ${operationName(event.operation).toUpperCase()}`;
            if (event.deletionCase !== DeletionCase.None)
                header += `  ·  ${caseName(event.deletionCase)}`;
            $('explain-step').textContent = header;

            const comparison = comparisonText(event);
            $('explain-comparison').hidden = !comparison;
            $('explain-comparison').textContent = comparison ? `compare  ${comparison}` : '';
            $('explain-action').textContent = event.action;
            $('explain-reason').textContent = event.reason;

            const traversal = isTraversal(event.operation);
            $('explain-output-label').hidden = !traversal;
            $('explain-output').hidden = !traversal;
            if (traversal)
                $('explain-output').textContent = event.sequence.length ? event.sequence.join('  →  ') : '—';

            // Level-order traversal: the queue, looked up as keys in this step's snapshot.
            const levelOrder = event.operation === OperationType.LevelOrderTraversal;
            $('explain-queue-label').hidden = !levelOrder;
            $('explain-queue').hidden = !levelOrder;
            if (levelOrder) {
                const waiting = event.queue.map((id) => snapshot.find(id)).filter(Boolean).map((n) => n.key);
                $('explain-queue').textContent = waiting.length ? waiting.join('   ') : 'empty';
            }
        }

        clearExplanation() {
            $('explain-step').textContent = 'NO STEP SELECTED';
            $('explain-comparison').hidden = true;
            $('explain-action').textContent = 'Run an operation to see it explained step by step.';
            $('explain-reason').textContent = 'Each step shows the comparison made, the action taken and the reason for it.';
            for (const id of ['explain-queue-label', 'explain-queue', 'explain-output-label', 'explain-output'])
                $(id).hidden = true;
        }

        setMeasurements(metrics, size, height) {
            const value = (k) => (metrics ? String(metrics[k]) : '—');
            $('m-visited').textContent = value('nodesVisited');
            $('m-comparisons').textContent = value('keyComparisons');
            $('m-traversal').textContent = value('traversalVisits');
            $('m-changes').textContent = value('structuralChanges');
            $('m-size').textContent = String(size);
            $('m-height').textContent = height >= 0 ? String(height) : '— (empty)';
        }

        refreshStats(snapshot) {
            const root = snapshot.find(snapshot.root);
            $('stat-root').textContent = root ? String(root.key) : 'null';
            $('stat-size').textContent = String(snapshot.size);
            $('stat-height').textContent = snapshot.empty() ? '—' : String(snapshot.height());
        }

        refreshNodeInfo() {
            const snapshot = this.canvas.snapshot;
            let id = this.canvas.selected;
            let prefix = 'selected';
            if (id === NO_NODE) {
                id = this.canvas.highlightedNode();
                prefix = 'active';
            }
            const out = $('stat-node');
            if (id === NO_NODE) {
                out.textContent = '— click a node to inspect';
                return;
            }
            const node = snapshot.find(id);
            if (!node) {
                out.textContent = `${prefix} node removed`;
                return;
            }
            const children = (node.left !== NO_NODE ? 1 : 0) + (node.right !== NO_NODE ? 1 : 0);
            const parent = snapshot.find(node.parent);
            out.textContent = `${prefix} ${node.key} · depth ${node.depth} · `
                + (children === 0 ? 'leaf' : children === 1 ? '1 child' : '2 children')
                + (parent ? ` · parent ${parent.key}` : ' · root');
        }

        setStatus(text, tone) {
            $('status').textContent = text;
            $('status').dataset.tone = tone;
        }

        // --- Dialogs -------------------------------------------------------

        showDialog(title, html, buttons = [{ label: 'OK', value: 'ok', primary: true }]) {
            const dialog = $('dialog');
            $('dialog-title').textContent = title;
            $('dialog-body').innerHTML = html;
            $('dialog-buttons').replaceChildren(...buttons.map((b) => {
                const button = document.createElement('button');
                button.value = b.value;
                button.textContent = b.label;
                if (b.primary)
                    button.className = 'primary';
                return button;
            }));
            dialog.returnValue = '';
            dialog.showModal();
            const fallback = buttons.find((b) => !b.primary) || buttons[0];
            dialog.querySelector(`button[value="${fallback.value}"]`).focus();
            return new Promise((resolve) => {
                dialog.addEventListener('close', () => resolve(dialog.returnValue || fallback.value), { once: true });
            });
        }

        showHelp() {
            this.showDialog('Using VisualDSA', `
                <p><b>Run an operation</b><br>Pick an operation in the sidebar, enter a key in the right panel and press
                Enter or the button. The algorithm runs immediately on the tree; the canvas then replays every recorded
                step.</p>
                <p><b>Playback</b><br>Previous / Play-Pause / Next / Restart, a speed selector (0.5× – 4×) and a timeline
                you can drag.</p>
                <p><b>Shortcuts</b><br><kbd>Ctrl</kbd>+<kbd>Space</kbd> play/pause · <kbd>Ctrl</kbd>+<kbd>←</kbd> /
                <kbd>Ctrl</kbd>+<kbd>→</kbd> step · <kbd>Ctrl</kbd>+<kbd>0</kbd> fit · <kbd>Ctrl</kbd>+<kbd>=</kbd> /
                <kbd>Ctrl</kbd>+<kbd>−</kbd> zoom · <kbd>F1</kbd> help. With the canvas focused: <kbd>Space</kbd>,
                <kbd>←</kbd>, <kbd>→</kbd>.</p>
                <p><b>Canvas</b><br>Scroll to zoom, drag to pan, double-click empty space to fit, click a node to inspect
                it.</p>
                <p><b>History</b><br>Click a history entry to replay that operation's recorded steps.</p>`);
        }

        showAbout() {
            this.showDialog('About VisualDSA', `
                <p><b>VisualDSA 1.0.0 · Web edition</b><br>Interactive Binary Search Tree Operations and Traversal
                Visualizer.</p>
                <p>A Data Structures and Algorithms mini project. This browser version is a JavaScript port of the C++17 /
                Qt 6 desktop application in the same repository: the same algorithms, recorded events, explanations and
                pseudocode. Every animation is a replay of events recorded while the real BST algorithm ran.</p>`);
        }
    }

    window.addEventListener('DOMContentLoaded', () => { VDSA.app = new App(); });
})(typeof globalThis !== 'undefined' ? globalThis : this);
