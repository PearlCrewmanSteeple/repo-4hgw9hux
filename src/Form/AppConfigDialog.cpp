#include "AppConfigDialog.h"
#include "../AppUiSettings.h"
#include "../CraftControl/CraftFactory.h"
#include "../CraftControl/CraftToolBar.h"
#include "../CraftControl/ToolLibrary/ToolLibrary.h"
#include "../Logger.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QRegExp>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

AppConfigDialog::AppConfigDialog(CraftToolBar* craftToolBar, QWidget* parent)
    : BaseDialog(parent)
    , m_craftToolBar(craftToolBar)
    , m_tabs(NULL)
    , m_toolLibraryFPathSpin(NULL)
    , m_joinOpenEntitiesFuzzSpin(NULL)
    , m_smallScreenCheck(NULL)
{
    setWindowTitle(QString::fromUtf8("应用配置"));
    setModal(true);
    setMinimumSize(520, 440);
    enableMinimizeButton(false);
    enableMaximizeButton(false);

    AppUiSettings::instance().reload();
    setupUi();
}

QString AppConfigDialog::displayNameForCraftType(const QString& craftType)
{
    QString craftName = CraftFactory::getCraftName(craftType);
    if (craftName.isEmpty())
        craftName = craftType;
    if (craftType.startsWith(QLatin1String("G"))) {
        QRegExp gCodePattern(QLatin1String("^G\\d+\\s+(.+)$"));
        if (gCodePattern.exactMatch(craftName))
            craftName = gCodePattern.cap(1);
    }
    return craftName;
}

void AppConfigDialog::setupUi()
{
    QWidget* cw = contentWidget();
    if (!cw)
        return;

    QVBoxLayout* root = new QVBoxLayout(cw);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    m_tabs = new QTabWidget(cw);

    QWidget* craftPage = new QWidget(m_tabs);
    QVBoxLayout* craftLay = new QVBoxLayout(craftPage);
    craftLay->setContentsMargins(8, 8, 8, 8);

    QLabel* hint = new QLabel(QString::fromUtf8(
        "勾选表示在工艺侧栏显示该项；取消勾选即隐藏（已配置的工艺数据不受影响）。"),
        craftPage);
    hint->setWordWrap(true);
    craftLay->addWidget(hint);

    QScrollArea* scroll = new QScrollArea(craftPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget* scrollInner = new QWidget(scroll);
    QVBoxLayout* innerLay = new QVBoxLayout(scrollInner);
    innerLay->setContentsMargins(4, 4, 4, 4);
    innerLay->setSpacing(6);

    m_craftChecks.clear();
    QList<QString> types = CraftFactory::getSupportedCrafts();
    foreach (const QString& craftType, types) {
        QCheckBox* cb = new QCheckBox(displayNameForCraftType(craftType), scrollInner);
        cb->setProperty("craftType", craftType);
        cb->setChecked(AppUiSettings::instance().isCraftVisible(craftType));
        innerLay->addWidget(cb);
        m_craftChecks.insert(craftType, cb);
    }
    innerLay->addStretch();
    scroll->setWidget(scrollInner);
    craftLay->addWidget(scroll);

    QHBoxLayout* craftBtnRow = new QHBoxLayout();
    QPushButton* allBtn = new QPushButton(QString::fromUtf8("全选"), craftPage);
    QPushButton* noneBtn = new QPushButton(QString::fromUtf8("全不选"), craftPage);
    QPushButton* resetBtn = new QPushButton(QString::fromUtf8("恢复默认（全部显示）"), craftPage);
    connect(allBtn, SIGNAL(clicked()), this, SLOT(onSelectAllCrafts()));
    connect(noneBtn, SIGNAL(clicked()), this, SLOT(onDeselectAllCrafts()));
    connect(resetBtn, SIGNAL(clicked()), this, SLOT(onResetCraftVisibility()));
    craftBtnRow->addWidget(allBtn);
    craftBtnRow->addWidget(noneBtn);
    craftBtnRow->addWidget(resetBtn);
    craftBtnRow->addStretch();
    craftLay->addLayout(craftBtnRow);

    m_tabs->addTab(craftPage, QString::fromUtf8("工艺显示"));

    QWidget* toolLibPage = new QWidget(m_tabs);
    QVBoxLayout* tlRoot = new QVBoxLayout(toolLibPage);
    tlRoot->setContentsMargins(8, 8, 8, 8);
    QFormLayout* tlForm = new QFormLayout();
    m_toolLibraryFPathSpin = new QSpinBox(toolLibPage);
    m_toolLibraryFPathSpin->setRange(1, 70000);
    m_toolLibraryFPathSpin->setValue(AppUiSettings::instance().toolLibraryFPath());
    tlForm->addRow(QString::fromUtf8("FPath"), m_toolLibraryFPathSpin);
    tlRoot->addLayout(tlForm);
    QLabel* tlHint = new QLabel(
        QString::fromUtf8(
            "刀半径同步：先读 R(50146,0)，为 0 时从 R[rno] 取数，非 0 时从 F[rno+20000000] 取数；寄存器值为直径且以千分之一 mm 存储时，写入刀具库为 读值÷2÷1000（mm 半径）。"
            "rno = f_radius_base_addr + (FPath \xe2\x88\x92 1) \xc3\x97 f_radius_fpath_stride + RowIndex（RowIndex = T 后刀号 \xe2\x88\x92 1，如 T3\xe2\x86\x922）。"
            "f_radius_* 在 cnc.ini 的 [ToolLibrary] 中配置，缺省 3101001 / 30000。修改 FPath 并确定后按新 rno 同步。"),
        toolLibPage);
    tlHint->setWordWrap(true);
    tlRoot->addWidget(tlHint);
    tlRoot->addStretch();
    m_tabs->addTab(toolLibPage, QString::fromUtf8("刀具库"));

    QWidget* cadPage = new QWidget(m_tabs);
    QVBoxLayout* cadRoot = new QVBoxLayout(cadPage);
    cadRoot->setContentsMargins(8, 8, 8, 8);
    QFormLayout* cadForm = new QFormLayout();
    m_joinOpenEntitiesFuzzSpin = new QDoubleSpinBox(cadPage);
    m_joinOpenEntitiesFuzzSpin->setRange(0.001, 1000.0);
    m_joinOpenEntitiesFuzzSpin->setDecimals(3);
    m_joinOpenEntitiesFuzzSpin->setSingleStep(0.1);
    m_joinOpenEntitiesFuzzSpin->setSuffix(QString::fromUtf8(" mm"));
    m_joinOpenEntitiesFuzzSpin->setValue(AppUiSettings::instance().joinOpenEntitiesFuzzMm());
    cadForm->addRow(QString::fromUtf8("粘合容差（fuzz）"), m_joinOpenEntitiesFuzzSpin);
    cadRoot->addLayout(cadForm);
    QLabel* cadHint = new QLabel(
        QString::fromUtf8(
            "开放线/弧/多段线点击「粘合」时，两端点距离不超过此值即视为可拼接；"
            "拼完后若整条链首尾也在此距离内，则自动标记为闭合轮廓。"),
        cadPage);
    cadHint->setWordWrap(true);
    cadRoot->addWidget(cadHint);
    cadRoot->addStretch();
    m_tabs->addTab(cadPage, QString::fromUtf8("CAD"));

    // --- 界面布局页签 ---
    QWidget* layoutPage = new QWidget(m_tabs);
    QVBoxLayout* layoutRoot = new QVBoxLayout(layoutPage);
    layoutRoot->setContentsMargins(8, 8, 8, 8);
    layoutRoot->setSpacing(10);

    m_smallScreenCheck = new QCheckBox(QString::fromUtf8("小屏幕模式"), layoutPage);
    m_smallScreenCheck->setChecked(AppUiSettings::instance().smallScreenMode());
    layoutRoot->addWidget(m_smallScreenCheck);

    QLabel* ssHint = new QLabel(
        QString::fromUtf8(
            "启用后隐藏左侧竖向形状工具栏，在顶部 FileBar 区域增加切换按钮，"
            "可在文件栏与横向形状栏之间切换，节省横向空间。"),
        layoutPage);
    ssHint->setWordWrap(true);
    layoutRoot->addWidget(ssHint);
    layoutRoot->addStretch();
    m_tabs->addTab(layoutPage, QString::fromUtf8("界面布局"));

    QWidget* futurePage = new QWidget(m_tabs);
    QVBoxLayout* fpLay = new QVBoxLayout(futurePage);
    fpLay->setContentsMargins(8, 8, 8, 8);
    QLabel* fpLabel = new QLabel(
        QString::fromUtf8("其它配置文件相关选项（如图层默认值、路径等）可在此后续增加页签。"),
        futurePage);
    fpLabel->setWordWrap(true);
    fpLay->addWidget(fpLabel);
    fpLay->addStretch();
    m_tabs->addTab(futurePage, QString::fromUtf8("更多（预留）"));

    root->addWidget(m_tabs);

    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->addStretch();
    QPushButton* okBtn = new QPushButton(QString::fromUtf8("确定"), cw);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8("取消"), cw);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkClicked()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
    bottom->addWidget(okBtn);
    bottom->addWidget(cancelBtn);
    root->addLayout(bottom);
}

void AppConfigDialog::onSelectAllCrafts()
{
    QMapIterator<QString, QCheckBox*> it(m_craftChecks);
    while (it.hasNext()) {
        it.next();
        if (it.value())
            it.value()->setChecked(true);
    }
}

void AppConfigDialog::onDeselectAllCrafts()
{
    QMapIterator<QString, QCheckBox*> it(m_craftChecks);
    while (it.hasNext()) {
        it.next();
        if (it.value())
            it.value()->setChecked(false);
    }
}

void AppConfigDialog::onResetCraftVisibility()
{
    AppUiSettings::instance().clearCraftVisibilityOverrides();
    QMapIterator<QString, QCheckBox*> it(m_craftChecks);
    while (it.hasNext()) {
        it.next();
        if (it.value())
            it.value()->setChecked(true);
    }
}

void AppConfigDialog::onOkClicked()
{
    QMapIterator<QString, QCheckBox*> it(m_craftChecks);
    while (it.hasNext()) {
        it.next();
        QCheckBox* cb = it.value();
        if (!cb)
            continue;
        AppUiSettings::instance().setCraftVisible(it.key(), cb->isChecked());
    }
    if (m_toolLibraryFPathSpin) {
        AppUiSettings::instance().setToolLibraryFPath(m_toolLibraryFPathSpin->value());
    }
    if (m_joinOpenEntitiesFuzzSpin) {
        AppUiSettings::instance().setJoinOpenEntitiesFuzzMm(m_joinOpenEntitiesFuzzSpin->value());
    }

    const bool oldSmallScreen = AppUiSettings::instance().smallScreenMode();
    const bool newSmallScreen = m_smallScreenCheck ? m_smallScreenCheck->isChecked() : oldSmallScreen;
    AppUiSettings::instance().setSmallScreenMode(newSmallScreen);

    if (!AppUiSettings::instance().save()) {
        LOG_WARNING("AppConfigDialog: save failed");
    }
    if (ToolLibrary* tl = ToolLibrary::instance()) {
        tl->syncRadiusFromController();
        tl->persistAndNotifyToolList();
    }
    if (m_craftToolBar)
        m_craftToolBar->rebuildCraftButtons();

    if (newSmallScreen != oldSmallScreen)
        emit smallScreenModeChanged(newSmallScreen);

    accept();
}
