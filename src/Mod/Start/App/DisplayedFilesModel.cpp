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

#include "DisplayedFilesModel.h"

#include <cctype>
#include <ranges>

#include <QThreadPool>

#include <App/Application.h>

#include "FcstdInfoSource.h"
#include "FileUtilities.h"
#include "F3DInfoSource.h"


using namespace Start;

/// Get information common to all file types.
static FileStats getCommonFileInfo(const std::string& path)
{
    FileStats result;
    const Base::FileInfo file(path);
    result.insert(
        std::make_pair(DisplayedFilesModelRoles::modifiedTime, getLastModifiedAsString(file))
    );
    result.insert(std::make_pair(DisplayedFilesModelRoles::path, path));
    result.insert(std::make_pair(DisplayedFilesModelRoles::size, humanReadableSize(file.size())));
    result.insert(std::make_pair(DisplayedFilesModelRoles::baseName, file.fileName()));
    return result;
}

static bool freecadCanOpen(const QString& extension)
{
    std::string ext = extension.toStdString();
    auto importTypes = App::GetApplication().getImportTypes();
    return std::ranges::find_if(
               importTypes,
               [&ext](const auto& item) {
                   return std::ranges::equal(item, ext, [](unsigned char a, unsigned char b) -> bool {
                       return std::tolower(a) == std::tolower(b);
                   });
               }
           )
        != importTypes.end();
}

DisplayedFilesModel::DisplayedFilesModel(QObject* parent, int thumbnailSizeHint)
    : QAbstractListModel(parent)
    , _thumbnailSizeHint(thumbnailSizeHint)
{
    _infoSourceTypes.emplace_back(&F3DInfoSource::type);
    _infoSourceTypes.emplace_back(&FcstdInfoSource::type);
}

int DisplayedFilesModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(_fileInfoCache.size());
}

QVariant DisplayedFilesModel::data(const QModelIndex& index, int role) const
{
    QMutexLocker locker(&_mutex);
    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(_fileInfoCache.size())) {
        return {};
    }
    const auto mapEntry = _fileInfoCache.at(row);
    switch (const auto roleAsType = static_cast<DisplayedFilesModelRoles>(role)) {
        case DisplayedFilesModelRoles::author:  // NOLINT(bugprone-branch-clone)
            [[fallthrough]];
        case DisplayedFilesModelRoles::baseName:
            [[fallthrough]];
        case DisplayedFilesModelRoles::company:
            [[fallthrough]];
        case DisplayedFilesModelRoles::creationTime:
            [[fallthrough]];
        case DisplayedFilesModelRoles::description:
            [[fallthrough]];
        case DisplayedFilesModelRoles::imageCachePath:
            [[fallthrough]];
        case DisplayedFilesModelRoles::license:
            [[fallthrough]];
        case DisplayedFilesModelRoles::modifiedTime:
            [[fallthrough]];
        case DisplayedFilesModelRoles::path:
            [[fallthrough]];
        case DisplayedFilesModelRoles::size:
            if (mapEntry.contains(roleAsType)) {
                return QString::fromStdString(mapEntry.at(roleAsType));
            }
            break;
        case DisplayedFilesModelRoles::image: {
            if (const auto& path = mapEntry.at(DisplayedFilesModelRoles::path);
                _imageCache.contains(path)) {
                return _imageCache[path];
            }
            break;
        }
        default:
            break;
    }
    switch (role) {
        case Qt::ItemDataRole::ToolTipRole: {
            auto toolTip = QString::fromStdString(mapEntry.at(DisplayedFilesModelRoles::path));
            auto addInfo = [&toolTip, &mapEntry](const QString& text, DisplayedFilesModelRoles role) {
                auto it = mapEntry.find(role);
                if (it != mapEntry.end()) {
                    auto str = QString::fromStdString(it->second);
                    QDateTime dt = QDateTime::fromString(str, Qt::DateFormat::ISODate);
                    toolTip.append(QChar('\n'));
                    toolTip.append(text);
                    toolTip.append(QChar(' '));
                    QLocale loc = QLocale::system();
                    toolTip.append(loc.toString(dt));
                }
            };

            addInfo(tr("Created at:"), DisplayedFilesModelRoles::creationTime);
            addInfo(tr("Modified at:"), DisplayedFilesModelRoles::modifiedTime);

            return toolTip;
        }
        default:
            // No other role gets handled
            break;
    }
    return {};
}

void DisplayedFilesModel::addInfoSourceType(const InfoSourceType& type)
{
    QMutexLocker locker(&_mutex);
    _infoSourceTypes.emplace_back(&type);
}

static InfoSource* createInfoSource(
    const std::vector<gsl::not_null<const InfoSourceType*>>& types,
    const std::filesystem::path& filePath,
    int thumbnailSizeHint
)
{
    // Iterate backwards so the last info source types that were added take priority
    for (gsl::not_null<const InfoSourceType*> type : std::views::reverse(types)) {
        if (type->handlesFile(filePath)) {
            return type->makeSource(filePath, thumbnailSizeHint);
        }
    }
    return nullptr;
}

void DisplayedFilesModel::addFile(const std::filesystem::path& filePath)
{
    const QFileInfo qfi(filePath);
    if (!qfi.isReadable()) {
        return;
    }

    if (!freecadCanOpen(qfi.suffix())) {
        return;
    }

    {
        QMutexLocker locker(&_mutex);
        _fileInfoCache.emplace_back(getCommonFileInfo(filePath));
    }

    InfoSource* source = createInfoSource(_infoSourceTypes, filePath, _thumbnailSizeHint);
    if (source) {
        connect(
            &source->signals,
            &InfoSource::Signals::infoAvailable,
            this,
            &DisplayedFilesModel::processNewFileInfo
        );
        // `source` will be deleted by the thread pool once work is done
        QThreadPool::globalInstance()->start(source);
    }
}

void DisplayedFilesModel::clear()
{
    QMutexLocker locker(&_mutex);
    _fileInfoCache.clear();
}

QHash<int, QByteArray> DisplayedFilesModel::roleNames() const
{
    static QHash<int, QByteArray> nameMap {
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::author), "author"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::baseName), "baseName"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::company), "company"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::creationTime), "creationTime"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::description), "description"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::image), "image"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::imageCachePath), "imageCachePath"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::license), "license"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::modifiedTime), "modifiedTime"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::path), "path"),
        std::make_pair(static_cast<int>(DisplayedFilesModelRoles::size), "size"),
    };
    return nameMap;
}

static std::size_t indexOfFile(const std::vector<FileStats>& fileInfoCache, const std::filesystem::path& filePath)
{
    auto it = std::ranges::find_if(fileInfoCache, [filePath](const FileStats& row) {
        auto pathIt = row.find(DisplayedFilesModelRoles::path);
        return pathIt != row.end() && pathIt->second == filePath;
    });
    return std::distance(fileInfoCache.begin(), it);
}

void DisplayedFilesModel::processNewFileInfo(
    const std::filesystem::path& filePath,
    const FileStats& stats,
    const QByteArray& thumbnail,
    const QString& thumbnailPath
)
{
    QMutexLocker locker(&_mutex);

    const std::size_t index = indexOfFile(_fileInfoCache, filePath);
    if (index == _fileInfoCache.size()) {
        return;
    }

    QList<int> changedRoles;
    auto& info = _fileInfoCache[index];
    for (auto stat : stats) {
        info.insert(stat);
        changedRoles.append(static_cast<int>(stat.first));
    }

    if (!thumbnail.isEmpty()) {
        _imageCache.insert(filePath, thumbnail);
        changedRoles.append(static_cast<int>(DisplayedFilesModelRoles::image));
    }
    if (!thumbnailPath.isEmpty()) {
        info.emplace(DisplayedFilesModelRoles::imageCachePath, thumbnailPath.toStdString());
        changedRoles.append(static_cast<int>(DisplayedFilesModelRoles::imageCachePath));
    }

    locker.unlock();
    QModelIndex qmi = createIndex(index, 0);
    Q_EMIT dataChanged(qmi, qmi, changedRoles);
}
