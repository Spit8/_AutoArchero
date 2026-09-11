#include "IfRoiRegexNode.h"
#include "FlowData.h"
#include "WorkflowData.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QWidget>

IfRoiRegexNode::IfRoiRegexNode() = default;
IfRoiRegexNode::~IfRoiRegexNode() = default;

QJsonObject IfRoiRegexNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("targetName")] = m_targetName;
    o[QStringLiteral("pattern")] = m_pattern;
    return o;
}

void IfRoiRegexNode::load(QJsonObject const &p)
{
    m_targetName = p.value(QStringLiteral("targetName")).toString();
    m_pattern = p.value(QStringLiteral("pattern")).toString();
    syncWidgetFromMembers();
}

unsigned int IfRoiRegexNode::nPorts(QtNodes::PortType portType) const
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

QtNodes::NodeDataType IfRoiRegexNode::dataType(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return RoiListData().type();
    return FlowData().type();
}

QString IfRoiRegexNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return QStringLiteral("rois");
    if (portType == QtNodes::PortType::Out)
        return index == 0 ? QStringLiteral("true") : QStringLiteral("false");
    return QStringLiteral("flow");
}

std::shared_ptr<QtNodes::NodeData> IfRoiRegexNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

QWidget *IfRoiRegexNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void IfRoiRegexNode::ensureWidget()
{
    if (m_widget)
        return;
    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    m_editName = new QLineEdit(m_widget);
    m_editName->setPlaceholderText(QStringLiteral("nom ROI"));
    m_editPattern = new QLineEdit(m_widget);
    m_editPattern->setPlaceholderText(QStringLiteral("regex"));
    m_editName->setMinimumWidth(100);
    m_editPattern->setMinimumWidth(100);
    form->addRow(QStringLiteral("Nom"), m_editName);
    form->addRow(QStringLiteral("Regex"), m_editPattern);
    connect(m_editName, &QLineEdit::editingFinished, this, &IfRoiRegexNode::onWidgetEdited);
    connect(m_editPattern, &QLineEdit::editingFinished, this, &IfRoiRegexNode::onWidgetEdited);
    syncWidgetFromMembers();
}

void IfRoiRegexNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_editName->setText(m_targetName);
    m_editPattern->setText(m_pattern);
    m_updatingWidget = false;
}

void IfRoiRegexNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;
    setTargetName(m_editName->text());
    setPattern(m_editPattern->text());
}

void IfRoiRegexNode::setTargetName(const QString &n)
{
    const QString t = n.trimmed();
    if (t == m_targetName)
        return;
    m_targetName = t;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void IfRoiRegexNode::setPattern(const QString &p)
{
    if (p == m_pattern)
        return;
    m_pattern = p;
    syncWidgetFromMembers();
    emit paramsChanged();
}
