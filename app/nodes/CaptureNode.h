#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>

#include <memory>

/// Triggers a screen capture + OCR/templates pipeline.
class CaptureNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT
public:
    CaptureNode() = default;

    static QString Name() { return QStringLiteral("Capture"); }

    QString name() const override { return Name(); }
    QString caption() const override { return QStringLiteral("Capture"); }
    bool captionVisible() const override { return true; }

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override;
    QString portCaption(QtNodes::PortType, QtNodes::PortIndex) const override;
    bool portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex) const override { return true; }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override;
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    QWidget *embeddedWidget() override { return nullptr; }
};
