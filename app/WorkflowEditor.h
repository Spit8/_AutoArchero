#pragma once

#include <QtNodes/Definitions>
#include <QtNodes/NodeStyle>

#include <QWidget>

#include <memory>
#include <optional>
#include <vector>

namespace QtNodes {
class DataFlowGraphModel;
class DataFlowGraphicsScene;
class GraphicsView;
class NodeDelegateModelRegistry;
}

class TapNode;
class WaitNode;

/// Hosts the QtNodes canvas for Gaming workflows.
class WorkflowEditor : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowEditor(QWidget *parent = nullptr);
    ~WorkflowEditor() override;

    QtNodes::DataFlowGraphModel *graphModel() const { return m_model.get(); }
    std::vector<QtNodes::NodeId> orderedNodeIds() const;
    /// Linear path from first source following out connections (port 0 / first link).
    std::vector<QtNodes::NodeId> executionPath() const;

    /// Green outline on the active runtime node; clears when nullopt.
    void setActiveNode(std::optional<QtNodes::NodeId> nodeId);

    bool exportToFile(const QString &path, QString *errorOut = nullptr);
    bool importFromFile(const QString &path, QString *errorOut = nullptr);

    std::optional<QtNodes::NodeId> selectedNodeId() const { return m_selectedNode; }
    QString selectedTypeName() const;

    TapNode *selectedTap() const;
    WaitNode *selectedWait() const;

    void setSelectedDisplayName(const QString &name);
    void setSelectedTapX(int v);
    void setSelectedTapY(int v);
    void setSelectedTapDurationMs(int v);
    void setSelectedWaitDurationMs(int v);
    void setSelectedWaitRandomMs(int v);

public slots:
    void addTapNode();
    void addWaitNode();
    void addCaptureNode();
    void addCheckNode();
    void addIfTemplateNode();
    void addIfRoiNode();
    void addIfRoiRegexNode();
    void addCombatNode();
    void addWhileNode();

signals:
    void selectionChanged(QtNodes::NodeId id, QString typeName);
    void selectionCleared();

private slots:
    void onSceneSelectionChanged();

private:
    void clearGraph();
    void applyDefaultStyle(QtNodes::NodeId nodeId);
    void applyActiveStyle(QtNodes::NodeId nodeId);
    void refreshNode(QtNodes::NodeId nodeId);
    QtNodes::NodeId addNodeAtCascade(const QString &typeName);
    void emitSelection();

    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> m_model;
    QtNodes::DataFlowGraphicsScene *m_scene = nullptr;
    QtNodes::GraphicsView *m_view = nullptr;

    std::optional<QtNodes::NodeId> m_activeNode;
    std::optional<QtNodes::NodeId> m_selectedNode;
    QtNodes::NodeStyle m_defaultStyle;
    int m_addCascade = 0;
};
