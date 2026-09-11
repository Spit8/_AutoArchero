#include "IfRoiNode.h"
#include "FlowData.h"
#include "WorkflowData.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QWidget>

IfRoiNode::IfRoiNode() = default;
IfRoiNode::~IfRoiNode() = default;

QJsonObject IfRoiNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("targetName")] = m_targetName;
    return o;
}

void IfRoiNode::load(QJsonObject const &p)
{
    m_targetName = p.value(QStringLiteral("targetName")).toString();
    syncWidgetFromMembers();
}

unsigned int IfRoiNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return 2;
    case QtNodes::PortType::Out:
        return 2;
    default:
        return 0;
    }
}

QtNodes::NodeDataType IfRoiNode::dataType(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return RoiListData().type();
    return FlowData().type();
}

QString IfRoiNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return QStringLiteral("rois");
    if (portType == QtNodes::PortType::Out)
        return index == 0 ? QStringLiteral("true") : QStringLiteral("false");
    return QStringLiteral("flow");
}

std::shared_ptr<QtNodes::NodeData> IfRoiNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

QWidget *IfRoiNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void IfRoiNode::ensureWidget()
{
    if (m_widget)
        return;
    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    m_editName = new QLineEdit(m_widget);
    m_editName->setPlaceholderText(QStringLiteral("nom ROI"));
    m_editName->setMinimumWidth(100);
    form->addRow(QStringLiteral("Nom"), m_editName);
    connect(m_editName, &QLineEdit::editingFinished, this, &IfRoiNode::onWidgetEdited);
    syncWidgetFromMembers();
}

void IfRoiNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_editName->setText(m_targetName);
    m_updatingWidget = false;
}

void IfRoiNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;
    setTargetName(m_editName->text());
}

void IfRoiNode::setTargetName(const QString &n)
{
    const QString t = n.trimmed();
    if (t == m_targetName)
        return;
    m_targetName = t;
    syncWidgetFromMembers();
    emit paramsChanged();
}
