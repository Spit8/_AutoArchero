#pragma once

#include "Types.h"
#include "WorkflowData.h"

#include <QVector>

/// Shared state while a workflow run is active.
struct WorkflowContext {
    PipelineResult lastResult;
    QVector<RoiHitItem> rois;
    QVector<TemplateHitItem> templates;
    bool hasResult = false;

    void clear()
    {
        lastResult = {};
        rois.clear();
        templates.clear();
        hasResult = false;
    }

    void setResult(const PipelineResult &r)
    {
        lastResult = r;
        hasResult = true;
        rois.clear();
        templates.clear();
        for (const OcrHit &h : r.hits)
            rois.push_back(roiHitFromOcr(h));
        for (const TemplateHit &h : r.templateHits)
            templates.push_back(templateHitFromMatch(h));
    }
};
