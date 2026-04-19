// SPDX-License-Identifier: LGPL-2.1-or-later
/****************************************************************************
 *                                                                          *
 *   Copyright (c) 2024 The FreeCAD Project Association AISBL               *
 *                                                                          *
 *   This file is part of FreeCAD.                                          *
 *                                                                          *
 *   FreeCAD is free software: you can redistribute it and/or modify it     *
 *   under the terms of the GNU Lesser General Public License as            *
 *   published by the Free Software Foundation, either version 2.1 of the   *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   FreeCAD is distributed in the hope that it will be useful, but         *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with FreeCAD. If not, see                                *
 *   <https://www.gnu.org/licenses/>.                                       *
 *                                                                          *
 ***************************************************************************/

#include "FileCardDelegate.h"

#include <cstdint>

#include <QApplication>
#include <QCache>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>

#include <App/Application.h>

#include "../App/DisplayedFilesModel.h"

using namespace Start;

namespace
{
struct ThumbnailData
{
    enum class Source : uint8_t
    {
        Default = 0,
        DataModelProvided = 1,
    };

    QPixmap pixmap;
    qint64 lastModified = 0;
    int thumbnailSize;
    Source source;

    ThumbnailData(QPixmap pixmap, const QString& path, int thumbnailSize, Source source)
        : pixmap(std::move(pixmap))
        , thumbnailSize(thumbnailSize)
        , source(source)
    {
        if (const QFileInfo fileInfo(path); fileInfo.exists()) {
            lastModified = fileInfo.lastModified().toSecsSinceEpoch();
        }
    }

    bool isStale(const QString& path, int newThumbnailSize, Source newSource) const
    {
        const QFileInfo fileInfo(path);
        return (lastModified != (fileInfo.exists() ? fileInfo.lastModified().toSecsSinceEpoch() : 0))
            || (thumbnailSize != newThumbnailSize)
            || (newSource > source)  // Thumbnail sources have priorities
            ;
    }
};
}  // namespace

static QCache<QString, ThumbnailData> thumbnailCache;
static constexpr const int CACHE_SIZE_MB = 50;  // 50MB cache limit

int FileCardDelegate::thumbnailSize()
{
    return App::GetApplication()
        .GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Start")
        ->GetInt("FileThumbnailIconsSize", DefaultThumbnailSize);
}

FileCardDelegate::FileCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    setObjectName(QStringLiteral("thumbnailWidget"));

    // Initialize cache size based on thumbnail size (only once)
    if (thumbnailCache.maxCost() == 0) {
        const auto thumbnailSize = FileCardDelegate::thumbnailSize();
        constexpr int BytesPerPixel = 4;  // RGBA
        int thumbnailMemory = thumbnailSize * thumbnailSize * BytesPerPixel;
        int maxCacheItems = (CACHE_SIZE_MB * 1024 * 1024) / thumbnailMemory;
        thumbnailCache.setMaxCost(maxCacheItems);
        Base::Console().log(
            "FileCardDelegate: Initialized thumbnail cache for %d items (%d MB)\n",
            maxCacheItems,
            CACHE_SIZE_MB
        );
    }
}

void FileCardDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    painter->save();
    // Step 1: Styling
    QStyleOptionButton buttonOption;
    buttonOption.initFrom(option.widget);
    buttonOption.rect = option.rect;
    buttonOption.state = QStyle::State_Enabled;

    if ((option.state & QStyle::State_MouseOver) != 0) {
        buttonOption.state |= QStyle::State_MouseOver;
    }
    if ((option.state & QStyle::State_Selected) != 0) {
        buttonOption.state |= QStyle::State_On;
    }
    if ((option.state & QStyle::State_Sunken) != 0) {
        buttonOption.state |= QStyle::State_Sunken;
    }
    QApplication::style()->drawControl(QStyle::CE_PushButton, &buttonOption, painter, &styleButton);

    // Step 2: Fetch required data
    using Roles = DisplayedFilesModelRoles;
    const auto thumbnailSize = FileCardDelegate::thumbnailSize();
    auto baseName = index.data(static_cast<int>(Roles::baseName)).toString();
    auto elidedName = painter->fontMetrics().elidedText(baseName, Qt::ElideRight, thumbnailSize);
    auto size = index.data(static_cast<int>(Roles::size)).toString();
    auto image = index.data(static_cast<int>(Roles::image)).toByteArray();
    auto path = index.data(static_cast<int>(Roles::path)).toString();

    QPixmap pixmap;
    // Check if we have this thumbnail already inside cache, don't load it once again.
    const auto source = image.isEmpty() ? ThumbnailData::Source::Default
                                        : ThumbnailData::Source::DataModelProvided;
    if (ThumbnailData* cachedThumbnailData = thumbnailCache.object(path);
        cachedThumbnailData != nullptr && !cachedThumbnailData->isStale(path, thumbnailSize, source)) {
        pixmap = cachedThumbnailData->pixmap;
    }
    else {
        // If not and encoded thumbnail bytes are available, attempt decoding them,
        // deleting the backing thumbnail file if that fails (assumed to be corrupt).
        if (pixmap.isNull() && !image.isEmpty() && !pixmap.loadFromData(image)) {
            // Don't be tempted to set `source` to `Default` here, the point is that even a
            // failed thumbnail load's fallback gets cached with that level of priority.
            Base::Console().log("Failed to load thumbnail for %s\n", path.toStdString());
            if (auto imageCachePath = index.data(static_cast<int>(Roles::imageCachePath)).toString();
                !imageCachePath.isEmpty()) {
                if (QFile imageCacheFile(imageCachePath); imageCacheFile.exists()) {
                    Base::Console().log(
                        "Deleting cached thumbnail at %s\n",
                        imageCachePath.toStdString()
                    );
                    // Ignore deletion failure, not critical
                    (void)imageCacheFile.remove();
                }
            }
        }
        // Check for null again in case no data model thumbnail is available yet or
        // the above loadFromData() failed
        if (pixmap.isNull()) {
            pixmap = loadThumbnail(path, thumbnailSize);
        }
        // Cache the thumbnail if valid.
        if (!pixmap.isNull()) {
            thumbnailCache.insert(path, new ThumbnailData(pixmap, path, thumbnailSize, source), 1);
        }
    }

    QPixmap scaledPixmap = pixmap.scaled(
        QSize(thumbnailSize, thumbnailSize),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // Step 4: Positioning
    QRect thumbnailRect(option.rect.x() + margin, option.rect.y() + margin, thumbnailSize, thumbnailSize);
    QRect textRect(
        option.rect.x() + margin,
        thumbnailRect.bottom() + margin,
        thumbnailSize,
        painter->fontMetrics().lineSpacing()
    );

    QRect sizeRect(
        option.rect.x() + margin,
        textRect.bottom() + textspacing,
        thumbnailSize,
        painter->fontMetrics().lineSpacing() + margin
    );

    // Step 5: Draw
    QRect pixmapRect(thumbnailRect.topLeft(), scaledPixmap.size());
    pixmapRect.moveCenter(thumbnailRect.center());
    painter->drawPixmap(pixmapRect.topLeft(), scaledPixmap);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);
    painter->drawText(sizeRect, Qt::AlignLeft | Qt::AlignTop, size);
    painter->restore();
}


QSize FileCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    const auto thumbnailSize = FileCardDelegate::thumbnailSize();

    QFontMetrics qfm(QGuiApplication::font());
    int textHeight = textspacing + qfm.lineSpacing() * 2;  // name + size
    int cardWidth = static_cast<int>(thumbnailSize) + 2 * margin;
    int cardHeight = static_cast<int>(thumbnailSize) + textHeight + 3 * margin;

    return {cardWidth, cardHeight};
}

QPixmap FileCardDelegate::loadThumbnail(const QString& path, int thumbnailSize) const
{
    QPixmap thumbnail;

    if (path.endsWith(QStringLiteral(".fcstd"), Qt::CaseSensitivity::CaseInsensitive)) {
        // This is a fallback, the model will have pulled the thumbnail out of the FCStd file if it
        // existed.
        QImageReader reader(QStringLiteral(":/icons/freecad-doc.svg"));
        reader.setScaledSize(QSize(thumbnailSize, thumbnailSize));
        thumbnail = QPixmap::fromImage(reader.read());
    }
    else if (path.endsWith(QStringLiteral(".fcmacro"), Qt::CaseSensitivity::CaseInsensitive)) {
        QImageReader reader(QStringLiteral(":/icons/MacroEditor.svg"));
        reader.setScaledSize(QSize(thumbnailSize, thumbnailSize));
        thumbnail = QPixmap::fromImage(reader.read());
    }

    // fallback to system icon if no thumbnail was generated
    if (thumbnail.isNull()) {
        QIcon icon = QFileIconProvider().icon(QFileInfo(path));
        if (!icon.isNull()) {
            thumbnail = icon.pixmap(thumbnailSize);
        }
        else {
            thumbnail = QPixmap(thumbnailSize, thumbnailSize);
            thumbnail.fill();
        }
    }

    return thumbnail;
}
