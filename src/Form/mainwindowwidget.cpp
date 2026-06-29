#include "mainwindowwidget.h"
#include "ui_mainwindowwidget.h"
#include "FloatingBallWidget.h"
#include "DrawPanel/drawpanel.h"
#include "CanvasController.h"
#include "common.h"
#include "CraftControl/CraftTableView.h"
#include "CraftControl/CraftTableViewWidget.h"
#include "CraftControl/CraftToolBar.h"
#include "CraftControl/CraftTableControlBar.h"
#include "CraftControl/FileControlWidget.h"
#include "GraphicsItemTableView.h"
#include "GraphicsItemTableControlBar.h"
#include "DocumentUiCoordinator.h"
#include "layeredscene.h"
#include "CadModel/CadDocumentInterface.h"
#include "ViewControl/CanvasOverlayRenderer.h"
#include "ViewControl/CanvasPreviewManager.h"
#include "ViewControl/CanvasSceneRenderer.h"
#include "ViewControl/CanvasSceneSynchronizer.h"
#include "CraftControl/GCodePreviewHelper.h"
#include <QDebug>
#include <QTimer>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QPushButton>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsPathItem>
#include "DrawPanel/DrawSet/StrokeHitPathItem.h"
#include "DrawPanel/DrawSet/StrokeHitPolygonItem.h"
#include "DrawPanel/DrawSet/StrokeHitEllipseItem.h"
#include "DrawPanel/DrawSet/CanvasGraphicsIdItem.h"
#include "CadModel/CadDocumentModel.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>
#include <QList>
#include <cmath>
#include "Logger.h"
#include "AppUiSettings.h"
#include "Form/PersistingSplitter.h"
#include "Form/PanelToolWidget.h"
#include "Form/ShapeToolBar.h"
#include "Form/FormUnderShape.h"
#include <QButtonGroup>
#include <QAbstractButton>
#include <QSplitter>
#include "Form/FormLayerPop.h"
#include <QStackedWidget>
#include <QToolButton>
#include <QHBoxLayout>

namespace {

enum ExclusiveToolGroupId {
    EXGID_LINE = 1,
    EXGID_CIRCLE,
    EXGID_ELLIPSE,
    EXGID_ARC,
    EXGID_FACE,
    EXGID_MARK,
    EXGID_TEXT,
    EXGID_COPY_OFFSET,
    EXGID_COPY_ROTATE,
    EXGID_COPY_ARRAY,
    EXGID_PANEL_SELECT = 32,
    EXGID_PANEL_BOX,
    EXGID_PANEL_MULTI,
    EXGID_PANEL_SELECT_SAME,
};
}

MainWindowWidget::MainWindowWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindowWidget),
    pCanvasCtl(NULL),
    m_docModel(NULL),
    m_docInterface(NULL),
    m_canvasSceneRenderer(NULL),
    m_canvasPreviewManager(NULL),
    m_canvasOverlayRenderer(NULL),
    m_canvasSceneSynchronizer(NULL),
    m_docUi(NULL),
    m_rotateHasFirst(false),
    m_rotateHasSecond(false),
    m_floatBall(NULL),
    m_cachedFloatBallAnchor(100, 100),
    m_trayIcon(NULL),
    m_exclusiveToolGroup(NULL),
    m_layoutPersistTimer(NULL),
    m_mainLayoutRestored(false),
    m_smallScreenTopBar(NULL),
    m_topBarToggleBtn(NULL),
    m_topBarStack(NULL),
    m_horizontalShapeBar(NULL),
    m_smallScreenActive(false),
    m_topBarCurrentPage(0),
    m_horizontalCraftBar(NULL)
{
    if (!parent)
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    ui->setupUi(this);

#ifdef USE_TRAY_FOR_MINIMIZE
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/IconFile/Logo.png"));
    m_trayIcon->setToolTip(QString::fromUtf8("主窗口 - 点击恢复"));
    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(onTrayActivated(QSystemTrayIcon::ActivationReason)));
    if (QSystemTrayIcon::isSystemTrayAvailable())
        m_trayIcon->show();
#endif

    initGraphics();
    initCraftView();
    initGraphicsItemTableView();
    initFileControlWidget();
    initDocumentUiCoordinator();
    initMainLayoutPersistence();
    initSmallScreenMode();
    initConections();
}

void MainWindowWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 使用屏幕坐标，这样在作为 lib 嵌入时也能正确计算球的位置
    m_cachedFloatBallAnchor = mapToGlobal(rect().bottomRight());
    if (!m_mainLayoutRestored) {
        restoreMainLayoutFromSettings();
        m_mainLayoutRestored = true;
    }
}

void MainWindowWidget::ensureFloatingBall()
{
    if (m_floatBall) {
        return;
    }
    m_floatBall = new FloatingBallWidget(0);
    connect(m_floatBall, SIGNAL(restoreRequested()),
            this, SLOT(onFloatingBallRestoreRequested()));
    connect(m_floatBall, SIGNAL(closeRequested()),
            this, SLOT(close()));
}

void MainWindowWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MainWindowWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange) {
        QWidget* w = window();
        // 嵌入外壳时用顶层窗状态；独立时 w 即 this
        if (w && !(w->windowState() & Qt::WindowMinimized)) {
            if (m_floatBall)
                m_floatBall->hide();
        }
    }
    QWidget::changeEvent(e);
}

void MainWindowWidget::onMainWindowMinimizeClicked()
{
    // 先显示球，再隐藏/最小化主窗口
    ensureFloatingBall();
    if (m_floatBall) {
        m_cachedFloatBallAnchor = mapToGlobal(rect().bottomRight());
        const int ballW = 60, ballH = 60, margin = 20;
        const int offsetLeft = 60, offsetUp = 100;
        int x = m_cachedFloatBallAnchor.x() - ballW - margin - offsetLeft;
        int y = m_cachedFloatBallAnchor.y() - ballH - margin - offsetUp;
        x = qMax(0, x);
        y = qMax(0, y);
        m_floatBall->move(x, y);
        // 在库项目中，确保窗口在显示前已正确初始化
        // 设置位置后再显示，确保窗口句柄正确创建
        m_floatBall->show();
        // 参考reference项目，显示后调用 raise() 确保窗口在最前
        m_floatBall->raise();
        // 注意：不要调用 activateWindow()，因为设置了 WA_ShowWithoutActivating
    }
    QWidget* top = window();
    if (top && top != this) {
        // 有 BaseDialog 等外壳：最小化顶层窗，子区随外壳一起最小化
        top->showMinimized();
    } else if (parentWidget()) {
        hide();
    } else {
        showMinimized();
    }
}

void MainWindowWidget::onShellToggleMaximizeClicked()
{
    QWidget* top = window();
    if (!top)
        return;
    const bool nowMax = (top->windowState() & Qt::WindowMaximized) != 0;
    if (nowMax) {
        top->showNormal();
        if (BaseDialog* dlg = qobject_cast<BaseDialog*>(top))
            dlg->updateMaximizeButtonIcon(false);
    } else {
        top->showMaximized();
        if (BaseDialog* dlg = qobject_cast<BaseDialog*>(top))
            dlg->updateMaximizeButtonIcon(true);
    }
}

MainWindowWidget::~MainWindowWidget()
{
    if (m_mainLayoutRestored && ui) {
        persistMainLayoutSnapshot();
        AppUiSettings::instance().save();
    }
    if (m_floatBall) {
        m_floatBall->disconnect(this);
        delete m_floatBall;
        m_floatBall = NULL;
    }
    if (m_trayIcon) {
        m_trayIcon->hide();
        m_trayIcon = NULL;
    }
    delete ui;
    delete pCanvasCtl;
}

void MainWindowWidget::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        QWidget* w = window();
        if (w) {
            w->showNormal();
            w->activateWindow();
            w->raise();
        }
    }
}

void MainWindowWidget::onFloatingBallRestoreRequested()
{
    if (m_floatBall) {
        m_floatBall->hide();
    }
    QWidget* w = window();
    if (w) {
        w->showNormal();
        w->activateWindow();
        w->raise();
    }
}

// 初始化图形
void MainWindowWidget::initGraphics()
{
    if (ui->widget_Panel) {
        QGraphicsScene* scene = ui->widget_Panel->scene();

        if (scene) {
            pCanvasCtl = new CanvasController(scene);
            pCanvasCtl->attachLayerObserver(ui->widget_layer->FormLayer());
            pCanvasCtl->attachShapeToolObserver(ui->widget_ShapeBar);
            // 设置DrawPanel到CanvasController，用于更新画图状态
            pCanvasCtl->setDrawPanel(ui->widget_Panel);

            LayeredScene* layeredScene = qobject_cast<LayeredScene*>(scene);
            if (!m_docModel) {
                m_docModel = new CadDocumentModel(this);
            }
            if (!m_docInterface) {
                m_docInterface = new CadDocumentInterface(m_docModel, this);
            } else {
                m_docInterface->setDocumentModel(m_docModel);
            }
            if (layeredScene) {
                layeredScene->setDocumentInterface(m_docInterface);
            }
            if (!m_canvasSceneRenderer) {
                m_canvasSceneRenderer = new CanvasSceneRenderer(m_docModel, layeredScene, this);
            } else {
                m_canvasSceneRenderer->setDocumentModel(m_docModel);
                m_canvasSceneRenderer->setScene(layeredScene);
            }
            if (!m_canvasPreviewManager) {
                m_canvasPreviewManager = new CanvasPreviewManager(this);
            }
            if (!m_canvasOverlayRenderer) {
                m_canvasOverlayRenderer = new CanvasOverlayRenderer(this);
            }
            m_canvasOverlayRenderer->setPreviewManager(m_canvasPreviewManager);
            GCodePreviewHelper::setPreviewManager(m_canvasPreviewManager);
            m_docInterface->setPreviewManager(m_canvasPreviewManager);
            if (ui->widget_Panel && ui->widget_Panel->canvasView()) {
                ui->widget_Panel->canvasView()->setOverlayRenderer(m_canvasOverlayRenderer);
            }
            m_docInterface->setSceneRenderer(m_canvasSceneRenderer);
            if (!m_canvasSceneSynchronizer) {
                m_canvasSceneSynchronizer = new CanvasSceneSynchronizer(this);
            }
            m_canvasSceneSynchronizer->setDocumentInterface(m_docInterface);
            m_canvasSceneSynchronizer->setScene(layeredScene);
            disconnect(m_canvasSceneRenderer, SIGNAL(sceneRegenerated()),
                       m_canvasSceneSynchronizer, SLOT(syncNow()));
            connect(m_canvasSceneRenderer, SIGNAL(sceneRegenerated()),
                    m_canvasSceneSynchronizer, SLOT(syncNow()));
            disconnect(m_canvasSceneRenderer, SIGNAL(entitiesRegenerated(QSet<int>)),
                       m_canvasSceneSynchronizer, SLOT(syncGraphicIds(QSet<int>)));
            connect(m_canvasSceneRenderer, SIGNAL(entitiesRegenerated(QSet<int>)),
                    m_canvasSceneSynchronizer, SLOT(syncGraphicIds(QSet<int>)));
            if (pCanvasCtl) {
                pCanvasCtl->setDocumentModel(m_docModel);
                pCanvasCtl->setPreviewManager(m_canvasPreviewManager);
                pCanvasCtl->setDocumentInterface(m_docInterface);
            }
            if (ui->widget_layer && ui->widget_layer->FormLayer()) {
                ui->widget_layer->FormLayer()->setDocumentInterface(m_docInterface);
            }
            if (ui->widget_Panel) {
                ui->widget_Panel->setDocumentInterface(m_docInterface);
            }
        }

        // 画布交互模式由 PanelToolWidget 设置 DrawPanel
        // 设置DrawPanel到面板下工具条
        PanelToolWidget* tools = findChild<PanelToolWidget*>("widget_PanelTools");
        if (tools) {
            tools->setDrawPanel(ui->widget_Panel);
            if (m_docInterface) {
                tools->setDocumentInterface(m_docInterface);
            }
        }
    }
}

// 初始化图形
void MainWindowWidget::initConections()
{
    if (!pCanvasCtl) {
        LOG_ERROR(QString::fromUtf8("initConections: pCanvasCtl 为空，已跳过画布/图层信号连接（请检查 DrawPanel 场景是否已创建）"));
        return;
    }

    connect(ui->widget_Panel, SIGNAL(mouseMoved(QPointF)),
            pCanvasCtl, SLOT(on_canvas_mouseMoved(QPointF)));
    connect(ui->widget_Panel, SIGNAL(viewportMouseLeft()),
            pCanvasCtl, SLOT(on_canvas_viewportMouseLeft()));
    connect(ui->widget_Panel, SIGNAL(mouseClicked(QPointF,Qt::MouseButton)),
            pCanvasCtl, SLOT(on_canvas_mouseClicked(QPointF,Qt::MouseButton)));
    connect(ui->widget_Panel, SIGNAL(mouseDoubleClicked(QPointF,Qt::MouseButton)),
            pCanvasCtl, SLOT(on_canvas_mouseDoubleClicked(QPointF,Qt::MouseButton)));
    connect(ui->widget_Panel, SIGNAL(requestSwitchToSingleSelectMode()),
            this, SLOT(onCanvasRequestSwitchToSingleSelectMode()));

    connect(ui->widget_ShapeBar, SIGNAL(toolChanged(ToolType)),
            pCanvasCtl, SLOT(on_canvas_changeShapeTool(ToolType)));

    FormLayerPop* layerPop = (ui->widget_layer) ? ui->widget_layer->FormLayer() : 0;
    if (!layerPop) {
        LOG_ERROR(QString::fromUtf8("initConections: FormLayerPop 为空，已跳过图层信号连接（FormUnderShape 是否提前 return 未创建图层面板？）"));
    } else {
        connect(layerPop, SIGNAL(layerAdded(int,QString)),
                pCanvasCtl, SLOT(on_layer_addLayerClicked(int,QString)));
        connect(layerPop, SIGNAL(layerDeleted(int)),
                pCanvasCtl, SLOT(on_layer_LayerRemoved(int)));
        connect(layerPop, SIGNAL(layerOrderUp(int)),
                pCanvasCtl, SLOT(on_layer_moveUp(int)));
        connect(layerPop, SIGNAL(layerOrderDown(int)),
                pCanvasCtl, SLOT(on_layer_moveDown(int)));
        connect(layerPop, SIGNAL(layerVisibilityChanged(int,bool)),
                pCanvasCtl, SLOT(on_layer_visibilityChanged(int,bool)));
        connect(layerPop, SIGNAL(layerLockedChanged(int,bool)),
                pCanvasCtl, SLOT(on_layer_lockedChanged(int,bool)));
        connect(layerPop, SIGNAL(layerLineStyleChanged(int,int)),
                pCanvasCtl, SLOT(on_layer_lineStyleChanged(int,int)));
        connect(layerPop, SIGNAL(currentLayerChanged(int)),
                pCanvasCtl, SLOT(on_layer_currentLayerChanged(int)));
    }

    FileControlWidget* fileControl = qobject_cast<FileControlWidget*>(ui->widget_FileControl);
    if (fileControl && pCanvasCtl) {
        connect(fileControl, SIGNAL(beforeClearAllData()),
                pCanvasCtl, SLOT(onBeforeClearAllData()));
    }

    setupExclusiveToolGroup();
}

// 初始化工艺视图
void MainWindowWidget::initCraftView()
{
    if (ui->widget_6) {
        CraftTableViewWidget* craftViewWidget = qobject_cast<CraftTableViewWidget*>(ui->widget_6);
        if (craftViewWidget) {
            CraftTableView* craftView = craftViewWidget->tableView();
            
            if (craftView && ui->widget_Panel) {
                craftViewWidget->setDrawPanel(ui->widget_Panel);
            }
            if (craftView && m_docModel) {
                craftView->setDocumentModel(m_docModel);
                craftView->setDocumentInterface(m_docInterface);
            }

            // 工艺栏：武装工艺（互斥、可再点取消）；双击画布时由 CanvasController 调用 addCraft
            if (ui->widget_2) {
                CraftToolBar* craftToolBar = qobject_cast<CraftToolBar*>(ui->widget_2);
                if (craftToolBar && craftViewWidget) {
                    connect(craftToolBar, SIGNAL(craftArmChanged(QString)),
                            this, SLOT(onCraftArmChanged(QString)));
                    if (pCanvasCtl && craftView) {
                        pCanvasCtl->setCraftToolBar(craftToolBar);
                        pCanvasCtl->setCraftTableView(craftView);
                    }
                    LOG_DEBUG_FMT("CraftToolBar craftArmChanged connected");
                } else {
                    LOG_DEBUG_FMT("Failed to connect: craftToolBar=%p, craftViewWidget=%p", craftToolBar, craftViewWidget);
                }
            } else {
                LOG_DEBUG_FMT("widget_2 is NULL");
            }

            // 连接控制栏到表格视图
            if (ui->widget_7) {
                CraftTableControlBar* controlBar = qobject_cast<CraftTableControlBar*>(ui->widget_7);
                if (controlBar && craftView) {
                    controlBar->setTableView(craftView);
                    LOG_DEBUG_FMT("CraftTableControlBar connected to CraftTableView");
                } else {
                    LOG_DEBUG_FMT("Failed to connect: controlBar=%p, craftView=%p", controlBar, craftView);
                }
            } else {
                LOG_DEBUG_FMT("widget_7 is NULL");
            }
        } else {
            LOG_DEBUG_FMT("widget_6 is not CraftTableViewWidget");
        }
    }
}

// 初始化图元表格视图
void MainWindowWidget::initGraphicsItemTableView()
{
    if (ui->widget_4) {
        GraphicsItemTableView* itemTableView = qobject_cast<GraphicsItemTableView*>(ui->widget_4);
        if (itemTableView && ui->widget_Panel) {
            QGraphicsScene* scene = ui->widget_Panel->scene();
            LayeredScene* layeredScene = qobject_cast<LayeredScene*>(scene);
            if (layeredScene) {
                itemTableView->setDrawPanel(ui->widget_Panel);
                itemTableView->setDocumentModel(m_docModel);
                itemTableView->setDocumentInterface(m_docInterface);
                LOG_DEBUG_FMT("GraphicsItemTableView initialized");
                
                // 初始化控制栏
                if (ui->widget_5) {
                    GraphicsItemTableControlBar* controlBar = qobject_cast<GraphicsItemTableControlBar*>(ui->widget_5);
                    if (controlBar && itemTableView) {
                        controlBar->setTableView(itemTableView);
                        LOG_DEBUG_FMT("GraphicsItemTableControlBar connected to GraphicsItemTableView");
                    } else {
                        LOG_DEBUG_FMT("Failed to connect: controlBar=%p, itemTableView=%p", controlBar, itemTableView);
                    }
                } else {
                    LOG_DEBUG_FMT("widget_5 is NULL");
                }
            } else {
                LOG_DEBUG_FMT("Failed to cast scene to LayeredScene");
            }
        } else {
            LOG_DEBUG_FMT("Failed to get GraphicsItemTableView or DrawPanel");
        }
    }
}

// 初始化文件控制Widget
void MainWindowWidget::initFileControlWidget()
{
    if (ui->widget_FileControl) {
        FileControlWidget* fileControl = qobject_cast<FileControlWidget*>(ui->widget_FileControl);
        if (fileControl && ui->widget_Panel) {
            QGraphicsScene* scene = ui->widget_Panel->scene();
            if (scene) {
                fileControl->setScene(scene);
            }
            fileControl->setDocumentModel(m_docModel);
            
            // 设置工艺视图
            if (ui->widget_6) {
                CraftTableViewWidget* craftViewWidget = qobject_cast<CraftTableViewWidget*>(ui->widget_6);
                if (craftViewWidget) {
                    CraftTableView* craftView = craftViewWidget->tableView();
                    if (craftView) {
                        fileControl->setCraftTableView(craftView);
                    }
                }
            }
            
            // 设置DrawPanel
            fileControl->setDrawPanel(ui->widget_Panel);
            CraftToolBar* craftToolBar = qobject_cast<CraftToolBar*>(ui->widget_2);
            if (craftToolBar)
                fileControl->setCraftToolBar(craftToolBar);

            GraphicsItemTableView* itemTableView = qobject_cast<GraphicsItemTableView*>(ui->widget_4);
            if (itemTableView) {
                connect(fileControl, SIGNAL(alignToRegisterRequested()),
                        itemTableView, SLOT(onAlignToRegister()));
                connect(itemTableView, SIGNAL(alignToRegisterApplied()),
                        fileControl, SLOT(requestCamAutoSaveDeferred()));
            }

            if (ui->widget_7) {
                CraftTableControlBar* craftControlBar = qobject_cast<CraftTableControlBar*>(ui->widget_7);
                if (craftControlBar)
                    craftControlBar->setFileControlWidget(fileControl);
            }

            fileControl->initializeSessionPersistence();
            connect(fileControl, SIGNAL(smallScreenModeChanged(bool)),
                    this, SLOT(onSmallScreenModeChanged(bool)));
            LOG_DEBUG_FMT("FileControlWidget initialized");
        } else {
            LOG_DEBUG_FMT("Failed to get FileControlWidget");
        }
    }
}

void MainWindowWidget::initDocumentUiCoordinator()
{
    if (!m_docModel || !ui) {
        return;
    }
    if (!m_docUi) {
        m_docUi = new DocumentUiCoordinator(this);
    }
    FormLayerPop* layerPop = ui->widget_layer ? ui->widget_layer->FormLayer() : 0;
    GraphicsItemTableView* itemTableView = qobject_cast<GraphicsItemTableView*>(ui->widget_4);

    m_docUi->setDocumentModel(m_docModel);
    m_docUi->setLayerPanel(layerPop);
    m_docUi->setGraphicsItemTableView(itemTableView);
    m_docUi->syncNow();
}

void MainWindowWidget::setupExclusiveToolGroup()
{
    if (m_exclusiveToolGroup) {
        m_exclusiveToolGroup->deleteLater();
        m_exclusiveToolGroup = NULL;
    }
    m_exclusiveToolGroup = new QButtonGroup(this);
    // 必须为 false：Qt4 在 exclusive 下「再点已选按钮」不会取消勾选，无法二次点击关闭工具
    m_exclusiveToolGroup->setExclusive(false);
    ShapeToolBar* bar = ui->widget_ShapeBar;
    PanelToolWidget* tools = ui->widget_PanelTools;
    if (!bar || !tools) {
        LOG_DEBUG_FMT("setupExclusiveToolGroup: ShapeToolBar or PanelToolWidget missing");
        return;
    }
    m_exclusiveToolGroup->addButton(bar->buttonLine(), EXGID_LINE);
    m_exclusiveToolGroup->addButton(bar->buttonCircle(), EXGID_CIRCLE);
    m_exclusiveToolGroup->addButton(bar->buttonEllipse(), EXGID_ELLIPSE);
    m_exclusiveToolGroup->addButton(bar->buttonArc(), EXGID_ARC);
    m_exclusiveToolGroup->addButton(bar->buttonFace(), EXGID_FACE);
    m_exclusiveToolGroup->addButton(bar->buttonMark(), EXGID_MARK);
    m_exclusiveToolGroup->addButton(bar->buttonText(), EXGID_TEXT);
    m_exclusiveToolGroup->addButton(bar->buttonCopyOffset(), EXGID_COPY_OFFSET);
    m_exclusiveToolGroup->addButton(bar->buttonCopyRotate(), EXGID_COPY_ROTATE);
    m_exclusiveToolGroup->addButton(bar->buttonCopyArray(), EXGID_COPY_ARRAY);
    m_exclusiveToolGroup->addButton(tools->buttonSelect(), EXGID_PANEL_SELECT);
    m_exclusiveToolGroup->addButton(tools->buttonBoxSelect(), EXGID_PANEL_BOX);
    m_exclusiveToolGroup->addButton(tools->buttonSelectSame(), EXGID_PANEL_SELECT_SAME);

    QList<QAbstractButton*> blist = m_exclusiveToolGroup->buttons();
    for (int i = 0; i < blist.size(); ++i) {
        connect(blist.at(i), SIGNAL(toggled(bool)),
                this, SLOT(onExclusiveMemberToggled(bool)));
    }
    applyExclusiveToolState();
}

void MainWindowWidget::onExclusiveMemberToggled(bool checked)
{
    QAbstractButton* btn = qobject_cast<QAbstractButton*>(sender());
    if (checked && btn && m_exclusiveToolGroup) {
        QList<QAbstractButton*> buttons = m_exclusiveToolGroup->buttons();
        for (int i = 0; i < buttons.size(); ++i) {
            QAbstractButton* o = buttons.at(i);
            if (o != btn) {
                o->blockSignals(true);
                o->setChecked(false);
                o->blockSignals(false);
            }
        }
    }
    scheduleApplyExclusiveToolState();
    syncHShapeBarFromVertical();
}

void MainWindowWidget::scheduleApplyExclusiveToolState()
{
    QTimer::singleShot(0, this, SLOT(applyExclusiveToolState()));
}

void MainWindowWidget::applyExclusiveToolState()
{
    if (!m_exclusiveToolGroup) return;
    QAbstractButton* cb = m_exclusiveToolGroup->checkedButton();
    PanelToolWidget* tools = ui->widget_PanelTools;
    if (!cb && tools && tools->buttonSelect()) {
        tools->buttonSelect()->blockSignals(true);
        tools->buttonSelect()->setChecked(true);
        tools->buttonSelect()->blockSignals(false);
        cb = m_exclusiveToolGroup->checkedButton();
    }
    if (!cb) return;
    int id = m_exclusiveToolGroup->id(cb);

    ShapeToolBar* bar = ui->widget_ShapeBar;
    FormUnderShape* under = ui->widget_layer;

    const bool isShape = (id >= EXGID_LINE && id <= EXGID_TEXT);
    const bool isCopy = (id >= EXGID_COPY_OFFSET && id <= EXGID_COPY_ARRAY);
    const bool isPanel = (id >= EXGID_PANEL_SELECT && id <= EXGID_PANEL_SELECT_SAME);

    if (isShape || isCopy) {
        CraftToolBar* craftBar = qobject_cast<CraftToolBar*>(ui->widget_2);
        if (craftBar)
            craftBar->clearArmedCraft();
    }

    if (!isShape && bar) {
        bar->resetExclusiveDrawModeToNone();
    }

    int copyMode = 0;
    if (isCopy) {
        if (id == EXGID_COPY_OFFSET) copyMode = 1;
        else if (id == EXGID_COPY_ROTATE) copyMode = 2;
        else if (id == EXGID_COPY_ARRAY) copyMode = 3;
    }
    if (under) {
        under->setCopyMode(copyMode);
    }

    if (tools) {
        if (isPanel) {
            if (id == EXGID_PANEL_SELECT) tools->applyExclusiveSelectMode();
            else if (id == EXGID_PANEL_BOX) tools->applyExclusiveBoxSelectMode();
            else if (id == EXGID_PANEL_SELECT_SAME) tools->applyExclusiveSameRadiusCircleSelectMode();
        } else {
            tools->applyExclusiveSelectMode();
        }
    }

    emit exclusiveWorkspaceModeChanged(id);
}

void MainWindowWidget::onCanvasRequestSwitchToSingleSelectMode()
{
    PanelToolWidget* tools = ui->widget_PanelTools;
    if (!tools || !m_exclusiveToolGroup)
        return;
    QAbstractButton* selectBtn = tools->buttonSelect();
    if (!selectBtn || selectBtn->isChecked())
        return;
    QList<QAbstractButton*> buttons = m_exclusiveToolGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i) {
        QAbstractButton* o = buttons.at(i);
        o->blockSignals(true);
        o->setChecked(o == selectBtn);
        o->blockSignals(false);
    }
    scheduleApplyExclusiveToolState();
}

void MainWindowWidget::onCraftArmChanged(const QString& craftType)
{
    if (ui->widget_Panel)
        ui->widget_Panel->setCraftPlacementActive(!craftType.isEmpty());

    if (craftType.isEmpty())
        return;

    ShapeToolBar* shapeBar = ui->widget_ShapeBar;
    if (shapeBar)
        shapeBar->resetExclusiveDrawModeToNone();

    if (!m_exclusiveToolGroup)
        return;

    QList<QAbstractButton*> buttons = m_exclusiveToolGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i) {
        QAbstractButton* o = buttons.at(i);
        int exId = m_exclusiveToolGroup->id(o);
        const bool isShapeBtn = (exId >= EXGID_LINE && exId <= EXGID_TEXT);
        const bool isCopyBtn = (exId >= EXGID_COPY_OFFSET && exId <= EXGID_COPY_ARRAY);
        if ((isShapeBtn || isCopyBtn) && o->isChecked()) {
            o->blockSignals(true);
            o->setChecked(false);
            o->blockSignals(false);
        }
    }

    PanelToolWidget* tools = ui->widget_PanelTools;
    if (!m_exclusiveToolGroup->checkedButton() && tools && tools->buttonSelect()) {
        tools->buttonSelect()->blockSignals(true);
        tools->buttonSelect()->setChecked(true);
        tools->buttonSelect()->blockSignals(false);
    }
    scheduleApplyExclusiveToolState();
}

// 旧复制预览链路已移除

void MainWindowWidget::initMainLayoutPersistence()
{
    m_layoutPersistTimer = new QTimer(this);
    m_layoutPersistTimer->setSingleShot(true);
    m_layoutPersistTimer->setInterval(450);
    connect(m_layoutPersistTimer, SIGNAL(timeout()),
            this, SLOT(onPersistMainLayoutTimer()));

    QSplitter* splitters[] = {
        ui->splitter_horizon,
        ui->splitter_right,
        ui->splitter_rightD,
        ui->splitter_left
    };
    const int n = sizeof(splitters) / sizeof(splitters[0]);
    for (int i = 0; i < n; ++i) {
        PersistingSplitter* ps = qobject_cast<PersistingSplitter*>(splitters[i]);
        if (ps)
            connect(ps, SIGNAL(userFinishedResize()),
                    this, SLOT(schedulePersistMainLayout()));
    }
}

void MainWindowWidget::restoreMainLayoutFromSettings()
{
    struct Row {
        QSplitter* splitter;
        const char* name;
    };
    const Row rows[] = {
        { ui->splitter_horizon, "splitter_horizon" },
        { ui->splitter_right, "splitter_right" },
        { ui->splitter_rightD, "splitter_rightD" },
        { ui->splitter_left, "splitter_left" }
    };
    const int n = sizeof(rows) / sizeof(rows[0]);
    for (int i = 0; i < n; ++i) {
        QSplitter* sp = rows[i].splitter;
        if (!sp)
            continue;
        const QList<int> sz = AppUiSettings::instance().splitterSizes(QString::fromLatin1(rows[i].name));
        if (sz.size() == sp->count() && sp->count() > 0)
            sp->setSizes(sz);
    }
}

void MainWindowWidget::persistMainLayoutSnapshot()
{
    struct Row {
        QSplitter* splitter;
        const char* name;
    };
    const Row rows[] = {
        { ui->splitter_horizon, "splitter_horizon" },
        { ui->splitter_right, "splitter_right" },
        { ui->splitter_rightD, "splitter_rightD" },
        { ui->splitter_left, "splitter_left" }
    };
    const int n = sizeof(rows) / sizeof(rows[0]);
    for (int i = 0; i < n; ++i) {
        QSplitter* sp = rows[i].splitter;
        if (!sp)
            continue;
        AppUiSettings::instance().setSplitterSizes(QString::fromLatin1(rows[i].name), sp->sizes());
    }
}

void MainWindowWidget::schedulePersistMainLayout()
{
    if (m_layoutPersistTimer)
        m_layoutPersistTimer->start();
}

void MainWindowWidget::onPersistMainLayoutTimer()
{
    persistMainLayoutSnapshot();
    AppUiSettings::instance().save();
}

// ===================== 小屏幕模式 =====================

void MainWindowWidget::initSmallScreenMode()
{
    ShapeToolBar* vBar = ui->widget_ShapeBar;
    if (!vBar)
        return;

    // 创建顶部切换容器（初始隐藏）
    m_smallScreenTopBar = new QWidget(this);
    m_smallScreenTopBar->setObjectName(QLatin1String("smallScreenTopBar"));
    m_smallScreenTopBar->setMinimumHeight(44);
    m_smallScreenTopBar->setMaximumHeight(54);

    QHBoxLayout* topLay = new QHBoxLayout(m_smallScreenTopBar);
    topLay->setContentsMargins(2, 2, 2, 2);
    topLay->setSpacing(4);

    // 切换按钮
    m_topBarToggleBtn = new QToolButton(m_smallScreenTopBar);
    m_topBarToggleBtn->setCheckable(false);
    m_topBarToggleBtn->setToolTip(QString::fromUtf8("切换文件栏/形状栏/工艺栏"));
    m_topBarToggleBtn->setText(QString::fromUtf8("▶形状"));
    m_topBarToggleBtn->setMinimumSize(60, 36);
    m_topBarToggleBtn->setStyleSheet(
        "QToolButton{padding:4px 8px;border:1px solid #5c8ebf;border-radius:6px;"
        "background:#e0eaf0;color:#213140;font-weight:600;}"
        "QToolButton:hover{background:#c5ddf0;}"
        "QToolButton:pressed{background:#2f73ff;color:#ffffff;}");
    connect(m_topBarToggleBtn, SIGNAL(clicked()), this, SLOT(onTopBarToggleClicked()));
    topLay->addWidget(m_topBarToggleBtn);

    // QStackedWidget 容纳文件栏和横向形状栏
    m_topBarStack = new QStackedWidget(m_smallScreenTopBar);

    // Page 0: placeholder for FileControlWidget (will be reparented)
    // We use a simple empty widget as placeholder; FileControlWidget will be inserted at runtime
    QWidget* filePlaceholder = new QWidget(m_topBarStack);
    m_topBarStack->addWidget(filePlaceholder);

    // Page 1: 横向形状栏
    m_horizontalShapeBar = new QWidget(m_topBarStack);
    m_horizontalShapeBar->setObjectName(QLatin1String("horizontalShapeBar"));
    QHBoxLayout* hBarLay = new QHBoxLayout(m_horizontalShapeBar);
    hBarLay->setContentsMargins(2, 2, 2, 2);
    hBarLay->setSpacing(4);

    // 为横向形状栏创建镜像按钮
    struct BtnDef {
        const char* tooltip;
        const char* text;
    };
    const BtnDef defs[] = {
        { "画线工具", "线" },
        { "画圆工具", "圆" },
        { "画椭圆工具", "椭圆" },
        { "画圆弧工具", "弧" },
        { "画面工具", "面" },
        { "标注工具", "Mark" },
        { "Text tool", "Txt" },
        { "偏移复制", "偏移" },
        { "旋转复制", "旋转" },
        { "阵列复制", "阵列" }
    };
    const int defCount = sizeof(defs) / sizeof(defs[0]);

    const QString hBtnStyle =
        "QToolButton{padding:4px 6px;border:1px solid #a8b9cd;border-radius:5px;"
        "background:#eef3f9;color:#213140;font-weight:600;font-size:11px;min-width:36px;}"
        "QToolButton:hover{border-color:#2f73ff;background:#e0ecff;}"
        "QToolButton:pressed{background:#d1e4ff;border-color:#1e5bd6;}"
        "QToolButton:checked{border:2px solid #0d55ff;background:#2f73ff;color:#ffffff;}";

    m_hShapeButtons.clear();
    for (int i = 0; i < defCount; ++i) {
        QToolButton* btn = new QToolButton(m_horizontalShapeBar);
        btn->setCheckable(true);
        btn->setToolTip(QString::fromUtf8(defs[i].tooltip));
        btn->setText(QString::fromUtf8(defs[i].text));
        btn->setMinimumHeight(32);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        btn->setStyleSheet(hBtnStyle);
        btn->setProperty("hShapeIdx", i);
        connect(btn, SIGNAL(toggled(bool)), this, SLOT(onHShapeBarButtonToggled(bool)));
        hBarLay->addWidget(btn);
        m_hShapeButtons.append(btn);
    }
    hBarLay->addStretch();

    m_topBarStack->addWidget(m_horizontalShapeBar);

    // Page 2: 横向工艺栏
    m_horizontalCraftBar = new QWidget(m_topBarStack);
    m_horizontalCraftBar->setObjectName(QLatin1String("horizontalCraftBar"));
    m_topBarStack->addWidget(m_horizontalCraftBar);
    rebuildHorizontalCraftBar();

    topLay->addWidget(m_topBarStack);

    m_smallScreenTopBar->hide();

    // 连接 CraftToolBar 的 rebuildCraftButtons 完成后刷新横向工艺栏
    CraftToolBar* craftBar = qobject_cast<CraftToolBar*>(ui->widget_2);
    if (craftBar) {
        connect(craftBar, SIGNAL(craftArmChanged(QString)),
                this, SLOT(onCraftToolBarRebuilt()));
    }

    // 应用启动时的小屏幕设置
    applySmallScreenMode(AppUiSettings::instance().smallScreenMode());
}

void MainWindowWidget::applySmallScreenMode(bool enabled)
{
    m_smallScreenActive = enabled;

    if (!m_smallScreenTopBar || !ui->widget_FileControl)
        return;

    if (enabled) {
        // 将 FileControlWidget 移入 m_topBarStack page 0
        QWidget* oldPage0 = m_topBarStack->widget(0);
        if (oldPage0 != ui->widget_FileControl) {
            m_topBarStack->removeWidget(oldPage0);
            oldPage0->deleteLater();
            m_topBarStack->insertWidget(0, ui->widget_FileControl);
        }
        m_topBarStack->setCurrentIndex(0);
        m_topBarCurrentPage = 0;
        m_topBarToggleBtn->setText(QString::fromUtf8("\u25b6\u5f62\u72b6"));

        // 将 m_smallScreenTopBar 插入 splitter_right 的第一个位置
        if (ui->splitter_right) {
            ui->splitter_right->insertWidget(0, m_smallScreenTopBar);
        }
        m_smallScreenTopBar->show();

        // 隐藏左侧竖向形状栏面板 (widget_)
        if (ui->widget_) {
            ui->widget_->hide();
        }
        // 隐藏右侧竖向工艺栏
        if (ui->widget_2) {
            ui->widget_2->hide();
        }
    } else {
        // 恢复 FileControlWidget 到 splitter_right 的第一个位置
        if (ui->splitter_right) {
            ui->splitter_right->insertWidget(0, ui->widget_FileControl);
        }
        ui->widget_FileControl->show();

        m_smallScreenTopBar->hide();

        // 显示左侧竖向形状栏面板
        if (ui->widget_) {
            ui->widget_->show();
        }
        // 显示右侧竖向工艺栏
        if (ui->widget_2) {
            ui->widget_2->show();
        }
    }

    syncHShapeBarFromVertical();
}

void MainWindowWidget::onSmallScreenModeChanged(bool enabled)
{
    applySmallScreenMode(enabled);
}

void MainWindowWidget::onTopBarToggleClicked()
{
    if (!m_topBarStack)
        return;

    // 循环切换: 0(文件) -> 1(形状) -> 2(工艺) -> 0(文件)
    m_topBarCurrentPage = (m_topBarCurrentPage + 1) % 3;
    m_topBarStack->setCurrentIndex(m_topBarCurrentPage);

    // 按钮文字显示下一个将切换到的名称
    switch (m_topBarCurrentPage) {
    case 0: // 当前是文件栏，下一个是形状栏
        m_topBarToggleBtn->setText(QString::fromUtf8("\u25b6\u5f62\u72b6"));
        break;
    case 1: // 当前是形状栏，下一个是工艺栏
        m_topBarToggleBtn->setText(QString::fromUtf8("\u25b6\u5de5\u827a"));
        syncHShapeBarFromVertical();
        break;
    case 2: // 当前是工艺栏，下一个是文件栏
        m_topBarToggleBtn->setText(QString::fromUtf8("\u25b6\u6587\u4ef6"));
        break;
    }
}

void MainWindowWidget::onHShapeBarButtonToggled(bool checked)
{
    QToolButton* btn = qobject_cast<QToolButton*>(sender());
    if (!btn)
        return;

    int idx = btn->property("hShapeIdx").toInt();
    ShapeToolBar* vBar = ui->widget_ShapeBar;
    if (!vBar)
        return;

    // 对应关系：索引 0-9 -> line, circle, ellipse, arc, face, mark, text, copyOffset, copyRotate, copyArray
    QAbstractButton* targets[] = {
        vBar->buttonLine(),
        vBar->buttonCircle(),
        vBar->buttonEllipse(),
        vBar->buttonArc(),
        vBar->buttonFace(),
        vBar->buttonMark(),
        vBar->buttonText(),
        vBar->buttonCopyOffset(),
        vBar->buttonCopyRotate(),
        vBar->buttonCopyArray()
    };
    const int targetCount = sizeof(targets) / sizeof(targets[0]);
    if (idx < 0 || idx >= targetCount)
        return;

    QAbstractButton* target = targets[idx];
    if (!target)
        return;

    // 互斥：先取消其它横向按钮
    if (checked) {
        for (int i = 0; i < m_hShapeButtons.size(); ++i) {
            QAbstractButton* o = m_hShapeButtons.at(i);
            if (o != btn) {
                o->blockSignals(true);
                o->setChecked(false);
                o->blockSignals(false);
            }
        }
    }

    // 同步到竖向形状栏的按钮（通过模拟点击/setChecked）
    if (target->isChecked() != checked) {
        target->blockSignals(true);
        target->setChecked(checked);
        target->blockSignals(false);
        // 触发互斥组逻辑
        if (checked) {
            onExclusiveMemberToggled(true);
        }
    }
}

void MainWindowWidget::syncHShapeBarFromVertical()
{
    if (!m_smallScreenActive || m_hShapeButtons.isEmpty())
        return;

    ShapeToolBar* vBar = ui->widget_ShapeBar;
    if (!vBar)
        return;

    QAbstractButton* targets[] = {
        vBar->buttonLine(),
        vBar->buttonCircle(),
        vBar->buttonEllipse(),
        vBar->buttonArc(),
        vBar->buttonFace(),
        vBar->buttonMark(),
        vBar->buttonText(),
        vBar->buttonCopyOffset(),
        vBar->buttonCopyRotate(),
        vBar->buttonCopyArray()
    };
    const int targetCount = sizeof(targets) / sizeof(targets[0]);
    const int syncCount = qMin(targetCount, m_hShapeButtons.size());

    for (int i = 0; i < syncCount; ++i) {
        QAbstractButton* hBtn = m_hShapeButtons.at(i);
        QAbstractButton* vBtn = targets[i];
        if (hBtn && vBtn) {
            bool vc = vBtn->isChecked();
            if (hBtn->isChecked() != vc) {
                hBtn->blockSignals(true);
                hBtn->setChecked(vc);
                hBtn->blockSignals(false);
            }
        }
    }
}

// ===================== 横向工艺栏 =====================

void MainWindowWidget::rebuildHorizontalCraftBar()
{
    if (!m_horizontalCraftBar)
        return;

    // 清除旧按钮
    m_hCraftButtons.clear();
    QLayout* oldLay = m_horizontalCraftBar->layout();
    if (oldLay) {
        QLayoutItem* item;
        while ((item = oldLay->takeAt(0)) != NULL) {
            if (item->widget())
                delete item->widget();
            delete item;
        }
        delete oldLay;
    }

    QHBoxLayout* hLay = new QHBoxLayout(m_horizontalCraftBar);
    hLay->setContentsMargins(2, 2, 2, 2);
    hLay->setSpacing(4);

    CraftToolBar* craftBar = qobject_cast<CraftToolBar*>(ui->widget_2);
    if (!craftBar)
        return;

    // 读取 CraftToolBar 的按钮组
    QButtonGroup* craftGroup = craftBar->findChild<QButtonGroup*>();
    if (!craftGroup)
        return;

    const QString hCraftBtnStyle =
        "QToolButton{padding:4px 8px;border:1px solid #a8b9cd;border-radius:5px;"
        "background:#eef3f9;color:#213140;font-weight:600;font-size:11px;min-width:40px;}"
        "QToolButton:hover{border-color:#e65100;background:#fff3cd;}"
        "QToolButton:pressed{background:#ffd54f;border-color:#e65100;}"
        "QToolButton:checked{border:2px solid #e65100;background:#fff3cd;color:#7c2100;}";

    QList<QAbstractButton*> craftButtons = craftGroup->buttons();
    for (int i = 0; i < craftButtons.size(); ++i) {
        QAbstractButton* srcBtn = craftButtons.at(i);
        if (!srcBtn)
            continue;

        QToolButton* hBtn = new QToolButton(m_horizontalCraftBar);
        hBtn->setCheckable(true);
        hBtn->setText(srcBtn->text());
        hBtn->setToolTip(srcBtn->toolTip());
        hBtn->setMinimumHeight(32);
        hBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        hBtn->setStyleSheet(hCraftBtnStyle);
        hBtn->setProperty("hCraftIdx", i);
        hBtn->setChecked(srcBtn->isChecked());
        connect(hBtn, SIGNAL(toggled(bool)), this, SLOT(onHCraftBarButtonToggled(bool)));
        hLay->addWidget(hBtn);
        m_hCraftButtons.append(hBtn);
    }
    hLay->addStretch();
}

void MainWindowWidget::onHCraftBarButtonToggled(bool checked)
{
    QToolButton* btn = qobject_cast<QToolButton*>(sender());
    if (!btn)
        return;

    int idx = btn->property("hCraftIdx").toInt();
    CraftToolBar* craftBar = qobject_cast<CraftToolBar*>(ui->widget_2);
    if (!craftBar)
        return;

    QButtonGroup* craftGroup = craftBar->findChild<QButtonGroup*>();
    if (!craftGroup)
        return;

    QList<QAbstractButton*> craftButtons = craftGroup->buttons();
    if (idx < 0 || idx >= craftButtons.size())
        return;

    // 互斥：先取消其它横向工艺按钮
    if (checked) {
        for (int i = 0; i < m_hCraftButtons.size(); ++i) {
            QAbstractButton* o = m_hCraftButtons.at(i);
            if (o != btn) {
                o->blockSignals(true);
                o->setChecked(false);
                o->blockSignals(false);
            }
        }
    }

    // 同步到竖向工艺栏
    QAbstractButton* target = craftButtons.at(idx);
    if (target && target->isChecked() != checked) {
        // 直接调用 CraftToolBar 的按钮点击逻辑
        target->setChecked(checked);
    }
}

void MainWindowWidget::onCraftToolBarRebuilt()
{
    // CraftToolBar 的工艺按钮可能被重建了，同步横向工艺栏状态
    if (!m_smallScreenActive)
        return;

    // 同步横向工艺栏的选中状态
    CraftToolBar* craftBar = qobject_cast<CraftToolBar*>(ui->widget_2);
    if (!craftBar)
        return;

    QButtonGroup* craftGroup = craftBar->findChild<QButtonGroup*>();
    if (!craftGroup)
        return;

    QList<QAbstractButton*> craftButtons = craftGroup->buttons();
    const int syncCount = qMin(craftButtons.size(), m_hCraftButtons.size());

    for (int i = 0; i < syncCount; ++i) {
        QAbstractButton* hBtn = m_hCraftButtons.at(i);
        QAbstractButton* vBtn = craftButtons.at(i);
        if (hBtn && vBtn) {
            bool vc = vBtn->isChecked();
            if (hBtn->isChecked() != vc) {
                hBtn->blockSignals(true);
                hBtn->setChecked(vc);
                hBtn->blockSignals(false);
            }
        }
    }
}
