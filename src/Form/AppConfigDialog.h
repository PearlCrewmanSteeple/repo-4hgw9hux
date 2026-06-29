#ifndef APPCONFIGDIALOG_H
#define APPCONFIGDIALOG_H

#include "BaseDialog.h"
#include <QMap>

class CraftToolBar;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QTabWidget;

/**
 * @brief 应用配置总入口（BaseDialog）
 * 集中管理界面相关 JSON/选项；首版支持工艺侧栏显示项，后续可增页签。
 */
class AppConfigDialog : public BaseDialog
{
    Q_OBJECT

public:
    explicit AppConfigDialog(CraftToolBar* craftToolBar, QWidget* parent = NULL);

signals:
    void smallScreenModeChanged(bool enabled);

private slots:
    void onSelectAllCrafts();
    void onDeselectAllCrafts();
    void onResetCraftVisibility();
    void onOkClicked();

private:
    void setupUi();
    static QString displayNameForCraftType(const QString& craftType);

    CraftToolBar* m_craftToolBar;
    QTabWidget* m_tabs;
    QMap<QString, QCheckBox*> m_craftChecks;
    QSpinBox* m_toolLibraryFPathSpin;
    QDoubleSpinBox* m_joinOpenEntitiesFuzzSpin;
    QCheckBox* m_smallScreenCheck;
};

#endif
