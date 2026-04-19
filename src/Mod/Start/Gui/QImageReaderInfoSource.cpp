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

#include "QImageReaderInfoSource.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>


using namespace Start;
using namespace StartGui;

constexpr InfoSource::Type QImageReaderInfoSource::type {
    .makeSource = [](QString filePath, int thumbnailSize) -> InfoSource* {
        return new QImageReaderInfoSource(std::move(filePath), thumbnailSize);
    },
    .handlesFile = [](const QFileInfo& qfi) -> bool {
        return !QImageReader::imageFormat(qfi.absoluteFilePath()).isEmpty();
    },
};

QImageReaderInfoSource::QImageReaderInfoSource(QString filePath, int thumbnailSizeHint)
    : filePath(std::move(filePath))
    , thumbnailSizeHint(thumbnailSizeHint)
{}

void QImageReaderInfoSource::run()
{
    QImageReader reader(filePath);

    // get original size to calculate proper aspect-preserving scaled size
    QSize originalSize = reader.size();
    if (originalSize.isValid()) {
        QSize scaledSize
            = originalSize.scaled(thumbnailSizeHint, thumbnailSizeHint, Qt::KeepAspectRatio);
        reader.setScaledSize(scaledSize);
    }

    const auto image = reader.read();
    if (!image.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "png");
        Q_EMIT signals.infoAvailable(filePath, {}, bytes, {});
        return;
    }

    Base::Console().log(
        "QImageReaderInfoSource: Failed to load image %s: %s\n",
        filePath.toStdString().c_str(),
        reader.errorString().toStdString().c_str()
    );
}
