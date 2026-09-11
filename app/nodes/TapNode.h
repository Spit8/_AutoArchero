#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class QLineEdit;
class QSpinBox;
class QWidget;

class TapNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    TapNode();
    ~TapNode() override;

    static QString Name() { return QStringLiteral("Tap"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("Tap"); }
    bool labelVisible() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    QWidget *embeddedWidget() override;

    QString displayName() const { return m_displayName; }
    void setDisplayName(const QString &name);

    int x() const { return m_x; }
    int y() const { return m_y; }
    int durationMs() const { return m_durationMs; }
    void setX(int v);
    void setY(int v);
    void setDurationMs(int v);

signals:
    void paramsChanged();

private:
    void ensureWidget();
    void syncWidgetFromMembers();
    void onWidgetEdited();

    QString m_displayName;
    int m_x = 0;
    int m_y = 0;
    int m_durationMs = 50;

    QWidget *m_widget = nullptr;
    QLineEdit *m_editName = nullptr;
    QSpinBox *m_spinX = nullptr;
    QSpinBox *m_spinY = nullptr;
    QSpinBox *m_spinDuration = nullptr;
    bool m_updatingWidget = false;
};
