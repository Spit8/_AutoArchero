#include "MainWindow.h"
#include "ui_mainwindow.h"

#include "AdbClient.h"
#include "CaptureWorker.h"
#include "ImageViewer.h"
#include "RoiStore.h"
#include "TemplateStore.h"
#include "TapNode.h"
#include "WaitNode.h"
#include "WorkflowEditor.h"
#include "WorkflowRunner.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#ifndef AUTOARCHERO_SOURCE_DIR
#define AUTOARCHERO_SOURCE_DIR "."
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qRegisterMetaType<PipelineResult>("PipelineResult");

    ui->setupUi(this);
    loadConfig();

    m_viewerSplit = new QSplitter(Qt::Horizontal, ui->viewerHost);
    m_viewer = new ImageViewer(m_viewerSplit);
    m_workflowEditor = new WorkflowEditor(m_viewerSplit);
    m_viewerSplit->addWidget(m_viewer);
    m_viewerSplit->addWidget(m_workflowEditor);
    m_workflowEditor->hide();
    m_viewerSplit->setStretchFactor(0, 1);
    m_viewerSplit->setStretchFactor(1, 1);
    m_viewerSplit->setSizes({700, 500});

    auto *layout = qobject_cast<QVBoxLayout *>(ui->viewerHost->layout());
    if (!layout) {
        layout = new QVBoxLayout(ui->viewerHost);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    while (layout->count() > 0) {
        QLayoutItem *it = layout->takeAt(0);
        // Keep labelCursorCoords from the .ui — re-parent later
        if (it->widget() && it->widget()->objectName() == QLatin1String("labelCursorCoords")) {
            delete it;
            continue;
        }
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    layout->addWidget(m_viewerSplit, 1);
    if (ui->labelCursorCoords) {
        layout->addWidget(ui->labelCursorCoords, 0);
        ui->labelCursorCoords->setText(QStringLiteral("x: —  y: —"));
    }

    connect(m_viewer, &ImageViewer::cursorPosChanged, this,
            [this](int x, int y, bool inside) {
                if (!ui->labelCursorCoords)
                    return;
                if (!inside)
                    ui->labelCursorCoords->setText(QStringLiteral("x: —  y: —"));
                else
                    ui->labelCursorCoords->setText(QStringLiteral("x: %1  y: %2").arg(x).arg(y));
            });

    connect(ui->sideTabs, &QTabWidget::currentChanged, this, &MainWindow::onSideTabChanged);
    connect(ui->btnWorkflowRun, &QPushButton::clicked, this, &MainWindow::onWorkflowRun);
    connect(ui->btnWorkflowLoop, &QPushButton::clicked, this, &MainWindow::onWorkflowLoop);
    connect(ui->btnWorkflowStop, &QPushButton::clicked, this, &MainWindow::onWorkflowStop);
    connect(ui->btnWorkflowExport, &QPushButton::clicked, this, &MainWindow::onWorkflowExport);
    connect(ui->btnWorkflowImport, &QPushButton::clicked, this, &MainWindow::onWorkflowImport);
    connect(m_workflowEditor, &WorkflowEditor::selectionChanged, this,
            &MainWindow::onWorkflowSelectionChanged);
    connect(m_workflowEditor, &WorkflowEditor::selectionCleared, this,
            &MainWindow::onWorkflowSelectionCleared);
    connect(ui->editNodeName, &QLineEdit::editingFinished, this, &MainWindow::onNodePropsEdited);
    connect(ui->spinTapX, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onNodePropsEdited);
    connect(ui->spinTapY, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onNodePropsEdited);
    connect(ui->spinTapDuration, qOverload<int>(&QSpinBox::valueChanged), this,
            &MainWindow::onNodePropsEdited);
    connect(ui->spinWaitMs, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onNodePropsEdited);
    connect(ui->spinWaitRandom, qOverload<int>(&QSpinBox::valueChanged), this,
            &MainWindow::onNodePropsEdited);
    onWorkflowSelectionCleared();

    m_adbUi = std::make_unique<AdbClient>(m_adbPath);
    m_workflowRunner = new WorkflowRunner(m_workflowEditor, m_adbUi.get(), this);
    m_workflowRunner->setCaptureHandler([this](WorkflowRunner::CaptureDone done) {
        m_pendingWorkflowCapture = std::move(done);
        emit captureRequested(fullOcrEnabled());
    });
    connect(m_workflowRunner, &WorkflowRunner::statusChanged, this,
            [this](const QString &text) {
                if (ui->labelWorkflowStatus)
                    ui->labelWorkflowStatus->setText(text);
            });
    connect(m_workflowRunner, &WorkflowRunner::runningChanged, this,
            &MainWindow::onWorkflowRunningChanged);
    onWorkflowRunningChanged(false);

    m_rois = std::make_unique<RoiStore>(QDir(m_projectRoot).filePath(QStringLiteral("assets/rois")));
    m_templates = std::make_unique<TemplateStore>(QDir(m_projectRoot).filePath(QStringLiteral("assets/templates")));
    m_roiList = m_rois->load();

    m_workerThread = new QThread(this);
    m_worker = new CaptureWorker(
        m_adbPath,
        m_pythonExe,
        m_projectRoot,
        m_rois->dir(),
        m_templates->dir());
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &MainWindow::captureRequested, m_worker, &CaptureWorker::runFrameOcr);
    connect(m_worker, &CaptureWorker::finished, this, &MainWindow::onPipelineFinished);
    connect(m_worker, &CaptureWorker::failed, this, &MainWindow::onPipelineFailed);
    m_workerThread->start();

    m_liveIdleTimer = new QTimer(this);
    m_liveIdleTimer->setSingleShot(true);
    m_liveIdleTimer->setInterval(1000);
    connect(m_liveIdleTimer, &QTimer::timeout, this, &MainWindow::onLiveIdleTimeout);

    connect(ui->comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    connect(ui->btnTestStart, &QPushButton::clicked, this, &MainWindow::onTestStart);
    connect(ui->btnTestStop, &QPushButton::clicked, this, &MainWindow::onTestStop);
    connect(ui->btnOcrNow, &QPushButton::clicked, this, &MainWindow::onOcrNow);
    connect(ui->chkDrawRoi, &QCheckBox::toggled, m_viewer, &ImageViewer::setDrawRoiEnabled);
    connect(ui->chkShowRoiGuides, &QCheckBox::toggled, this, [this](bool) { refreshOverlays(); });
    connect(m_viewer, &ImageViewer::roiDrawn, this, &MainWindow::onRoiDrawn);
    connect(m_viewer, &ImageViewer::hitRenameRequested, this, &MainWindow::onHitRenameRequested);
    connect(ui->btnSaveRoi, &QPushButton::clicked, this, &MainWindow::onSaveRoi);
    connect(ui->btnDeleteRoi, &QPushButton::clicked, this, &MainWindow::onDeleteRoi);
    connect(ui->btnSaveTemplate, &QPushButton::clicked, this, &MainWindow::onSaveTemplate);
    connect(ui->btnDeleteTemplate, &QPushButton::clicked, this, &MainWindow::onDeleteTemplate);
    connect(ui->btnReloadAssets, &QPushButton::clicked, this, &MainWindow::onReloadAssets);
    connect(ui->btnTapCenter, &QPushButton::clicked, this, &MainWindow::onTapCenter);
    connect(ui->btnZoomIn, &QPushButton::clicked, m_viewer, &ImageViewer::zoomIn);
    connect(ui->btnZoomOut, &QPushButton::clicked, m_viewer, &ImageViewer::zoomOut);
    connect(ui->btnZoomFit, &QPushButton::clicked, m_viewer, &ImageViewer::resetZoom);
    connect(ui->listRois, &QListWidget::itemSelectionChanged, this, &MainWindow::onRoiListSelectionChanged);
    ui->listRois->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listRois, &QListWidget::customContextMenuRequested, this, &MainWindow::onRoiListContextMenu);
    ui->listTemplates->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listTemplates, &QListWidget::customContextMenuRequested, this,
            &MainWindow::onTemplateListContextMenu);
    connect(ui->btnCopyDebug, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->editDebug->toPlainText());
        ui->labelStatus->setText(QStringLiteral("Status: Debug copié dans le presse-papiers"));
    });

    refreshRoiList();
    refreshTemplateList();
    updateModeUi();
    updateLiveIndicator();
    setStatus(QStringLiteral("Prêt — clic droit sur une BB pour Renommer"));
}

MainWindow::~MainWindow()
{
    m_liveActive = false;
    if (m_liveIdleTimer)
        m_liveIdleTimer->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(5000);
    }
    delete ui;
}

void MainWindow::loadConfig()
{
    m_projectRoot = QDir::fromNativeSeparators(QString::fromUtf8(AUTOARCHERO_SOURCE_DIR));
    m_pythonExe = QDir(m_projectRoot).filePath(QStringLiteral(".venv/Scripts/python.exe"));
    m_adbPath = QStringLiteral("C:/Program Files/BlueStacks_nxt/HD-Adb.exe");

    const QString ini = QDir(m_projectRoot).filePath(QStringLiteral("config.ini"));
    if (QFileInfo::exists(ini)) {
        QSettings s(ini, QSettings::IniFormat);
        const QString root = s.value(QStringLiteral("paths/project_root")).toString().trimmed();
        const QString py = s.value(QStringLiteral("paths/python_exe")).toString().trimmed();
        const QString adb = s.value(QStringLiteral("paths/adb_path")).toString().trimmed();
        if (!root.isEmpty())
            m_projectRoot = root;
        if (!py.isEmpty())
            m_pythonExe = py;
        if (!adb.isEmpty())
            m_adbPath = adb;
    }
}

int MainWindow::currentRoiRow() const
{
    return ui->listRois->currentRow();
}

QString MainWindow::modeName() const
{
    return ui->comboMode->currentText();
}

bool MainWindow::fullOcrEnabled() const
{
    return ui->chkFullOcr->isChecked();
}

void MainWindow::enqueueCapture()
{
    emit captureRequested(fullOcrEnabled());
}

void MainWindow::scheduleNextLiveCapture()
{
    if (!m_liveActive)
        return;
    m_liveIdleTimer->start();
    updateLiveIndicator();
}

void MainWindow::updateModeUi()
{
    const bool isTest = modeName() == QLatin1String("Test");
    ui->btnTestStart->setEnabled(isTest);
    ui->btnTestStop->setEnabled(isTest);
    ui->btnOcrNow->setEnabled(!isTest);
    if (!isTest) {
        m_liveActive = false;
        m_liveIdleTimer->stop();
    }
    updateLiveIndicator();
}

void MainWindow::updateLiveIndicator()
{
    if (m_liveActive) {
        const QString wait = m_liveIdleTimer->isActive()
                                 ? QStringLiteral("LIVE — prochaine capture dans 1s")
                                 : QStringLiteral("LIVE — OCR en cours…");
        ui->labelLiveState->setText(wait);
        ui->labelLiveState->setStyleSheet(
            QStringLiteral("font-weight: 700; padding: 8px; background: #2e7d4f; color: #e8fff0; border-radius: 4px;"));
    } else {
        ui->labelLiveState->setText(QStringLiteral("PAUSE — live arrêté"));
        ui->labelLiveState->setStyleSheet(
            QStringLiteral("font-weight: 700; padding: 8px; background: #5c4a4a; color: #ffe8e8; border-radius: 4px;"));
    }
}

void MainWindow::onModeChanged(int)
{
    updateModeUi();
    setStatus(QStringLiteral("Mode %1").arg(modeName()));
}

void MainWindow::onTestStart()
{
    if (modeName() != QLatin1String("Test"))
        return;
    m_liveActive = true;
    m_liveIdleTimer->stop();
    enqueueCapture();
    updateLiveIndicator();
    setStatus(QStringLiteral("Test live démarré (fin OCR + 1s)"));
}

void MainWindow::onTestStop()
{
    m_liveActive = false;
    m_liveIdleTimer->stop();
    updateLiveIndicator();
    setStatus(QStringLiteral("Test live stoppé"));
}

void MainWindow::onLiveIdleTimeout()
{
    if (!m_liveActive)
        return;
    enqueueCapture();
    updateLiveIndicator();
}

void MainWindow::onOcrNow()
{
    requestFrameOcr(QStringLiteral("manual"));
}

void MainWindow::onSideTabChanged(int index)
{
    if (!m_viewerSplit || !ui->sideTabs)
        return;
    QWidget *page = ui->sideTabs->widget(index);
    const bool workflow = (page == ui->tabWorkflow);
    if (m_workflowEditor)
        m_workflowEditor->setVisible(workflow);
    if (ui->labelCursorCoords)
        ui->labelCursorCoords->setVisible(true);
    if (!workflow && m_workflowRunner && m_workflowRunner->isRunning())
        m_workflowRunner->stop();
}

void MainWindow::onWorkflowRun()
{
    if (ui->sideTabs && ui->tabWorkflow)
        ui->sideTabs->setCurrentWidget(ui->tabWorkflow);
    if (m_workflowEditor)
        m_workflowEditor->setVisible(true);
    if (m_workflowRunner && m_workflowRunner->isRunning())
        return;
    if (m_workflowRunner)
        m_workflowRunner->startSingle();
}

void MainWindow::onWorkflowLoop()
{
    if (ui->sideTabs && ui->tabWorkflow)
        ui->sideTabs->setCurrentWidget(ui->tabWorkflow);
    if (m_workflowEditor)
        m_workflowEditor->setVisible(true);
    if (m_workflowRunner && m_workflowRunner->isRunning())
        return;
    if (m_workflowRunner)
        m_workflowRunner->startLoop();
}

void MainWindow::onWorkflowRunningChanged(bool running)
{
    const QString activeStyle = QStringLiteral(
        "QPushButton { background-color: #2ecc71; color: #111; font-weight: bold; }");

    const bool isLoop = running && m_workflowRunner
                        && m_workflowRunner->mode() == WorkflowRunner::Mode::Loop;
    const bool isSingle = running && !isLoop;

    if (ui->btnWorkflowRun) {
        ui->btnWorkflowRun->setChecked(isSingle);
        ui->btnWorkflowRun->setEnabled(!running);
        ui->btnWorkflowRun->setStyleSheet(isSingle ? activeStyle : QString());
    }
    if (ui->btnWorkflowLoop) {
        ui->btnWorkflowLoop->setChecked(isLoop);
        ui->btnWorkflowLoop->setEnabled(!running);
        ui->btnWorkflowLoop->setStyleSheet(isLoop ? activeStyle : QString());
    }
}

void MainWindow::onWorkflowStop()
{
    if (m_workflowRunner)
        m_workflowRunner->stop();
}

void MainWindow::onWorkflowExport()
{
    if (!m_workflowEditor)
        return;
    if (m_workflowRunner && m_workflowRunner->isRunning())
        m_workflowRunner->stop();

    QDir dir(QDir(m_projectRoot).filePath(QStringLiteral("assets/workflows")));
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Exporter le workflow"),
        dir.filePath(QStringLiteral("workflow.flow")),
        QStringLiteral("Workflow (*.flow *.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".flow"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".flow");
    }

    QString err;
    if (!m_workflowEditor->exportToFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Export workflow"), err);
        return;
    }
    if (ui->labelWorkflowStatus)
        ui->labelWorkflowStatus->setText(QStringLiteral("Exporté: %1").arg(QFileInfo(path).fileName()));
    setStatus(QStringLiteral("Workflow exporté"));
}

void MainWindow::onWorkflowImport()
{
    if (!m_workflowEditor)
        return;
    if (m_workflowRunner && m_workflowRunner->isRunning())
        m_workflowRunner->stop();

    const QString dir = QDir(m_projectRoot).filePath(QStringLiteral("assets/workflows"));
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Importer un workflow"),
        dir,
        QStringLiteral("Workflow (*.flow *.json)"));
    if (path.isEmpty())
        return;

    QString err;
    if (!m_workflowEditor->importFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Import workflow"), err);
        return;
    }
    if (m_workflowEditor)
        m_workflowEditor->setVisible(true);
    if (ui->sideTabs && ui->tabWorkflow)
        ui->sideTabs->setCurrentWidget(ui->tabWorkflow);
    if (ui->labelWorkflowStatus)
        ui->labelWorkflowStatus->setText(QStringLiteral("Importé: %1").arg(QFileInfo(path).fileName()));
    setStatus(QStringLiteral("Workflow importé"));
}

void MainWindow::setNodePropsVisible(bool hasSelection)
{
    if (ui->labelNoNodeSelected)
        ui->labelNoNodeSelected->setVisible(!hasSelection);
    if (ui->labelNodeName)
        ui->labelNodeName->setVisible(hasSelection);
    if (ui->editNodeName)
        ui->editNodeName->setVisible(hasSelection);
    if (ui->stackNodeTypeProps)
        ui->stackNodeTypeProps->setVisible(hasSelection);
}

void MainWindow::onWorkflowSelectionCleared()
{
    if (m_nodeParamsConn)
        disconnect(m_nodeParamsConn);
    setNodePropsVisible(false);
    if (ui->stackNodeTypeProps)
        ui->stackNodeTypeProps->setCurrentWidget(ui->pagePropsEmpty);
}

void MainWindow::onWorkflowSelectionChanged(QtNodes::NodeId, QString typeName)
{
    Q_UNUSED(typeName);
    bindSelectedNodeParams();
    syncNodePropsFromSelection();
}

void MainWindow::bindSelectedNodeParams()
{
    if (m_nodeParamsConn)
        disconnect(m_nodeParamsConn);
    if (!m_workflowEditor)
        return;

    if (TapNode *tap = m_workflowEditor->selectedTap()) {
        m_nodeParamsConn = connect(tap, &TapNode::paramsChanged, this,
                                   &MainWindow::syncNodePropsFromSelection);
    } else if (WaitNode *wait = m_workflowEditor->selectedWait()) {
        m_nodeParamsConn = connect(wait, &WaitNode::paramsChanged, this,
                                   &MainWindow::syncNodePropsFromSelection);
    }
}

void MainWindow::syncNodePropsFromSelection()
{
    if (!m_workflowEditor || m_updatingNodeProps)
        return;

    m_updatingNodeProps = true;

    if (TapNode *tap = m_workflowEditor->selectedTap()) {
        setNodePropsVisible(true);
        ui->editNodeName->setText(tap->displayName());
        ui->spinTapX->setValue(tap->x());
        ui->spinTapY->setValue(tap->y());
        ui->spinTapDuration->setValue(tap->durationMs());
        ui->stackNodeTypeProps->setCurrentWidget(ui->pagePropsTap);
    } else if (WaitNode *wait = m_workflowEditor->selectedWait()) {
        setNodePropsVisible(true);
        ui->editNodeName->setText(wait->displayName());
        ui->spinWaitMs->setValue(wait->durationMs());
        ui->spinWaitRandom->setValue(wait->randomMs());
        ui->stackNodeTypeProps->setCurrentWidget(ui->pagePropsWait);
    } else {
        onWorkflowSelectionCleared();
    }

    m_updatingNodeProps = false;
}

void MainWindow::onNodePropsEdited()
{
    if (m_updatingNodeProps || !m_workflowEditor)
        return;

    m_updatingNodeProps = true;
    if (m_workflowEditor->selectedTap()) {
        m_workflowEditor->setSelectedDisplayName(ui->editNodeName->text());
        m_workflowEditor->setSelectedTapX(ui->spinTapX->value());
        m_workflowEditor->setSelectedTapY(ui->spinTapY->value());
        m_workflowEditor->setSelectedTapDurationMs(ui->spinTapDuration->value());
    } else if (m_workflowEditor->selectedWait()) {
        m_workflowEditor->setSelectedDisplayName(ui->editNodeName->text());
        m_workflowEditor->setSelectedWaitDurationMs(ui->spinWaitMs->value());
        m_workflowEditor->setSelectedWaitRandomMs(ui->spinWaitRandom->value());
    }
    m_updatingNodeProps = false;
}

void MainWindow::requestFrameOcr(const QString &reason)
{
    if (modeName() != QLatin1String("Gaming"))
        ui->comboMode->setCurrentText(QStringLiteral("Gaming"));
    ui->labelGamingLast->setText(QStringLiteral("Dernière demande: %1").arg(reason));
    enqueueCapture();
    setStatus(QStringLiteral("Gaming OCR demandé…"));
}

void MainWindow::refreshOverlays()
{
    m_viewer->clearOverlays();
    if (ui->chkShowRoiGuides->isChecked())
        m_viewer->showRois(m_roiList);
    if (!m_lastHits.isEmpty())
        m_viewer->showOcrHits(m_lastHits);
    if (!m_lastTemplateHits.isEmpty())
        m_viewer->showTemplateHits(m_lastTemplateHits);
}

void MainWindow::refreshRoiList()
{
    const int keepRow = currentRoiRow();
    ui->listRois->clear();
    for (const Roi &r : m_roiList) {
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        ui->listRois->addItem(
            QStringLiteral("%1 — %2,%3 (%4×%5)")
                .arg(r.name)
                .arg(cx)
                .arg(cy)
                .arg(r.w)
                .arg(r.h));
    }
    if (keepRow >= 0 && keepRow < ui->listRois->count())
        ui->listRois->setCurrentRow(keepRow);
}

void MainWindow::refreshTemplateList()
{
    ui->listTemplates->clear();
    for (const TemplateEntry &e : m_templates->listEntries()) {
        QString line = e.name;
        if (!e.hasPng)
            line += QStringLiteral(" — PNG manquant");
        else if (!e.hasJson)
            line += QStringLiteral(" — JSON manquant");
        else
            line += QStringLiteral(" — OK");
        auto *item = new QListWidgetItem(line);
        item->setData(Qt::UserRole, e.name);
        if (!e.hasPng)
            item->setForeground(QColor(200, 80, 80));
        ui->listTemplates->addItem(item);
    }
}

void MainWindow::onRoiListSelectionChanged()
{
    const int row = currentRoiRow();
    if (row < 0 || row >= m_roiList.size())
        return;
    ui->editRoiName->setText(m_roiList[row].name);
}

void MainWindow::onPipelineFinished(const PipelineResult &result)
{
    m_frame = result.image;
    m_lastHits = result.hits;
    m_lastTemplateHits = result.templateHits;
    m_viewer->setFramePixmap(QPixmap::fromImage(m_frame));
    refreshOverlays();
    ui->listHits->clear();
    for (const OcrHit &h : result.hits) {
        auto *item = new QListWidgetItem(ImageViewer::formatOcrLabel(h.roiName, h.text));
        item->setData(Qt::UserRole, h.box);
        item->setData(Qt::UserRole + 1, h.text);
        item->setData(Qt::UserRole + 2, ImageViewer::ocrDisplayName(h.roiName));
        if (ImageViewer::ocrDisplayName(h.roiName) == QLatin1String("?"))
            item->setForeground(QColor(255, 40, 200));
        else
            item->setForeground(QColor(40, 180, 70));
        ui->listHits->addItem(item);
    }
    for (const TemplateHit &h : result.templateHits) {
        auto *item = new QListWidgetItem(ImageViewer::formatTemplateLabel(h.name, h.conf));
        item->setData(Qt::UserRole, h.box);
        item->setData(Qt::UserRole + 1, h.name);
        item->setForeground(QColor(220, 120, 40));
        ui->listHits->addItem(item);
    }
    QString status = QStringLiteral("Frame %1x%2 — OCR %3 — TPL %4")
                         .arg(m_frame.width())
                         .arg(m_frame.height())
                         .arg(result.hits.size())
                         .arg(result.templateHits.size());
    if (!result.warning.isEmpty())
        status += QStringLiteral(" | warn: %1").arg(result.warning);
    setStatus(status);

    if (m_pendingWorkflowCapture) {
        auto cb = std::move(m_pendingWorkflowCapture);
        m_pendingWorkflowCapture = {};
        cb(true, result, {});
        return;
    }
    scheduleNextLiveCapture();
}

void MainWindow::onPipelineFailed(const QString &message)
{
    setStatus(QStringLiteral("Erreur: %1").arg(message));
    if (m_pendingWorkflowCapture) {
        auto cb = std::move(m_pendingWorkflowCapture);
        m_pendingWorkflowCapture = {};
        cb(false, {}, message);
        return;
    }
    scheduleNextLiveCapture();
}

void MainWindow::onRoiDrawn(int x, int y, int w, int h)
{
    m_lastBox = QRect(x, y, w, h);
    m_hasLastBox = true;
    ui->labelLastBox->setText(QStringLiteral("Dernière box: %1,%2 %3x%4").arg(x).arg(y).arg(w).arg(h));
}

void MainWindow::upsertRoi(const Roi &r)
{
    m_rois->save(r);
    m_roiList.erase(std::remove_if(m_roiList.begin(), m_roiList.end(),
                                   [&](const Roi &o) { return o.name == r.name; }),
                    m_roiList.end());
    m_roiList.push_back(r);
    refreshRoiList();
    refreshOverlays();
}

void MainWindow::onSaveRoi()
{
    if (!m_hasLastBox) {
        QMessageBox::information(this, QStringLiteral("ROI"), QStringLiteral("Dessine d'abord une ROI."));
        return;
    }
    Roi r;
    r.name = ui->editRoiName->text().trimmed();
    if (r.name.isEmpty())
        r.name = QStringLiteral("zone");
    r.x = m_lastBox.x();
    r.y = m_lastBox.y();
    r.w = m_lastBox.width();
    r.h = m_lastBox.height();
    r.enabled = true;
    try {
        upsertRoi(r);
        setStatus(QStringLiteral("ROI sauvée: %1").arg(r.name));
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("ROI"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onDeleteRoi()
{
    const int row = currentRoiRow();
    QString name;
    if (row >= 0 && row < m_roiList.size())
        name = m_roiList[row].name;
    else
        name = ui->editRoiName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("ROI"),
                                 QStringLiteral("Sélectionne une ROI dans la liste."));
        return;
    }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Supprimer ROI"),
        QStringLiteral("Supprimer la zone « %1 » du catalogue ?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    try {
        m_rois->remove(name);
        m_roiList = m_rois->load();
        m_lastHits.erase(std::remove_if(m_lastHits.begin(), m_lastHits.end(),
                                        [&](const OcrHit &h) { return h.roiName == name; }),
                         m_lastHits.end());
        refreshRoiList();
        refreshOverlays();
        ui->listHits->clear();
        for (const OcrHit &h : m_lastHits) {
            auto *item = new QListWidgetItem(ImageViewer::formatOcrLabel(h.roiName, h.text));
            item->setData(Qt::UserRole, h.box);
            item->setData(Qt::UserRole + 1, h.text);
            item->setData(Qt::UserRole + 2, ImageViewer::ocrDisplayName(h.roiName));
            if (ImageViewer::ocrDisplayName(h.roiName) == QLatin1String("?"))
                item->setForeground(QColor(255, 40, 200));
            else
                item->setForeground(QColor(40, 180, 70));
            ui->listHits->addItem(item);
        }
        setStatus(QStringLiteral("ROI supprimée: %1").arg(name));
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("ROI"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onRoiListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = ui->listRois->itemAt(pos);
    if (!item)
        return;
    ui->listRois->setCurrentItem(item);
    QMenu menu(this);
    QAction *del = menu.addAction(QStringLiteral("Supprimer ROI"));
    if (menu.exec(ui->listRois->mapToGlobal(pos)) == del)
        onDeleteRoi();
}

void MainWindow::onDeleteTemplate()
{
    QListWidgetItem *item = ui->listTemplates->currentItem();
    QString name;
    if (item)
        name = item->data(Qt::UserRole).toString();
    if (name.isEmpty())
        name = ui->editTemplateName->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Template"),
                                 QStringLiteral("Sélectionne un template dans la liste."));
        return;
    }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Supprimer template"),
        QStringLiteral("Supprimer le template « %1 » du catalogue ?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    try {
        m_templates->remove(name);
        m_lastTemplateHits.erase(
            std::remove_if(m_lastTemplateHits.begin(), m_lastTemplateHits.end(),
                           [&](const TemplateHit &h) { return h.name == name; }),
            m_lastTemplateHits.end());
        refreshTemplateList();
        refreshOverlays();
        setStatus(QStringLiteral("Template supprimé: %1").arg(name));
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Template"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onTemplateListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = ui->listTemplates->itemAt(pos);
    if (!item)
        return;
    ui->listTemplates->setCurrentItem(item);
    QMenu menu(this);
    QAction *del = menu.addAction(QStringLiteral("Supprimer template"));
    if (menu.exec(ui->listTemplates->mapToGlobal(pos)) == del)
        onDeleteTemplate();
}

void MainWindow::onHitRenameRequested(QRect box, QString currentName, QString currentValue)
{
    QString suggested = currentName;
    if (suggested.isEmpty() || suggested == QLatin1String("?"))
        suggested = QStringLiteral("zone");
    bool ok = false;
    const QString newName = QInputDialog::getText(
                                this,
                                QStringLiteral("Renommer zone"),
                                QStringLiteral("Nom gaming pour cette bounding box%1:")
                                    .arg(currentValue.isEmpty()
                                             ? QString()
                                             : QStringLiteral(" (%1)").arg(currentValue)),
                                QLineEdit::Normal,
                                suggested,
                                &ok)
                                .trimmed();
    if (!ok || newName.isEmpty())
        return;

    const int pad = 4;
    Roi r;
    r.name = newName;
    r.x = qMax(0, box.x() - pad);
    r.y = qMax(0, box.y() - pad);
    r.w = box.width() + 2 * pad;
    r.h = box.height() + 2 * pad;
    if (!m_frame.isNull()) {
        r.w = qMin(r.w, m_frame.width() - r.x);
        r.h = qMin(r.h, m_frame.height() - r.y);
    }
    r.enabled = true;

    try {
        if (!currentName.isEmpty() && currentName != QLatin1String("?")
            && currentName != newName) {
            // Try rename existing ROI file if it matches currentName
            bool found = false;
            for (const Roi &existing : m_roiList) {
                if (existing.name == currentName) {
                    found = true;
                    break;
                }
            }
            if (found) {
                m_rois->rename(currentName, newName);
                // Update box too
                m_rois->save(r);
                m_roiList = m_rois->load();
            } else {
                upsertRoi(r);
            }
        } else {
            upsertRoi(r);
        }

        // Retag overlapping OCR hits → Name: Value (no cyan-only ghost box)
        bool retagged = false;
        for (OcrHit &h : m_lastHits) {
            if (h.box == box || h.box.intersects(box) || box.contains(h.box.center())) {
                h.roiName = newName;
                retagged = true;
            }
        }
        if (!retagged) {
            OcrHit synthetic;
            synthetic.roiName = newName;
            synthetic.text = currentValue.isEmpty() ? QStringLiteral("…") : currentValue;
            synthetic.conf = 1.0;
            synthetic.box = box;
            m_lastHits.push_back(synthetic);
        }
        refreshRoiList();
        refreshOverlays();
        setStatus(QStringLiteral("Zone nommée: %1 — rescannera au prochain OCR").arg(newName));
        ui->sideTabs->setCurrentWidget(ui->tabZones);
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("ROI"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onSaveTemplate()
{
    if (m_frame.isNull()) {
        QMessageBox::information(this, QStringLiteral("Template"), QStringLiteral("Capture une frame d'abord."));
        return;
    }
    if (!m_hasLastBox) {
        QMessageBox::information(this, QStringLiteral("Template"),
                                 QStringLiteral("Dessine d'abord une box (onglet Zones)."));
        return;
    }
    try {
        const auto paths = m_templates->saveCrop(m_frame, ui->editTemplateName->text().trimmed(), m_lastBox);
        refreshTemplateList();
        setStatus(QStringLiteral("Template: %1 (crop natif)")
                      .arg(QFileInfo(paths.first).fileName()));
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Template"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::onReloadAssets()
{
    m_roiList = m_rois->load();
    refreshRoiList();
    refreshTemplateList();
    const auto entries = m_templates->listEntries();
    int missingPng = 0;
    for (const TemplateEntry &e : entries) {
        if (e.hasJson && !e.hasPng)
            ++missingPng;
    }
    if (!m_frame.isNull())
        refreshOverlays();
    QString msg = QStringLiteral("ROI: %1 — templates PNG: %2")
                      .arg(m_roiList.size())
                      .arg(m_templates->countEntries());
    if (missingPng > 0)
        msg += QStringLiteral(" (%1 JSON sans PNG)").arg(missingPng);
    setStatus(msg);
}

void MainWindow::onTapCenter()
{
    try {
        int x = 0, y = 0;
        if (!m_frame.isNull()) {
            x = m_frame.width() / 2;
            y = m_frame.height() / 2;
        } else {
            const auto sz = m_adbUi->wmSize();
            x = sz.first / 2;
            y = sz.second / 2;
        }
        m_adbUi->tap(x, y);
        setStatus(QStringLiteral("Tap fantôme ADB (%1,%2)").arg(x).arg(y));
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("ADB"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::setStatus(const QString &text)
{
    ui->labelStatus->setText(QStringLiteral("Status: %1").arg(text));
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text);
    ui->editDebug->appendPlainText(line);
    // Keep view scrolled to latest
    QTextCursor c = ui->editDebug->textCursor();
    c.movePosition(QTextCursor::End);
    ui->editDebug->setTextCursor(c);
}
