#include "WhileNode.h"
#include "FlowData.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QSpinBox>
#include <QWidget>

WhileNode::WhileNode() = default;

WhileNode::~WhileNode() = default;

QJsonObject WhileNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("maxEnabled")] = m_maxEnabled;
    o[QStringLiteral("maxIterations")] = m_maxIterations;
    return o;
}

void WhileNode::load(QJsonObject const &p)
{
    m_maxEnabled = p.value(QStringLiteral("maxEnabled")).toBool(true);
    m_maxIterations = p.value(QStringLiteral("maxIterations")).toInt(100);
    if (m_maxIterations < 1)
        m_maxIterations = 1;
    syncWidgetFromMembers();
}

unsigned int WhileNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
    case QtNodes::PortType::Out:
        return 2;
    default:
        return 0;
    }
}

QtNodes::NodeDataType WhileNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return FlowData().type();
}

QString WhileNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In)
        return index == 0 ? QStringLiteral("enter") : QStringLiteral("break");
    return index == 0 ? QStringLiteral("body") : QStringLiteral("done");
}

std::shared_ptr<QtNodes::NodeData> WhileNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

void WhileNode::ensureWidget()
{
    if (m_widget)
        return;

    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    form->setSpacing(2);
    form->setHorizontalSpacing(6);

    m_checkMax = new QCheckBox(QStringLiteral("Limite d'itérations"), m_widget);
    m_spinMax = new QSpinBox(m_widget);
    m_spinMax->setRange(1, 1000000);
    m_spinMax->setValue(100);

    form->addRow(m_checkMax);
    form->addRow(QStringLiteral("Max"), m_spinMax);

    connect(m_checkMax, &QCheckBox::toggled, this, &WhileNode::onWidgetEdited);
    connect(m_spinMax, qOverload<int>(&QSpinBox::valueChanged), this, &WhileNode::onWidgetEdited);

    syncWidgetFromMembers();
}

QWidget *WhileNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void WhileNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_checkMax->setChecked(m_maxEnabled);
    m_spinMax->setValue(m_maxIterations);
    m_spinMax->setEnabled(m_maxEnabled);
    m_updatingWidget = false;
}

void WhileNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;

    const bool enabled = m_checkMax->isChecked();
    const int maxIt = m_spinMax->value();
    const bool changed = (enabled != m_maxEnabled) || (maxIt != m_maxIterations);
    if (!changed) {
        m_spinMax->setEnabled(m_maxEnabled);
        return;
    }

    m_maxEnabled = enabled;
    m_maxIterations = maxIt;
    m_spinMax->setEnabled(m_maxEnabled);
    emit paramsChanged();
}

void WhileNode::setMaxEnabled(bool v)
{
    if (m_maxEnabled == v)
        return;
    m_maxEnabled = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void WhileNode::setMaxIterations(int v)
{
    v = qMax(1, v);
    if (m_maxIterations == v)
        return;
    m_maxIterations = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}
