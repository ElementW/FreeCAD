// SPDX-License-Identifier: LGPL-2.1-or-later
/***************************************************************************
 *   Copyright (c) 2004 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#define MEASURE_ICON_LOAD_TIME

#ifdef MEASURE_ICON_LOAD_TIME
# include <chrono>
#endif
#include <string>

#include <QtConcurrent>
#include <QApplication>
#include <QBitmap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QMap>
#include <QIconEngine>
#include <QImageReader>
#include <QPainter>
#include <QPalette>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QScreen>
#include <QString>
#include <QStyleOption>
#include <QSvgRenderer>
#include <QWriteLocker>

#include <Inventor/fields/SoSFImage.h>

#include <App/Application.h>
#include <Base/Console.h>
#include <Base/ConsoleObserver.h>

#include "BitmapFactory.h"

using namespace Gui;

namespace Gui
{
class BitmapFactoryInstP
{
public:
    QReadWriteLock iconCacheLock;
    QMap<std::string, QIcon> iconCache;

    QStringList supportedFormats;

    bool useIconTheme;
    bool loadIconsAsync;
};
}  // namespace Gui

static QIcon loadIcon(BitmapFactoryInstP* d, const char* name);

namespace
{
class AsyncLoadIconEngine: public QIconEngine
{
public:
    AsyncLoadIconEngine(BitmapFactoryInstP* d, std::string name)
        : name(std::move(name))
    {
        icon = QtConcurrent::run([d, pixmapName = std::string(this->name)]() -> QIcon {
            return loadIcon(d, pixmapName.c_str());
        });
    }
    ~AsyncLoadIconEngine()
    {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) final override
    {
        getIcon().paint(painter, rect, Qt::AlignCenter, mode, state);
    }

    QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) final override
    {
        return getIcon().actualSize(size, mode, state);
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) final override
    {
        return getIcon().pixmap(size, mode, state);
    }

    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state) final override
    {
        return getIcon().addPixmap(pixmap, mode, state);
    }

    void addFile(const QString& fileName, const QSize& size, QIcon::Mode mode, QIcon::State state) final override
    {
        return getIcon().addFile(fileName, size, mode, state);
    }

    QString key() const final override
    {
        return QString::fromStdString(name);
    }

    QIconEngine* clone() const final override
    {
        return new AsyncLoadIconEngine(*this);
    }

    QList<QSize> availableSizes(
        QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off
    ) final override
    {
        return getIcon().availableSizes(mode, state);
    }

    QString iconName() final override
    {
        return getIcon().name();
    }

    bool isNull() final override
    {
        return getIcon().isNull();
    }

private:
    QIcon getIcon() const
    {
#ifdef MEASURE_ICON_LOAD_TIME
        if (!icon.isResultReadyAt(0)) {
            const auto begin = std::chrono::high_resolution_clock::now();
            auto iconValue = icon.result();
            const auto duration = std::chrono::high_resolution_clock::now() - begin;
            const auto threadName = QThread::currentThread() == QCoreApplication::instance()->thread()
                ? "UI"
                : "non-UI";
            Base::Console().log(
                "BitmapFactory: %s thread stalled %d µs waiting for icon %s\n",
                threadName,
                std::chrono::duration_cast<std::chrono::microseconds>(duration).count(),
                name.c_str()
            );
            return iconValue;
        }
#endif
        return icon.result();
    }

    std::string name;
    QFuture<QIcon> icon;
};
}  // namespace

/// Loads an icon pixmap by path.
static bool loadPixmap(const QString& filename, QPixmap& pixmap)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }

    // First check if it's an SVG since we have our own loading logic
    if (filename.endsWith(QStringLiteral("svg"))) {
        const QByteArray content = file.readAll();
        pixmap = BitmapFactory().pixmapFromSvg(content, QSize(64, 64));
    }
    else {
        // Try with Qt plugins
        QImageReader reader(&file);
        pixmap = QPixmap::fromImageReader(&reader);
    }

    return !pixmap.isNull();
}

static QPixmap fallbackPixmap()
{
    static constexpr int Size = 64;
    // clang-format off
    static constexpr const char * const Xpm[] = {
        "2 2 2 1",
        ". c #000000",
        "x c #FF00FF",
        ".x",
        "x."
    };
    // clang-format on
    return QPixmap(Xpm).scaled(Size, Size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

/// Loads (decodes/renders) an icon by name.
static QIcon loadIcon(BitmapFactoryInstP* d, const char* name)
{
#ifdef MEASURE_ICON_LOAD_TIME
    struct LoadTimeMeasure
    {
        const char* name;
        bool isGuiThread;
        std::chrono::high_resolution_clock::time_point begin;
        LoadTimeMeasure(const char* name)
            : name(name)
            , isGuiThread(QThread::currentThread() == QCoreApplication::instance()->thread())
            , begin(std::chrono::high_resolution_clock::now())
        {}
        ~LoadTimeMeasure()
        {
            const auto duration = std::chrono::high_resolution_clock::now() - begin;
            Base::Console().log(
                isGuiThread ? "BitmapFactory: UI thread took %d µs synchronously for icon %s\n"
                            : "BitmapFactory: took %d µs off the UI thread for icon %s\n",
                std::chrono::duration_cast<std::chrono::microseconds>(duration).count(),
                name
            );
        }
    } measure {name};
#endif

    QPixmap pixmap;

    // Try whether an absolute path is given
    QString fileName = QString::fromUtf8(name);
    if (loadPixmap(fileName, pixmap)) {
        return pixmap;
    }

    // Try to find it in the 'icons' search paths
    fileName.prepend(QStringLiteral("icons:"));
    if (loadPixmap(fileName, pixmap)) {
        return pixmap;
    }

    // Go through supported file formats
    for (const auto& format : d->supportedFormats) {
        QString path = QStringLiteral("%1.%2").arg(fileName, format);
        if (loadPixmap(path, pixmap)) {
            return pixmap;
        }
    }

    Base::Console().warning("Cannot find icon: %s\n", name);
    return fallbackPixmap();
}

/// Get an icon by name from the cache or loads it if absent.
static QIcon getIcon(BitmapFactoryInstP* d, const char* name, bool async)
{
    if (!name || *name == '\0') {
        return {};
    }

    // First check in the cache.
    {
        QReadLocker locker(&d->iconCacheLock);
        if (auto it = d->iconCache.find(name); it != d->iconCache.end()) {
            return it.value();
        }
    }

    // Otherwise load the icon and cache it.
    QIcon icon = async ? QIcon(new AsyncLoadIconEngine(d, name)) : loadIcon(d, name);

    {
        QWriteLocker locker(&d->iconCacheLock);
        d->iconCache.insert(name, icon);
    }

    return icon;
}

BitmapFactoryInst* BitmapFactoryInst::_pcSingleton = nullptr;

BitmapFactoryInst& BitmapFactoryInst::instance()
{
    if (!_pcSingleton) {
        _pcSingleton = new BitmapFactoryInst;
        std::map<std::string, std::string>::const_iterator it;
        it = App::GetApplication().Config().find("ProgramIcons");
        if (it != App::GetApplication().Config().end()) {
            QString home = QString::fromStdString(App::Application::getHomePath());
            QString path = QString::fromUtf8(it->second.c_str());
            if (QDir(path).isRelative()) {
                path = QFileInfo(QDir(home), path).absoluteFilePath();
            }
            _pcSingleton->addPath(path);
        }
        _pcSingleton->addPath(
            QStringLiteral("%1/icons").arg(QString::fromStdString(App::Application::getHomePath()))
        );
        _pcSingleton->addPath(
            QStringLiteral("%1/icons").arg(QString::fromStdString(App::Application::getUserAppDataDir()))
        );
        _pcSingleton->addPath(QLatin1String(":/icons/"));
        _pcSingleton->addPath(QLatin1String(":/Icons/"));
    }

    return *_pcSingleton;
}

void BitmapFactoryInst::destruct()
{
    if (_pcSingleton) {
        delete _pcSingleton;
    }
    _pcSingleton = nullptr;
}

BitmapFactoryInst::BitmapFactoryInst()
{
    d = new BitmapFactoryInstP;

    auto bitmapsGroup = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Bitmaps"
    );
    auto themeGroup = bitmapsGroup->GetGroup("Theme");
    auto paths = bitmapsGroup->GetASCIIs("CustomPath");
    for (auto& path : paths) {
        addPath(QString::fromUtf8(path.c_str()));
    }
    d->loadIconsAsync = bitmapsGroup->GetBool("AsyncLoad", true);
    d->useIconTheme
        = themeGroup->GetBool("UseIconTheme", themeGroup->GetBool("ThemeSearchPaths", false));

    const auto formats = QImageReader::supportedImageFormats();
    d->supportedFormats.reserve(formats.size() + 1);
    d->supportedFormats.append("svg");  // Check for SVG first to use special import mechanism
    for (const auto& format : formats) {
        const auto lower = format.toLower();
        if (lower != "svg") {
            d->supportedFormats.append(QString::fromLatin1(lower.constData()));
        }
    }
}

BitmapFactoryInst::~BitmapFactoryInst()
{
    delete d;
}

void BitmapFactoryInst::addPath(const QString& path)
{
    QDir::addSearchPath(QStringLiteral("icons"), path);
}

void BitmapFactoryInst::removePath(const QString& path)
{
    QStringList iconPaths = QDir::searchPaths(QStringLiteral("icons"));
    int pos = iconPaths.indexOf(path);
    if (pos != -1) {
        iconPaths.removeAt(pos);
        QDir::setSearchPaths(QStringLiteral("icons"), iconPaths);
    }
}

QStringList BitmapFactoryInst::getPaths() const
{
    return QDir::searchPaths(QStringLiteral("icons"));
}

QStringList BitmapFactoryInst::findIconFiles() const
{
    QStringList files, filters;
    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (QList<QByteArray>::iterator it = formats.begin(); it != formats.end(); ++it) {
        filters << QStringLiteral("*.%1").arg(QString::fromLatin1(*it).toLower());
    }

    QStringList paths = QDir::searchPaths(QStringLiteral("icons"));
    paths.removeDuplicates();
    for (QStringList::Iterator pt = paths.begin(); pt != paths.end(); ++pt) {
        QDir d(*pt);
        d.setNameFilters(filters);
        QFileInfoList fi = d.entryInfoList();
        for (QFileInfoList::iterator it = fi.begin(); it != fi.end(); ++it) {
            files << it->absoluteFilePath();
        }
    }

    files.removeDuplicates();
    return files;
}

void BitmapFactoryInst::addPixmapToCache(const char* name, const QPixmap& icon)
{
    QWriteLocker locker(&d->iconCacheLock);
    d->iconCache[name].addPixmap(icon);
}

bool BitmapFactoryInst::findPixmapInCache(const char* name, QPixmap& px) const
{
    QReadLocker locker(&d->iconCacheLock);
    auto it = d->iconCache.find(name);
    if (it != d->iconCache.end()) {
        px = it.value().pixmap(it.value().availableSizes()[0]);
        return true;
    }
    return false;
}

QIcon BitmapFactoryInst::iconFromTheme(const char* name, const QIcon& fallback)
{
    // If Qt icon themes are enabled and that icon is provided, use it
    if (d->useIconTheme) {
        const QString iconName = QString::fromUtf8(name);
        if (QIcon icon = QIcon::fromTheme(iconName, fallback); !icon.isNull()) {
            return icon;
        }
    }

    // Otherwise do as if asked for the default theme icon
    return iconFromDefaultTheme(name, fallback);
}

QIcon Gui::BitmapFactoryInst::iconFromDefaultTheme(const char* name, const QIcon& fallback)
{
    if (fallback.isNull()) {
        return getIcon(d, name, d->loadIconsAsync);
    }

    // TODO: Implement async fallback

    QIcon icon = getIcon(d, name, false);
    if (icon.isNull()) {
        return fallback;
    }

    return icon;
}

QPixmap BitmapFactoryInst::pixmap(const char* name) const
{
    QIcon icon = getIcon(d, name, false);
    return icon.pixmap(icon.availableSizes()[0]);
}

QPixmap BitmapFactoryInst::pixmapFromSvg(
    const char* name,
    const QSizeF& size,
    const ColorMap& colorMapping
) const
{
    static qreal dpr = getMaximumDPR();

    // If an absolute path is given
    QPixmap icon;
    QString iconPath;
    QString fn = QString::fromUtf8(name);
    if (QFile(fn).exists()) {
        iconPath = fn;
    }

    // try to find it in the 'icons' search paths
    if (iconPath.isEmpty()) {
        QString fileName = QStringLiteral("icons:") + fn;
        QFileInfo fi(fileName);
        if (fi.exists()) {
            iconPath = fi.filePath();
        }
        else {
            fileName += QLatin1String(".svg");
            fi.setFile(fileName);
            if (fi.exists()) {
                iconPath = fi.filePath();
            }
        }
    }

    if (!iconPath.isEmpty()) {
        QFile file(iconPath);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QByteArray content = file.readAll();
            icon = pixmapFromSvg(content, size * dpr, colorMapping);
        }
    }

    if (!icon.isNull()) {
        icon.setDevicePixelRatio(dpr);
    }

    return icon;
}

QPixmap BitmapFactoryInst::pixmapFromSvg(
    const QByteArray& originalContents,
    const QSizeF& size,
    const ColorMap& colorMapping
) const
{
    QString stringContents = QString::fromUtf8(originalContents);
    for (const auto& colorToColor : colorMapping) {
        ulong fromColor = colorToColor.first;
        ulong toColor = colorToColor.second;
        QString fromColorString = QStringLiteral("#%1").arg(fromColor, 6, 16, QChar::fromLatin1('0'));
        QString toColorString = QStringLiteral("#%1").arg(toColor, 6, 16, QChar::fromLatin1('0'));
        stringContents = stringContents.replace(fromColorString, toColorString);
    }
    QByteArray contents = stringContents.toUtf8();

    QImage image(size.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(0x00000000);

    QPainter p(&image);
    QSvgRenderer svg;
    {
        // tmp. disable the report window to suppress some bothering warnings
        const Base::ILoggerBlocker blocker("ReportOutput", Base::ConsoleSingleton::MsgType_Wrn);
        svg.load(contents);
    }
    svg.render(&p);
    p.end();

    return QPixmap::fromImage(image);
}

QStringList BitmapFactoryInst::pixmapNames() const
{
    QStringList names;
    QReadLocker locker(&d->iconCacheLock);
    names.reserve(d->iconCache.size());
    for (auto it = d->iconCache.begin(); it != d->iconCache.end(); ++it) {
        QString item = QString::fromUtf8(it.key().c_str());
        if (!names.contains(item)) {
            names << item;
        }
    }
    return names;
}

QPixmap BitmapFactoryInst::resize(int w, int h, const QPixmap& p, Qt::BGMode bgmode) const
{
    if (bgmode == Qt::TransparentMode) {
        if (p.width() == 0 || p.height() == 0) {
            w = 1;
        }

        QPixmap pix = p;
        int x = pix.width() > w ? 0 : (w - pix.width()) / 2;
        int y = pix.height() > h ? 0 : (h - pix.height()) / 2;

        if (x == 0 && y == 0) {
            return pix;
        }

        QPixmap pm(w, h);
        QBitmap mask(w, h);
        mask.fill(Qt::color0);

        QBitmap bm = pix.mask();
        if (!bm.isNull()) {
            QPainter painter(&mask);
            painter.drawPixmap(QPoint(x, y), bm, QRect(0, 0, pix.width(), pix.height()));
            pm.setMask(mask);
        }
        else {
            pm.setMask(mask);
            pm = fillRect(x, y, pix.width(), pix.height(), pm, Qt::OpaqueMode);
        }

        QPainter pt;
        pt.begin(&pm);
        pt.drawPixmap(x, y, pix);
        pt.end();
        return pm;
    }
    else {  // Qt::OpaqueMode
        QPixmap pix = p;

        if (pix.width() == 0 || pix.height() == 0) {
            return pix;  // do not resize a null pixmap
        }

        QPalette pal = qApp->palette();
        QColor dl = pal.color(QPalette::Disabled, QPalette::Light);
        QColor dt = pal.color(QPalette::Disabled, QPalette::Text);

        QPixmap pm(w, h);
        pm.fill(dl);

        QPainter pt;
        pt.begin(&pm);
        pt.setPen(dl);
        pt.drawPixmap(1, 1, pix);
        pt.setPen(dt);
        pt.drawPixmap(0, 0, pix);
        pt.end();
        return pm;
    }
}

QPixmap BitmapFactoryInst::fillRect(int x, int y, int w, int h, const QPixmap& p, Qt::BGMode bgmode) const
{
    QBitmap b = p.mask();
    if (b.isNull()) {
        return p;  // sorry, but cannot do anything
    }

    QPixmap pix = p;

    // modify the mask
    QPainter pt;
    pt.begin(&b);
    if (bgmode == Qt::OpaqueMode) {
        pt.fillRect(x, y, w, h, Qt::color1);  // make opaque
    }
    else {                                    // Qt::TransparentMode
        pt.fillRect(x, y, w, h, Qt::color0);  // make transparent
    }
    pt.end();

    pix.setMask(b);

    return pix;
}

QPixmap BitmapFactoryInst::merge(const QPixmap& p1, const QPixmap& p2, bool vertical) const
{
    int width = 0;
    int height = 0;

    int x = 0;
    int y = 0;

    // get the size for the new pixmap
    if (vertical) {
        y = p1.height();
        width = qMax(p1.width(), p2.width());
        height = p1.height() + p2.height();
    }
    else {
        x = p1.width();
        width = p1.width() + p2.width();
        height = qMax(p1.height(), p2.height());
    }

    QPixmap res(width, height);
    QBitmap mask(width, height);
    QBitmap mask1 = p1.mask();
    QBitmap mask2 = p2.mask();
    mask.fill(Qt::color0);

    auto* pt1 = new QPainter(&res);
    pt1->drawPixmap(0, 0, p1);
    pt1->drawPixmap(x, y, p2);
    delete pt1;

    auto* pt2 = new QPainter(&mask);
    pt2->drawPixmap(0, 0, mask1);
    pt2->drawPixmap(x, y, mask2);
    delete pt2;

    res.setMask(mask);
    return res;
}

QPixmap BitmapFactoryInst::merge(const QPixmap& p1, const QPixmap& p2, Position pos) const
{
    // does the similar as the method above except that this method does not resize the resulting pixmap
    int x = 0, y = 0;
    qreal dpr1 = p1.devicePixelRatio();
    qreal dpr2 = p2.devicePixelRatio();

    switch (pos) {
        case TopLeft:
            break;
        case TopRight:
            x = p1.width() / dpr1 - p2.width() / dpr2;
            break;
        case BottomLeft:
            y = p1.height() / dpr1 - p2.height() / dpr2;
            break;
        case BottomRight:
            x = p1.width() / dpr1 - p2.width() / dpr2;
            y = p1.height() / dpr1 - p2.height() / dpr2;
            break;
    }

    QPixmap p = p1;
    p = fillRect(x, y, p2.width(), p2.height(), p, Qt::OpaqueMode);

    QPainter pt;
    pt.begin(&p);
    pt.setPen(Qt::NoPen);
    pt.drawRect(x, y, p2.width(), p2.height());
    pt.drawPixmap(x, y, p2);
    pt.end();

    return p;
}

QPixmap BitmapFactoryInst::disabled(const QPixmap& p) const
{
    QStyleOption opt;
    opt.palette = QApplication::palette();
    return QApplication::style()->generatedIconPixmap(QIcon::Disabled, p, &opt);
}

QPixmap BitmapFactoryInst::empty(QSize size) const
{
    qreal dpr = getMaximumDPR();

    QPixmap res(size * dpr);
    res.fill(Qt::transparent);
    res.setDevicePixelRatio(dpr);

    return res;
}

void BitmapFactoryInst::convert(const QImage& p, SoSFImage& img) const
{
    SbVec2s size;
    size[0] = p.width();
    size[1] = p.height();

    int buffersize = static_cast<int>(p.sizeInBytes());

    int numcomponents = 0;
    QVector<QRgb> table = p.colorTable();
    if (!table.isEmpty()) {
        if (p.hasAlphaChannel()) {
            if (p.allGray()) {
                numcomponents = 2;
            }
            else {
                numcomponents = 4;
            }
        }
        else {
            if (p.allGray()) {
                numcomponents = 1;
            }
            else {
                numcomponents = 3;
            }
        }
    }
    else {
        numcomponents = buffersize / (size[0] * size[1]);
    }

    int depth = numcomponents;

    // Coin3D only supports up to 32-bit images
    if (numcomponents == 8) {
        numcomponents = 4;
    }

    // allocate image data
    img.setValue(size, numcomponents, nullptr);

    unsigned char* bytes = img.startEditing(size, numcomponents);

    int width = (int)size[0];
    int height = (int)size[1];

    for (int y = 0; y < height; y++) {
        unsigned char* line = &bytes[width * numcomponents * (height - (y + 1))];
        for (int x = 0; x < width; x++) {
            QColor col = p.pixelColor(x, y);
            switch (depth) {
                default:
                    break;
                case 1: {
                    QRgb rgb = col.rgb();
                    line[0] = qGray(rgb);
                } break;
                case 2: {
                    QRgb rgb = col.rgba();
                    line[0] = qGray(rgb);
                    line[1] = qAlpha(rgb);
                } break;
                case 3: {
                    QRgb rgb = col.rgb();
                    line[0] = qRed(rgb);
                    line[1] = qGreen(rgb);
                    line[2] = qBlue(rgb);
                } break;
                case 4: {
                    QRgb rgb = col.rgba();
                    line[0] = qRed(rgb);
                    line[1] = qGreen(rgb);
                    line[2] = qBlue(rgb);
                    line[3] = qAlpha(rgb);
                } break;
                case 8: {
                    QRgba64 rgb = col.rgba64();
                    line[0] = qRed(rgb);
                    line[1] = qGreen(rgb);
                    line[2] = qBlue(rgb);
                    line[3] = qAlpha(rgb);
                } break;
            }

            line += numcomponents;
        }
    }

    img.finishEditing();
}

void BitmapFactoryInst::convert(const SoSFImage& p, QImage& img) const
{
    SbVec2s size;
    int numcomponents;

    const unsigned char* bytes = p.getValue(size, numcomponents);
    if (!bytes) {
        return;
    }

    int width = (int)size[0];
    int height = (int)size[1];

    img = QImage(width, height, QImage::Format_RGB32);
    QRgb* bits = (QRgb*)img.bits();

    for (int y = 0; y < height; y++) {
        const unsigned char* line = &bytes[width * numcomponents * (height - (y + 1))];
        for (int x = 0; x < width; x++) {
            switch (numcomponents) {
                default:
                case 1:
                    *bits++ = qRgb(line[0], line[0], line[0]);
                    break;
                case 2:
                    *bits++ = qRgba(line[0], line[0], line[0], line[1]);
                    break;
                case 3:
                    *bits++ = qRgb(line[0], line[1], line[2]);
                    break;
                case 4:
                    *bits++ = qRgba(line[0], line[1], line[2], line[3]);
                    break;
            }

            line += numcomponents;
        }
    }
}

QIcon BitmapFactoryInst::mergePixmap(
    const QIcon& base,
    const QPixmap& px,
    Gui::BitmapFactoryInst::Position position
)
{
    QIcon overlayedIcon;

    int w = QApplication::style()->pixelMetric(QStyle::PM_ListViewIconSize);

    overlayedIcon.addPixmap(
        Gui::BitmapFactory().merge(base.pixmap(w, w, QIcon::Normal, QIcon::Off), px, position),
        QIcon::Normal,
        QIcon::Off
    );

    overlayedIcon.addPixmap(
        Gui::BitmapFactory().merge(base.pixmap(w, w, QIcon::Normal, QIcon::On), px, position),
        QIcon::Normal,
        QIcon::Off
    );

    return overlayedIcon;
}

qreal BitmapFactoryInst::getMaximumDPR()
{
    qreal dpr = 1.0F;

    for (QScreen* screen : QGuiApplication::screens()) {
        dpr = std::max(screen->devicePixelRatio(), dpr);
    }

    return dpr;
}
