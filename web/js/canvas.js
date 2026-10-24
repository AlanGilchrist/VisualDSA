// VisualDSA web — Layer B: tree layout and the canvas renderer
// (port of src/visualization/TreeLayout.cpp and TreeCanvas.cpp).
(function (global) {
    'use strict';

    const VDSA = global.VDSA;
    const { NO_NODE, Emphasis, SecondaryRole, OperationType, PseudoLine, TreeSnapshot, comparisonText, isTraversal } = VDSA;

    const MIN_ZOOM = 0.25;
    const MAX_ZOOM = 3.0;

    // x = inorder rank * columnWidth, y = depth * levelHeight. Every node gets
    // its own inorder column, so nodes never overlap, subtrees never interleave,
    // and reading the tree left to right gives the keys in sorted order.
    const LayoutMetrics = Object.freeze({ columnWidth: 54, levelHeight: 82, nodeRadius: 21 });

    class TreeLayout {
        constructor(snapshot, metrics = LayoutMetrics) {
            this.metrics = metrics;
            this.positions = new Map();
            this.bounds = null; // {left, top, right, bottom} of all node circles
            const r = metrics.nodeRadius;
            snapshot.inorderIds().forEach((id, rank) => {
                const node = snapshot.find(id);
                const pos = { x: rank * metrics.columnWidth, y: node.depth * metrics.levelHeight };
                this.positions.set(id, pos);
                const b = { left: pos.x - r, top: pos.y - r, right: pos.x + r, bottom: pos.y + r };
                this.bounds = this.bounds ? {
                    left: Math.min(this.bounds.left, b.left),
                    top: Math.min(this.bounds.top, b.top),
                    right: Math.max(this.bounds.right, b.right),
                    bottom: Math.max(this.bounds.bottom, b.bottom),
                } : b;
            });
        }

        contains(id) { return this.positions.has(id); }
        position(id) { return this.positions.get(id) || { x: 0, y: 0 }; }

        // Where an absent child of `parent` on `side` would be drawn.
        slotPosition(parent, side) {
            const p = this.position(parent);
            const dx = this.metrics.columnWidth * 0.5;
            return { x: side === 'left' ? p.x - dx : p.x + dx, y: p.y + this.metrics.levelHeight };
        }
    }

    // --- Colour helpers ------------------------------------------------------

    function readPalette() {
        const style = getComputedStyle(document.documentElement);
        const token = (name) => style.getPropertyValue(`--${name}`).trim();
        return {
            background: token('background'), surface: token('surface'), elevated: token('elevated'),
            textPrimary: token('text-primary'), textSecondary: token('text-secondary'), textMuted: token('text-muted'),
            accent: token('accent'), success: token('success'), error: token('error'), border: token('border'),
            nodeFill: token('node-fill'), nodeBorder: token('node-border'), accentFill: token('accent-fill'),
            successFill: token('success-fill'), errorFill: token('error-fill'), edge: token('edge'),
            grid: token('grid-dot'),
            uiFont: token('font-ui'), monoFont: token('font-mono'),
        };
    }

    function withAlpha(hex, opacity) {
        const h = hex.replace('#', '');
        const n = parseInt(h.length === 3 ? h.split('').map((c) => c + c).join('') : h, 16);
        const a = Math.max(0, Math.min(1, opacity));
        return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`;
    }

    const easeOutCubic = (t) => 1 - Math.pow(1 - t, 3);

    // Line between two circle centres, trimmed so it starts and ends at the rims.
    function rimToRim(a, b, ra, rb) {
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const len = Math.hypot(dx, dy);
        if (len <= ra + rb)
            return null;
        return {
            x1: a.x + dx * (ra / len), y1: a.y + dy * (ra / len),
            x2: a.x + dx * (1 - rb / len), y2: a.y + dy * (1 - rb / len),
        };
    }

    function roleLabel(role) {
        switch (role) {
        case SecondaryRole.NextNode: return 'next';
        case SecondaryRole.OnlyChild: return 'only child';
        case SecondaryRole.Successor: return 'successor';
        case SecondaryRole.Parent: return 'parent';
        case SecondaryRole.Target: return 'target';
        }
        return '';
    }

    // --- TreeCanvas ----------------------------------------------------------

    class TreeCanvas {
        constructor(canvas, { onNodeSelected = () => {}, onZoomChanged = () => {} } = {}) {
            this.canvas = canvas;
            this.ctx = canvas.getContext('2d');
            this.onNodeSelected = onNodeSelected;
            this.onZoomChanged = onZoomChanged;
            this.palette = readPalette();

            this.snapshot = new TreeSnapshot();
            this.fromSnapshot = this.snapshot;
            this.layout = new TreeLayout(this.snapshot);
            this.fromPositions = new Map();
            this.event = null;
            this.selected = NO_NODE;

            this.progress = 1;
            this.transitionStart = 0;
            this.transitionMs = 0;
            this.frame = 0;

            this.zoom = 1;
            this.worldCenter = { x: 0, y: 0 };
            this.pan = { x: 0, y: 0 };
            this.autoFit = true;
            this.width = 0;
            this.height = 0;

            this.drag = null;
            this.installInput();
            new ResizeObserver(() => this.resize()).observe(canvas.parentElement);
            this.resize();
        }

        // --- Frames ---

        showTree(snapshot, transitionMs = 0) { this.setFrame(snapshot, null, transitionMs); }
        showStep(snapshot, event, transitionMs = 0) { this.setFrame(snapshot, event, transitionMs); }

        setFrame(snapshot, event, transitionMs) {
            // Start the next transition from whatever is on screen right now,
            // so a new frame arriving mid-animation never makes nodes jump.
            const onScreen = new Map();
            for (const node of this.snapshot.nodes)
                onScreen.set(node.id, this.animatedPosition(node.id));

            this.fromPositions = onScreen;
            this.fromSnapshot = this.snapshot;
            this.snapshot = snapshot;
            this.layout = new TreeLayout(snapshot);
            this.event = event;
            if (this.selected !== NO_NODE && !snapshot.find(this.selected))
                this.selected = NO_NODE;

            let moved = this.fromSnapshot.size !== snapshot.size;
            for (const node of snapshot.nodes) {
                const from = this.fromPositions.get(node.id);
                const to = this.layout.position(node.id);
                if (!from || from.x !== to.x || from.y !== to.y) {
                    moved = true;
                    break;
                }
            }

            if (this.autoFit)
                this.applyFit();
            if (moved && transitionMs > 0) {
                this.progress = 0;
                this.transitionMs = transitionMs;
                this.transitionStart = performance.now();
            } else {
                this.progress = 1;
            }
            this.requestPaint();
        }

        isAnimating() { return this.progress < 1; }
        highlightedNode() { return this.event ? this.event.nodeId : NO_NODE; }
        isNewNode(id) { return this.progress < 1 && !this.fromPositions.has(id); }

        animatedPosition(id) {
            const target = this.layout.position(id);
            if (this.progress >= 1)
                return target;
            const from = this.fromPositions.get(id);
            if (!from)
                return target;
            return { x: from.x + (target.x - from.x) * this.progress, y: from.y + (target.y - from.y) * this.progress };
        }

        setSelectedNode(id) {
            this.selected = this.snapshot.find(id) ? id : NO_NODE;
            this.requestPaint();
        }

        // --- View transform: zoom, pan, fit ---

        contentBounds() {
            const b = this.layout.bounds;
            if (!b)
                return null;
            // Room for the ROOT label and comparison badges above, and for ghost
            // slots / role tags one level below the deepest node.
            return { left: b.left - 48, top: b.top - 44, right: b.right + 48, bottom: b.bottom + this.layout.metrics.levelHeight * 0.85 };
        }

        viewportArea() {
            // Leave space for the overlay badges at the top and the output strip at the bottom.
            return { left: 16, top: 48, right: this.width - 16, bottom: this.height - 56 };
        }

        toScreen(p) {
            return {
                x: this.width / 2 + this.pan.x + (p.x - this.worldCenter.x) * this.zoom,
                y: this.height / 2 + this.pan.y + (p.y - this.worldCenter.y) * this.zoom,
            };
        }

        toWorld(p) {
            return {
                x: (p.x - this.width / 2 - this.pan.x) / this.zoom + this.worldCenter.x,
                y: (p.y - this.height / 2 - this.pan.y) / this.zoom + this.worldCenter.y,
            };
        }

        applyFit() {
            const bounds = this.contentBounds();
            const area = this.viewportArea();
            const aw = area.right - area.left;
            const ah = area.bottom - area.top;
            if (!bounds || aw <= 0 || ah <= 0) {
                this.zoom = 1;
                this.worldCenter = { x: 0, y: 0 };
                this.pan = { x: 0, y: 0 };
                return;
            }
            const fit = Math.min(aw / (bounds.right - bounds.left), ah / (bounds.bottom - bounds.top));
            this.zoom = Math.max(MIN_ZOOM, Math.min(fit, 1.25)); // never blow small trees up beyond 125%
            this.worldCenter = { x: (bounds.left + bounds.right) / 2, y: (bounds.top + bounds.bottom) / 2 };
            this.pan = { x: (area.left + area.right) / 2 - this.width / 2, y: (area.top + area.bottom) / 2 - this.height / 2 };
        }

        setZoomAround(zoom, anchor) {
            zoom = Math.max(MIN_ZOOM, Math.min(MAX_ZOOM, zoom));
            const world = this.toWorld(anchor);
            this.zoom = zoom;
            this.pan = {
                x: anchor.x - this.width / 2 - (world.x - this.worldCenter.x) * zoom,
                y: anchor.y - this.height / 2 - (world.y - this.worldCenter.y) * zoom,
            };
            this.autoFit = false;
            this.requestPaint();
            this.onZoomChanged(this.zoom);
        }

        center() { return { x: this.width / 2, y: this.height / 2 }; }
        zoomIn() { this.setZoomAround(this.zoom * 1.2, this.center()); }
        zoomOut() { this.setZoomAround(this.zoom / 1.2, this.center()); }

        fitToView() {
            this.autoFit = true;
            this.applyFit();
            this.requestPaint();
            this.onZoomChanged(this.zoom);
        }

        resetZoom() {
            this.autoFit = false;
            this.zoom = 1;
            const b = this.contentBounds();
            this.worldCenter = b ? { x: (b.left + b.right) / 2, y: (b.top + b.bottom) / 2 } : { x: 0, y: 0 };
            const area = this.viewportArea();
            this.pan = { x: (area.left + area.right) / 2 - this.width / 2, y: (area.top + area.bottom) / 2 - this.height / 2 };
            this.requestPaint();
            this.onZoomChanged(this.zoom);
        }

        nodeAt(screen) {
            const world = this.toWorld(screen);
            const r = this.layout.metrics.nodeRadius + 2;
            for (const node of this.snapshot.nodes) {
                const p = this.animatedPosition(node.id);
                if (Math.hypot(world.x - p.x, world.y - p.y) <= r)
                    return node.id;
            }
            return NO_NODE;
        }

        // --- Input ---

        resize() {
            const rect = this.canvas.parentElement.getBoundingClientRect();
            const dpr = window.devicePixelRatio || 1;
            this.width = Math.max(1, Math.floor(rect.width));
            this.height = Math.max(1, Math.floor(rect.height));
            this.canvas.width = Math.round(this.width * dpr);
            this.canvas.height = Math.round(this.height * dpr);
            this.canvas.style.width = `${this.width}px`;
            this.canvas.style.height = `${this.height}px`;
            this.dpr = dpr;
            if (this.autoFit)
                this.applyFit();
            this.paint();
        }

        installInput() {
            const c = this.canvas;
            const local = (e) => {
                const r = c.getBoundingClientRect();
                return { x: e.clientX - r.left, y: e.clientY - r.top };
            };

            c.addEventListener('wheel', (e) => {
                e.preventDefault();
                const steps = -e.deltaY / (e.deltaMode === 1 ? 3 : 100);
                if (steps)
                    this.setZoomAround(this.zoom * Math.pow(1.15, steps), local(e));
            }, { passive: false });

            c.addEventListener('pointerdown', (e) => {
                if (e.button !== 0)
                    return;
                c.setPointerCapture(e.pointerId);
                const p = local(e);
                this.drag = { start: p, last: p, moved: false };
            });
            c.addEventListener('pointermove', (e) => {
                const p = local(e);
                if (this.drag) {
                    if (Math.abs(p.x - this.drag.start.x) + Math.abs(p.y - this.drag.start.y) > 3)
                        this.drag.moved = true;
                    if (this.drag.moved) {
                        c.style.cursor = 'grabbing';
                        this.pan = { x: this.pan.x + p.x - this.drag.last.x, y: this.pan.y + p.y - this.drag.last.y };
                        this.autoFit = false;
                        this.requestPaint();
                    }
                    this.drag.last = p;
                } else {
                    c.style.cursor = this.nodeAt(p) !== NO_NODE ? 'pointer' : 'default';
                }
            });
            const release = (e) => {
                if (!this.drag)
                    return;
                const wasClick = !this.drag.moved;
                this.drag = null;
                c.style.cursor = '';
                if (wasClick && e.type === 'pointerup') {
                    const id = this.nodeAt(local(e));
                    this.setSelectedNode(id);
                    this.onNodeSelected(id);
                }
            };
            c.addEventListener('pointerup', release);
            c.addEventListener('pointercancel', release);
            c.addEventListener('dblclick', (e) => {
                if (this.nodeAt(local(e)) === NO_NODE)
                    this.fitToView();
            });
        }

        // --- Painting ---

        requestPaint() {
            if (!this.frame)
                this.frame = requestAnimationFrame((now) => this.tick(now));
        }

        tick(now) {
            this.frame = 0;
            if (this.progress < 1) {
                const t = Math.min(1, (now - this.transitionStart) / this.transitionMs);
                this.progress = t >= 1 ? 1 : easeOutCubic(t);
            }
            this.paint();
            if (this.progress < 1)
                this.requestPaint();
        }

        font(weight, px, family) { return `${weight} ${px}px ${family}`; }

        paint() {
            const ctx = this.ctx;
            const P = this.palette;
            ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
            ctx.fillStyle = P.background;
            ctx.fillRect(0, 0, this.width, this.height);
            this.drawGrid(ctx);

            ctx.save();
            const origin = this.toScreen({ x: 0, y: 0 });
            ctx.translate(origin.x, origin.y);
            ctx.scale(this.zoom, this.zoom);
            this.drawEdges(ctx);
            this.drawPendingSlot(ctx);
            this.drawNodes(ctx);
            this.drawBadges(ctx);
            ctx.restore();

            this.drawOverlays(ctx);
        }

        drawGrid(ctx) {
            // A faint dot grid gives the workspace depth without competing with the tree.
            const spacing = 28;
            const origin = this.toScreen({ x: 0, y: 0 });
            const ox = ((origin.x % spacing) + spacing) % spacing;
            const oy = ((origin.y % spacing) + spacing) % spacing;
            ctx.fillStyle = this.palette.grid;
            for (let x = ox; x < this.width; x += spacing) {
                for (let y = oy; y < this.height; y += spacing)
                    ctx.fillRect(x - 0.7, y - 0.7, 1.4, 1.4);
            }
        }

        line(ctx, seg, color, width, dash = []) {
            if (!seg)
                return;
            ctx.strokeStyle = color;
            ctx.lineWidth = width;
            ctx.lineCap = 'round';
            ctx.setLineDash(dash);
            ctx.beginPath();
            ctx.moveTo(seg.x1, seg.y1);
            ctx.lineTo(seg.x2, seg.y2);
            ctx.stroke();
            ctx.setLineDash([]);
        }

        drawEdges(ctx) {
            const P = this.palette;
            const r = this.layout.metrics.nodeRadius;
            const e = this.event;

            // Edges on the active path: each consecutive (parent, child) pair.
            const pathChildren = new Set();
            if (e) {
                for (let i = 1; i < e.path.length; ++i) {
                    const child = this.snapshot.find(e.path[i]);
                    if (child && child.parent === e.path[i - 1])
                        pathChildren.add(child.id);
                }
            }

            // Edges of removed nodes fade out during a transition.
            if (this.progress < 1) {
                for (const old of this.fromSnapshot.nodes) {
                    if (old.parent === NO_NODE || this.snapshot.find(old.id))
                        continue;
                    const a = this.fromPositions.get(old.parent);
                    const b = this.fromPositions.get(old.id);
                    if (a && b)
                        this.line(ctx, rimToRim(a, b, r, r), withAlpha(P.error, 1 - this.progress), 1.6);
                }
            }

            for (const node of this.snapshot.nodes) {
                if (node.parent === NO_NODE)
                    continue;
                const seg = rimToRim(this.animatedPosition(node.parent), this.animatedPosition(node.id), r, r);
                const opacity = this.isNewNode(node.id) ? this.progress : 1;
                const onPath = pathChildren.has(node.id);
                const nextHop = e && e.secondaryRole === SecondaryRole.NextNode && e.secondaryId === node.id
                    && e.nodeId === node.parent;
                if (nextHop)
                    this.line(ctx, seg, withAlpha(P.accent, opacity), 2.4, [7.2, 6]);
                else if (onPath)
                    this.line(ctx, seg, withAlpha(P.accent, opacity), 2.4);
                else
                    this.line(ctx, seg, withAlpha(P.edge, opacity), 1.6);
            }
        }

        drawPendingSlot(ctx) {
            const e = this.event;
            if (!e || e.pendingParent === NO_NODE || !this.layout.contains(e.pendingParent))
                return;
            const P = this.palette;
            const r = this.layout.metrics.nodeRadius;
            const parent = this.animatedPosition(e.pendingParent);
            const target = this.layout.position(e.pendingParent);
            const slotTarget = this.layout.slotPosition(e.pendingParent, e.pendingSide);
            const slot = { x: parent.x + slotTarget.x - target.x, y: parent.y + slotTarget.y - target.y };
            const color = e.emphasis === Emphasis.Error ? P.error : P.accent;

            this.line(ctx, rimToRim(parent, slot, r, r * 0.8), withAlpha(color, 0.85), 1.6, [4.8, 4.8]);
            ctx.setLineDash([4.8, 4.8]);
            ctx.beginPath();
            ctx.arc(slot.x, slot.y, r * 0.8, 0, Math.PI * 2);
            ctx.fillStyle = withAlpha(color, 0.06);
            ctx.fill();
            ctx.stroke();
            ctx.setLineDash([]);

            const inserting = e.operation === OperationType.Insert || e.line === PseudoLine.UpdInsert;
            ctx.font = this.font(600, 11, P.monoFont);
            ctx.fillStyle = color;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(inserting ? `+${e.key}` : 'null', slot.x, slot.y + 0.5);
        }

        styleFor(node) {
            const P = this.palette;
            const s = { fill: P.nodeFill, border: P.nodeBorder, borderWidth: 1.5, text: P.textPrimary, halo: null, ring: null, opacity: 1, scale: 1 };
            const e = this.event;
            if (!e)
                return s;
            if (e.completed.includes(node.id)) {
                s.fill = P.successFill;
                s.border = withAlpha(P.success, 0.75);
            }
            if (e.path.includes(node.id)) {
                s.border = withAlpha(P.accent, 0.6);
                s.borderWidth = 1.8;
            }
            if (node.id === e.secondaryId) {
                switch (e.secondaryRole) {
                case SecondaryRole.Target: s.ring = P.error; break;
                case SecondaryRole.Parent: s.ring = P.textSecondary; break;
                case SecondaryRole.Successor:
                case SecondaryRole.OnlyChild:
                case SecondaryRole.NextNode: s.ring = P.accent; break;
                }
            }
            if (node.id === e.nodeId) {
                const tone = this.emphasisColors(e.emphasis);
                s.fill = tone.fill;
                s.border = tone.color;
                s.borderWidth = 2.6;
                if (e.emphasis !== Emphasis.Neutral)
                    s.halo = withAlpha(tone.color, 0.22);
            }
            return s;
        }

        emphasisColors(emphasis) {
            const P = this.palette;
            switch (emphasis) {
            case Emphasis.Success: return { color: P.success, fill: P.successFill };
            case Emphasis.Error:
            case Emphasis.Removing: return { color: P.error, fill: P.errorFill };
            case Emphasis.Neutral: return { color: P.textSecondary, fill: P.nodeFill };
            }
            return { color: P.accent, fill: P.accentFill };
        }

        drawNode(ctx, c, key, s) {
            const r = this.layout.metrics.nodeRadius * s.scale;
            ctx.save();
            ctx.globalAlpha = Math.max(0, s.opacity);
            if (s.halo) {
                ctx.beginPath();
                ctx.arc(c.x, c.y, r + 7, 0, Math.PI * 2);
                ctx.fillStyle = s.halo;
                ctx.fill();
            }
            if (s.ring) {
                ctx.beginPath();
                ctx.arc(c.x, c.y, r + 6, 0, Math.PI * 2);
                ctx.setLineDash([4, 4]);
                ctx.lineWidth = 1.6;
                ctx.strokeStyle = s.ring;
                ctx.stroke();
                ctx.setLineDash([]);
            }
            ctx.beginPath();
            ctx.arc(c.x, c.y, r, 0, Math.PI * 2);
            ctx.fillStyle = s.fill;
            ctx.fill();
            ctx.lineWidth = s.borderWidth;
            ctx.strokeStyle = s.border;
            ctx.stroke();

            const text = String(key);
            const px = text.length >= 5 ? 11 : (text.length === 4 ? 12 : 14);
            ctx.font = this.font(600, px * s.scale, this.palette.uiFont);
            ctx.fillStyle = s.text;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(text, c.x, c.y + 0.5);
            ctx.restore();
        }

        drawNodes(ctx) {
            const P = this.palette;
            // Removed nodes shrink and fade out in red during the transition.
            if (this.progress < 1) {
                for (const old of this.fromSnapshot.nodes) {
                    if (this.snapshot.find(old.id))
                        continue;
                    const at = this.fromPositions.get(old.id);
                    if (!at)
                        continue;
                    this.drawNode(ctx, at, old.key, {
                        fill: P.errorFill, border: P.error, borderWidth: 2, text: P.textPrimary, halo: null, ring: null,
                        opacity: 1 - this.progress, scale: 1 - 0.3 * this.progress,
                    });
                }
            }

            // Plain nodes first, highlighted ones on top.
            const e = this.event;
            const highlighted = (id) => !!e && (id === e.nodeId || id === e.secondaryId);
            for (let pass = 0; pass < 2; ++pass) {
                for (const node of this.snapshot.nodes) {
                    if (highlighted(node.id) !== (pass === 1))
                        continue;
                    const s = this.styleFor(node);
                    if (this.isNewNode(node.id)) {
                        s.opacity = this.progress;
                        s.scale = 0.55 + 0.45 * this.progress;
                    }
                    const c = this.animatedPosition(node.id);
                    this.drawNode(ctx, c, node.key, s);
                    if (node.id === this.selected) {
                        ctx.beginPath();
                        ctx.arc(c.x, c.y, this.layout.metrics.nodeRadius + 11, 0, Math.PI * 2);
                        ctx.setLineDash([1.2, 2.4]);
                        ctx.lineWidth = 1.2;
                        ctx.strokeStyle = withAlpha(P.textPrimary, 0.55);
                        ctx.stroke();
                        ctx.setLineDash([]);
                    }
                }
            }
        }

        drawBadges(ctx) {
            const P = this.palette;
            const r = this.layout.metrics.nodeRadius;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';

            if (this.snapshot.root !== NO_NODE) {
                const root = this.animatedPosition(this.snapshot.root);
                ctx.font = this.font(600, 9, P.uiFont);
                if ('letterSpacing' in ctx)
                    ctx.letterSpacing = '1.2px';
                ctx.fillStyle = P.textMuted;
                ctx.fillText('ROOT', root.x, root.y - r - 15);
                if ('letterSpacing' in ctx)
                    ctx.letterSpacing = '0px';
            }
            const e = this.event;
            if (!e)
                return;

            // Order badges on nodes already output by a traversal.
            ctx.font = this.font(700, 9, P.uiFont);
            e.completed.forEach((id, i) => {
                if (!this.layout.contains(id))
                    return;
                const p = this.animatedPosition(id);
                const c = { x: p.x + r * 0.78, y: p.y - r * 0.78 };
                ctx.beginPath();
                ctx.arc(c.x, c.y, 8, 0, Math.PI * 2);
                ctx.fillStyle = P.success;
                ctx.fill();
                ctx.fillStyle = P.background;
                ctx.fillText(String(i + 1), c.x, c.y + 0.5);
            });

            // Role tag under the secondary node.
            if (e.secondaryRole !== SecondaryRole.None && this.layout.contains(e.secondaryId)) {
                const c = this.animatedPosition(e.secondaryId);
                ctx.font = this.font(500, 10, P.uiFont);
                ctx.fillStyle = e.secondaryRole === SecondaryRole.Target ? P.error : P.textSecondary;
                ctx.fillText(roleLabel(e.secondaryRole), c.x, c.y + r + 15);
            }

            // Comparison badge next to the node being compared.
            const comparison = comparisonText(e);
            if (comparison && this.layout.contains(e.nodeId)) {
                const c = this.animatedPosition(e.nodeId);
                ctx.font = this.font(600, 12, P.monoFont);
                const w = ctx.measureText(comparison).width + 14;
                const h = 22;
                const x = c.x + r + 6;
                const y = c.y - r - 16;
                const tone = e.comparedKey === e.key ? P.success : P.accent;
                ctx.beginPath();
                ctx.roundRect(x, y, w, h, 4);
                ctx.fillStyle = P.elevated;
                ctx.fill();
                ctx.lineWidth = 1;
                ctx.strokeStyle = withAlpha(tone, 0.55);
                ctx.stroke();
                ctx.fillStyle = tone;
                ctx.fillText(comparison, x + w / 2, y + h / 2 + 0.5);
            }
        }

        drawOverlays(ctx) {
            const P = this.palette;
            const area = { left: 16, top: 14, right: this.width - 16, bottom: this.height - 14 };
            ctx.textBaseline = 'middle';

            if (this.snapshot.empty() && this.progress >= 1) {
                ctx.textAlign = 'center';
                ctx.font = this.font(500, 17, P.uiFont);
                ctx.fillStyle = P.textSecondary;
                ctx.fillText('The tree is empty', this.width / 2, this.height / 2 - 12);
                ctx.font = this.font(400, 13, P.uiFont);
                ctx.fillStyle = P.textMuted;
                ctx.fillText('Insert a key, or open Overview to build or load the sample tree.', this.width / 2, this.height / 2 + 16);
            }
            const e = this.event;
            if (!e)
                return;

            if (e.tentative) {
                const text = 'WORKING COPY  ·  NOT YET COMMITTED';
                ctx.font = this.font(600, 10.5, P.uiFont);
                if ('letterSpacing' in ctx)
                    ctx.letterSpacing = '1px';
                const w = ctx.measureText(text).width + 22;
                ctx.beginPath();
                ctx.roundRect(area.left, area.top, w, 26, 4);
                ctx.fillStyle = withAlpha(P.accent, 0.08);
                ctx.fill();
                ctx.lineWidth = 1;
                ctx.strokeStyle = withAlpha(P.accent, 0.6);
                ctx.stroke();
                ctx.fillStyle = P.accent;
                ctx.textAlign = 'center';
                ctx.fillText(text, area.left + w / 2, area.top + 13.5);
                if ('letterSpacing' in ctx)
                    ctx.letterSpacing = '0px';
            }

            if (isTraversal(e.operation)) {
                const label = 'OUTPUT';
                ctx.font = this.font(600, 10.5, P.uiFont);
                const labelWidth = ctx.measureText(label).width;
                ctx.font = this.font(500, 13.5, P.monoFont);
                const maxWidth = (area.right - area.left) * 0.75;
                const keys = e.sequence.map(String);
                let values = keys.length ? keys.join('  →  ') : '—';
                // Elide from the left so the newest output stays visible.
                for (let skip = 1; ctx.measureText(values).width > maxWidth && skip < keys.length; ++skip)
                    values = `…  ${keys.slice(skip).join('  →  ')}`;
                const w = labelWidth + ctx.measureText(values).width + 40;
                const top = area.bottom - 34;
                ctx.beginPath();
                ctx.roundRect(area.left, top, w, 34, 5);
                ctx.fillStyle = P.surface;
                ctx.fill();
                ctx.lineWidth = 1;
                ctx.strokeStyle = P.border;
                ctx.stroke();
                ctx.textAlign = 'left';
                ctx.font = this.font(600, 10.5, P.uiFont);
                ctx.fillStyle = P.textMuted;
                ctx.fillText(label, area.left + 14, top + 17.5);
                ctx.font = this.font(500, 13.5, P.monoFont);
                ctx.fillStyle = P.success;
                ctx.fillText(values, area.left + 26 + labelWidth, top + 17.5);
            }

            // Colour legend, bottom-right, only when there is room.
            if (this.width >= 760) {
                const items = [[P.accent, 'Current / path'], [P.success, 'Found / output'], [P.error, 'Removing / missing']];
                ctx.font = this.font(400, 11.5, P.uiFont);
                ctx.textAlign = 'left';
                let x = area.right;
                for (const [color, text] of items.reverse()) {
                    x -= ctx.measureText(text).width;
                    ctx.fillStyle = P.textMuted;
                    ctx.fillText(text, x, area.bottom - 17);
                    x -= 14;
                    ctx.beginPath();
                    ctx.arc(x + 5, area.bottom - 17, 4, 0, Math.PI * 2);
                    ctx.fillStyle = color;
                    ctx.fill();
                    x -= 18;
                }
            }
        }
    }

    Object.assign(VDSA, { TreeLayout, TreeCanvas, LayoutMetrics });
})(typeof globalThis !== 'undefined' ? globalThis : this);
