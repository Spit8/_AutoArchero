#pragma once

#include "Types.h"

#include <QtNodes/NodeData>

#include <QImage>
#include <QString>
#include <QVector>

struct RoiHitItem {
    QString name;
    QString value;
    double conf = 0.0;
    int cx = 0;
    int cy = 0;
};

struct TemplateHitItem {
    QString name;
    double conf = 0.0;
    int cx = 0;
    int cy = 0;
};

inline RoiHitItem roiHitFromOcr(const OcrHit &h)
{
    RoiHitItem item;
    item.name = h.roiName;
    item.value = h.text;
    item.conf = h.conf;
    item.cx = h.box.center().x();
    item.cy = h.box.center().y();
    return item;
}

inline TemplateHitItem templateHitFromMatch(const TemplateHit &h)
{
    TemplateHitItem item;
    item.name = h.name;
    item.conf = h.conf;
    item.cx = h.box.center().x();
    item.cy = h.box.center().y();
    return item;
}

class FrameData : public QtNodes::NodeData
{
public:
    FrameData() = default;
    explicit FrameData(QImage img)
        : image(std::move(img))
    {
    }

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("frame"), QStringLiteral("Frame")};
    }

    QImage image;
};

class RoiListData : public QtNodes::NodeData
{
public:
    RoiListData() = default;
    explicit RoiListData(QVector<RoiHitItem> items)
        : items(std::move(items))
    {
    }

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("roi_list"), QStringLiteral("ROI list")};
    }

    QVector<RoiHitItem> items;
};

class TemplateListData : public QtNodes::NodeData
{
public:
    TemplateListData() = default;
    explicit TemplateListData(QVector<TemplateHitItem> items)
        : items(std::move(items))
    {
    }

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("tpl_list"), QStringLiteral("Templates")};
    }

    QVector<TemplateHitItem> items;
};
