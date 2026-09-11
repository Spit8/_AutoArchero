#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <memory>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QWidget;

class CombatNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    CombatNode();
    ~CombatNode() override;

    static QString Name() { return QStringLiteral("Combat"); }
    static QStringList directionChoices();

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("Combat"); }
    bool captionVisible() const override { return true; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    QWidget *embeddedWidget() override;

    int moveCount() const { return m_moveCount; }
    int startX() const { return m_startX; }
    int startY() const { return m_startY; }
    int moveDurationMs() const { return m_moveDurationMs; }
    int pauseMs() const { return m_pauseMs; }
    QString dir1() const { return m_dir1; }
    QString dir2() const { return m_dir2; }

    void setMoveCount(int v);
    void setStartX(int v);
    void setStartY(int v);
    void setMoveDurationMs(int v);
    void setPauseMs(int v);
    void setDir1(const QString &d);
    void setDir2(const QString &d);

signals:
    void paramsChanged();

private:
    void ensureWidget();
    void syncWidgetFromMembers();
    void onWidgetEdited();

    int m_moveCount = 8;
    int m_startX = 528;
    int m_startY = 1522;
    int m_moveDurationMs = 250;
    int m_pauseMs = 200;
    QString m_dir1 = QStringLiteral("W");
    QString m_dir2 = QStringLiteral("E");

    QWidget *m_widget = nullptr;
    QSpinBox *m_spinCount = nullptr;
    QSpinBox *m_spinX = nullptr;
    QSpinBox *m_spinY = nullptr;
    QSpinBox *m_spinMoveMs = nullptr;
    QSpinBox *m_spinPauseMs = nullptr;
    QComboBox *m_comboDir1 = nullptr;
    QComboBox *m_comboDir2 = nullptr;
    bool m_updatingWidget = false;
};
