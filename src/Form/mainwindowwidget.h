#ifndef MAINWINDOWWIDGET_H
#define MAINWINDOWWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QShowEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QSystemTrayIcon>
#include "BaseDialog.h"
#include <cmath>

class QButtonGroup;
class QTimer;
class CanvasController;
class CadDocumentInterface;
class CadDocumentModel;
class DocumentUiCoordinator;
class FloatingBallWidget;
class CanvasOverlayRenderer;
class CanvasPreviewManager;
class CanvasSceneRenderer;
class CanvasSceneSynchronizer;
class QGraphicsItem;
class QGraphicsLineItem;
class QPushButton;
class QStackedWidget;
class QToolButton;
template <typename T> class QList;
namespace Ui {
class MainWindowWidget;
}

class MainWindowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindowWidget(QWidget *parent = 0);
    ~MainWindowWidget();

    void initGraphics();

protected:
    void showEvent(QShowEvent *event);
    void changeEvent(QEvent *e);
    void keyPressEvent(QKeyEvent *event);

signals:
    /** 形状栏+复制+面板工具在同一互斥组内切换时发出一次（id 见 mainwindowwidget.cpp 中 ExclusiveToolGroupId） */
    void exclusiveWorkspaceModeChanged(int exclusiveGroupId);

private slots:
    void onFloatingBallRestoreRequested();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    /** 先显示球再最小化，避免在 changeEvent(WindowMinimized) 里操作球触发 X11 BadDrawable */
    void onMainWindowMinimizeClicked();
    /** 主程序外壳标题栏「最大化」：对顶层 window() 切换，并同步 BaseDialog 按钮图标 */
    void onShellToggleMaximizeClicked();
    void scheduleApplyExclusiveToolState();
    void applyExclusiveToolState();
    /** Qt4 独占组内再点当前键不会取消选中；改为非 exclusive，在勾选时手动互斥 */
    void onExclusiveMemberToggled(bool checked);
    /** 工艺侧栏武装某工艺时：退出形状/复制绘图类工具，保留面板选择模式（单选/框选/多选等） */
    void onCraftArmChanged(const QString& craftType);
    /** 画布在单选/框选/多选/同参选下按右键：回到单选 */
    void onCanvasRequestSwitchToSingleSelectMode();
    void schedulePersistMainLayout();
    void onPersistMainLayoutTimer();
    void onSmallScreenModeChanged(bool enabled);
    void onTopBarToggleClicked();
    void onHShapeBarButtonToggled(bool checked);
    void onHCraftBarButtonToggled(bool checked);
    void onCraftToolBarRebuilt();

private:
    // 旋转复制（三点式）：first=旋转基点，second=基准方向点，third=目标方向点
    inline bool hasRotateFirst() const { return m_rotateHasFirst; }
    inline bool hasRotateSecond() const { return m_rotateHasSecond; }
    inline void resetRotateClicks() { m_rotateHasFirst = m_rotateHasSecond = false; }
    inline qreal angleRadFrom(const QPointF& a, const QPointF& b) const {
        return std::atan2(b.y() - a.y(), b.x() - a.x());
    }
    void initConections();
    void initCraftView();
    void initGraphicsItemTableView();
    void initFileControlWidget();
    void initDocumentUiCoordinator();
    void ensureFloatingBall();
    void setupExclusiveToolGroup();
    void initMainLayoutPersistence();
    void restoreMainLayoutFromSettings();
    void persistMainLayoutSnapshot();
    void initSmallScreenMode();
    void applySmallScreenMode(bool enabled);
    void syncHShapeBarFromVertical();
    void rebuildHorizontalCraftBar();
    Ui::MainWindowWidget *ui;
    CanvasController *pCanvasCtl;
    CadDocumentModel *m_docModel;
    CadDocumentInterface *m_docInterface;
    CanvasSceneRenderer *m_canvasSceneRenderer;
    CanvasPreviewManager *m_canvasPreviewManager;
    CanvasOverlayRenderer *m_canvasOverlayRenderer;
    CanvasSceneSynchronizer *m_canvasSceneSynchronizer;
    DocumentUiCoordinator *m_docUi;
    // 三点式旋转的前两点
    QPointF m_rotateFirstPoint;   // 旋转基点（first）
    QPointF m_rotateSecondPoint;  // 基准方向点（second）
    bool m_rotateHasFirst;
    bool m_rotateHasSecond;
    FloatingBallWidget *m_floatBall;
    QPoint m_cachedFloatBallAnchor;
    QSystemTrayIcon *m_trayIcon;
    QButtonGroup *m_exclusiveToolGroup;
    QTimer* m_layoutPersistTimer;
    bool m_mainLayoutRestored;
    // 小屏幕模式
    QWidget* m_smallScreenTopBar;
    QToolButton* m_topBarToggleBtn;
    QStackedWidget* m_topBarStack;
    QWidget* m_horizontalShapeBar;
    QWidget* m_horizontalCraftBar;
    QList<QAbstractButton*> m_hShapeButtons;
    QList<QAbstractButton*> m_hCraftButtons;
    bool m_smallScreenActive;
    int m_topBarCurrentPage;  // 0=filebar, 1=shapebar, 2=craftbar
};

#endif // MAINWINDOWWIDGET_H
