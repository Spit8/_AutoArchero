#pragma once

#include "Types.h"

#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class AdbClient;
class CaptureWorker;
class ImageViewer;
class RoiStore;
class TemplateStore;
class QThread;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void requestFrameOcr(const QString &reason = QStringLiteral("automate"));

signals:
    void captureRequested(bool fullScreen);

private slots:
    void onModeChanged(int index);
    void onTestStart();
    void onTestStop();
    void onOcrNow();
    void onRoiDrawn(int x, int y, int w, int h);
    void onSaveRoi();
    void onDeleteRoi();
    void onRoiListContextMenu(const QPoint &pos);
    void onHitRenameRequested(QRect box, QString currentName, QString currentValue);
    void onSaveTemplate();
    void onReloadAssets();
    void onTapCenter();
    void onRoiListSelectionChanged();
    void onPipelineFinished(const PipelineResult &result);
    void onPipelineFailed(const QString &message);
    void onLiveIdleTimeout();

private:
    void loadConfig();
    void updateModeUi();
    void updateLiveIndicator();
    void setStatus(const QString &text);
    void refreshOverlays();
    void refreshRoiList();
    void refreshTemplateList();
    void enqueueCapture();
    void scheduleNextLiveCapture();
    void upsertRoi(const Roi &r);
    QString modeName() const;
    bool fullOcrEnabled() const;
    int currentRoiRow() const;

    Ui::MainWindow *ui = nullptr;
    ImageViewer *m_viewer = nullptr;

    std::unique_ptr<AdbClient> m_adbUi;
    std::unique_ptr<RoiStore> m_rois;
    std::unique_ptr<TemplateStore> m_templates;

    QThread *m_workerThread = nullptr;
    CaptureWorker *m_worker = nullptr;

    QVector<Roi> m_roiList;
    QVector<OcrHit> m_lastHits;
    QVector<TemplateHit> m_lastTemplateHits;
    QImage m_frame;
    QRect m_lastBox;
    bool m_hasLastBox = false;

    QString m_projectRoot;
    QString m_pythonExe;
    QString m_adbPath;

    bool m_liveActive = false;
    QTimer *m_liveIdleTimer = nullptr;
};
