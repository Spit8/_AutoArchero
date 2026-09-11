#include "CaptureNode.h"
#include "FlowData.h"
#include "WorkflowData.h"

unsigned int CaptureNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return 1;
    case QtNodes::PortType::Out:
        return 2; // flow, frame
    default:
        return 0;
    }
}

QtNodes::NodeDataType CaptureNode::dataType(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::Out && index == 1)
        return FrameData().type();
    return FlowData().type();
}

QString CaptureNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::Out && index == 1)
        return QStringLiteral("frame");
    if (portType == QtNodes::PortType::Out)
        return QStringLiteral("flow");
    return QStringLiteral("flow");
}

std::shared_ptr<QtNodes::NodeData> CaptureNode::outData(QtNodes::PortIndex index)
{
    if (index == 1)
        return std::make_shared<FrameData>();
    return std::make_shared<FlowData>();
}
