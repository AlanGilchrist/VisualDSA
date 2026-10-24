// VisualDSA web — playback of a recorded operation
// (port of src/visualization/AnimationController.cpp).
(function (global) {
    'use strict';

    const VDSA = global.VDSA;
    const BASE_INTERVAL_MS = 1000; // step duration at 1x

    class AnimationController {
        constructor({ onRecordChanged = () => {}, onStepChanged = () => {}, onPlayingChanged = () => {}, onSpeedChanged = () => {} } = {}) {
            this.onRecordChanged = onRecordChanged;
            this.onStepChanged = onStepChanged;
            this.onPlayingChanged = onPlayingChanged;
            this.onSpeedChanged = onSpeedChanged;
            this.record = null;
            this.index = 0;
            this.speed = 1;
            this.playing = false;
            this.timer = 0;
        }

        get stepCount() { return this.record ? this.record.events.length : 0; }
        get currentStep() { return this.index; }
        hasRecord() { return !!this.record; }
        atEnd() { return this.index >= this.stepCount - 1; }
        stepIntervalMs() { return Math.max(1, Math.floor(BASE_INTERVAL_MS / this.speed)); }

        currentEvent() {
            if (!this.record || this.index < 0 || this.index >= this.stepCount)
                return null;
            return this.record.events[this.index];
        }

        currentSnapshot() {
            const event = this.currentEvent();
            return event ? this.record.snapshots[event.snapshotIndex] : null;
        }

        load(record, autoplay) {
            this.stopTimer(); // cancel any previous playback first
            this.setPlaying(false);
            this.record = record;
            this.index = 0;
            this.onRecordChanged();
            this.onStepChanged(this.index, this.stepCount);
            if (autoplay && this.stepCount > 1)
                this.play();
        }

        play() {
            if (this.stepCount <= 1)
                return;
            if (this.atEnd())
                this.setStep(0); // playing a finished timeline starts it again
            this.startTimer();
            this.setPlaying(true);
        }

        pause() {
            this.stopTimer();
            this.setPlaying(false);
        }

        togglePlayPause() {
            if (this.playing)
                this.pause();
            else
                this.play();
        }

        // Manual stepping always pauses automatic playback so the two never compete.
        next() { this.pause(); this.setStep(this.index + 1); }
        previous() { this.pause(); this.setStep(this.index - 1); }
        jumpTo(step) { this.pause(); this.setStep(step); }

        restart() {
            this.pause();
            this.setStep(0);
            this.play();
        }

        setSpeed(speed) {
            speed = Math.max(0.25, Math.min(8, speed));
            if (speed === this.speed)
                return;
            this.speed = speed;
            if (this.timer)
                this.startTimer();
            this.onSpeedChanged(speed);
        }

        advance() {
            if (this.index + 1 < this.stepCount)
                this.setStep(this.index + 1);
            if (this.atEnd() || this.stepCount === 0)
                this.pause();
        }

        setStep(step) {
            if (this.stepCount === 0)
                return;
            step = Math.max(0, Math.min(this.stepCount - 1, step));
            if (step === this.index)
                return;
            this.index = step;
            this.onStepChanged(this.index, this.stepCount);
        }

        startTimer() {
            this.stopTimer();
            this.timer = setInterval(() => this.advance(), this.stepIntervalMs());
        }

        stopTimer() {
            clearInterval(this.timer);
            this.timer = 0;
        }

        setPlaying(playing) {
            if (this.playing === playing)
                return;
            this.playing = playing;
            this.onPlayingChanged(playing);
        }
    }

    Object.assign(VDSA, { AnimationController });
})(typeof globalThis !== 'undefined' ? globalThis : this);
