#include "FileControlWidget.h"
#include "ui_FileControlWidget.h"
#include "DataManager.h"
#include "CraftTreeModel.h"
#include "CraftTableView.h"
#include "ICraft.h"
#include "Logger.h"
#include "Import/DxfFileSelectDialog.h"
#include "Import/DxfGraphicsImporter.h"
#include "Import/DirectorySelectDialog.h"
#include "../Form/DrawPanel/drawpanel.h"
#include "../Form/layeredscene.h"
#include "../Form/GraphicsItemTableView.h"
#include "../../CustomMessageBox.h"
#include "../../BaseDialog.h"
#include "../../PathManager.h"
#include "AppFileLayout.h"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPen>
#include <QFileDialog>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QApplication>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QSet>
#include <QtAlgorithms>
#include <QSettings>
#include <QFileInfo>
#include "ToolLibrary/ToolLibraryDialog.h"
#include "ToolLibrary/ToolLibrary.h"
#include "../Form/AppConfigDialog.h"
#include "CadModel/CadDocumentModel.h"
#include "CommonDataControll.h"

namespace {
const char kSessionGroup[] = "Session";
const char kLastProjectDirKey[] = "lastProjectDir";

static void reloadToolLibraryRadiiAfterGraphicsImport()
{
    ToolLibrary* tl = ToolLibrary::instance();
    if (tl) {
        tl->reloadRadiiFromControllerAndNotify();
    }
}

static QString promptExportProjectName(QWidget* parent, bool* ok)
{
    if (ok) {
        *ok = false;
    }

    BaseDialog dialog(parent);
    dialog.setWindowTitle(QString::fromUtf8("导出项目"));
    dialog.enableCloseButton(true);
    dialog.setFixedSize(420, 210);

    QWidget* content = dialog.contentWidget();
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(30, 26, 30, 24);
    layout->setSpacing(14);

    QLabel* titleLabel = new QLabel(QString::fromUtf8("请输入项目名称："), content);
    QLineEdit* nameEdit = new QLineEdit(QString::fromUtf8("Project"), content);
    nameEdit->setMinimumHeight(36);
    nameEdit->selectAll();

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        content);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("取消"));

    layout->addWidget(titleLabel);
    layout->addWidget(nameEdit);
    layout->addStretch();
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    nameEdit->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }

    if (ok) {
        *ok = true;
    }
    return nameEdit->text().trimmed();
}
} // namespace

FileControlWidget::FileControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FileControlWidget)
    , m_scene(NULL)
    , m_docModel(NULL)
    , m_craftView(NULL)
    , m_craftToolBar(NULL)
    , m_drawPanel(NULL)
    , m_autoSaveTimer(NULL)
    , m_pendingDxfPreviewItem(NULL)
{
    ui->setupUi(this);
    
    // 连接按钮信号
    connect(ui->importProjectButton, SIGNAL(clicked()), this, SLOT(onImportProject()));
    connect(ui->exportProjectButton, SIGNAL(clicked()), this, SLOT(onExportProject()));
    connect(ui->importDxfButton, SIGNAL(clicked()), this, SLOT(onImportDxf()));
    if (ui->alignRegisterButton) {
        connect(ui->alignRegisterButton, SIGNAL(clicked()), this, SLOT(onAlignRegisterClicked()));
    }
    if (ui->toolLibraryButton) {
        connect(ui->toolLibraryButton, SIGNAL(clicked()), this, SLOT(onToolLibraryClicked()));
    }
    if (ui->appConfigButton) {
        connect(ui->appConfigButton, SIGNAL(clicked()), this, SLOT(onAppConfigClicked()));
    }
}

FileControlWidget::~FileControlWidget()
{
    cancelPendingDxfPlacement();
    delete ui;
}

void FileControlWidget::setScene(QGraphicsScene* scene)
{
    m_scene = scene;
}

void FileControlWidget::setDocumentModel(CadDocumentModel* docModel)
{
    m_docModel = docModel;
}

void FileControlWidget::setCraftTableView(CraftTableView* craftView)
{
    m_craftView = craftView;
}

void FileControlWidget::setCraftToolBar(CraftToolBar* craftToolBar)
{
    m_craftToolBar = craftToolBar;
}

void FileControlWidget::setDrawPanel(DrawPanel* drawPanel)
{
    m_drawPanel = drawPanel;
}

void FileControlWidget::onAlignRegisterClicked()
{
    emit alignToRegisterRequested();
}

void FileControlWidget::onToolLibraryClicked()
{
    ToolLibraryDialog* dialog = new ToolLibraryDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

void FileControlWidget::onAppConfigClicked()
{
    AppConfigDialog* dialog = new AppConfigDialog(m_craftToolBar, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, SIGNAL(smallScreenModeChanged(bool)),
            this, SIGNAL(smallScreenModeChanged(bool)));
    dialog->exec();
}

void FileControlWidget::onImportProject()
{
    LOG_FUNCTION_ENTRY();

    if (!m_docModel) {
        CustomMessageBox::warning(this, QString::fromUtf8("错误"), 
                            QString::fromUtf8("文档模型未就绪"));
        LOG_FUNCTION_EXIT();
        return;
    }
    {
        const int formalGfx = m_docModel ? m_docModel->graphicIds().size() : 0;
        LOG_INFO(QString::fromUtf8("onImportProject: 用户点击导入；当前画布正式图元=%1，getAllCrafts()=%2")
                 .arg(formalGfx).arg(getAllCrafts().size()));
    }

    // 使用基于BaseDialog的目录选择对话框
    QString defaultDir = "/root";
    if (PathManager::useCustomPaths()) {
        defaultDir = PathManager::getProjectDir();
    }
    {
        CommonDataControll commonDataCtrl;
        if (commonDataCtrl.CheckUSBExist()) {
            defaultDir = QLatin1String("/tmp/usb");
        }
    }
    QString projectFolder = DirectorySelectDialog::getExistingDirectory(
        this,
        QString::fromUtf8("选择项目文件夹"),
        defaultDir
    );

    if (projectFolder.isEmpty()) {
        LOG_FUNCTION_EXIT();
        return;
    }

    QString dxfPath = findProjectFile(projectFolder, "dxf", AppFileLayout::projectGraphicsDxfFileName());
    if (dxfPath.isEmpty()) {
        CustomMessageBox::warning(this, QString::fromUtf8("错误"), 
                            QString::fromUtf8("在项目文件夹中未找到DXF文件"));
        LOG_FUNCTION_EXIT();
        return;
    }

    const QString yamlProbe = findProjectFile(projectFolder, "yaml", AppFileLayout::projectParametersYamlFileName());
    LOG_INFO(QString::fromUtf8("onImportProject: 已选目录=%1 dxf=%2 yaml=%3 yaml存在=%4")
             .arg(projectFolder).arg(dxfPath).arg(yamlProbe.isEmpty() ? QString::fromUtf8("(无)") : yamlProbe)
             .arg(!yamlProbe.isEmpty()));

    importProjectFromFolderImpl(projectFolder, false);
    LOG_FUNCTION_EXIT();
}

void FileControlWidget::onExportProject()
{
    LOG_FUNCTION_ENTRY();
    
    if (!m_docModel) {
        CustomMessageBox::warning(this, QString::fromUtf8("错误"), 
                            QString::fromUtf8("文档模型未就绪"));
        LOG_FUNCTION_EXIT();
        return;
    }

    const QList<ICraft*> crafts = getAllCrafts();
    {
        const int formalGfx = m_docModel->graphicIds().size();
        LOG_INFO(QString::fromUtf8("onExportProject: 用户点击导出；工艺数=%1，画布正式图元=%2")
                 .arg(crafts.size()).arg(formalGfx));
    }
    if (!hasExportableContent()) {
        CustomMessageBox::warning(this, QString::fromUtf8("提示"), 
                            QString::fromUtf8("没有可导出的图形或工艺"));
        LOG_FUNCTION_EXIT();
        return;
    }

    bool ok;
    QString projectName = promptExportProjectName(this, &ok);

    if (!ok || projectName.isEmpty()) {
        LOG_FUNCTION_EXIT();
        return;
    }

    // 使用基于BaseDialog的目录选择对话框（库构建：默认 /home/disk3/machine）
    QString defaultDir = AppFileLayout::exportProjectDefaultParentDir();
    {
        CommonDataControll commonDataCtrl;
        const bool usb = commonDataCtrl.CheckUSBExist();
        if (usb) {
            defaultDir = QLatin1String("/tmp/usb");
        }
        LOG_INFO(QString::fromUtf8("onExportProject: CheckUSBExist=%1，目录选择默认=%2").arg(usb).arg(defaultDir));
    }
    QString parentDir = DirectorySelectDialog::getExistingDirectory(
        this,
        QString::fromUtf8("选择项目保存位置"),
        defaultDir
    );

    if (parentDir.isEmpty()) {
        LOG_FUNCTION_EXIT();
        return;
    }

    // 构建项目文件夹路径
    QDir parentDirObj(parentDir);
    QString projectFolder = parentDirObj.filePath(projectName);

    // 检查项目文件夹是否已存在
    bool projectExists = QDir(projectFolder).exists();
    if (projectExists) {
        // 只要项目文件夹存在就提示是否替换
        CustomMessageBox::StandardButton ret = CustomMessageBox::question(this, QString::fromUtf8("确认"), 
                                       QString::fromUtf8("项目\"%1\"已存在，是否替换？").arg(projectName),
                                       CustomMessageBox::Yes | CustomMessageBox::No);
        if (ret != CustomMessageBox::Yes) {
            LOG_FUNCTION_EXIT();
            return;
        }
    }

    // 创建项目文件夹
    QDir dir;
    if (!dir.exists(projectFolder)) {
        if (!dir.mkpath(projectFolder)) {
            CustomMessageBox::critical(this, QString::fromUtf8("错误"), 
                                 QString::fromUtf8("无法创建项目文件夹"));
            LOG_FUNCTION_EXIT();
            return;
        }
    }

    QString err;
    LOG_INFO(QString::fromUtf8("onExportProject: 调用 saveProjectToFolder，folder=%1 工艺数=%2")
             .arg(projectFolder).arg(crafts.size()));
    if (!saveProjectToFolder(projectFolder, &err)) {
        CustomMessageBox::critical(this, QString::fromUtf8("错误"), err);
        emit exportCompleted(false);
        LOG_FUNCTION_EXIT();
        return;
    }

    QString msg = crafts.isEmpty()
        ? QString::fromUtf8("项目导出成功（仅图形）\n文件夹：%1").arg(projectFolder)
        : QString::fromUtf8("项目导出成功\n文件夹：%1").arg(projectFolder);
    if (crafts.isEmpty()) {
        msg += QString::fromUtf8(
            "\n\nparameters.yaml 仅为 crafts: 空壳；若需保留工艺，请先在工艺表添加工艺后再导出。");
    }
    LOG_INFO(QString::fromUtf8("onExportProject: 导出完成，最终写入工艺数=%1").arg(crafts.size()));
    CustomMessageBox::information(this, QString::fromUtf8("成功"), msg);
    persistLastProjectDir(projectFolder);
    emit exportCompleted(true);
    LOG_FUNCTION_EXIT();
}

void FileControlWidget::onImportDxf()
{
    LOG_FUNCTION_ENTRY();

    if (!m_docModel) {
        CustomMessageBox::warning(this, QString::fromUtf8("错误"), 
                            QString::fromUtf8("文档模型未就绪"));
        LOG_FUNCTION_EXIT();
        return;
    }
    // 使用基于BaseDialog的文件选择对话框（支持多选）
    QString defaultDir;
#ifdef DXF_IMPORT_FIXED_ROOT
    defaultDir = QLatin1String(DXF_IMPORT_FIXED_ROOT);
#else
    defaultDir = QLatin1String("/root");
    if (PathManager::useCustomPaths()) {
        defaultDir = PathManager::getUserFilesDir();
    }
#endif
    {
        CommonDataControll commonDataCtrl;
        if (commonDataCtrl.CheckUSBExist()) {
            defaultDir = QLatin1String("/tmp/usb");
        }
    }
    
    DxfFileSelectDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("选择 DXF / DWG 文件（可多选）"));
    dialog.setFilter(QString::fromLatin1("*.dxf *.dwg"));
    dialog.setDirectory(defaultDir);
    
    if (dialog.exec() != QDialog::Accepted) {
        LOG_FUNCTION_EXIT();
        return;
    }
    
    QStringList filePaths = dialog.getSelectedFiles();
    double offsetX = dialog.getOffsetX();
    double offsetY = dialog.getOffsetY();
    bool autoOffset = dialog.isAutoOffsetEnabled();
    QPointF dxfCenter = dialog.getSelectedDxfCenter();
    
    LOG_INFO(QString("DXF import offset: X=%1, Y=%2").arg(offsetX, 0, 'f', 3).arg(offsetY, 0, 'f', 3));

    if (filePaths.isEmpty()) {
        LOG_FUNCTION_EXIT();
        return;
    }

    if (!autoOffset && filePaths.size() == 1 && m_drawPanel) {
        beginPendingDxfPlacement(filePaths, dxfCenter);
        LOG_FUNCTION_EXIT();
        return;
    }

    importDxfFilesWithOffset(filePaths, offsetX, offsetY);
    LOG_FUNCTION_EXIT();
}

void FileControlWidget::importDxfFilesWithOffset(const QStringList& filePaths, double offsetX, double offsetY)
{
    QElapsedTimer totalTimer;
    QElapsedTimer stageTimer;
    totalTimer.start();
    stageTimer.start();
    QGraphicsScene* scene = workingScene();

    int successCount = 0;
    int failCount = 0;
    QStringList failedFiles;
    
    QSet<int> idsBeforeImport = m_docModel->graphicIds();
    LOG_INFO(QString("Document entity count before import: %1").arg(idsBeforeImport.size()));

    m_docModel->beginImportBatch();
    foreach (const QString& filePath, filePaths) {
        QElapsedTimer fileTimer;
        fileTimer.start();
        const QString suf = QFileInfo(filePath).suffix().toLower();
        const QString fmt = (suf == QLatin1String("dwg")) ? QLatin1String("DWG") : QLatin1String("DXF");
        const bool importOk = m_docModel->requestImportGraphicsFile(filePath, fmt);
        const qint64 fileMs = fileTimer.elapsed();
        if (fileMs >= 100) {
            qWarning() << QString::fromUtf8("[Perf][FileControlWidget.importDxfFilesWithOffset.file] file=%1 ok=%2 elapsed=%3ms")
                          .arg(QFileInfo(filePath).fileName()).arg(importOk ? 1 : 0).arg(fileMs);
        }
        if (importOk) {
            successCount++;
        } else {
            failCount++;
            failedFiles.append(QFileInfo(filePath).fileName());
        }
    }
    const qint64 requestFilesMs = stageTimer.elapsed();
    stageTimer.start();
    m_docModel->endImportBatch();
    const qint64 endBatchMs = stageTimer.elapsed();
    
    // 应用偏移值（如果X或Y偏移值不为0）
    stageTimer.start();
    if ((qAbs(offsetX) > 1e-6 || qAbs(offsetY) > 1e-6) && successCount > 0) {
        QSet<int> idsAfterImport = m_docModel->graphicIds();
        LOG_INFO(QString("Document entity count after import: %1").arg(idsAfterImport.size()));
        
        int offsetAppliedCount = 0;
        QSet<int> newImportedIds;
        QSet<int>::const_iterator importedIt = idsAfterImport.constBegin();
        for (; importedIt != idsAfterImport.constEnd(); ++importedIt) {
            if (!idsBeforeImport.contains(*importedIt)) {
                newImportedIds.insert(*importedIt);
            }
        }
        offsetAppliedCount = m_docModel->requestTranslateEntitiesByGraphicIds(newImportedIds, offsetX, offsetY);

        // 更新场景以显示变化
        if (scene) {
            scene->update();
        }
        
        // 刷新图元表格以显示更新后的坐标
        // 通过查找所有GraphicsItemTableView实例并刷新它们
        // refreshTable()内部会检查场景，所以直接调用即可
        QList<GraphicsItemTableView*> tableViews = this->findChildren<GraphicsItemTableView*>();
        foreach (GraphicsItemTableView* tableView, tableViews) {
            if (tableView) {
                tableView->refreshTable();
            }
        }
        // 也尝试通过父窗口查找
        QWidget* parentWidget = this->parentWidget();
        if (parentWidget) {
            QList<GraphicsItemTableView*> parentTableViews = parentWidget->findChildren<GraphicsItemTableView*>();
            foreach (GraphicsItemTableView* tableView, parentTableViews) {
                if (tableView) {
                    tableView->refreshTable();
                }
            }
        }
        // 通过QApplication查找所有GraphicsItemTableView（最彻底的方法）
        QList<QWidget*> allWidgets = QApplication::allWidgets();
        foreach (QWidget* widget, allWidgets) {
            GraphicsItemTableView* tableView = qobject_cast<GraphicsItemTableView*>(widget);
            if (tableView) {
                tableView->refreshTable();
            }
        }
        
        LOG_INFO(QString("Applied offset to %1 newly imported item(s) (X=%2, Y=%3)")
                 .arg(offsetAppliedCount)
                 .arg(offsetX, 0, 'f', 3)
                 .arg(offsetY, 0, 'f', 3));
    } else {
        if (qAbs(offsetX) <= 1e-6 && qAbs(offsetY) <= 1e-6) {
            LOG_INFO("Offset is zero, skip applying offset");
        } else         if (successCount == 0) {
            LOG_INFO("No file imported successfully, skip applying offset");
        }
    }
    const qint64 offsetMs = stageTimer.elapsed();

    stageTimer.start();
    if (successCount > 0) {
        reloadToolLibraryRadiiAfterGraphicsImport();
    }
    const qint64 reloadToolMs = stageTimer.elapsed();

    // 显示导入结果（使用CustomMessageBox）
    stageTimer.start();
    if (failCount == 0) {
        CustomMessageBox::information(this, QString::fromUtf8("成功"), 
                                QString::fromUtf8("成功导入 %1 个DXF文件").arg(successCount));
        emit dxfImportCompleted(true);
        requestCamAutoSaveNow();
    } else if (successCount > 0) {
        QString message = QString::fromUtf8("部分导入成功：\n成功：%1 个\n失败：%2 个")
                         .arg(successCount).arg(failCount);
        if (!failedFiles.isEmpty()) {
            message += QString::fromUtf8("\n失败的文件：\n%1").arg(failedFiles.join("\n"));
        }
        CustomMessageBox::warning(this, QString::fromUtf8("部分成功"), message);
        emit dxfImportCompleted(true);
        requestCamAutoSaveNow();
    } else {
        QString message = QString::fromUtf8("所有DXF文件导入失败（共 %1 个）").arg(failCount);
        if (!failedFiles.isEmpty()) {
            message += QString::fromUtf8("\n失败的文件：\n%1").arg(failedFiles.join("\n"));
        }
        CustomMessageBox::critical(this, QString::fromUtf8("错误"), message);
        emit dxfImportCompleted(false);
    }
    const qint64 notifyUiMs = stageTimer.elapsed();
    
    // 如果导入成功，自动对焦到导入图形的中心并调整视图以显示所有导入的图形
    // 使用QTimer延迟执行，确保场景已经完全更新
    if (m_drawPanel && successCount > 0) {
        QTimer::singleShot(100, this, SLOT(onFocusImportedItems()));
    }
    const qint64 totalMs = totalTimer.elapsed();
    if (totalMs >= 100 || !filePaths.isEmpty()) {
        qWarning() << QString::fromUtf8("[Perf][FileControlWidget.importDxfFilesWithOffset] files=%1 success=%2 fail=%3 total=%4ms requestFiles=%5ms endBatch=%6ms offset=%7ms reloadTool=%8ms notifyUi=%9ms")
                      .arg(filePaths.size()).arg(successCount).arg(failCount).arg(totalMs)
                      .arg(requestFilesMs).arg(endBatchMs).arg(offsetMs).arg(reloadToolMs).arg(notifyUiMs);
    }
    
    LOG_FUNCTION_EXIT();
}

void FileControlWidget::beginPendingDxfPlacement(const QStringList& filePaths, const QPointF& dxfCenter)
{
    cancelPendingDxfPlacement();
    m_pendingDxfPlacementFiles = filePaths;
    m_pendingDxfPlacementCenter = dxfCenter;
    if (!filePaths.isEmpty()) {
        QPointF previewCenter;
        m_pendingDxfPlacementPath = buildDxfPlacementPreviewPath(filePaths.first(), &previewCenter);
        if (!m_pendingDxfPlacementPath.isEmpty()) {
            m_pendingDxfPlacementCenter = previewCenter;
        }
    }
    if (m_drawPanel) {
        connect(m_drawPanel, SIGNAL(mouseClicked(QPointF,Qt::MouseButton)),
                this, SLOT(onPendingDxfPlacementClicked(QPointF,Qt::MouseButton)));
        connect(m_drawPanel, SIGNAL(mouseMoved(QPointF)),
                this, SLOT(onPendingDxfPlacementMoved(QPointF)));
        m_drawPanel->setInputMode(CrossHairView::ModeSingleSelect);
        m_drawPanel->setDragMode(QGraphicsView::NoDrag);
    }
}

void FileControlWidget::finishPendingDxfPlacement(const QPointF& scenePos)
{
    if (m_pendingDxfPlacementFiles.isEmpty()) {
        return;
    }
    QStringList files = m_pendingDxfPlacementFiles;
    QPointF center = m_pendingDxfPlacementCenter;
    cancelPendingDxfPlacement();
    double offsetX = scenePos.x() - center.x();
    double offsetY = scenePos.y() - center.y();
    importDxfFilesWithOffset(files, offsetX, offsetY);
}

void FileControlWidget::cancelPendingDxfPlacement()
{
    if (m_drawPanel) {
        disconnect(m_drawPanel, SIGNAL(mouseClicked(QPointF,Qt::MouseButton)),
                   this, SLOT(onPendingDxfPlacementClicked(QPointF,Qt::MouseButton)));
        disconnect(m_drawPanel, SIGNAL(mouseMoved(QPointF)),
                   this, SLOT(onPendingDxfPlacementMoved(QPointF)));
    }
    if (m_pendingDxfPreviewItem) {
        delete m_pendingDxfPreviewItem;
        m_pendingDxfPreviewItem = NULL;
    }
    m_pendingDxfPlacementFiles.clear();
    m_pendingDxfPlacementCenter = QPointF();
    m_pendingDxfPlacementPath = QPainterPath();
}

void FileControlWidget::updatePendingDxfPreview(const QPointF& scenePos)
{
    QGraphicsScene* scene = workingScene();
    if (!scene || m_pendingDxfPlacementFiles.isEmpty()) {
        return;
    }
    QPainterPath path;
    if (!m_pendingDxfPlacementPath.isEmpty()) {
        path = m_pendingDxfPlacementPath.translated(scenePos.x() - m_pendingDxfPlacementCenter.x(),
                                                    scenePos.y() - m_pendingDxfPlacementCenter.y());
    } else {
        const double halfSize = 12.0;
        path.addRect(scenePos.x() - halfSize, scenePos.y() - halfSize, halfSize * 2.0, halfSize * 2.0);
        path.moveTo(scenePos.x() - halfSize * 1.5, scenePos.y());
        path.lineTo(scenePos.x() + halfSize * 1.5, scenePos.y());
        path.moveTo(scenePos.x(), scenePos.y() - halfSize * 1.5);
        path.lineTo(scenePos.x(), scenePos.y() + halfSize * 1.5);
    }
    if (!m_pendingDxfPreviewItem) {
        m_pendingDxfPreviewItem = scene->addPath(path, QPen(QColor(255, 140, 0), 0, Qt::DashLine));
        m_pendingDxfPreviewItem->setZValue(1000000.0);
    } else {
        m_pendingDxfPreviewItem->setPath(path);
    }
}

void FileControlWidget::onPendingDxfPlacementClicked(QPointF scenePos, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        finishPendingDxfPlacement(scenePos);
    } else if (button == Qt::RightButton) {
        cancelPendingDxfPlacement();
    }
}

void FileControlWidget::onPendingDxfPlacementMoved(QPointF scenePos)
{
    updatePendingDxfPreview(scenePos);
}

QPainterPath FileControlWidget::buildDxfPlacementPreviewPath(const QString& filePath, QPointF* centerOut) const
{
    QPainterPath path;
    DxfGraphicsImporter importer;
    QList<DxfGraphicsImporter::PolygonData> polygonsData;
    QMap<QString, QPair<int, int> > layerColorMap;
    double originX = 0.0;
    double originY = 0.0;
    double originZ = 0.0;
    const bool isDwg = QFileInfo(filePath).suffix().compare(QLatin1String("dwg"), Qt::CaseInsensitive) == 0;
    const bool parsed = isDwg ? importer.parseDwgFile(filePath, polygonsData, layerColorMap, &originX, &originY, &originZ)
                              : importer.parseDxfFile(filePath, polygonsData, layerColorMap, &originX, &originY, &originZ);
    if (!parsed) {
        return path;
    }

    for (int i = 0; i < polygonsData.size(); ++i) {
        const DxfGraphicsImporter::PolygonData& data = polygonsData.at(i);
        if (data.polygon.isEmpty()) {
            continue;
        }
        QPolygonF scenePoly;
        for (int j = 0; j < data.polygon.size(); ++j) {
            const QPointF p = data.polygon.at(j);
            scenePoly << QPointF(p.x() + originX, -(p.y() + originY));
        }
        if (scenePoly.isEmpty()) {
            continue;
        }
        path.moveTo(scenePoly.first());
        for (int j = 1; j < scenePoly.size(); ++j) {
            path.lineTo(scenePoly.at(j));
        }
        if (data.isClosed && scenePoly.size() > 2) {
            path.closeSubpath();
        }
    }

    if (centerOut && !path.isEmpty()) {
        *centerOut = path.boundingRect().center();
    }
    return path;
}

QString FileControlWidget::findProjectFile(const QString& folderPath, 
                                          const QString& extension, 
                                          const QString& defaultName) const
{
    LOG_FUNCTION_ENTRY();
    QDir dir(folderPath);
    
    // 优先查找默认名称的文件
    if (!defaultName.isEmpty()) {
        QString defaultPath = dir.filePath(defaultName);
        if (QFileInfo(defaultPath).exists()) {
            LOG_FUNCTION_EXIT();
            return defaultPath;
        }
    }
    
    // 查找所有匹配扩展名的文件
    QStringList filters;
    filters << QString("*.%1").arg(extension);
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    if (files.isEmpty()) {
        LOG_FUNCTION_EXIT();
        return QString();
    }
    
    // 如果只有一个文件，直接返回
    if (files.size() == 1) {
        LOG_FUNCTION_EXIT();
        return files.first().absoluteFilePath();
    }
    
    // 多个文件时，让用户选择（这里简化处理，返回第一个）
    // TODO: 可以实现文件选择对话框
    LOG_FUNCTION_EXIT();
    return files.first().absoluteFilePath();
}

QList<ICraft*> FileControlWidget::getAllCrafts() const
{
    LOG_FUNCTION_ENTRY();
    QList<ICraft*> crafts;
    
    if (!m_craftView) {
        LOG_FUNCTION_EXIT();
        return crafts;
    }
    
    QAbstractItemModel* abstractModel = m_craftView->model();
    CraftTreeModel* model = qobject_cast<CraftTreeModel*>(abstractModel);
    if (!model) {
        LOG_FUNCTION_EXIT();
        return crafts;
    }
    
    // 必须使用模型内部列表：重建树时若某工艺暂无图元 ID 被跳过，仍须在工程保存/GCode 等逻辑中出现
    crafts = model->getAllCrafts();
    
    LOG_FUNCTION_EXIT();
    return crafts;
}

void FileControlWidget::clearAllData()
{
    LOG_FUNCTION_ENTRY();

    if (!m_docModel) {
        LOG_ERROR(QString::fromUtf8("clearAllData: CadDocumentModel 未就绪，已拒绝执行"));
        LOG_FUNCTION_EXIT();
        return;
    }

    LOG_INFO(QString::fromUtf8("clearAllData: emit beforeClearAllData（画圆/吸附/复制后导入须先复位控制器状态）"));
    emit beforeClearAllData();

    QGraphicsScene* scene = workingScene();
    const int graphicCountBefore = m_docModel->graphicIds().size();
    const bool isLayered = scene && qobject_cast<LayeredScene*>(scene);
    LOG_INFO(QString::fromUtf8("clearAllData: 开始；清工艺/文档前 graphicIds=%1，LayeredScene=%2")
             .arg(graphicCountBefore).arg(isLayered ? QString::fromUtf8("是") : QString::fromUtf8("否")));
    
    // 先清空工艺模型，避免视图仍持有对旧工艺/图形ID的引用时再清场景导致崩溃
    if (m_craftView) {
        QAbstractItemModel* abstractModel = m_craftView->model();
        CraftTreeModel* model = qobject_cast<CraftTreeModel*>(abstractModel);
        if (model) {
            model->clear();
        }
    }
    LOG_INFO(QString::fromUtf8("clearAllData: 工艺模型已 clear"));
    
    m_docModel->requestClearDocument();
    if (scene) {
        LayeredScene* lay = qobject_cast<LayeredScene*>(scene);
        if (lay) {
            lay->resetRendererStateAfterDocumentClear();
        }
        LOG_INFO(QString::fromUtf8("clearAllData: renderer state reset 后 graphicIds=%1")
                 .arg(m_docModel->graphicIds().size()));
    }

    LOG_FUNCTION_EXIT();
}

void FileControlWidget::initializeSessionPersistence()
{
    if (!PathManager::useCustomPaths())
        return;

    if (!m_autoSaveTimer) {
        m_autoSaveTimer = new QTimer(this);
        m_autoSaveTimer->setInterval(60000);
        connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(onAutoSaveTimer()));
    }
    m_autoSaveTimer->start();

    QTimer::singleShot(150, this, SLOT(tryRestoreLastProjectDeferred()));
}

bool FileControlWidget::hasExportableContent() const
{
    if (m_docModel && !m_docModel->graphicIds().isEmpty()) {
        return true;
    }
    return !getAllCrafts().isEmpty();
}

bool FileControlWidget::saveProjectToFolder(const QString& projectFolder, QString* errOut)
{
    if (!m_docModel) {
        if (errOut)
            *errOut = QString::fromUtf8("文档模型未就绪");
        return false;
    }
    QDir d;
    if (!d.exists(projectFolder)) {
        if (!d.mkpath(projectFolder)) {
            if (errOut)
                *errOut = QString::fromUtf8("无法创建目录");
            return false;
        }
    }
    QList<ICraft*> crafts = getAllCrafts();
    LOG_INFO(QString::fromUtf8("saveProjectToFolder: 将导出工艺数=%1，目录=%2")
             .arg(crafts.size()).arg(projectFolder));
    const QString dxfPath = QDir(projectFolder).filePath(AppFileLayout::projectGraphicsDxfFileName());
    const QString yamlPath = QDir(projectFolder).filePath(AppFileLayout::projectParametersYamlFileName());
    DataManager dataManager;
    const bool dxfOk = (m_docModel && !m_docModel->dxfGeometryBatches().isEmpty())
        ? m_docModel->exportDxfFromDocumentGeometry(dxfPath)
        : false;
    if (!dxfOk) {
        if (errOut)
            *errOut = QString::fromUtf8("导出 DXF 失败");
        return false;
    }
    if (!dataManager.exportParameters(crafts, yamlPath, "YAML")) {
        if (errOut)
            *errOut = QString::fromUtf8("导出 YAML 失败");
        return false;
    }
    LOG_INFO(QString::fromUtf8("saveProjectToFolder: 完成 DXF+YAML，YAML=%1").arg(yamlPath));
    return true;
}

bool FileControlWidget::importProjectFromFolderImpl(const QString& projectFolder, bool silent)
{
    LOG_FUNCTION_ENTRY();
    if (!m_docModel) {
        if (!silent) {
            CustomMessageBox::critical(this, QString::fromUtf8("错误"),
                                 QString::fromUtf8("文档模型未就绪"));
        } else {
            LOG_ERROR(QString::fromUtf8("Restore project failed: CadDocumentModel not ready"));
        }
        LOG_FUNCTION_EXIT();
        return false;
    }

    const QString dxfPath = findProjectFile(projectFolder, "dxf", AppFileLayout::projectGraphicsDxfFileName());
    const QString yamlPath = findProjectFile(projectFolder, "yaml", AppFileLayout::projectParametersYamlFileName());

    if (dxfPath.isEmpty()) {
        if (!silent) {
            CustomMessageBox::warning(this, QString::fromUtf8("错误"),
                                QString::fromUtf8("在项目文件夹中未找到DXF文件"));
        } else {
            LOG_WARNING(QString::fromUtf8("Restore project failed, no DXF: %1").arg(projectFolder));
        }
        LOG_FUNCTION_EXIT();
        return false;
    }

    clearAllData();
    // 勿在此处 processEvents：craft_control.log 曾出现第三次导入在 clear 后、importGraphics 前无后续日志，
    // 疑为事件循环重入表格/图层导致崩溃；leaveClearDeferralMode 已保证不再依赖 QTimer(0) 入场景。
    LOG_INFO(QString::fromUtf8("导入项目：clearAllData 完成，开始导入 DXF"));

    DataManager dataManager;
    QList<ICraft*> crafts;

    const QString graphicsSuf = QFileInfo(dxfPath).suffix().toLower();
    const QString graphicsFmt = (graphicsSuf == QLatin1String("dwg")) ? QLatin1String("DWG") : QLatin1String("DXF");
    const bool graphicsImported = m_docModel->requestImportGraphicsFile(dxfPath, graphicsFmt);
    if (!graphicsImported) {
        if (!silent) {
            CustomMessageBox::critical(this, QString::fromUtf8("错误"),
                                 QString::fromUtf8("导入图形失败"));
        } else {
            LOG_ERROR(QString::fromUtf8("Restore project: graphics import failed: %1").arg(projectFolder));
        }
        emit importCompleted(false);
        LOG_FUNCTION_EXIT();
        return false;
    }

    const int graphicIdsAfterDxf = m_docModel->graphicIds().size();
    LOG_INFO(QString::fromUtf8("导入项目: DXF 已写入文档，graphicIds=%1，文件=%2")
             .arg(graphicIdsAfterDxf).arg(dxfPath));

    if (!yamlPath.isEmpty()) {
        LOG_INFO(QString::fromUtf8("导入项目: 开始读 YAML，路径=%1").arg(yamlPath));
        if (!dataManager.importParameters(yamlPath, crafts)) {
            LOG_WARNING(QString::fromUtf8("导入项目: importParameters 返回 false， crafts.size()=%1").arg(crafts.size()));
            if (!silent) {
                CustomMessageBox::warning(this, QString::fromUtf8("提示"),
                                    QString::fromUtf8("导入工艺参数失败，仅导入图形"));
            } else {
                LOG_WARNING(QString::fromUtf8("Restore project: YAML import failed, graphics only: %1").arg(projectFolder));
            }
        } else {
            LOG_INFO(QString::fromUtf8("导入项目: importParameters 成功，解析到工艺数=%1").arg(crafts.size()));
            if (m_craftView) {
                QAbstractItemModel* abstractModel = m_craftView->model();
                CraftTreeModel* model = qobject_cast<CraftTreeModel*>(abstractModel);
                if (model) {
                    foreach (ICraft* craft, crafts) {
                        model->addCraft(craft);
                    }
                    LOG_INFO(QString::fromUtf8("导入项目: 已 addCraft 到树，模型内工艺数=%1").arg(model->getAllCrafts().size()));
                } else {
                    LOG_WARNING(QString::fromUtf8("导入项目: m_craftView 无 CraftTreeModel，工艺未挂到树"));
                }
            } else {
                LOG_WARNING(QString::fromUtf8("导入项目: m_craftView 为空，工艺未挂到树"));
            }
        }
    } else {
        LOG_INFO(QString::fromUtf8("导入项目: 未找到 YAML，跳过工艺（仅图形）"));
    }

    if (m_docModel && !crafts.isEmpty()) {
        QSet<int> yamlIds;
        for (int ci = 0; ci < crafts.size(); ++ci) {
            ICraft* c = crafts[ci];
            if (!c) {
                continue;
            }
            const QList<int> tg = c->getTargetGraphicIds();
            for (int j = 0; j < tg.size(); ++j) {
                if (tg[j] > 0) {
                    yamlIds.insert(tg[j]);
                }
            }
        }
        const QSet<int> documentIds = m_docModel->graphicIds();
        QSet<int> missing;
        foreach (int yid, yamlIds) {
            if (!documentIds.contains(yid)) {
                missing.insert(yid);
            }
        }
        if (!missing.isEmpty()) {
            QList<int> ml = missing.toList();
            qSort(ml);
            QStringList parts;
            const int cap = 80;
            for (int i = 0; i < ml.size() && i < cap; ++i) {
                parts << QString::number(ml[i]);
            }
            const QString tail = (ml.size() > cap) ? QString::fromUtf8(" ...") : QString();
            LOG_WARNING(QString::fromUtf8("[ProjectImport] parameters.yaml 引用但文档中不存在的 graphicId 共 %1 个: %2%3")
                        .arg(missing.size()).arg(parts.join(QLatin1String(","))).arg(tail));
        }
        LOG_INFO(QString::fromUtf8("[ProjectImport] YAML 引用 distinct graphicId 数=%1，文档 distinct formal id 数=%2")
                 .arg(yamlIds.size()).arg(documentIds.size()));
    }

    if (!yamlPath.isEmpty() && crafts.isEmpty()) {
        const qint64 yamlBytes = QFileInfo(yamlPath).size();
        LOG_INFO(QString::fromUtf8("导入项目: YAML 路径存在但工艺=0，parameters.yaml 大小=%1 字节。"
                                   " 与「导出时工艺列表为空」生成的文件一致时，属数据问题而非导入崩溃。")
                 .arg(yamlBytes));
    }

    if (!silent) {
        QString msg = crafts.isEmpty()
            ? QString::fromUtf8("项目导入成功（仅图形）")
            : QString::fromUtf8("项目导入成功");
        if (crafts.isEmpty() && !yamlPath.isEmpty()) {
            const qint64 yamlBytes = QFileInfo(yamlPath).size();
            if (yamlBytes <= 80) {
                msg += QString::fromUtf8(
                    "\n\nparameters.yaml 几乎为空（仅含 crafts: 头），"
                    "与导出时工艺表为空生成的文件一致；请先在工艺表中添加工艺后再导出工程。");
            }
        }
        LOG_INFO(QString::fromUtf8("导入项目: 即将弹出成功提示框，工艺数=%1").arg(crafts.size()));
        CustomMessageBox::information(this, QString::fromUtf8("成功"), msg);
        LOG_INFO(QString::fromUtf8("导入项目: 成功提示框已关闭"));
    } else {
        LOG_INFO(QString::fromUtf8("Project restored: %1").arg(projectFolder));
    }

    persistLastProjectDir(projectFolder);
    reloadToolLibraryRadiiAfterGraphicsImport();
    LOG_INFO(QString::fromUtf8("导入项目: emit importCompleted(true)，requestCamAutoSaveNow"));
    emit importCompleted(true);
    requestCamAutoSaveNow();

    if (m_drawPanel)
        QTimer::singleShot(100, this, SLOT(onFocusImportedItems()));

    LOG_FUNCTION_EXIT();
    return true;
}

void FileControlWidget::persistLastProjectDir(const QString& dir)
{
    if (dir.isEmpty())
        return;
    const QString iniPath = AppFileLayout::lastCamProjectIniWritePath();
    if (iniPath.isEmpty())
        return;
    QSettings s(iniPath, QSettings::IniFormat);
    s.beginGroup(QString::fromLatin1(kSessionGroup));
    s.setValue(QString::fromLatin1(kLastProjectDirKey), dir);
    s.endGroup();
    s.sync();
}

QString FileControlWidget::readPersistedLastProjectDir() const
{
    const QString iniPath = AppFileLayout::lastCamProjectIniPath();
    if (iniPath.isEmpty() || !QFile::exists(iniPath))
        return QString();
    QSettings s(iniPath, QSettings::IniFormat);
    s.beginGroup(QString::fromLatin1(kSessionGroup));
    const QString v = s.value(QString::fromLatin1(kLastProjectDirKey)).toString();
    s.endGroup();
    return v;
}

void FileControlWidget::onAutoSaveTimer()
{
    performCamAutoSave();
}

void FileControlWidget::requestCamAutoSaveDeferred()
{
    QTimer::singleShot(0, this, SLOT(requestCamAutoSaveNow()));
}

void FileControlWidget::requestCamAutoSaveNow()
{
    performCamAutoSave();
}

void FileControlWidget::onGCodeConversionFinishedForAutoSave(bool success)
{
    if (success)
        performCamAutoSave();
}

void FileControlWidget::performCamAutoSave()
{
    if (!workingScene() || !PathManager::useCustomPaths())
        return;

    const QString folder = AppFileLayout::camAutosaveDirPath();
    if (folder.isEmpty())
        return;
    if (!hasExportableContent())
        return;

    PathManager::ensureDirExists(folder);
    QString err;
    if (!saveProjectToFolder(folder, &err)) {
        LOG_WARNING(QString::fromUtf8("CAM auto-save failed: %1").arg(err));
        return;
    }
    LOG_DEBUG(QString::fromUtf8("CAM auto-save: %1").arg(folder));
}

void FileControlWidget::tryRestoreLastProjectDeferred()
{
    if (!PathManager::useCustomPaths() || !workingScene())
        return;

    const QString lastDir = readPersistedLastProjectDir();
    if (!lastDir.isEmpty() && QFileInfo(lastDir).isDir()) {
        const QString dxfPath = findProjectFile(lastDir, "dxf", AppFileLayout::projectGraphicsDxfFileName());
        if (!dxfPath.isEmpty()) {
            if (importProjectFromFolderImpl(lastDir, true))
                return;
        }
    }

    const QString autoDir = AppFileLayout::camAutosaveDirPath();
    if (!autoDir.isEmpty() && QFileInfo(autoDir).isDir()) {
        const QString dxfPath = findProjectFile(autoDir, "dxf", AppFileLayout::projectGraphicsDxfFileName());
        if (!dxfPath.isEmpty())
            importProjectFromFolderImpl(autoDir, true);
    }
}

void FileControlWidget::onFocusImportedItems()
{
    LOG_FUNCTION_ENTRY();
    
    // 确保场景已经更新（处理所有待处理的事件）
    QApplication::processEvents();
    
    // 检查场景中是否有图形项
    const int documentGraphicCount = m_docModel ? m_docModel->graphicIds().size() : 0;
    if (documentGraphicCount > 0) {
        LOG_INFO(QString("Fitting view to document, %1 graphics item(s)").arg(documentGraphicCount));
        
        // 先调整视图以适应所有图形
        if (m_drawPanel) {
            m_drawPanel->zoomFit();
            
            // 再次处理事件，确保视图更新
            QApplication::processEvents();
            
            // 然后将视图中心对准导入图形的边界框中心（确保对焦）
            m_drawPanel->centerOnItems();
            
            LOG_INFO(QString("View fit completed"));
        }
    } else {
        LOG_WARNING(QString("Document is empty or has no graphics items, cannot fit view"));
    }
    
    LOG_FUNCTION_EXIT();
}

QGraphicsScene* FileControlWidget::workingScene() const
{
    return m_scene;
}
