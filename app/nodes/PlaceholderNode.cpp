#include "PlaceholderNode.h"
#include "FlowData.h"

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

unsigned int PlaceholderNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType PlaceholderNode::dataType(PortType, PortIndex) const
{
    return FlowData().type();
}

std::shared_ptr<NodeData> PlaceholderNode::outData(PortIndex)
{
    return std::make_shared<FlowData>();
}
