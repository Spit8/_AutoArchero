#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class QCheckBox;
class QSpinBox;
class QWidget;

class WhileNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    WhileNode();
    ~WhileNode() override;

    static QString Name() { return QStringLiteral("While"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("While"); }
    bool captionVisible() const override { return true; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    QString portCaption(QtNodes::PortType, QtNodes::PortIndex) const override;
    bool portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex) const override { return true; }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    QWidget *embeddedWidget() override;

    bool maxEnabled() const { return m_maxEnabled; }
    int maxIterations() const { return m_maxIterations; }
    void setMaxEnabled(bool v);
    void setMaxIterations(int v);

signals:
    void paramsChanged();

private:
    void ensureWidget();
    void syncWidgetFromMembers();
    void onWidgetEdited();

    bool m_maxEnabled = true;
    int m_maxIterations = 100;

    QWidget *m_widget = nullptr;
    QCheckBox *m_checkMax = nullptr;
    QSpinBox *m_spinMax = nullptr;
    bool m_updatingWidget = false;
};
