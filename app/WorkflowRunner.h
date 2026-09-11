#pragma once

#include "Types.h"
#include "WorkflowContext.h"

#include <QtNodes/Definitions>

#include <QObject>
#include <QTimer>

#include <functional>
#include <optional>
#include <vector>

class AdbClient;
class WhileNode;
class WorkflowEditor;

/// Executes workflow nodes (Tap/Wait/Capture/Check/If/Combat/While) with branching.
class WorkflowRunner : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Single, Loop };

    using CaptureDone = std::function<void(bool ok, const PipelineResult &result, const QString &error)>;
    using CaptureHandler = std::function<void(CaptureDone done)>;

    WorkflowRunner(WorkflowEditor *editor, AdbClient *adb, QObject *parent = nullptr);

    bool isRunning() const { return m_running; }
    Mode mode() const { return m_mode; }
    const WorkflowContext &context() const { return m_ctx; }

    void setCaptureHandler(CaptureHandler handler) { m_captureHandler = std::move(handler); }

public slots:
    void startSingle();
    void startLoop();
    void stop();

signals:
    void statusChanged(const QString &text);
    void runningChanged(bool running);

private:
    struct LoopFrame {
        QtNodes::NodeId whileId = 0;
        int iteration = 0;
        bool maxEnabled = true;
        int maxIterations = 100;
    };

    void start(Mode mode);
    void setRunning(bool running);
    void finish(const QString &status);
    void abort(const QString &status);
    void executeCurrent();
    void goNext(int outPort = 0);
    void restartLoop();
    void clearLoopStack();
    void scheduleWait(int ms);
    void runTapAsync(int x, int y, int durationMs);
    void runCaptureAsync();
    void runCombatAsync();
    void runCheck();
    void runIfBranch(bool condition, const QString &label);
    void runWhile(WhileNode *node, int inPort);

    std::optional<QtNodes::NodeId> findStartNode() const;
    std::optional<QtNodes::ConnectionId> successorConnection(QtNodes::NodeId from, int outPort) const;

    WorkflowEditor *m_editor = nullptr;
    AdbClient *m_adb = nullptr;
    CaptureHandler m_captureHandler;
    WorkflowContext m_ctx;

    QTimer m_waitTimer;
    std::optional<QtNodes::NodeId> m_current;
    std::optional<QtNodes::NodeId> m_startNode;
    int m_arriveInPort = 0;
    std::vector<LoopFrame> m_loopStack;
    int m_loopCount = 0;
    Mode m_mode = Mode::Single;
    bool m_running = false;
    int m_generation = 0;
};
