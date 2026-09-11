#include "IfTemplateNode.h"
#include "FlowData.h"
#include "WorkflowData.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QWidget>

IfTemplateNode::IfTemplateNode()
    : m_targetName(QStringLiteral(""))
{
}

IfTemplateNode::~IfTemplateNode() = default;

QJsonObject IfTemplateNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("targetName")] = m_targetName;
    return o;
}

void IfTemplateNode::load(QJsonObject const &p)
{
    m_targetName = p.value(QStringLiteral("targetName")).toString();
    syncWidgetFromMembers();
}

unsigned int IfTemplateNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return 2; // flow, tpl_list
    case QtNodes::PortType::Out:
        return 2; // true, false
    default:
        return 0;
    }
}

QtNodes::NodeDataType IfTemplateNode::dataType(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return TemplateListData().type();
    return FlowData().type();
}

QString IfTemplateNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return QStringLiteral("tpls");
    if (portType == QtNodes::PortType::Out)
        return index == 0 ? QStringLiteral("true") : QStringLiteral("false");
    return QStringLiteral("flow");
}

std::shared_ptr<QtNodes::NodeData> IfTemplateNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

QWidget *IfTemplateNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void IfTemplateNode::ensureWidget()
{
    if (m_widget)
        return;
    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    m_editName = new QLineEdit(m_widget);
    m_editName->setPlaceholderText(QStringLiteral("nom template"));
    m_editName->setMinimumWidth(100);
    form->addRow(QStringLiteral("Nom"), m_editName);
    connect(m_editName, &QLineEdit::editingFinished, this, &IfTemplateNode::onWidgetEdited);
    syncWidgetFromMembers();
}

void IfTemplateNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_editName->setText(m_targetName);
    m_updatingWidget = false;
}

void IfTemplateNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;
    setTargetName(m_editName->text());
}

void IfTemplateNode::setTargetName(const QString &n)
{
    const QString t = n.trimmed();
    if (t == m_targetName)
        return;
    m_targetName = t;
    syncWidgetFromMembers();
    emit paramsChanged();
}
