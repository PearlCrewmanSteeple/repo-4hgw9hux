#ifndef SHAPETOOLBAR_H
#define SHAPETOOLBAR_H

#include <QWidget>
#include <QIcon>
#include <QStringList>
#include "common.h"
#include "ShapeCirclePopup.h"
#include "ShapeLinePopup.h"
#include "ShapePolygonPopup.h"
#include "ShapeEllipsePopup.h"
#include "ShapeArcPopup.h"
#include "ShapeDimensionPopup.h"

// 前向声明
//class ShapeCirclePopup;
#include "ArrowToolButton.h"
#include <QAbstractButton>
#include <QToolButton>
class QVBoxLayout;

class ShapeToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit ShapeToolBar(QWidget *parent = 0);
    ~ShapeToolBar();

    ToolType getCurrentShape();
    double getRadius();
    double getCenterX();
    double getCenterY();
    double getRectangleParamCenterX() const;
    double getRectangleParamCenterY() const;
    double getRectangleParamLength() const;
    double getRectangleParamWidth() const;
    int getSideCount();
    double getOffsetDistance();
    
    // 清除所有状态（用于互斥）
    void clearAllStates();
    /** 非绘图工具独占时复位内部绘图模式并发 TOOL_NONE（不操作按钮，由主窗口 QButtonGroup 负责勾选） */
    void resetExclusiveDrawModeToNone();
    QAbstractButton* buttonLine() const { return static_cast<QAbstractButton*>(lineButton); }
    QAbstractButton* buttonCircle() const { return static_cast<QAbstractButton*>(circleButton); }
    QAbstractButton* buttonEllipse() const { return static_cast<QAbstractButton*>(ellipseButton); }
    QAbstractButton* buttonArc() const { return static_cast<QAbstractButton*>(arcButton); }
    QAbstractButton* buttonFace() const { return static_cast<QAbstractButton*>(faceButton); }
    QAbstractButton* buttonMark() const { return static_cast<QAbstractButton*>(markButton); }
    QAbstractButton* buttonText() const { return static_cast<QAbstractButton*>(textButton); }
    QAbstractButton* buttonCopyOffset() const { return static_cast<QAbstractButton*>(copyOffsetBtn); }
    QAbstractButton* buttonCopyRotate() const { return static_cast<QAbstractButton*>(copyRotateBtn); }
    QAbstractButton* buttonCopyArray() const { return static_cast<QAbstractButton*>(copyArrayBtn); }

    /**
     * @brief 在指定 widget 旁边显示对应工具的弹出面板（供小屏幕横向工具栏使用）
     * @param toolIndex 工具索引: 0=线, 1=圆, 2=椭圆, 3=弧, 4=面(多边形), 5=标注
     * @param nearWidget 弹出面板锚定的 widget
     */
    void showPopupNearWidget(int toolIndex, QWidget* nearWidget);

    /**
     * @brief 触发指定工具的主体点击逻辑（设置绘图模式并 emit toolChanged）
     * @param toolIndex 工具索引: 0=线, 1=圆, 2=椭圆, 3=弧, 4=面, 5=标注, 6=文字
     */
    void triggerToolBodyClick(int toolIndex);
    
public slots:
    // 清除当前选中的形状（公共槽函数，供外部调用和信号连接）
    void clearCurrentShape();
    // 清除复制模式（公共槽函数，供互斥时调用）
    void clearCopyStates();
protected:
    // 重写离开事件
    void leaveEvent(QEvent *event);

signals:
    void toolChanged(ToolType);
    void radiusChange(double);
private slots:
    // 按钮点击槽函数
    void onLineButtonClicked();
    void onFaceButtonClicked();
    void onTextButtonClicked();

    // 圆按钮的特殊槽函数
    void onCircleButtonArrowClicked();
    void onCircleButtonBodyClicked();

    // 线按钮的特殊槽函数
    void onLineButtonArrowClicked();
    void onLineButtonBodyClicked();

    // 矩形/多边形按钮的特殊槽函数
    void onFaceButtonArrowClicked();
    void onFaceButtonBodyClicked();

    // 椭圆按钮的特殊槽函数
    void onEllipseButtonArrowClicked();
    void onEllipseButtonBodyClicked();

    // 圆弧按钮的特殊槽函数
    void onArcButtonArrowClicked();
    void onArcButtonBodyClicked();
    void onMarkButtonArrowClicked();
    void onMarkButtonBodyClicked();
    void onDimPopChange();

    // 弹出窗口关闭处理
    void onShapeCirclePopupClosed();
    void onCirclePopChange();
    void onLinePopChange();
    void onPolygonPopChange();
    void onEllipsePopChange();
    void onArcPopChange();
    // 复制按钮
    void onCopyOffsetToggled(bool checked);
    void onCopyRotateToggled(bool checked);
    void onCopyArrayToggled(bool checked);
    
private:
    // UI初始化函数
    void setupUI();
    void setupConnections();
    void setupChineseTexts();

    // 图标创建函数
    QIcon createLineIcon();
    QIcon createCircleIcon();
    QIcon createEllipseIcon();
    QIcon createArcIcon();
    QIcon createRectangleIcon();
    QIcon createMarkIcon();
    QIcon createTextIcon();

    // 工具栏按钮
    ArrowToolButton *lineButton;
    ArrowToolButton *circleButton;
    ArrowToolButton *ellipseButton;
    ArrowToolButton *arcButton;
    ArrowToolButton *faceButton;
    ArrowToolButton *markButton;
    ArrowToolButton *textButton;
    ShapeDimensionPopup *dimensionPopup;

    // 圆选项弹出窗口
    ShapeCirclePopup *circlePopup;
    // 线选项弹出窗口
    ShapeLinePopup *linePopup;
    // 椭圆选项弹出窗口
    ShapeEllipsePopup *ellipsePopup;
    // 圆弧选项弹出窗口
    ShapeArcPopup *arcPopup;
    // 多边形选项弹出窗口
    ShapePolygonPopup *polygonPopup;

    // 布局
    QVBoxLayout *mainLayout;

    // 文本列表
    QStringList chineseTexts;

    // 复制按钮
    QToolButton* copyOffsetBtn;
    QToolButton* copyRotateBtn;
    QToolButton* copyArrayBtn;

    struct DrawBarData{
    DrawMode SelectMode;
    ToolType LineType;
    ToolType CircleType;
    ToolType EllipseType;
    ToolType ArcType;
    ToolType RectType;
    ToolType PolygonType;
    ToolType TextType;
    ToolType DimType;
    int Radius;
    };
    DrawBarData m_DrawData;

};

#endif // SHAPETOOLBAR_H
