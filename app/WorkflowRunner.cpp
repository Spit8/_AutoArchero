#include "WorkflowRunner.h"

#include "AdbClient.h"
#include "CaptureNode.h"
#include "CheckRoiTemplatesNode.h"
#include "CombatNode.h"
#include "IfRoiNode.h"
#include "IfRoiRegexNode.h"
#include "IfTemplateNode.h"
#include "TapNode.h"
#include "WaitNode.h"
#include "WhileNode.h"
#include "WorkflowEditor.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModel>

#include <QtConcurrent/QtConcurrent>

#include <QPoint>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <exception>

using QtNodes::ConnectionId;
using QtNodes::NodeId;
using QtNodes::PortType;

namespace {

QPoint directionDelta(const QString &dir, int dist = 120)
{
    const QString d = dir.toUpper();
    int dx = 0;
    int dy = 0;
    if (d.contains(QLatin1Char('N')))
        dy -= dist;
    if (d.contains(QLatin1Char('S')))
        dy += dist;
    if (d.contains(QLatin1Char('W')))
        dx -= dist;
    if (d.contains(QLatin1Char('E')))
        dx += dist;
    if (dx == 0 && dy == 0)
        dx = dist;
    return {dx, dy};
}

} // namespace

WorkflowRunner::WorkflowRunner(WorkflowEditor *editor, AdbClient *adb, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_adb(adb)
{
    m_waitTimer.setSingleShot(true);
    connect(&m_waitTimer, &QTimer::timeout, this, [this]() {
        if (!m_running)
            return;
        goNext(0);
    });
}

void WorkflowRunner::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged(m_running);
}

void WorkflowRunner::startSingle()
{
    start(Mode::Single);
}

void WorkflowRunner::startLoop()
{
    start(Mode::Loop);
}

void WorkflowRunner::start(Mode mode)
{
    if (m_running)
        return;
    if (!m_editor || !m_adb) {
        emit statusChanged(QStringLiteral("Runtime indisponible"));
        return;
    }

    m_startNode = findStartNode();
    m_current = m_startNode;
    m_arriveInPort = 0;
    m_loopCount = 1;
    m_mode = mode;
    m_ctx.clear();
    clearLoopStack();

    if (!m_current.has_value()) {
        emit statusChanged(QStringLiteral("Aucun nœud source (graphe vide)"));
        return;
    }

    ++m_generation;
    setRunning(true);
    if (m_mode == Mode::Loop)
        emit statusChanged(QStringLiteral("Loop démarré (tour #%1)…").arg(m_loopCount));
    else
        emit statusChanged(QStringLiteral("Single démarré…"));
    executeCurrent();
}

void WorkflowRunner::stop()
{
    if (!m_running && !m_waitTimer.isActive()) {
        if (m_editor)
            m_editor->setActiveNode(std::nullopt);
        return;
    }
    abort(QStringLiteral("Arrêté"));
}

void WorkflowRunner::finish(const QString &status)
{
    m_waitTimer.stop();
    ++m_generation;
    m_current = std::nullopt;
    clearLoopStack();
    if (m_editor)
        m_editor->setActiveNode(std::nullopt);
    setRunning(false);
    emit statusChanged(status);
}

void WorkflowRunner::abort(const QString &status)
{
    finish(status);
}

void WorkflowRunner::clearLoopStack()
{
    m_loopStack.clear();
}

void WorkflowRunner::restartLoop()
{
    m_startNode = findStartNode();
    m_current = m_startNode;
    m_arriveInPort = 0;
    m_ctx.clear();
    clearLoopStack();
    ++m_loopCount;
    if (!m_current.has_value()) {
        abort(QStringLiteral("Chemin vide — loop interrompu"));
        return;
    }
    emit statusChanged(QStringLiteral("Loop tour #%1…").arg(m_loopCount));
    executeCurrent();
}

std::optional<NodeId> WorkflowRunner::findStartNode() const
{
    if (!m_editor || !m_editor->graphModel())
        return std::nullopt;
    auto *model = m_editor->graphModel();
    const auto path = m_editor->executionPath();
    if (!path.empty())
        return path.front();

    // Fallback: first node with no incoming flow connection
    std::optional<NodeId> best;
    for (NodeId id : model->allNodeIds()) {
        bool hasIn = false;
        for (const ConnectionId &c : model->allConnectionIds(id)) {
            if (c.inNodeId == id)
                hasIn = true;
        }
        if (!hasIn && (!best.has_value() || id < *best))
            best = id;
    }
    return best;
}

std::optional<ConnectionId> WorkflowRunner::successorConnection(NodeId from, int outPort) const
{
    if (!m_editor || !m_editor->graphModel())
        return std::nullopt;
    auto *model = m_editor->graphModel();
    std::vector<ConnectionId> outs;
    for (const ConnectionId &c : model->allConnectionIds(from)) {
        if (c.outNodeId == from && static_cast<int>(c.outPortIndex) == outPort)
            outs.push_back(c);
    }
    if (outs.empty())
        return std::nullopt;
    std::sort(outs.begin(), outs.end(), [](const ConnectionId &a, const ConnectionId &b) {
        return a.inNodeId < b.inNodeId;
    });
    return outs.front();
}

void WorkflowRunner::goNext(int outPort)
{
    if (!m_running || !m_current.has_value())
        return;
    const auto next = successorConnection(*m_current, outPort);
    if (!next.has_value()) {
        if (m_mode == Mode::Loop) {
            restartLoop();
            return;
        }
        finish(QStringLiteral("Terminé"));
        return;
    }
    m_current = next->inNodeId;
    m_arriveInPort = static_cast<int>(next->inPortIndex);
    executeCurrent();
}

void WorkflowRunner::executeCurrent()
{
    if (!m_running || !m_editor || !m_current.has_value())
        return;

    const NodeId id = *m_current;
    auto *model = m_editor->graphModel();
    if (!model || !model->nodeExists(id)) {
        abort(QStringLiteral("Nœud introuvable pendant l'exécution"));
        return;
    }

    m_editor->setActiveNode(id);

    if (model->delegateModel<CaptureNode>(id)) {
        emit statusChanged(QStringLiteral("Runtime: Capture…"));
        runCaptureAsync();
        return;
    }

    if (model->delegateModel<CheckRoiTemplatesNode>(id)) {
        runCheck();
        return;
    }

    if (auto *ift = model->delegateModel<IfTemplateNode>(id)) {
        const QString want = ift->targetName();
        bool ok = false;
        for (const TemplateHitItem &h : m_ctx.templates) {
            if (h.name.compare(want, Qt::CaseInsensitive) == 0) {
                ok = true;
                break;
            }
        }
        runIfBranch(ok, QStringLiteral("IfTemplate \"%1\"").arg(want));
        return;
    }

    if (auto *ifr = model->delegateModel<IfRoiNode>(id)) {
        const QString want = ifr->targetName();
        bool ok = false;
        for (const RoiHitItem &h : m_ctx.rois) {
            if (h.name.compare(want, Qt::CaseInsensitive) == 0) {
                ok = true;
                break;
            }
        }
        runIfBranch(ok, QStringLiteral("IfRoi \"%1\"").arg(want));
        return;
    }

    if (auto *ifr = model->delegateModel<IfRoiRegexNode>(id)) {
        const QString want = ifr->targetName();
        const QRegularExpression re(ifr->pattern());
        bool ok = false;
        if (re.isValid()) {
            for (const RoiHitItem &h : m_ctx.rois) {
                if (h.name.compare(want, Qt::CaseInsensitive) != 0)
                    continue;
                if (re.match(h.value).hasMatch()) {
                    ok = true;
                    break;
                }
            }
        }
        runIfBranch(ok, QStringLiteral("IfRoiRegex \"%1\" ~ %2").arg(want, ifr->pattern()));
        return;
    }

    if (model->delegateModel<CombatNode>(id)) {
        emit statusChanged(QStringLiteral("Runtime: Combat…"));
        runCombatAsync();
        return;
    }

    if (auto *wh = model->delegateModel<WhileNode>(id)) {
        runWhile(wh, m_arriveInPort);
        return;
    }

    if (auto *tap = model->delegateModel<TapNode>(id)) {
        emit statusChanged(QStringLiteral("Runtime: Tap \"%1\" @%2,%3")
                               .arg(tap->displayName())
                               .arg(tap->x())
                               .arg(tap->y()));
        runTapAsync(tap->x(), tap->y(), tap->durationMs());
        return;
    }

    if (auto *wait = model->delegateModel<WaitNode>(id)) {
        int ms = wait->durationMs();
        const int rnd = wait->randomMs();
        if (rnd > 0) {
            const int delta = QRandomGenerator::global()->bounded(-rnd, rnd + 1);
            ms = qMax(0, ms + delta);
        }
        emit statusChanged(QStringLiteral("Runtime: Wait \"%1\" %2 ms")
                               .arg(wait->displayName())
                               .arg(ms));
        scheduleWait(ms);
        return;
    }

    const QString typeName = model->nodeData(id, QtNodes::NodeRole::Type).toString();
    emit statusChanged(QStringLiteral("Runtime: skip \"%1\"").arg(typeName));
    goNext(0);
}

void WorkflowRunner::runIfBranch(bool condition, const QString &label)
{
    emit statusChanged(QStringLiteral("Runtime: %1 → %2")
                           .arg(label, condition ? QStringLiteral("true") : QStringLiteral("false")));
    goNext(condition ? 0 : 1);
}

void WorkflowRunner::runWhile(WhileNode *node, int inPort)
{
    if (!node || !m_current.has_value())
        return;

    const NodeId id = *m_current;
    constexpr int kBreakPort = 1;

    if (inPort == kBreakPort) {
        while (!m_loopStack.empty() && m_loopStack.back().whileId != id)
            m_loopStack.pop_back();
        if (!m_loopStack.empty() && m_loopStack.back().whileId == id)
            m_loopStack.pop_back();
        emit statusChanged(QStringLiteral("Runtime: While break → done"));
        goNext(1);
        return;
    }

    if (m_loopStack.empty() || m_loopStack.back().whileId != id) {
        LoopFrame frame;
        frame.whileId = id;
        frame.iteration = 0;
        frame.maxEnabled = node->maxEnabled();
        frame.maxIterations = node->maxIterations();
        m_loopStack.push_back(frame);
        emit statusChanged(QStringLiteral("Runtime: While enter (iter 0) → body"));
        goNext(0);
        return;
    }

    LoopFrame &top = m_loopStack.back();
    ++top.iteration;
    top.maxEnabled = node->maxEnabled();
    top.maxIterations = node->maxIterations();

    if (top.maxEnabled && top.iteration >= top.maxIterations) {
        const int maxIt = top.maxIterations;
        m_loopStack.pop_back();
        emit statusChanged(QStringLiteral("Runtime: While max %1 atteint → done").arg(maxIt));
        goNext(1);
        return;
    }

    emit statusChanged(QStringLiteral("Runtime: While iter %1 → body").arg(top.iteration));
    goNext(0);
}

void WorkflowRunner::runCheck()
{
    if (!m_ctx.hasResult) {
        abort(QStringLiteral("Check: aucune Capture préalable"));
        return;
    }
    // Rebuild lists from lastResult (idempotent)
    m_ctx.setResult(m_ctx.lastResult);
    emit statusChanged(QStringLiteral("Runtime: Check → %1 ROI, %2 templates")
                           .arg(m_ctx.rois.size())
                           .arg(m_ctx.templates.size()));
    goNext(0);
}

void WorkflowRunner::scheduleWait(int ms)
{
    if (ms <= 0) {
        goNext(0);
        return;
    }
    m_waitTimer.start(ms);
}

void WorkflowRunner::runTapAsync(int x, int y, int durationMs)
{
    const int gen = m_generation;
    AdbClient *adb = m_adb;
    (void)QtConcurrent::run([this, gen, adb, x, y, durationMs]() {
        QString err;
        try {
            if (!adb) {
                err = QStringLiteral("ADB absent");
            } else if (durationMs > 0) {
                adb->swipe(x, y, x, y, durationMs);
            } else {
                adb->tap(x, y);
            }
        } catch (const std::exception &ex) {
            err = QString::fromUtf8(ex.what());
        } catch (...) {
            err = QStringLiteral("Erreur ADB inconnue");
        }

        QMetaObject::invokeMethod(
            this,
            [this, gen, err]() {
                if (gen != m_generation || !m_running)
                    return;
                if (!err.isEmpty()) {
                    abort(QStringLiteral("Erreur ADB: %1").arg(err));
                    return;
                }
                goNext(0);
            },
            Qt::QueuedConnection);
    });
}

void WorkflowRunner::runCaptureAsync()
{
    if (!m_captureHandler) {
        abort(QStringLiteral("Capture: handler absent"));
        return;
    }
    const int gen = m_generation;
    m_captureHandler([this, gen](bool ok, const PipelineResult &result, const QString &error) {
        QMetaObject::invokeMethod(
            this,
            [this, gen, ok, result, error]() {
                if (gen != m_generation || !m_running)
                    return;
                if (!ok) {
                    abort(QStringLiteral("Capture échouée: %1").arg(error));
                    return;
                }
                m_ctx.setResult(result);
                emit statusChanged(QStringLiteral("Runtime: Capture OK (%1 ROI, %2 tpl)")
                                       .arg(m_ctx.rois.size())
                                       .arg(m_ctx.templates.size()));
                goNext(0);
            },
            Qt::QueuedConnection);
    });
}

void WorkflowRunner::runCombatAsync()
{
    auto *model = m_editor->graphModel();
    auto *combat = model ? model->delegateModel<CombatNode>(*m_current) : nullptr;
    if (!combat) {
        abort(QStringLiteral("Combat introuvable"));
        return;
    }

    const int gen = m_generation;
    const int count = combat->moveCount();
    const int sx = combat->startX();
    const int sy = combat->startY();
    const int moveMs = combat->moveDurationMs();
    const int pauseMs = combat->pauseMs();
    const QString d1 = combat->dir1();
    const QString d2 = combat->dir2();
    AdbClient *adb = m_adb;

    (void)QtConcurrent::run([this, gen, adb, count, sx, sy, moveMs, pauseMs, d1, d2]() {
        QString err;
        try {
            if (!adb)
                throw std::runtime_error("ADB absent");
            const int half = count / 2;
            for (int i = 0; i < count; ++i) {
                if (gen != m_generation)
                    return;
                const QString dir = (i < half) ? d1 : d2;
                const QPoint delta = directionDelta(dir);
                adb->swipe(sx, sy, sx + delta.x(), sy + delta.y(), moveMs);
                if (pauseMs > 0)
                    QThread::msleep(static_cast<unsigned long>(pauseMs));
            }
        } catch (const std::exception &ex) {
            err = QString::fromUtf8(ex.what());
        } catch (...) {
            err = QStringLiteral("Erreur Combat");
        }

        QMetaObject::invokeMethod(
            this,
            [this, gen, err]() {
                if (gen != m_generation || !m_running)
                    return;
                if (!err.isEmpty()) {
                    abort(QStringLiteral("Erreur Combat: %1").arg(err));
                    return;
                }
                goNext(0);
            },
            Qt::QueuedConnection);
    });
}
