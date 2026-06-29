#ifndef APPUISETTINGS_H
#define APPUISETTINGS_H

#include <QString>
#include <QMap>
#include <QList>

/**
 * @brief 应用界面与杂项配置（单一 JSON；路径与探测顺序见 AppFileLayout::appUiSettings*）
 * 支持：工艺工具栏显示/隐藏、刀具库 FPath、主窗分割条尺寸、图元表列宽等。
 */
class AppUiSettings
{
public:
    static AppUiSettings& instance();

    void reload();
    bool save();

    /// 未在配置中显式设为 false 的工艺视为显示
    bool isCraftVisible(const QString& craftType) const;

    void setCraftVisible(const QString& craftType, bool visible);
    void clearCraftVisibilityOverrides();

    /// 刀具库半径寄存器分组索引 FPath，默认 1；RNo 基址与步长见 cnc.ini [ToolLibrary]
    int toolLibraryFPath() const { return m_toolLibraryFPath; }
    void setToolLibraryFPath(int fpath);

    /** 主界面 QSplitter::objectName() 对应的一组 sizes（与子窗格数量一致时才会被应用） */
    QList<int> splitterSizes(const QString& objectName) const;
    void setSplitterSizes(const QString& objectName, const QList<int>& sizes);

    /** 图元表（GraphicsItemTableView）列宽，顺序为列 0..n-1 */
    QList<int> graphicsItemTableColumnWidths() const { return m_graphicsItemTableColumnWidths; }
    void setGraphicsItemTableColumnWidths(const QList<int>& widths);
    bool lowPerformanceCanvasMode() const { return m_lowPerformanceCanvasMode; }
    bool canvasPerfLoggingEnabled() const { return m_canvasPerfLoggingEnabled; }
    int canvasMouseMoveThrottleMs() const;
    int canvasMouseMovePixelThreshold() const;
    int canvasHoverDelayMs() const;

    /// 开放图元「粘合」端点容差（mm），同 AutoCAD Join fuzz；默认 1.0
    double joinOpenEntitiesFuzzMm() const { return m_joinOpenEntitiesFuzzMm; }
    void setJoinOpenEntitiesFuzzMm(double mm);

    /// 小屏幕模式：隐藏左侧竖向形状栏，顶部切换 filebar/横向形状栏
    bool smallScreenMode() const { return m_smallScreenMode; }
    void setSmallScreenMode(bool enabled) { m_smallScreenMode = enabled; }

    /** 写入目标路径（首次保存前由 resolveSavePath 决定） */
    QString settingsFilePath() const { return m_activePath; }

private:
    AppUiSettings();
    AppUiSettings(const AppUiSettings&);
    AppUiSettings& operator=(const AppUiSettings&);

    QString resolveSavePath() const;
    QStringList candidateReadPaths() const;
    bool loadFromFile(const QString& path);
    static QString jsonEscape(const QString& s);

    QMap<QString, bool> m_craftHidden;
    QMap<QString, QList<int> > m_splitterSizes;
    QList<int> m_graphicsItemTableColumnWidths;
    QString m_activePath;
    int m_toolLibraryFPath;
    bool m_lowPerformanceCanvasMode;
    bool m_canvasPerfLoggingEnabled;
    int m_canvasMouseMoveThrottleMs;
    int m_canvasMouseMovePixelThreshold;
    int m_canvasHoverDelayMs;
    double m_joinOpenEntitiesFuzzMm;
    bool m_smallScreenMode;
};

#endif
