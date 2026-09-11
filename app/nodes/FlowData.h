#pragma once

#include <QtNodes/NodeData>

/// Shared flow port type for Gaming workflow nodes.
class FlowData : public QtNodes::NodeData
{
public:
    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("flow"), QStringLiteral("Flow")};
    }
};
