#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

class QLineEdit;
class QWidget;

class IfRoiRegexNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    IfRoiRegexNode();
    ~IfRoiRegexNode() override;

    static QString Name() { return QStringLiteral("IfRoiRegex"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("If ROI Regex"); }
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

    QString targetName() const { return m_targetName; }
    QString pattern() const { return m_pattern; }
    void setTargetName(const QString &n);
    void setPattern(const QString &p);

signals:
    void paramsChanged();

private:
    void ensureWidget();
    void syncWidgetFromMembers();
    void onWidgetEdited();

    QString m_targetName;
    QString m_pattern;
    QWidget *m_widget = nullptr;
    QLineEdit *m_editName = nullptr;
    QLineEdit *m_editPattern = nullptr;
    bool m_updatingWidget = false;
};
