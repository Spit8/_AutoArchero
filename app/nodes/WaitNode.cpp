#include "WaitNode.h"
#include "FlowData.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

WaitNode::WaitNode()
    : m_displayName(QStringLiteral("Wait"))
{
}

WaitNode::~WaitNode() = default;

QJsonObject WaitNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("displayName")] = m_displayName;
    o[QStringLiteral("durationMs")] = m_durationMs;
    o[QStringLiteral("randomMs")] = m_randomMs;
    return o;
}

void WaitNode::load(QJsonObject const &p)
{
    if (p.contains(QStringLiteral("displayName")))
        m_displayName = p.value(QStringLiteral("displayName")).toString(QStringLiteral("Wait"));
    m_durationMs = p.value(QStringLiteral("durationMs")).toInt(1000);
    m_randomMs = p.value(QStringLiteral("randomMs")).toInt(0);
    syncWidgetFromMembers();
}

unsigned int WaitNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
    case QtNodes::PortType::Out:
        return 1;
    default:
        return 0;
    }
}

QtNodes::NodeDataType WaitNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return FlowData().type();
}

std::shared_ptr<QtNodes::NodeData> WaitNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

void WaitNode::ensureWidget()
{
    if (m_widget)
        return;

    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    form->setSpacing(2);
    form->setHorizontalSpacing(6);

    m_editName = new QLineEdit(m_widget);
    m_editName->setPlaceholderText(QStringLiteral("Nom"));
    m_editName->setMinimumWidth(100);

    m_spinDuration = new QSpinBox(m_widget);
    m_spinDuration->setRange(0, 600000);
    m_spinDuration->setSuffix(QStringLiteral(" ms"));
    m_spinRandom = new QSpinBox(m_widget);
    m_spinRandom->setRange(0, 60000);
    m_spinRandom->setSuffix(QStringLiteral(" ms"));
    m_spinRandom->setPrefix(QStringLiteral("± "));

    form->addRow(QStringLiteral("Nom"), m_editName);
    form->addRow(QStringLiteral("Durée"), m_spinDuration);
    form->addRow(QStringLiteral("Random"), m_spinRandom);

    connect(m_editName, &QLineEdit::editingFinished, this, &WaitNode::onWidgetEdited);
    connect(m_spinDuration, qOverload<int>(&QSpinBox::valueChanged), this, &WaitNode::onWidgetEdited);
    connect(m_spinRandom, qOverload<int>(&QSpinBox::valueChanged), this, &WaitNode::onWidgetEdited);

    syncWidgetFromMembers();
}

QWidget *WaitNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void WaitNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_editName->setText(m_displayName);
    m_spinDuration->setValue(m_durationMs);
    m_spinRandom->setValue(m_randomMs);
    m_updatingWidget = false;
}

void WaitNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;

    QString n = m_editName->text().trimmed();
    if (n.isEmpty())
        n = QStringLiteral("Wait");

    const bool changed = (n != m_displayName) || (m_spinDuration->value() != m_durationMs)
                         || (m_spinRandom->value() != m_randomMs);
    if (!changed)
        return;

    m_displayName = n;
    m_durationMs = m_spinDuration->value();
    m_randomMs = m_spinRandom->value();
    emit paramsChanged();
}

void WaitNode::setDisplayName(const QString &name)
{
    const QString n = name.trimmed().isEmpty() ? QStringLiteral("Wait") : name.trimmed();
    if (n == m_displayName)
        return;
    m_displayName = n;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void WaitNode::setDurationMs(int v)
{
    v = qMax(0, v);
    if (m_durationMs == v)
        return;
    m_durationMs = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void WaitNode::setRandomMs(int v)
{
    v = qMax(0, v);
    if (m_randomMs == v)
        return;
    m_randomMs = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}
