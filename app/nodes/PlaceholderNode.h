#pragma once

#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>

#include <memory>

/// Temporary scaffold node for the Workflow canvas (not a Gaming node).
class PlaceholderNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    PlaceholderNode() = default;

    static QString Name() { return QStringLiteral("Placeholder"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("Placeholder"); }

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    QWidget *embeddedWidget() override { return nullptr; }
};
