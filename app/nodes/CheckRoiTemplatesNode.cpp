#include "CheckRoiTemplatesNode.h"
#include "FlowData.h"
#include "WorkflowData.h"

unsigned int CheckRoiTemplatesNode::nPorts(QtNodes::PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return 2; // flow, frame
    case QtNodes::PortType::Out:
        return 3; // flow, rois, templates
    default:
        return 0;
    }
}

QtNodes::NodeDataType CheckRoiTemplatesNode::dataType(QtNodes::PortType portType,
                                                      QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return FrameData().type();
    if (portType == QtNodes::PortType::Out && index == 1)
        return RoiListData().type();
    if (portType == QtNodes::PortType::Out && index == 2)
        return TemplateListData().type();
    return FlowData().type();
}

QString CheckRoiTemplatesNode::portCaption(QtNodes::PortType portType, QtNodes::PortIndex index) const
{
    if (portType == QtNodes::PortType::In && index == 1)
        return QStringLiteral("frame");
    if (portType == QtNodes::PortType::Out && index == 1)
        return QStringLiteral("rois");
    if (portType == QtNodes::PortType::Out && index == 2)
        return QStringLiteral("tpls");
    return QStringLiteral("flow");
}

std::shared_ptr<QtNodes::NodeData> CheckRoiTemplatesNode::outData(QtNodes::PortIndex index)
{
    if (index == 1)
        return std::make_shared<RoiListData>();
    if (index == 2)
        return std::make_shared<TemplateListData>();
    return std::make_shared<FlowData>();
}
