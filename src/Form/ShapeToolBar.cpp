#include "ShapeToolBar.h"
//#include "ShapeCirclePopup.h"
#include "ArrowToolButton.h"
#include <cstring>
#include "DrawPanel/DrawSet/TextInputDialog.h"
#include "DrawPanel/DrawSet/CanvasShapeText.h"
#include <QApplication>
#include <QStyle>
#include <QDebug>
#include <QPainter>
#include <QVBoxLayout>
#include <QDialog>
#include <QSizePolicy>

// ==================== ShapeToolBar 实现 ====================

ShapeToolBar::ShapeToolBar(QWidget *parent)
    : QWidget(parent)
    , lineButton(0)
    , circleButton(0)
    , ellipseButton(0)
    , arcButton(0)
    , faceButton(0)
    , markButton(0)
    , textButton(0)
    , circlePopup(0)
    , linePopup(0)
    , ellipsePopup(0)
    , arcPopup(0)
    , polygonPopup(0)
    , mainLayout(0)
    , copyOffsetBtn(0)
    , copyRotateBtn(0)
    , copyArrayBtn(0)
{
    // m_DrawData 未初始化时 SelectMode 为随机值，resetExclusiveDrawModeToNone 会误发 toolChanged，启动期曾触发崩溃链
    std::memset(&m_DrawData, 0, sizeof(m_DrawData));
    m_DrawData.SelectMode = MODE_SELECT;

    // 先初始化中文文本列表
    setupChineseTexts();

    setupUI();
    setupConnections();

    // 设置工具栏样式
    this->setStyleSheet(
        "ShapeToolBar {"
        "    background-color: #2c3e50;"  // 深蓝色背景，如图片中的工具栏
        "    border-right: 1px solid #34495e;"
        "}"
        ""
        "ArrowToolButton {"
        "    background-color: transparent;"
        "    border: none;"               // 无边框
        "    border-radius: 3px;"
        "    padding: 12px 8px;"          // 增加内边距
        "    margin: 4px 2px;"           // 外边距
        "    text-align: center;"         // 文字居中
        "    font-size: 11px;"
        "    font-weight: bold;"          // 加粗字体
        "    color: #ecf0f1;"            // 浅色文字
        "    min-width: 60px;"           // 最小宽度
        "}"
        ""
        "ArrowToolButton:checked {"
        "    background-color: #3498db;"  // 选中时的蓝色背景
        "    color: white;"               // 白色文字
        "}"
        ""
        "ArrowToolButton:hover {"
        "    background-color: #34495e;"  // 悬停时的深蓝色
        "}"
        ""
        "ArrowToolButton:pressed {"
        "    background-color: #2980b9;"  // 按下时的蓝色
        "}"
    );
}

ShapeToolBar::~ShapeToolBar()
{
    if (circlePopup) {
        circlePopup->close();
        delete circlePopup;
    }
    if (linePopup) {
        linePopup->close();
        delete linePopup;
    }
    if (ellipsePopup) {
        ellipsePopup->close();
        delete ellipsePopup;
    }
    if (arcPopup) {
        arcPopup->close();
        delete arcPopup;
    }
    if (polygonPopup) {
        polygonPopup->close();
        delete polygonPopup;
    }
}

void ShapeToolBar::setupChineseTexts()
{
    chineseTexts.clear();

    // 根据图片内容添加所有文本（繁体字）
    chineseTexts << QString::fromUtf8("画线")
                 << QString::fromUtf8("画圆")
                 << QString::fromUtf8("画椭圆")
                 << QString::fromUtf8("画圆弧")
                 << QString::fromUtf8("画面")
                 << QString("Mark")
                 << QString("Text");

    qDebug() << "[init] toolbar text list created, count:" << chineseTexts.size();
}

void ShapeToolBar::setupUI()
{
    // 设置主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);  // 减小间距
    mainLayout->setContentsMargins(4, 10, 4, 10);
    this->setMinimumWidth(60);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 1. 画线按钮 - 只显示图标，不显示文字
    lineButton = new ArrowToolButton(this);
    lineButton->setCheckable(true);
    lineButton->setArrowClickEnabled(true);
    lineButton->setArrowAreaWidth(25);
    lineButton->setIconSize(QSize(24, 24));
    lineButton->setToolTip(QString::fromUtf8("画线工具"));
    lineButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 设置一个简单的线形图标
    lineButton->setLeftIcon(createLineIcon());

    // 2. 画圆按钮 - 只显示图标
    circleButton = new ArrowToolButton(this);
    circleButton->setCheckable(true);
    circleButton->setArrowClickEnabled(true);
    circleButton->setArrowAreaWidth(25);
    circleButton->setIconSize(QSize(24, 24));
    circleButton->setToolTip(QString::fromUtf8("画圆工具"));
    circleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 设置一个简单的圆形图标
    circleButton->setLeftIcon(createCircleIcon());

    // 3. 画椭圆按钮 - 只显示图标
    ellipseButton = new ArrowToolButton(this);
    ellipseButton->setCheckable(true);
    ellipseButton->setArrowClickEnabled(true);
    ellipseButton->setArrowAreaWidth(25);
    ellipseButton->setIconSize(QSize(24, 24));
    ellipseButton->setToolTip(QString::fromUtf8("画椭圆工具"));
    ellipseButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 设置一个简单的椭圆图标
    ellipseButton->setLeftIcon(createEllipseIcon());

    // 4. 画圆弧按钮 - 只显示图标
    arcButton = new ArrowToolButton(this);
    arcButton->setCheckable(true);
    arcButton->setArrowClickEnabled(true);
    arcButton->setArrowAreaWidth(25);
    arcButton->setIconSize(QSize(24, 24));
    arcButton->setToolTip(QString::fromUtf8("画圆弧工具"));
    arcButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 设置一个简单的圆弧图标
    arcButton->setLeftIcon(createArcIcon());

    // 5. 画面按钮
    faceButton = new ArrowToolButton(this);
    faceButton->setCheckable(true);
    faceButton->setArrowClickEnabled(true);
    faceButton->setArrowAreaWidth(25);
    faceButton->setIconSize(QSize(24, 24));
    faceButton->setToolTip(QString::fromUtf8("画面工具"));
    faceButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    faceButton->setLeftIcon(createRectangleIcon());

    // 6. Mark按钮
    markButton = new ArrowToolButton(this);
    markButton->setCheckable(true);
    markButton->setArrowClickEnabled(true);           // 启用下拉箭头弹出维度菜单
    markButton->setArrowAreaWidth(25);
    markButton->setIconSize(QSize(24, 24));
    markButton->setToolTip(QString::fromUtf8("标注工具"));
    markButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    markButton->setLeftIcon(createMarkIcon());

    // 7. Text按钮
    textButton = new ArrowToolButton(this);
    textButton->setCheckable(true);
    textButton->setArrowClickEnabled(false);
    textButton->setIconSize(QSize(24, 24));
    textButton->setToolTip(QString("Text tool"));
    textButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textButton->setLeftIcon(createTextIcon());

    // 将按钮添加到主布局
    mainLayout->addWidget(lineButton);
    mainLayout->addWidget(circleButton);
    mainLayout->addWidget(ellipseButton);
    mainLayout->addWidget(arcButton);
    mainLayout->addWidget(faceButton);

    mainLayout->addSpacing(15);

    mainLayout->addWidget(markButton);
    mainLayout->addWidget(textButton);

    mainLayout->addSpacing(12);

    copyOffsetBtn = new QToolButton(this);
    copyOffsetBtn->setCheckable(true);
    copyOffsetBtn->setAutoRaise(false);
    copyOffsetBtn->setToolTip(QString::fromUtf8("偏移复制"));
    copyOffsetBtn->setText(QString::fromUtf8("偏移"));
    copyOffsetBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    copyOffsetBtn->setIconSize(QSize(22, 22));
    copyOffsetBtn->setMinimumHeight(40);
    copyOffsetBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    copyOffsetBtn->setIcon(QIcon(":/IconShape/Paste.png"));

    copyRotateBtn = new QToolButton(this);
    copyRotateBtn->setCheckable(true);
    copyRotateBtn->setAutoRaise(false);
    copyRotateBtn->setToolTip(QString::fromUtf8("旋转复制"));
    copyRotateBtn->setText(QString::fromUtf8("旋转"));
    copyRotateBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    copyRotateBtn->setIconSize(QSize(22, 22));
    copyRotateBtn->setMinimumHeight(40);
    copyRotateBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    copyRotateBtn->setIcon(QIcon(":/IconShape/Rotate.png"));

    copyArrayBtn = new QToolButton(this);
    copyArrayBtn->setCheckable(true);
    copyArrayBtn->setAutoRaise(false);
    copyArrayBtn->setToolTip(QString::fromUtf8("阵列复制"));
    copyArrayBtn->setText(QString::fromUtf8("阵列"));
    copyArrayBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    copyArrayBtn->setIconSize(QSize(22, 22));
    copyArrayBtn->setMinimumHeight(40);
    copyArrayBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    copyArrayBtn->setIcon(QIcon(":/IconShape/Paste_Array.png"));

    const QString baseBtnStyle =
        "QToolButton{padding:4px 2px;border:1px solid #a8b9cd;border-radius:6px;background:#eef3f9;color:#213140;font-weight:600;}"
        "QToolButton:hover{border-color:#2f73ff;background:#e0ecff;}"
        "QToolButton:pressed{background:#d1e4ff;border-color:#1e5bd6;}"
        "QToolButton:checked{border:2px solid #0d55ff;background:#2f73ff;color:#ffffff;}";
    copyOffsetBtn->setStyleSheet(baseBtnStyle);
    copyRotateBtn->setStyleSheet(baseBtnStyle);
    copyArrayBtn->setStyleSheet(baseBtnStyle);

    mainLayout->addSpacing(6);
    mainLayout->addWidget(copyOffsetBtn);
    mainLayout->addWidget(copyRotateBtn);
    mainLayout->addWidget(copyArrayBtn);

    mainLayout->addStretch();

    // 创建弹出窗口
    circlePopup = new ShapeCirclePopup(this);
    circlePopup->hide();
    
    linePopup = new ShapeLinePopup(this);
    linePopup->hide();
    
    ellipsePopup = new ShapeEllipsePopup(this);
    ellipsePopup->hide();
    
    arcPopup = new ShapeArcPopup(this);
    arcPopup->hide();
    
    polygonPopup = new ShapePolygonPopup(this);
    polygonPopup->hide();

    dimensionPopup = new ShapeDimensionPopup(this);
    dimensionPopup->hide();

}

void ShapeToolBar::setupConnections()
{
    // 画线按钮需要连接两个信号：箭头点击和按钮主体点击
    connect(lineButton, SIGNAL(arrowClicked()),
            this, SLOT(onLineButtonArrowClicked()));
    connect(lineButton, SIGNAL(buttonClicked()),
            this, SLOT(onLineButtonBodyClicked()));
    connect(lineButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(linePopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onLinePopChange()));

    // 画圆按钮需要连接两个信号：箭头点击和按钮主体点击
    connect(circleButton, SIGNAL(arrowClicked()),
            this, SLOT(onCircleButtonArrowClicked()));
    connect(circleButton, SIGNAL(buttonClicked()),
            this, SLOT(onCircleButtonBodyClicked()));
    connect(circleButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(circlePopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onCirclePopChange()));
    
    // 画椭圆按钮需要连接两个信号：箭头点击和按钮主体点击
    connect(ellipseButton, SIGNAL(arrowClicked()),
            this, SLOT(onEllipseButtonArrowClicked()));
    connect(ellipseButton, SIGNAL(buttonClicked()),
            this, SLOT(onEllipseButtonBodyClicked()));
    connect(ellipseButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(ellipsePopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onEllipsePopChange()));
    
    // 画圆弧按钮需要连接两个信号：箭头点击和按钮主体点击
    connect(arcButton, SIGNAL(arrowClicked()),
            this, SLOT(onArcButtonArrowClicked()));
    connect(arcButton, SIGNAL(buttonClicked()),
            this, SLOT(onArcButtonBodyClicked()));
    connect(arcButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(arcPopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onArcPopChange()));
    
    // 画面按钮需要连接两个信号：箭头点击和按钮主体点击
    connect(faceButton, SIGNAL(arrowClicked()),
            this, SLOT(onFaceButtonArrowClicked()));
    connect(faceButton, SIGNAL(buttonClicked()),
            this, SLOT(onFaceButtonBodyClicked()));
    connect(faceButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(polygonPopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onPolygonPopChange()));
    

    connect(markButton, SIGNAL(arrowClicked()),
            this, SLOT(onMarkButtonArrowClicked()));
    connect(markButton, SIGNAL(buttonClicked()),
            this, SLOT(onMarkButtonBodyClicked()));
    connect(dimensionPopup, SIGNAL(buttonClicked(int)),
            this, SLOT(onDimPopChange()));
    connect(markButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));
    connect(textButton, SIGNAL(buttonClicked()),
            this, SLOT(onTextButtonClicked()));
    connect(textButton, SIGNAL(shouldClearState()),
            this, SLOT(clearCurrentShape()));

    // 复制按钮
    connect(copyOffsetBtn, SIGNAL(toggled(bool)),
            this, SLOT(onCopyOffsetToggled(bool)));
    connect(copyRotateBtn, SIGNAL(toggled(bool)),
            this, SLOT(onCopyRotateToggled(bool)));
    connect(copyArrayBtn, SIGNAL(toggled(bool)),
            this, SLOT(onCopyArrayToggled(bool)));
}

void ShapeToolBar::onLineButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] line tool arrow clicked");

    if (linePopup && linePopup->isVisible()) {
        // 如果面板已显示，则隐藏
        if (linePopup && linePopup->isVisible()) {
            linePopup->hide();
            lineButton->setChecked(false);
            qDebug() << QString::fromUtf8("[toolbar] hide line options panel");
        }
    } else {
        // 如果面板未显示，则显示
        if (linePopup) {
            linePopup->showNearWidget(lineButton);
            qDebug() << QString::fromUtf8("[toolbar] show line options panel");
            if (lineButton) lineButton->setChecked(true);
            m_DrawData.LineType = static_cast<ToolType>(linePopup->currentLine());
            m_DrawData.SelectMode = MODE_LINE;
            emit toolChanged(m_DrawData.LineType);
        }
    }
}

void ShapeToolBar::onLineButtonBodyClicked()
{
    // buttonClicked 在父类切换 checked 之前发出；取消选中走 shouldClearState -> clearCurrentShape
    m_DrawData.LineType = static_cast<ToolType>(linePopup->currentLine());
    m_DrawData.SelectMode = MODE_LINE;
    emit toolChanged(m_DrawData.LineType);
}

void ShapeToolBar::onCircleButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] circle tool arrow clicked");

    if (circlePopup && circlePopup->isVisible()) {
        // 如果面板已显示，则隐藏
        if (circlePopup && circlePopup->isVisible()) {
            circlePopup->hide();
            circleButton->setChecked(false);
            qDebug() << QString::fromUtf8("[toolbar] hide circle options panel");
        }
    } else {
        // 如果面板未显示，则显示
        if (circlePopup) {
            circlePopup->showNearWidget(circleButton);
            qDebug() << QString::fromUtf8("[toolbar] show circle options panel");
            if (circleButton) circleButton->setChecked(true);
            m_DrawData.CircleType = static_cast<ToolType>(circlePopup->currentCircle());
            m_DrawData.SelectMode = MODE_CIRCLE;
            if(TOOL_CENTER_RADIUS_CIRCLE == m_DrawData.CircleType)
            {
                emit radiusChange(circlePopup->currentRadius());
            }
            emit toolChanged(m_DrawData.CircleType);
        }
    }


}

void ShapeToolBar::onCircleButtonBodyClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] circle tool main button clicked");

    m_DrawData.CircleType = static_cast<ToolType>(circlePopup->currentCircle());
    if(TOOL_CENTER_RADIUS_CIRCLE == m_DrawData.CircleType)
    {
        emit radiusChange(circlePopup->currentRadius());
    }
    m_DrawData.SelectMode = MODE_CIRCLE;
    emit toolChanged(m_DrawData.CircleType);
}

void ShapeToolBar::onEllipseButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] ellipse tool arrow clicked");

    if (ellipsePopup && ellipsePopup->isVisible()) {
        // 如果面板已显示，则隐藏
        if (ellipsePopup && ellipsePopup->isVisible()) {
            ellipsePopup->hide();
            ellipseButton->setChecked(false);
            qDebug() << QString::fromUtf8("[toolbar] hide ellipse options panel");
        }
    } else {
        // 如果面板未显示，则显示
        if (ellipsePopup) {
            ellipsePopup->showNearWidget(ellipseButton);
            qDebug() << QString::fromUtf8("[toolbar] show ellipse options panel");
            if (ellipseButton) ellipseButton->setChecked(true);
            m_DrawData.EllipseType = static_cast<ToolType>(ellipsePopup->currentEllipse());
            m_DrawData.SelectMode = MODE_ELLIPSE;
            emit toolChanged(m_DrawData.EllipseType);
        }
    }
}

void ShapeToolBar::onEllipseButtonBodyClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] ellipse tool main button clicked");

    m_DrawData.EllipseType = static_cast<ToolType>(ellipsePopup->currentEllipse());
    m_DrawData.SelectMode = MODE_ELLIPSE;
    emit toolChanged(m_DrawData.EllipseType);
}

void ShapeToolBar::onArcButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] arc tool arrow clicked");

    if (arcPopup && arcPopup->isVisible()) {
        // 如果面板已显示，则隐藏
        if (arcPopup && arcPopup->isVisible()) {
            arcPopup->hide();
            arcButton->setChecked(false);
            qDebug() << QString::fromUtf8("[toolbar] hide arc options panel");
        }
    } else {
        // 如果面板未显示，则显示
        if (arcPopup) {
            arcPopup->showNearWidget(arcButton);
            qDebug() << QString::fromUtf8("[toolbar] show arc options panel");
            if (arcButton) arcButton->setChecked(true);
            m_DrawData.ArcType = static_cast<ToolType>(arcPopup->currentArc());
            m_DrawData.SelectMode = MODE_ARC;
            emit toolChanged(m_DrawData.ArcType);
        }
    }
}

void ShapeToolBar::onArcButtonBodyClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] arc tool main button clicked");

    m_DrawData.ArcType = static_cast<ToolType>(arcPopup->currentArc());
    m_DrawData.SelectMode = MODE_ARC;
    emit toolChanged(m_DrawData.ArcType);
}

void ShapeToolBar::onFaceButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] polygon tool arrow clicked");

    if (polygonPopup && polygonPopup->isVisible()) {
        // 如果面板已显示，则隐藏
        if (polygonPopup && polygonPopup->isVisible()) {
            polygonPopup->hide();
            faceButton->setChecked(false);
            qDebug() << QString::fromUtf8("[toolbar] hide polygon options panel");
        }
    } else {
        // 如果面板未显示，则显示
        if (polygonPopup) {
            polygonPopup->showNearWidget(faceButton);
            qDebug() << QString::fromUtf8("[toolbar] show polygon options panel");
            if (faceButton) faceButton->setChecked(true);
            ToolType selectedType = static_cast<ToolType>(polygonPopup->currentPolygon());
            if(selectedType == TOOL_RECTANGLE || selectedType == TOOL_RECTANGLE_CENTER_WH)
            {
                m_DrawData.RectType = selectedType;
                m_DrawData.SelectMode = MODE_RECT;
                emit toolChanged(m_DrawData.RectType);
            }
            else
            {
                m_DrawData.PolygonType = selectedType;
                m_DrawData.SelectMode = MODE_POLYGON;
                emit toolChanged(m_DrawData.PolygonType);
            }
        }
    }
}

void ShapeToolBar::onFaceButtonBodyClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] polygon tool main button clicked");

    ToolType selectedType = static_cast<ToolType>(polygonPopup->currentPolygon());

    if(selectedType == TOOL_RECTANGLE || selectedType == TOOL_RECTANGLE_CENTER_WH)
    {
        m_DrawData.RectType = selectedType;
        m_DrawData.SelectMode = MODE_RECT;
        emit toolChanged(m_DrawData.RectType);
    }
    else
    {
        m_DrawData.PolygonType = selectedType;
        m_DrawData.SelectMode = MODE_POLYGON;
        emit toolChanged(m_DrawData.PolygonType);
    }
}

void ShapeToolBar::onMarkButtonArrowClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] dimension tool arrow clicked");

    if (!dimensionPopup) dimensionPopup = new ShapeDimensionPopup(this);
    if (dimensionPopup->isVisible()) {
        dimensionPopup->hide();
        if (markButton) markButton->setChecked(false);
        return;
    }

    dimensionPopup->setCurrentDimension(m_DrawData.DimType == TOOL_NONE ? TOOL_DIM_ALIGN : m_DrawData.DimType);
    dimensionPopup->showNearWidget(markButton);
    if (markButton) markButton->setChecked(true);
    m_DrawData.DimType = static_cast<ToolType>(dimensionPopup->currentDimension());
    m_DrawData.SelectMode = MODE_MARK;
    emit toolChanged(m_DrawData.DimType);
}

void ShapeToolBar::onMarkButtonBodyClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] dimension tool main button clicked");

    if (!dimensionPopup) dimensionPopup = new ShapeDimensionPopup(this);
    m_DrawData.DimType = static_cast<ToolType>(m_DrawData.DimType == TOOL_NONE ? TOOL_DIM_ALIGN : dimensionPopup->currentDimension());
    dimensionPopup->setCurrentDimension(m_DrawData.DimType);
    m_DrawData.SelectMode = MODE_MARK;
    emit toolChanged(m_DrawData.DimType);
}

void ShapeToolBar::onDimPopChange()
{
    if (!dimensionPopup) return;
    int dimTool = dimensionPopup->currentDimension();
    m_DrawData.DimType = static_cast<ToolType>(dimTool);
    m_DrawData.SelectMode = MODE_MARK;
    emit toolChanged(m_DrawData.DimType);
}

void ShapeToolBar::onTextButtonClicked()
{
    qDebug() << QString::fromUtf8("[toolbar] Text tool selected");

    m_DrawData.TextType = TOOL_TEXT;
    m_DrawData.SelectMode = MODE_TEXT;

    // 点击文字按钮时弹出dialog
    TextInputDialog dialog;
    if (dialog.exec() == QDialog::Accepted) {
        // 用户点击确定，保存文字参数
        QString text = dialog.text();
        QFont font = dialog.font();
        int alignment = dialog.alignment();

        if (!text.isEmpty()) {
            CanvasShapeText::setTextParams(text, font, alignment);
            emit toolChanged(m_DrawData.TextType);
        } else {
            m_DrawData.SelectMode = MODE_SELECT;
            if (textButton) textButton->setChecked(false);
            emit toolChanged(TOOL_NONE);
        }
    } else {
        m_DrawData.SelectMode = MODE_SELECT;
        if (textButton) textButton->setChecked(false);
        emit toolChanged(TOOL_NONE);
    }
}

void ShapeToolBar::onShapeCirclePopupClosed()
{
    // 弹出窗口关闭时的处理
    qDebug() << QString::fromUtf8("[toolbar] options panel closed");
    circleButton->setChecked(false);
}

// 重写离开事件
void ShapeToolBar::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
}

// 创建线形图标
QIcon ShapeToolBar::createLineIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawLine(6, 12, 18, 12);

    return QIcon(pixmap);
}

// 创建圆形图标
QIcon ShapeToolBar::createCircleIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRect(6, 6, 12, 12));

    return QIcon(pixmap);
}

// 创建椭圆图标
QIcon ShapeToolBar::createEllipseIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    // 绘制一个椭圆（宽度大于高度）
    painter.drawEllipse(QRect(4, 7, 16, 10));

    return QIcon(pixmap);
}

// 创建圆弧图标
QIcon ShapeToolBar::createArcIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    // 绘制一个圆弧（从左上到右下）
    QRectF rect(4, 4, 16, 16);
    painter.drawArc(rect, 45 * 16, 180 * 16);  // 从45度开始，绘制180度

    return QIcon(pixmap);
}

// 创建矩形图标
QIcon ShapeToolBar::createRectangleIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(6, 6, 12, 12));

    return QIcon(pixmap);
}

// 创建标记图标
QIcon ShapeToolBar::createMarkIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawEllipse(QPoint(12, 8), 3, 3);
    painter.drawLine(12, 11, 12, 16);

    return QIcon(pixmap);
}

// 创建文字图标
QIcon ShapeToolBar::createTextIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));

    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, "T");

    return QIcon(pixmap);
}
/*    MODE_LINE = 0,      // 直线模式
    MODE_RECT,      // 矩形模式
    MODE_CIRCLE,    // 圆形模式
    MODE_POLYGON,   // 多边形模式
    MODE_MARK,
    MODE_TEXT,
    MODE_SELECT     // 选择模式*/
void ShapeToolBar::onCirclePopChange(){
    // 立即更新内部变量，使其与pop中的选择保持一致
    m_DrawData.CircleType = static_cast<ToolType>(circlePopup->currentCircle());
    
    // 如果当前已经是画圆模式，发出信号通知外部更新
    if(MODE_CIRCLE == m_DrawData.SelectMode){
        emit toolChanged(m_DrawData.CircleType);
        if(TOOL_CENTER_RADIUS_CIRCLE == m_DrawData.CircleType){
            emit radiusChange(circlePopup->currentRadius());
        }
    }
}

void ShapeToolBar::onLinePopChange(){
    // 立即更新内部变量，使其与pop中的选择保持一致
    m_DrawData.LineType = static_cast<ToolType>(linePopup->currentLine());
    
    // 如果当前已经是画线模式，发出信号通知外部更新
    if(MODE_LINE == m_DrawData.SelectMode){
        emit toolChanged(m_DrawData.LineType);
    }
}

void ShapeToolBar::onPolygonPopChange(){
    // 立即更新内部变量，使其与pop中的选择保持一致
    ToolType selectedType = static_cast<ToolType>(polygonPopup->currentPolygon());
    
    if(selectedType == TOOL_RECTANGLE || selectedType == TOOL_RECTANGLE_CENTER_WH)
    {
        m_DrawData.RectType = selectedType;
        // 如果当前已经是画面模式，发出信号通知外部更新
        if(MODE_RECT == m_DrawData.SelectMode){
            emit toolChanged(m_DrawData.RectType);
        }
    }
    else
    {
        m_DrawData.PolygonType = selectedType;
        // 如果当前已经是多边形模式，发出信号通知外部更新
        if(MODE_POLYGON == m_DrawData.SelectMode){
            emit toolChanged(m_DrawData.PolygonType);
        }
    }
}

void ShapeToolBar::onEllipsePopChange(){
    // 立即更新内部变量，使其与pop中的选择保持一致
    m_DrawData.EllipseType = static_cast<ToolType>(ellipsePopup->currentEllipse());
    
    // 如果当前已经是画椭圆模式，发出信号通知外部更新
    if(MODE_ELLIPSE == m_DrawData.SelectMode){
        emit toolChanged(m_DrawData.EllipseType);
    }
}

void ShapeToolBar::onArcPopChange(){
    // 立即更新内部变量，使其与pop中的选择保持一致
    m_DrawData.ArcType = static_cast<ToolType>(arcPopup->currentArc());
    
    // 如果当前已经是画圆弧模式，发出信号通知外部更新
    if(MODE_ARC == m_DrawData.SelectMode){
        emit toolChanged(m_DrawData.ArcType);
    }
}

// 兼容旧槽函数，委托给主体点击逻辑
void ShapeToolBar::onLineButtonClicked()
{
    onLineButtonBodyClicked();
}

void ShapeToolBar::onFaceButtonClicked()
{
    onFaceButtonBodyClicked();
}

ToolType ShapeToolBar::getCurrentShape(){
    switch(m_DrawData.SelectMode){
    case MODE_LINE:
        return m_DrawData.LineType;
        break;
    case MODE_RECT:
        return m_DrawData.RectType;
        break;
    case MODE_CIRCLE:
        return m_DrawData.CircleType;
        qDebug() << m_DrawData.CircleType << "m_DrawData.CircleType";
        break;
    case MODE_ELLIPSE:
        return m_DrawData.EllipseType;
        break;
    case MODE_ARC:
        return m_DrawData.ArcType;
        break;
    case MODE_POLYGON:
        return m_DrawData.PolygonType;
        break;
    case MODE_TEXT:
        return m_DrawData.TextType;
        break;
    case MODE_MARK:
        // 将 Mark 模式作为“测距”工具返回
        return m_DrawData.DimType;
        break;
    default:
        // 默认返回直线模式或者其他合适的默认值
        return TOOL_NONE;
        break;
    }
}

double ShapeToolBar::getRadius()
{
    return circlePopup->currentRadius();
}

double ShapeToolBar::getCenterX()
{
    return circlePopup->currentCenterX();
}

double ShapeToolBar::getCenterY()
{
    return circlePopup->currentCenterY();
}

double ShapeToolBar::getRectangleParamCenterX() const
{
    return polygonPopup ? polygonPopup->currentRectCenterX() : 0.0;
}

double ShapeToolBar::getRectangleParamCenterY() const
{
    return polygonPopup ? polygonPopup->currentRectCenterY() : 0.0;
}

double ShapeToolBar::getRectangleParamLength() const
{
    return polygonPopup ? polygonPopup->currentRectLength() : 100.0;
}

double ShapeToolBar::getRectangleParamWidth() const
{
    return polygonPopup ? polygonPopup->currentRectWidth() : 80.0;
}

int ShapeToolBar::getSideCount()
{
    return polygonPopup->currentSideCount();
}

double ShapeToolBar::getOffsetDistance()
{
    return linePopup->currentOffsetDistance();
}

void ShapeToolBar::clearCurrentShape(){
    m_DrawData.SelectMode = MODE_SELECT;
    // 同步清除形状按钮的选中样式，避免UI仍显示为选中
    if (lineButton) lineButton->setChecked(false);
    if (circleButton) circleButton->setChecked(false);
    if (ellipseButton) ellipseButton->setChecked(false);
    if (arcButton) arcButton->setChecked(false);
    if (faceButton) faceButton->setChecked(false);
    if (markButton) markButton->setChecked(false);
    if (textButton) textButton->setChecked(false);
    emit toolChanged(TOOL_NONE);
}

void ShapeToolBar::resetExclusiveDrawModeToNone()
{
    if (m_DrawData.SelectMode == MODE_SELECT)
        return;
    m_DrawData.SelectMode = MODE_SELECT;
    emit toolChanged(TOOL_NONE);
}

void ShapeToolBar::clearAllStates()
{
    m_DrawData.SelectMode = MODE_SELECT;
    if (lineButton) lineButton->setChecked(false);
    if (circleButton) circleButton->setChecked(false);
    if (ellipseButton) ellipseButton->setChecked(false);
    if (arcButton) arcButton->setChecked(false);
    if (faceButton) faceButton->setChecked(false);
    if (markButton) markButton->setChecked(false);
    if (textButton) textButton->setChecked(false);
    emit toolChanged(TOOL_NONE);
}

void ShapeToolBar::clearCopyStates(){
    if (copyOffsetBtn) copyOffsetBtn->setChecked(false);
    if (copyRotateBtn) copyRotateBtn->setChecked(false);
    if (copyArrayBtn) copyArrayBtn->setChecked(false);
}

void ShapeToolBar::onCopyOffsetToggled(bool checked){
    if (checked){
        clearCurrentShape();
    }
}

void ShapeToolBar::onCopyRotateToggled(bool checked){
    if (checked){
        clearCurrentShape();
    }
}

void ShapeToolBar::onCopyArrayToggled(bool checked){
    if (checked){
        clearCurrentShape();
    }
}

void ShapeToolBar::showPopupNearWidget(int toolIndex, QWidget* nearWidget)
{
    if (!nearWidget)
        return;
    switch (toolIndex) {
    case 0: // 线
        if (linePopup) linePopup->showNearWidget(nearWidget);
        break;
    case 1: // 圆
        if (circlePopup) circlePopup->showNearWidget(nearWidget);
        break;
    case 2: // 椭圆
        if (ellipsePopup) ellipsePopup->showNearWidget(nearWidget);
        break;
    case 3: // 弧
        if (arcPopup) arcPopup->showNearWidget(nearWidget);
        break;
    case 4: // 面(多边形)
        if (polygonPopup) polygonPopup->showNearWidget(nearWidget);
        break;
    case 5: // 标注
        if (dimensionPopup) dimensionPopup->showNearWidget(nearWidget);
        break;
    }
}

void ShapeToolBar::triggerToolBodyClick(int toolIndex)
{
    switch (toolIndex) {
    case 0: // 线
        if (linePopup) {
            m_DrawData.LineType = static_cast<ToolType>(linePopup->currentLine());
            m_DrawData.SelectMode = MODE_LINE;
            emit toolChanged(m_DrawData.LineType);
        }
        break;
    case 1: // 圆
        if (circlePopup) {
            m_DrawData.CircleType = static_cast<ToolType>(circlePopup->currentCircle());
            m_DrawData.SelectMode = MODE_CIRCLE;
            emit toolChanged(m_DrawData.CircleType);
        }
        break;
    case 2: // 椭圆
        if (ellipsePopup) {
            m_DrawData.EllipseType = static_cast<ToolType>(ellipsePopup->currentEllipse());
            m_DrawData.SelectMode = MODE_ELLIPSE;
            emit toolChanged(m_DrawData.EllipseType);
        }
        break;
    case 3: // 弧
        if (arcPopup) {
            m_DrawData.ArcType = static_cast<ToolType>(arcPopup->currentArc());
            m_DrawData.SelectMode = MODE_ARC;
            emit toolChanged(m_DrawData.ArcType);
        }
        break;
    case 4: // 面
        if (polygonPopup) {
            m_DrawData.PolygonType = static_cast<ToolType>(polygonPopup->currentPolygon());
            m_DrawData.SelectMode = MODE_POLYGON;
            emit toolChanged(m_DrawData.PolygonType);
        }
        break;
    case 5: // 标注
        if (dimensionPopup) {
            m_DrawData.DimType = static_cast<ToolType>(dimensionPopup->currentDimension());
            m_DrawData.SelectMode = MODE_DIMENSION;
            emit toolChanged(m_DrawData.DimType);
        }
        break;
    case 6: // 文字
        m_DrawData.TextType = TOOL_TEXT;
        m_DrawData.SelectMode = MODE_TEXT;
        emit toolChanged(m_DrawData.TextType);
        break;
    }
}
