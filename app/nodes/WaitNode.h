#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class QLineEdit;
class QSpinBox;
class QWidget;

class WaitNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    WaitNode();
    ~WaitNode() override;

    static QString Name() { return QStringLiteral("Wait"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("Wait"); }
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

    int durationMs() const { return m_durationMs; }
    int randomMs() const { return m_randomMs; }
    void setDurationMs(int v);
    void setRandomMs(int v);

signals:
    void paramsChanged();

private:
    void ensureWidget();
    void syncWidgetFromMembers();
    void onWidgetEdited();

    QString m_displayName;
    int m_durationMs = 1000;
    int m_randomMs = 0;

    QWidget *m_widget = nullptr;
    QLineEdit *m_editName = nullptr;
    QSpinBox *m_spinDuration = nullptr;
    QSpinBox *m_spinRandom = nullptr;
    bool m_updatingWidget = false;
};
