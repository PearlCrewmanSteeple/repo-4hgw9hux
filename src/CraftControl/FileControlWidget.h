#ifndef FILECONTROLWIDGET_H
#define FILECONTROLWIDGET_H

#include <QWidget>
#include <QList>
#include <QStringList>
#include <QPointF>
#include <QPainterPath>

class QTimer;
class QGraphicsScene;
class CraftTableView;
class CraftToolBar;
class DrawPanel;
class CraftTreeModel;
class ICraft;
class CadDocumentModel;
class QGraphicsPathItem;

namespace Ui {
class FileControlWidget;
}

/**
 * @brief 文件导入导出控制Widget
 * 提供项目导入、导出和G代码转换功能
 */
class FileControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileControlWidget(QWidget *parent = NULL);
    ~FileControlWidget();

    // 设置场景和工艺视图（用于获取数据）
    void setScene(QGraphicsScene* scene);
    void setDocumentModel(CadDocumentModel* docModel);
    void setCraftTableView(CraftTableView* craftView);
    void setCraftToolBar(CraftToolBar* craftToolBar);
    void setDrawPanel(DrawPanel* drawPanel);

    /**
     * @brief 库构建下：启动 CAM 自动保存定时器，并延迟尝试恢复上次/自动保存的项目
     * 需在 setScene / setCraftTableView 之后调用（由主窗口初始化末尾调用）
     */
    void initializeSessionPersistence();

public slots:
    /** 下一事件循环再执行 CAM 自动保存（用于对齐等批量改图后，等场景刷新完毕） */
    void requestCamAutoSaveDeferred();
    /** 立即执行与定时器相同的 CAM 自动保存（需 useCustomPaths 且有可导出内容） */
    void requestCamAutoSaveNow();
    /** 工艺栏 G 代码转换完成时由 CraftTableControlBar 连接，成功则触发一次自动保存 */
    void onGCodeConversionFinishedForAutoSave(bool success);

signals:
    /// 在清空工艺与场景之前发出，供 CanvasController 等复位预览/复制/吸附状态（避免画圆后导入悬空指针崩溃）
    void beforeClearAllData();
    void importCompleted(bool success);
    void exportCompleted(bool success);
    void gCodeConversionCompleted(bool success);
    void dxfImportCompleted(bool success);
    /** 请求开始「对齐到寄存器」流程（由主窗口连接到 GraphicsItemTableView） */
    void alignToRegisterRequested();
    /** 应用配置对话框中小屏幕模式开关变化 */
    void smallScreenModeChanged(bool enabled);

private slots:
    void onImportProject();
    void onExportProject();
    void onImportDxf();
    void onAlignRegisterClicked();
    void onFocusImportedItems();  // 延迟对焦导入的图形项
    void onToolLibraryClicked();
    void onAppConfigClicked();
    void onAutoSaveTimer();
    void tryRestoreLastProjectDeferred();
    void onPendingDxfPlacementClicked(QPointF scenePos, Qt::MouseButton button);
    void onPendingDxfPlacementMoved(QPointF scenePos);

private:
    void performCamAutoSave();
    // 查找项目文件夹中的文件
    QString findProjectFile(const QString& folderPath, const QString& extension, const QString& defaultName = QString()) const;
    
    // 获取所有工艺对象
    QList<ICraft*> getAllCrafts() const;
    
    // 清空场景和工艺模型
    void clearAllData();

    bool hasExportableContent() const;
    bool saveProjectToFolder(const QString& projectFolder, QString* errOut);
    bool importProjectFromFolderImpl(const QString& projectFolder, bool silent);
    void persistLastProjectDir(const QString& dir);
    QString readPersistedLastProjectDir() const;
    QGraphicsScene* workingScene() const;
    void importDxfFilesWithOffset(const QStringList& filePaths, double offsetX, double offsetY);
    void beginPendingDxfPlacement(const QStringList& filePaths, const QPointF& dxfCenter);
    void finishPendingDxfPlacement(const QPointF& scenePos);
    void cancelPendingDxfPlacement();
    void updatePendingDxfPreview(const QPointF& scenePos);
    QPainterPath buildDxfPlacementPreviewPath(const QString& filePath, QPointF* centerOut) const;

    Ui::FileControlWidget *ui;
    QGraphicsScene* m_scene;
    CadDocumentModel* m_docModel;
    CraftTableView* m_craftView;
    CraftToolBar* m_craftToolBar;
    DrawPanel* m_drawPanel;
    QTimer* m_autoSaveTimer;
    QStringList m_pendingDxfPlacementFiles;
    QPointF m_pendingDxfPlacementCenter;
    QPainterPath m_pendingDxfPlacementPath;
    QGraphicsPathItem* m_pendingDxfPreviewItem;
};

#endif // FILECONTROLWIDGET_H
