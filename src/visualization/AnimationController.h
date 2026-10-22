#pragma once

#include "OperationResult.h"

#include <QObject>
#include <QTimer>

#include <memory>

namespace vdsa {

// Plays back a recorded operation one step at a time.
//
// The controller never runs algorithms or touches the tree: it only decides
// which recorded step is current. A single QTimer drives automatic playback,
// so there is never more than one pending advance, and loading a new record
// always cancels the previous playback.
class AnimationController : public QObject {
    Q_OBJECT

public:
    static constexpr int kBaseIntervalMs = 1000; // step duration at 1x

    explicit AnimationController(QObject* parent = nullptr);
    ~AnimationController() override;

    void load(std::shared_ptr<const OperationRecord> record, bool autoplay = true);
    void clear();

    bool hasRecord() const { return record_ != nullptr; }
    std::shared_ptr<const OperationRecord> record() const { return record_; }
    int currentStep() const { return index_; }
    int stepCount() const;
    bool isPlaying() const { return playing_; }
    bool atEnd() const { return stepCount() > 0 && index_ == stepCount() - 1; }
    double speed() const { return speed_; }
    int stepIntervalMs() const;

    const AlgorithmEvent* currentEvent() const;
    const TreeSnapshot* currentSnapshot() const;

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void next();
    void previous();
    void restart();
    void reset();
    void jumpTo(int step);
    void setSpeed(double speed);

signals:
    void recordChanged();
    void stepChanged(int step, int count);
    void playingChanged(bool playing);
    void speedChanged(double speed);
    void finished();

private:
    void advance();
    void setStep(int step);
    void setPlaying(bool playing);

    std::shared_ptr<const OperationRecord> record_;
    QTimer timer_;
    int index_ = 0;
    bool playing_ = false;
    double speed_ = 1.0;
};

} // namespace vdsa
