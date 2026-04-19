// SPDX-License-Identifier: LGPL-2.1-or-later
// SPDX-FileCopyrightText: 2026 Céleste Wouters <foss@elementw.net>
// SPDX-FileNotice: Part of the FreeCAD project.

/******************************************************************************
 *                                                                            *
 *   FreeCAD is free software: you can redistribute it and/or modify          *
 *   it under the terms of the GNU Lesser General Public License as           *
 *   published by the Free Software Foundation, either version 2.1            *
 *   of the License, or (at your option) any later version.                   *
 *                                                                            *
 *   FreeCAD is distributed in the hope that it will be useful,               *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty              *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 *   See the GNU Lesser General Public License for more details.              *
 *                                                                            *
 *   You should have received a copy of the GNU Lesser General Public         *
 *   License along with FreeCAD. If not, see https://www.gnu.org/licenses     *
 *                                                                            *
 ******************************************************************************/

#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>

#include "DisplayedFilesModel.h"


class QFileInfo;

namespace Start
{

class InfoSourceSignals: public QObject
{
    Q_OBJECT
public:
Q_SIGNALS:
    void infoAvailable(
        const QString& filePath,
        const FileStats& stats,
        const QByteArray& thumbnail,
        const QString& thumbnailPath
    );
};

class InfoSource;

struct InfoSourceType
{
    using Constructor = InfoSource*(QString filePath, int thumbnailSizeHint);
    using HandlesFile = bool(const QFileInfo&);
    Constructor* makeSource;
    HandlesFile* handlesFile;
};

class InfoSource: public QRunnable
{
    Q_DISABLE_COPY_MOVE(InfoSource)

public:
    using Signals = InfoSourceSignals;
    using Type = InfoSourceType;

    InfoSource() = default;
    ~InfoSource() override = default;

    void run() override = 0;

    // Having a signal QObject as part of the QRunnable ensures signal connections
    // are properly cleaned up when the QRunnable gets destroyed as it finishes its work.
    Signals signals;
};

}  // namespace Start
