#include "CombatNode.h"
#include "FlowData.h"

#include <QComboBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QSpinBox>
#include <QWidget>

CombatNode::CombatNode() = default;
CombatNode::~CombatNode() = default;

QStringList CombatNode::directionChoices()
{
    return {QStringLiteral("N"),  QStringLiteral("S"),  QStringLiteral("W"),  QStringLiteral("E"),
            QStringLiteral("NW"), QStringLiteral("NE"), QStringLiteral("SE"), QStringLiteral("SW")};
}

QJsonObject CombatNode::save() const
{
    QJsonObject o = NodeDelegateModel::save();
    o[QStringLiteral("moveCount")] = m_moveCount;
    o[QStringLiteral("startX")] = m_startX;
    o[QStringLiteral("startY")] = m_startY;
    o[QStringLiteral("moveDurationMs")] = m_moveDurationMs;
    o[QStringLiteral("pauseMs")] = m_pauseMs;
    o[QStringLiteral("dir1")] = m_dir1;
    o[QStringLiteral("dir2")] = m_dir2;
    return o;
}

void CombatNode::load(QJsonObject const &p)
{
    m_moveCount = p.value(QStringLiteral("moveCount")).toInt(8);
    m_startX = p.value(QStringLiteral("startX")).toInt(528);
    m_startY = p.value(QStringLiteral("startY")).toInt(1522);
    m_moveDurationMs = p.value(QStringLiteral("moveDurationMs")).toInt(250);
    m_pauseMs = p.value(QStringLiteral("pauseMs")).toInt(200);
    m_dir1 = p.value(QStringLiteral("dir1")).toString(QStringLiteral("W"));
    m_dir2 = p.value(QStringLiteral("dir2")).toString(QStringLiteral("E"));
    syncWidgetFromMembers();
}

unsigned int CombatNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
    case QtNodes::PortType::Out:
        return 1;
    default:
        return 0;
    }
}

QtNodes::NodeDataType CombatNode::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return FlowData().type();
}

std::shared_ptr<QtNodes::NodeData> CombatNode::outData(QtNodes::PortIndex)
{
    return std::make_shared<FlowData>();
}

QWidget *CombatNode::embeddedWidget()
{
    ensureWidget();
    return m_widget;
}

void CombatNode::ensureWidget()
{
    if (m_widget)
        return;
    m_widget = new QWidget();
    auto *form = new QFormLayout(m_widget);
    form->setContentsMargins(4, 2, 4, 2);
    form->setSpacing(2);

    m_spinCount = new QSpinBox(m_widget);
    m_spinCount->setRange(1, 500);
    m_spinX = new QSpinBox(m_widget);
    m_spinX->setRange(0, 99999);
    m_spinY = new QSpinBox(m_widget);
    m_spinY->setRange(0, 99999);
    m_spinMoveMs = new QSpinBox(m_widget);
    m_spinMoveMs->setRange(10, 60000);
    m_spinMoveMs->setSuffix(QStringLiteral(" ms"));
    m_spinPauseMs = new QSpinBox(m_widget);
    m_spinPauseMs->setRange(0, 60000);
    m_spinPauseMs->setSuffix(QStringLiteral(" ms"));
    m_comboDir1 = new QComboBox(m_widget);
    m_comboDir1->addItems(directionChoices());
    m_comboDir2 = new QComboBox(m_widget);
    m_comboDir2->addItems(directionChoices());

    form->addRow(QStringLiteral("Mouvements"), m_spinCount);
    form->addRow(QStringLiteral("Start X"), m_spinX);
    form->addRow(QStringLiteral("Start Y"), m_spinY);
    form->addRow(QStringLiteral("Durée"), m_spinMoveMs);
    form->addRow(QStringLiteral("Pause"), m_spinPauseMs);
    form->addRow(QStringLiteral("Dir 1"), m_comboDir1);
    form->addRow(QStringLiteral("Dir 2"), m_comboDir2);

    connect(m_spinCount, qOverload<int>(&QSpinBox::valueChanged), this, &CombatNode::onWidgetEdited);
    connect(m_spinX, qOverload<int>(&QSpinBox::valueChanged), this, &CombatNode::onWidgetEdited);
    connect(m_spinY, qOverload<int>(&QSpinBox::valueChanged), this, &CombatNode::onWidgetEdited);
    connect(m_spinMoveMs, qOverload<int>(&QSpinBox::valueChanged), this, &CombatNode::onWidgetEdited);
    connect(m_spinPauseMs, qOverload<int>(&QSpinBox::valueChanged), this, &CombatNode::onWidgetEdited);
    connect(m_comboDir1, &QComboBox::currentTextChanged, this, [this](const QString &) { onWidgetEdited(); });
    connect(m_comboDir2, &QComboBox::currentTextChanged, this, [this](const QString &) { onWidgetEdited(); });

    syncWidgetFromMembers();
}

void CombatNode::syncWidgetFromMembers()
{
    if (!m_widget)
        return;
    m_updatingWidget = true;
    m_spinCount->setValue(m_moveCount);
    m_spinX->setValue(m_startX);
    m_spinY->setValue(m_startY);
    m_spinMoveMs->setValue(m_moveDurationMs);
    m_spinPauseMs->setValue(m_pauseMs);
    m_comboDir1->setCurrentText(m_dir1);
    m_comboDir2->setCurrentText(m_dir2);
    m_updatingWidget = false;
}

void CombatNode::onWidgetEdited()
{
    if (m_updatingWidget || !m_widget)
        return;
    m_moveCount = m_spinCount->value();
    m_startX = m_spinX->value();
    m_startY = m_spinY->value();
    m_moveDurationMs = m_spinMoveMs->value();
    m_pauseMs = m_spinPauseMs->value();
    m_dir1 = m_comboDir1->currentText();
    m_dir2 = m_comboDir2->currentText();
    emit paramsChanged();
}

void CombatNode::setMoveCount(int v)
{
    v = qMax(1, v);
    if (m_moveCount == v)
        return;
    m_moveCount = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setStartX(int v)
{
    if (m_startX == v)
        return;
    m_startX = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setStartY(int v)
{
    if (m_startY == v)
        return;
    m_startY = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setMoveDurationMs(int v)
{
    v = qMax(10, v);
    if (m_moveDurationMs == v)
        return;
    m_moveDurationMs = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setPauseMs(int v)
{
    v = qMax(0, v);
    if (m_pauseMs == v)
        return;
    m_pauseMs = v;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setDir1(const QString &d)
{
    if (d == m_dir1)
        return;
    m_dir1 = d;
    syncWidgetFromMembers();
    emit paramsChanged();
}

void CombatNode::setDir2(const QString &d)
{
    if (d == m_dir2)
        return;
    m_dir2 = d;
    syncWidgetFromMembers();
    emit paramsChanged();
}
