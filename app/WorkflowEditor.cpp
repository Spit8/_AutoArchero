#include "WorkflowEditor.h"
#include "CaptureNode.h"
#include "CheckRoiTemplatesNode.h"
#include "CombatNode.h"
#include "IfRoiNode.h"
#include "IfRoiRegexNode.h"
#include "IfTemplateNode.h"
#include "PlaceholderNode.h"
#include "TapNode.h"
#include "WaitNode.h"
#include "WhileNode.h"

#include <QtNodes/ConnectionIdUtils>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QFile>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QToolButton>
#include <QVBoxLayout>

#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using QtNodes::ConnectionId;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::DataFlowGraphModel;
using QtNodes::GraphicsView;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeDelegateModelRegistry;
using QtNodes::NodeId;
using QtNodes::NodeStyle;

namespace {

QToolButton *makeToolButton(QWidget *parent, const QString &text, const QString &tip,
                            const QString &iconPath = {})
{
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    if (!iconPath.isEmpty())
        btn->setIcon(QIcon(iconPath));
    btn->setToolButtonStyle(iconPath.isEmpty() ? Qt::ToolButtonTextOnly
                                               : Qt::ToolButtonTextBesideIcon);
    btn->setToolTip(tip);
    return btn;
}

} // namespace

WorkflowEditor::WorkflowEditor(QWidget *parent)
    : QWidget(parent)
{
    m_defaultStyle = NodeStyle();

    m_registry = std::make_shared<NodeDelegateModelRegistry>();
    m_registry->registerModel<TapNode>(QStringLiteral("Actions"));
    m_registry->registerModel<WaitNode>(QStringLiteral("Actions"));
    m_registry->registerModel<CombatNode>(QStringLiteral("Actions"));
    m_registry->registerModel<CaptureNode>(QStringLiteral("Vision"));
    m_registry->registerModel<CheckRoiTemplatesNode>(QStringLiteral("Vision"));
    m_registry->registerModel<IfTemplateNode>(QStringLiteral("Logic"));
    m_registry->registerModel<IfRoiNode>(QStringLiteral("Logic"));
    m_registry->registerModel<IfRoiRegexNode>(QStringLiteral("Logic"));
    m_registry->registerModel<WhileNode>(QStringLiteral("Logic"));
    m_registry->registerModel<PlaceholderNode>(QStringLiteral("Demo"));

    m_model = std::make_unique<DataFlowGraphModel>(m_registry);
    m_scene = new DataFlowGraphicsScene(*m_model, this);
    m_view = new GraphicsView(m_scene, this);
    m_view->setWindowTitle(QStringLiteral("Workflow"));

    auto *btnTap = makeToolButton(this, QStringLiteral("Tap"), QStringLiteral("Ajouter Tap"),
                                  QStringLiteral(":/icons/tap.svg"));
    auto *btnWait = makeToolButton(this, QStringLiteral("Wait"), QStringLiteral("Ajouter Wait"),
                                   QStringLiteral(":/icons/wait.svg"));
    auto *btnCapture = makeToolButton(this, QStringLiteral("Capture"), QStringLiteral("Capture écran"));
    auto *btnCheck = makeToolButton(this, QStringLiteral("Check"), QStringLiteral("Check ROI/Templates"));
    auto *btnIfTpl = makeToolButton(this, QStringLiteral("IfTpl"), QStringLiteral("If Template"));
    auto *btnIfRoi = makeToolButton(this, QStringLiteral("IfRoi"), QStringLiteral("If ROI"));
    auto *btnIfRx = makeToolButton(this, QStringLiteral("IfRx"), QStringLiteral("If ROI Regex"));
    auto *btnWhile = makeToolButton(this, QStringLiteral("While"), QStringLiteral("Boucle locale (enter/break)"));
    auto *btnCombat = makeToolButton(this, QStringLiteral("Combat"), QStringLiteral("Combat moves"));

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(4, 4, 4, 0);
    toolbar->addWidget(btnCapture);
    toolbar->addWidget(btnCheck);
    toolbar->addWidget(btnIfTpl);
    toolbar->addWidget(btnIfRoi);
    toolbar->addWidget(btnIfRx);
    toolbar->addWidget(btnWhile);
    toolbar->addWidget(btnTap);
    toolbar->addWidget(btnWait);
    toolbar->addWidget(btnCombat);
    toolbar->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(toolbar);
    layout->addWidget(m_view, 1);

    connect(btnTap, &QToolButton::clicked, this, &WorkflowEditor::addTapNode);
    connect(btnWait, &QToolButton::clicked, this, &WorkflowEditor::addWaitNode);
    connect(btnCapture, &QToolButton::clicked, this, &WorkflowEditor::addCaptureNode);
    connect(btnCheck, &QToolButton::clicked, this, &WorkflowEditor::addCheckNode);
    connect(btnIfTpl, &QToolButton::clicked, this, &WorkflowEditor::addIfTemplateNode);
    connect(btnIfRoi, &QToolButton::clicked, this, &WorkflowEditor::addIfRoiNode);
    connect(btnIfRx, &QToolButton::clicked, this, &WorkflowEditor::addIfRoiRegexNode);
    connect(btnWhile, &QToolButton::clicked, this, &WorkflowEditor::addWhileNode);
    connect(btnCombat, &QToolButton::clicked, this, &WorkflowEditor::addCombatNode);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &WorkflowEditor::onSceneSelectionChanged);
}

WorkflowEditor::~WorkflowEditor() = default;

std::vector<NodeId> WorkflowEditor::orderedNodeIds() const
{
    std::vector<NodeId> ids;
    if (!m_model)
        return ids;
    const auto set = m_model->allNodeIds();
    ids.assign(set.begin(), set.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<NodeId> WorkflowEditor::executionPath() const
{
    std::vector<NodeId> path;
    if (!m_model)
        return path;

    const auto all = m_model->allNodeIds();
    if (all.empty())
        return path;

    std::unordered_map<NodeId, int> inDegree;
    std::unordered_map<NodeId, std::vector<NodeId>> successors;
    for (NodeId id : all) {
        inDegree[id] = 0;
        successors[id] = {};
    }

    for (NodeId id : all) {
        std::vector<ConnectionId> outs;
        for (const ConnectionId &cid : m_model->allConnectionIds(id)) {
            if (cid.outNodeId == id)
                outs.push_back(cid);
        }
        std::sort(outs.begin(), outs.end(), [](const ConnectionId &a, const ConnectionId &b) {
            if (a.outPortIndex != b.outPortIndex)
                return a.outPortIndex < b.outPortIndex;
            return a.inNodeId < b.inNodeId;
        });
        for (const ConnectionId &cid : outs) {
            successors[id].push_back(cid.inNodeId);
            ++inDegree[cid.inNodeId];
        }
    }

    std::vector<NodeId> sources;
    for (NodeId id : all) {
        if (inDegree[id] == 0)
            sources.push_back(id);
    }
    std::sort(sources.begin(), sources.end());
    if (sources.empty())
        return path; // cycle covering every node

    NodeId cur = sources.front();
    std::unordered_set<NodeId> visited;
    while (true) {
        if (visited.count(cur))
            break; // cycle
        visited.insert(cur);
        path.push_back(cur);
        const auto &nexts = successors[cur];
        if (nexts.empty())
            break;
        cur = nexts.front();
    }
    return path;
}

void WorkflowEditor::clearGraph()
{
    setActiveNode(std::nullopt);
    m_selectedNode = std::nullopt;
    if (!m_model)
        return;
    const auto ids = m_model->allNodeIds();
    for (NodeId id : ids)
        m_model->deleteNode(id);
    Q_EMIT selectionCleared();
}

bool WorkflowEditor::exportToFile(const QString &path, QString *errorOut)
{
    if (!m_model) {
        if (errorOut)
            *errorOut = QStringLiteral("Modèle workflow absent");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Impossible d'écrire: %1").arg(path);
        return false;
    }
    const QJsonObject sceneJson = m_model->save();
    file.write(QJsonDocument(sceneJson).toJson(QJsonDocument::Indented));
    return true;
}

bool WorkflowEditor::importFromFile(const QString &path, QString *errorOut)
{
    if (!m_model) {
        if (errorOut)
            *errorOut = QStringLiteral("Modèle workflow absent");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Impossible de lire: %1").arg(path);
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("JSON invalide: %1").arg(parseError.errorString());
        return false;
    }
    clearGraph();
    m_model->load(doc.object());
    setActiveNode(std::nullopt);
    m_addCascade = 0;
    if (m_view)
        m_view->centerScene();
    onSceneSelectionChanged();
    return true;
}

void WorkflowEditor::applyDefaultStyle(NodeId nodeId)
{
    auto *delegate = m_model->delegateModel<NodeDelegateModel>(nodeId);
    if (!delegate)
        return;
    delegate->setNodeStyle(m_defaultStyle);
    refreshNode(nodeId);
}

void WorkflowEditor::applyActiveStyle(NodeId nodeId)
{
    auto *delegate = m_model->delegateModel<NodeDelegateModel>(nodeId);
    if (!delegate)
        return;
    NodeStyle style = m_defaultStyle;
    style.NormalBoundaryColor = QColor(40, 200, 70);
    style.SelectedBoundaryColor = QColor(40, 220, 80);
    style.PenWidth = 3.5f;
    style.HoveredPenWidth = 4.5f;
    delegate->setNodeStyle(style);
    refreshNode(nodeId);
}

void WorkflowEditor::refreshNode(NodeId nodeId)
{
    Q_EMIT m_model->nodeUpdated(nodeId);
}

void WorkflowEditor::setActiveNode(std::optional<NodeId> nodeId)
{
    if (m_activeNode.has_value() && m_model->nodeExists(*m_activeNode))
        applyDefaultStyle(*m_activeNode);

    m_activeNode = nodeId;

    if (m_activeNode.has_value() && m_model->nodeExists(*m_activeNode))
        applyActiveStyle(*m_activeNode);
}

NodeId WorkflowEditor::addNodeAtCascade(const QString &typeName)
{
    const NodeId id = m_model->addNode(typeName);
    const qreal offset = 40.0 * m_addCascade;
    m_model->setNodeData(id, QtNodes::NodeRole::Position, QPointF(120 + offset, 100 + offset));
    ++m_addCascade;
    applyDefaultStyle(id);

    if (m_scene) {
        m_scene->clearSelection();
        if (auto *ngo = m_scene->nodeGraphicsObject(id))
            ngo->setSelected(true);
    }
    onSceneSelectionChanged();
    return id;
}

void WorkflowEditor::addTapNode()
{
    addNodeAtCascade(TapNode::Name());
}

void WorkflowEditor::addWaitNode()
{
    addNodeAtCascade(WaitNode::Name());
}

void WorkflowEditor::addCaptureNode()
{
    addNodeAtCascade(CaptureNode::Name());
}

void WorkflowEditor::addCheckNode()
{
    addNodeAtCascade(CheckRoiTemplatesNode::Name());
}

void WorkflowEditor::addIfTemplateNode()
{
    addNodeAtCascade(IfTemplateNode::Name());
}

void WorkflowEditor::addIfRoiNode()
{
    addNodeAtCascade(IfRoiNode::Name());
}

void WorkflowEditor::addIfRoiRegexNode()
{
    addNodeAtCascade(IfRoiRegexNode::Name());
}

void WorkflowEditor::addCombatNode()
{
    addNodeAtCascade(CombatNode::Name());
}

void WorkflowEditor::addWhileNode()
{
    addNodeAtCascade(WhileNode::Name());
}

void WorkflowEditor::onSceneSelectionChanged()
{
    std::optional<NodeId> next;
    if (m_scene) {
        const auto nodes = m_scene->selectedNodes();
        if (nodes.size() == 1)
            next = nodes.front();
    }

    if (m_selectedNode == next)
        return;
    m_selectedNode = next;
    emitSelection();
}

void WorkflowEditor::emitSelection()
{
    if (!m_selectedNode.has_value() || !m_model->nodeExists(*m_selectedNode)) {
        m_selectedNode = std::nullopt;
        Q_EMIT selectionCleared();
        return;
    }
    const QString typeName = m_model->nodeData(*m_selectedNode, QtNodes::NodeRole::Type).toString();
    Q_EMIT selectionChanged(*m_selectedNode, typeName);
}

QString WorkflowEditor::selectedTypeName() const
{
    if (!m_selectedNode.has_value() || !m_model)
        return {};
    return m_model->nodeData(*m_selectedNode, QtNodes::NodeRole::Type).toString();
}

TapNode *WorkflowEditor::selectedTap() const
{
    if (!m_selectedNode.has_value() || !m_model)
        return nullptr;
    return m_model->delegateModel<TapNode>(*m_selectedNode);
}

WaitNode *WorkflowEditor::selectedWait() const
{
    if (!m_selectedNode.has_value() || !m_model)
        return nullptr;
    return m_model->delegateModel<WaitNode>(*m_selectedNode);
}

void WorkflowEditor::setSelectedDisplayName(const QString &name)
{
    if (auto *tap = selectedTap()) {
        tap->setDisplayName(name);
        return;
    }
    if (auto *wait = selectedWait())
        wait->setDisplayName(name);
}

void WorkflowEditor::setSelectedTapX(int v)
{
    if (auto *tap = selectedTap())
        tap->setX(v);
}

void WorkflowEditor::setSelectedTapY(int v)
{
    if (auto *tap = selectedTap())
        tap->setY(v);
}

void WorkflowEditor::setSelectedTapDurationMs(int v)
{
    if (auto *tap = selectedTap())
        tap->setDurationMs(v);
}

void WorkflowEditor::setSelectedWaitDurationMs(int v)
{
    if (auto *wait = selectedWait())
        wait->setDurationMs(v);
}

void WorkflowEditor::setSelectedWaitRandomMs(int v)
{
    if (auto *wait = selectedWait())
        wait->setRandomMs(v);
}
