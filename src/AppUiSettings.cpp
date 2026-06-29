#include "AppUiSettings.h"
#include "AppFileLayout.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptValueIterator>

namespace {
const char kCraftVisibility[] = "craftVisibility";
const char kToolLibraryFPath[] = "toolLibraryFPath";
const char kSplitters[] = "splitters";
const char kGraphicsItemTableColumnWidths[] = "graphicsItemTableColumnWidths";
const char kLowPerformanceCanvasMode[] = "lowPerformanceCanvasMode";
const char kCanvasPerfLoggingEnabled[] = "canvasPerfLoggingEnabled";
const char kCanvasMouseMoveThrottleMs[] = "canvasMouseMoveThrottleMs";
const char kCanvasMouseMovePixelThreshold[] = "canvasMouseMovePixelThreshold";
const char kCanvasHoverDelayMs[] = "canvasHoverDelayMs";
const char kJoinOpenEntitiesFuzzMm[] = "joinOpenEntitiesFuzzMm";
const char kSmallScreenMode[] = "smallScreenMode";

bool readIntArray(const QScriptValue& arrValue, QList<int>* out)
{
    if (!out || !arrValue.isValid())
        return false;
    out->clear();
    if (!arrValue.isArray())
        return false;
    const int len = arrValue.property(QLatin1String("length")).toInt32();
    for (int i = 0; i < len; ++i) {
        const QScriptValue e = arrValue.property(i);
        if (e.isNumber())
            out->append(e.toInt32());
    }
    return !out->isEmpty();
}
} // namespace

AppUiSettings& AppUiSettings::instance()
{
    static AppUiSettings s;
    return s;
}

AppUiSettings::AppUiSettings()
    : m_toolLibraryFPath(1)
    , m_lowPerformanceCanvasMode(false)
    , m_canvasPerfLoggingEnabled(false)
    , m_canvasMouseMoveThrottleMs(16)
    , m_canvasMouseMovePixelThreshold(2)
    , m_canvasHoverDelayMs(180)
    , m_joinOpenEntitiesFuzzMm(1.0)
    , m_smallScreenMode(false)
{
    reload();
}

void AppUiSettings::reload()
{
    m_craftHidden.clear();
    m_splitterSizes.clear();
    m_graphicsItemTableColumnWidths.clear();
    m_activePath.clear();
    m_toolLibraryFPath = 1;
    m_lowPerformanceCanvasMode = false;
    m_canvasPerfLoggingEnabled = false;
    m_canvasMouseMoveThrottleMs = 16;
    m_canvasMouseMovePixelThreshold = 2;
    m_canvasHoverDelayMs = 180;
    m_joinOpenEntitiesFuzzMm = 1.0;
    m_smallScreenMode = false;

    foreach (const QString& path, candidateReadPaths()) {
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile() && loadFromFile(fi.absoluteFilePath())) {
            m_activePath = AppFileLayout::appUiSettingsWritePath();
            LOG_DEBUG(QString("AppUiSettings loaded: %1 (save target: %2)").arg(path, m_activePath));
            return;
        }
    }

    m_activePath = resolveSavePath();
    LOG_DEBUG(QString("AppUiSettings using default path: %1").arg(m_activePath));
}

bool AppUiSettings::isCraftVisible(const QString& craftType) const
{
    if (craftType.isEmpty())
        return true;
    return !m_craftHidden.value(craftType, false);
}

void AppUiSettings::setCraftVisible(const QString& craftType, bool visible)
{
    if (craftType.isEmpty())
        return;
    if (visible)
        m_craftHidden.remove(craftType);
    else
        m_craftHidden[craftType] = true;
}

void AppUiSettings::clearCraftVisibilityOverrides()
{
    m_craftHidden.clear();
}

void AppUiSettings::setToolLibraryFPath(int fpath)
{
    m_toolLibraryFPath = fpath >= 1 ? fpath : 1;
}

QList<int> AppUiSettings::splitterSizes(const QString& objectName) const
{
    return m_splitterSizes.value(objectName);
}

void AppUiSettings::setSplitterSizes(const QString& objectName, const QList<int>& sizes)
{
    if (objectName.isEmpty() || sizes.isEmpty())
        return;
    m_splitterSizes[objectName] = sizes;
}

void AppUiSettings::setGraphicsItemTableColumnWidths(const QList<int>& widths)
{
    m_graphicsItemTableColumnWidths = widths;
}

int AppUiSettings::canvasMouseMoveThrottleMs() const
{
    int v = m_canvasMouseMoveThrottleMs;
    if (m_lowPerformanceCanvasMode && v < 24)
        v = 24;
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    return v;
}

int AppUiSettings::canvasMouseMovePixelThreshold() const
{
    int v = m_canvasMouseMovePixelThreshold;
    if (m_lowPerformanceCanvasMode && v < 2)
        v = 2;
    if (v < 0)
        v = 0;
    if (v > 20)
        v = 20;
    return v;
}

int AppUiSettings::canvasHoverDelayMs() const
{
    int v = m_canvasHoverDelayMs;
    if (m_lowPerformanceCanvasMode && v < 250)
        v = 250;
    if (v < 0)
        v = 0;
    if (v > 1000)
        v = 1000;
    return v;
}

void AppUiSettings::setJoinOpenEntitiesFuzzMm(double mm)
{
    if (mm < 0.001)
        mm = 0.001;
    if (mm > 1000.0)
        mm = 1000.0;
    m_joinOpenEntitiesFuzzMm = mm;
}

QString AppUiSettings::jsonEscape(const QString& s)
{
    QString out;
    out.reserve(s.length() + 8);
    for (int i = 0; i < s.length(); ++i) {
        const QChar c = s.at(i);
        const ushort u = c.unicode();
        if (c == QLatin1Char('\\'))
            out += QLatin1String("\\\\");
        else if (c == QLatin1Char('"'))
            out += QLatin1String("\\\"");
        else if (c == QLatin1Char('\n'))
            out += QLatin1String("\\n");
        else if (c == QLatin1Char('\r'))
            out += QLatin1String("\\r");
        else if (c == QLatin1Char('\t'))
            out += QLatin1String("\\t");
        else if (u < 32)
            out += QString("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
        else
            out += c;
    }
    return out;
}

QStringList AppUiSettings::candidateReadPaths() const
{
    return AppFileLayout::appUiSettingsReadCandidates();
}

QString AppUiSettings::resolveSavePath() const
{
    return AppFileLayout::appUiSettingsWritePath();
}

bool AppUiSettings::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray fileData = file.readAll();
    file.close();

    QScriptEngine engine;
    const QString jsonSupport =
        QLatin1String(
            "var JSON = JSON || {};"
            "(function () {"
            "  if (typeof JSON.parse === 'function') return;"
            "  JSON.parse = function(text) {"
            "    return eval('(' + text + ')');"
            "  };"
            "})();");
    engine.evaluate(jsonSupport);

    QScriptValue jsonObject = engine.evaluate(QLatin1String("JSON"));
    QScriptValue parseFunction = jsonObject.property(QLatin1String("parse"));
    QScriptValue result =
        parseFunction.call(QScriptValue(), QScriptValueList() << QString::fromUtf8(fileData.constData()));

    if (engine.hasUncaughtException()) {
        LOG_WARNING(QString("AppUiSettings JSON parse error: %1")
                        .arg(engine.uncaughtException().toString()));
        return false;
    }

    QScriptValue cv = result.property(QLatin1String(kCraftVisibility));
    if (cv.isObject()) {
        QScriptValueIterator it(cv);
        while (it.hasNext()) {
            it.next();
            const QString key = it.name();
            if (key.isEmpty())
                continue;
            const QScriptValue v = it.value();
            if (v.isBool() && v.toBool() == false)
                m_craftHidden[key] = true;
            else if (v.isNumber() && v.toNumber() == 0)
                m_craftHidden[key] = true;
        }
    }

    const QScriptValue fp = result.property(QLatin1String(kToolLibraryFPath));
    if (fp.isNumber()) {
        const int v = fp.toInt32();
        m_toolLibraryFPath = v >= 1 ? v : 1;
    }

    const QScriptValue sp = result.property(QLatin1String(kSplitters));
    if (sp.isObject()) {
        QScriptValueIterator it(sp);
        while (it.hasNext()) {
            it.next();
            const QString key = it.name();
            if (key.isEmpty())
                continue;
            QList<int> sizes;
            if (readIntArray(it.value(), &sizes))
                m_splitterSizes[key] = sizes;
        }
    }

    const QScriptValue gcw = result.property(QLatin1String(kGraphicsItemTableColumnWidths));
    QList<int> cw;
    if (readIntArray(gcw, &cw))
        m_graphicsItemTableColumnWidths = cw;

    const QScriptValue lowPerf = result.property(QLatin1String(kLowPerformanceCanvasMode));
    if (lowPerf.isBool()) {
        m_lowPerformanceCanvasMode = lowPerf.toBool();
    } else if (lowPerf.isNumber()) {
        m_lowPerformanceCanvasMode = lowPerf.toInt32() != 0;
    }

    const QScriptValue perfLogging = result.property(QLatin1String(kCanvasPerfLoggingEnabled));
    if (perfLogging.isBool()) {
        m_canvasPerfLoggingEnabled = perfLogging.toBool();
    } else if (perfLogging.isNumber()) {
        m_canvasPerfLoggingEnabled = perfLogging.toInt32() != 0;
    }

    const QScriptValue moveThrottle = result.property(QLatin1String(kCanvasMouseMoveThrottleMs));
    if (moveThrottle.isNumber())
        m_canvasMouseMoveThrottleMs = moveThrottle.toInt32();

    const QScriptValue movePixel = result.property(QLatin1String(kCanvasMouseMovePixelThreshold));
    if (movePixel.isNumber())
        m_canvasMouseMovePixelThreshold = movePixel.toInt32();

    const QScriptValue hoverDelay = result.property(QLatin1String(kCanvasHoverDelayMs));
    if (hoverDelay.isNumber())
        m_canvasHoverDelayMs = hoverDelay.toInt32();

    const QScriptValue joinFuzz = result.property(QLatin1String(kJoinOpenEntitiesFuzzMm));
    if (joinFuzz.isNumber())
        setJoinOpenEntitiesFuzzMm(joinFuzz.toNumber());

    const QScriptValue ssm = result.property(QLatin1String(kSmallScreenMode));
    if (ssm.isBool()) {
        m_smallScreenMode = ssm.toBool();
    } else if (ssm.isNumber()) {
        m_smallScreenMode = ssm.toInt32() != 0;
    }

    return true;
}

bool AppUiSettings::save()
{
    if (m_activePath.isEmpty())
        m_activePath = resolveSavePath();

    QFileInfo outFi(m_activePath);
    QDir outDir = outFi.dir();
    if (!outDir.exists()) {
        if (!outDir.mkpath(QLatin1String("."))) {
            LOG_WARNING(QString("AppUiSettings: cannot create directory for %1").arg(m_activePath));
            return false;
        }
    }

    QString body = QLatin1String("{\n  \"") + QLatin1String(kCraftVisibility) + QLatin1String("\": {");
    bool first = true;
    QMapIterator<QString, bool> it(m_craftHidden);
    while (it.hasNext()) {
        it.next();
        if (!it.value())
            continue;
        if (!first)
            body += QLatin1Char(',');
        first = false;
        body += QLatin1String("\n    \"");
        body += jsonEscape(it.key());
        body += QLatin1String("\": false");
    }
    if (!first)
        body += QLatin1Char('\n');
    body += QLatin1String("  }");
    body += QLatin1String(",\n  \"") + QLatin1String(kToolLibraryFPath) + QLatin1String("\": ");
    body += QString::number(m_toolLibraryFPath);

    body += QLatin1String(",\n  \"") + QLatin1String(kSplitters) + QLatin1String("\": {");
    bool fs = true;
    QMapIterator<QString, QList<int> > sit(m_splitterSizes);
    while (sit.hasNext()) {
        sit.next();
        if (sit.value().isEmpty())
            continue;
        if (!fs)
            body += QLatin1Char(',');
        fs = false;
        body += QLatin1String("\n    \"");
        body += jsonEscape(sit.key());
        body += QLatin1String("\": [");
        for (int i = 0; i < sit.value().size(); ++i) {
            if (i)
                body += QLatin1String(", ");
            body += QString::number(sit.value().at(i));
        }
        body += QLatin1Char(']');
    }
    if (!fs)
        body += QLatin1Char('\n');
    body += QLatin1String("  }");

    body += QLatin1String(",\n  \"") + QLatin1String(kGraphicsItemTableColumnWidths) + QLatin1String("\": [");
    for (int i = 0; i < m_graphicsItemTableColumnWidths.size(); ++i) {
        if (i)
            body += QLatin1String(", ");
        body += QString::number(m_graphicsItemTableColumnWidths.at(i));
    }
    body += QLatin1String("]\n}\n");
    body.chop(2);
    body += QLatin1String(",\n  \"") + QLatin1String(kLowPerformanceCanvasMode) + QLatin1String("\": ");
    body += m_lowPerformanceCanvasMode ? QLatin1String("true") : QLatin1String("false");
    body += QLatin1String(",\n  \"") + QLatin1String(kCanvasPerfLoggingEnabled) + QLatin1String("\": ");
    body += m_canvasPerfLoggingEnabled ? QLatin1String("true") : QLatin1String("false");
    body += QLatin1String(",\n  \"") + QLatin1String(kCanvasMouseMoveThrottleMs) + QLatin1String("\": ");
    body += QString::number(m_canvasMouseMoveThrottleMs);
    body += QLatin1String(",\n  \"") + QLatin1String(kCanvasMouseMovePixelThreshold) + QLatin1String("\": ");
    body += QString::number(m_canvasMouseMovePixelThreshold);
    body += QLatin1String(",\n  \"") + QLatin1String(kCanvasHoverDelayMs) + QLatin1String("\": ");
    body += QString::number(m_canvasHoverDelayMs);
    body += QLatin1String(",\n  \"") + QLatin1String(kJoinOpenEntitiesFuzzMm) + QLatin1String("\": ");
    body += QString::number(m_joinOpenEntitiesFuzzMm, 'f', 3);
    body += QLatin1String(",\n  \"") + QLatin1String(kSmallScreenMode) + QLatin1String("\": ");
    body += m_smallScreenMode ? QLatin1String("true") : QLatin1String("false");
    body += QLatin1String("\n}\n");

    QFile file(m_activePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        LOG_WARNING(QString("AppUiSettings: cannot write %1").arg(m_activePath));
        return false;
    }
    file.write(body.toUtf8());
    file.close();
    LOG_INFO(QString("AppUiSettings saved: %1").arg(m_activePath));
    return true;
}
