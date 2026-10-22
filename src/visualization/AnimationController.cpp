#include "AnimationController.h"

#include <algorithm>

namespace vdsa {

AnimationController::AnimationController(QObject* parent) : QObject(parent)
{
    timer_.setInterval(stepIntervalMs());
    connect(&timer_, &QTimer::timeout, this, &AnimationController::advance);
}

AnimationController::~AnimationController() { timer_.stop(); }

int AnimationController::stepCount() const
{
    return record_ ? static_cast<int>(record_->events.size()) : 0;
}

int AnimationController::stepIntervalMs() const
{
    return std::max(1, static_cast<int>(kBaseIntervalMs / speed_));
}

const AlgorithmEvent* AnimationController::currentEvent() const
{
    if (!record_ || index_ < 0 || index_ >= stepCount())
        return nullptr;
    return &record_->events[static_cast<std::size_t>(index_)];
}

const TreeSnapshot* AnimationController::currentSnapshot() const
{
    const AlgorithmEvent* event = currentEvent();
    return event ? &record_->snapshotFor(*event) : nullptr;
}

void AnimationController::load(std::shared_ptr<const OperationRecord> record, bool autoplay)
{
    timer_.stop(); // cancel any previous playback first
    setPlaying(false);
    record_ = std::move(record);
    index_ = 0;
    emit recordChanged();
    emit stepChanged(index_, stepCount());
    if (autoplay && stepCount() > 1)
        play();
}

void AnimationController::clear()
{
    timer_.stop();
    setPlaying(false);
    record_.reset();
    index_ = 0;
    emit recordChanged();
    emit stepChanged(0, 0);
}

void AnimationController::play()
{
    if (stepCount() <= 1)
        return;
    if (atEnd())
        setStep(0); // playing a finished timeline starts it again
    timer_.start(stepIntervalMs());
    setPlaying(true);
}

void AnimationController::pause()
{
    timer_.stop();
    setPlaying(false);
}

void AnimationController::togglePlayPause()
{
    if (playing_)
        pause();
    else
        play();
}

// Manual stepping always pauses automatic playback so the two never compete.
void AnimationController::next()
{
    pause();
    setStep(index_ + 1);
}

void AnimationController::previous()
{
    pause();
    setStep(index_ - 1);
}

void AnimationController::restart()
{
    pause();
    setStep(0);
    play();
}

void AnimationController::reset()
{
    pause();
    setStep(0);
}

void AnimationController::jumpTo(int step)
{
    pause();
    setStep(step);
}

void AnimationController::setSpeed(double speed)
{
    speed = std::clamp(speed, 0.25, 8.0);
    if (qFuzzyCompare(speed, speed_))
        return;
    speed_ = speed;
    if (timer_.isActive())
        timer_.start(stepIntervalMs());
    else
        timer_.setInterval(stepIntervalMs());
    emit speedChanged(speed_);
}

void AnimationController::advance()
{
    if (index_ + 1 < stepCount())
        setStep(index_ + 1);
    if (atEnd() || stepCount() == 0) {
        pause();
        emit finished();
    }
}

void AnimationController::setStep(int step)
{
    if (stepCount() == 0)
        return;
    step = std::clamp(step, 0, stepCount() - 1);
    if (step == index_)
        return;
    index_ = step;
    emit stepChanged(index_, stepCount());
}

void AnimationController::setPlaying(bool playing)
{
    if (playing_ == playing)
        return;
    playing_ = playing;
    emit playingChanged(playing_);
}

} // namespace vdsa
