#include "TapNode.h"
#include "FlowData.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

TapNode::TapNode()
    : m_displayName(QStringLiteral("Tap"))
{
}

TapNode::~TapNode() = default;

QJsonObject TapNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("displayName")] = m_displayName;
    o[QStringLiteral("x")] = m_x;
    o[QStringLiteral("y")] = m_y;
    o[QStringLiteral("durationMs")] = m_durationMs;
    return o;
}

void TapNode::load(QJsonObject const &p)
{
    if (p.contains(QStringLiteral("displayName")))
        m_displayName = p.value(QStringLiteral("displayName")).toString(QStringLiteral("Tap"));
    m_x = p.value(QStringLiteral("x")).toInt(0);
    m_y = p.value(QStringLiteral("y")).toInt(0);
    m_durationMs = p.value(QStringLiteral("durationMs")).toInt(50);
    syncWidgetFromMembers();
}

unsigned int TapNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
    case QtNodes::PortType::Out:
        return 1;
    default:
        return 0;
    }
}

QtNodes::NodeDataType TapNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return FlowData().type();
}

std::shared_ptr<QtNodes::NodeData> TapNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

void TapNode::ensureWidget()
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

    m_spinX = new QSpinBox(m_widget);
    m_spinX->setRange(0, 99999);
    m_spinY = new QSpinBox(m_widget);
    m_spinY->setRange(0, 99999);
    m_spinDuration = new QSpinBox(m_widget);
    m_spinDuration->setRange(0, 60000);
    m_spinDuration->setSuffix(QStringLiteral(" ms"));

    form->addRow(QStringLiteral("Nom"), m_editName);
    form->addRow(QStringLiteral("X"), m_spinX);
    form->addRow(QStringLiteral("Y"), m_spinY);
    form->addRow(QStringLiteral("Durée"), m_spinDuration);

    connect(m_editName, &QLineEdit::editingFinished, this, &TapNode::onWidgetEdited);
    connect(m_spinX, qOverload<int>(&QSpinBox::valueChanged), this, &TapNode::onWidgetEdited);
    connect(m_spinY, qOverload<int>(&QSpinBox::valueChanged), this, &TapNode::onWidgetEdited);
    connect(m_spinDuration, qOverload<int>(&QSpinBox::valueChanged), this, &TapNode::onWidgetEdited);

    syncWidgetFromMembers();
}

QWidget *TapNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void TapNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_editName->setText(m_displayName);
    m_spinX->setValue(m_x);
    m_spinY->setValue(m_y);
    m_spinDuration->setValue(m_durationMs);
    m_updatingWidget = false;
}

void TapNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;

    QString n = m_editName->text().trimmed();
    if (n.isEmpty())
        n = QStringLiteral("Tap");

    const bool changed = (n != m_displayName) || (m_spinX->value() != m_x)
                         || (m_spinY->value() != m_y)
                         || (m_spinDuration->value() != m_durationMs);
    if (!changed)
        return;

    m_displayName = n;
    m_x = m_spinX->value();
    m_y = m_spinY->value();
    m_durationMs = m_spinDuration->value();
    emit paramsChanged();
}

void TapNode::setDisplayName(const QString &name)
{
    const QString n = name.trimmed().isEmpty() ? QStringLiteral("Tap") : name.trimmed();
    if (n == m_displayName)
        return;
    m_displayName = n;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void TapNode::setX(int v)
{
    if (m_x == v)
        return;
    m_x = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void TapNode::setY(int v)
{
    if (m_y == v)
        return;
    m_y = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void TapNode::setDurationMs(int v)
{
    v = qMax(0, v);
    if (m_durationMs == v)
        return;
    m_durationMs = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}
