#pragma once

#include <QFrame>

class QComboBox;
class QLabel;
class QSlider;
class QToolButton;

namespace vdsa {

// Bottom bar: previous / play-pause / next / restart, speed, timeline.
// It only emits requests; the AnimationController owns playback state.
class PlaybackPanel : public QFrame {
    Q_OBJECT

public:
    explicit PlaybackPanel(QWidget* parent = nullptr);

    void setState(int step, int count, bool playing);
    void setSpeed(double speed);

signals:
    void previousRequested();
    void playPauseRequested();
    void nextRequested();
    void restartRequested();
    void seekRequested(int step);
    void speedRequested(double speed);

private:
    QToolButton* previous_;
    QToolButton* playPause_;
    QToolButton* next_;
    QToolButton* restart_;
    QComboBox* speed_;
    QSlider* timeline_;
    QLabel* stepLabel_;
    bool playing_ = false;
};

} // namespace vdsa
