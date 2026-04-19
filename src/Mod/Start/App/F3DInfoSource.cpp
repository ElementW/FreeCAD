// SPDX-License-Identifier: LGPL-2.1-or-later
// SPDX-FileCopyrightText: 2026 FreeCAD contributors
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

#include "F3DInfoSource.h"

#include <mutex>
#include <ranges>
#include <string_view>

#include <QProcess>

#include <Base/Console.h>
#include <Base/Parameter.h>

#include <App/Application.h>

#include "FileUtilities.h"


using namespace Start;
using namespace std::string_view_literals;
using namespace std::views;

/// Gather together all of the f3d information protected by the mutex: data in this struct
/// should be accessed only after a call to setupF3D() to ensure synchronization.
static struct F3DInstallation
{
    bool initialized {false};
    int major {0};
    int minor {0};
    QStringList baseArgs;
} f3d;

static std::mutex mutex;

static std::array<int, 3> extractF3DVersion(std::string_view stdoutString)
{
    std::array<int, 3> ver {0, 0, 0};
    constexpr auto versionMarker = "Version: "sv;
    for (const auto lineRange : stdoutString | split("\n"sv)) {
        const std::string_view line {lineRange.begin(), lineRange.end()};
        if (!line.starts_with(versionMarker)) {
            continue;
        }
        const auto substring = line.substr(versionMarker.size());
        int i = 0;
        for (const auto num : substring | split("."sv) | take(3)) {
            const auto [p, ec] = std::from_chars(num.begin(), num.end(), ver[i++]);  // NOLINT
            if (ec != std::errc {}) {
                return {0, 0, 0};
            }
        }
        if (i != 3) {
            return {0, 0, 0};
        }
        break;
    }
    return ver;
}

static std::string getF3dPath()
{
    const ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );
    return hGrp->GetASCII("f3d", "f3d");
}

static QStringList getF3DOptions(const QString& f3dPath)
{
    // F3D is under active development, and the available options change with some regularity.
    // Rather than hardcode per version, just check the ones we care about.
    QStringList optionsToTest {
        QStringLiteral("--load-plugins=occt"),
        QStringLiteral("--config=thumbnail"),
        QStringLiteral("--verbose=quiet"),
        QStringLiteral("--resolution=256,256"),
        QStringLiteral("--filename=0"),
        QStringLiteral("--grid=0"),
        QStringLiteral("--axis=0"),
        QStringLiteral("--no-background"),
        QStringLiteral("--max-size=100")  // Max input file size in MB
    };
    QStringList goodOptions;
    for (const auto& option : optionsToTest) {
        QStringList args;
        args << option << QStringLiteral("--no-render");
        QProcess process;
        process.start(f3dPath, args);
        if (!process.waitForFinished()) {
            process.kill();
            continue;
        }
        auto stderrAsBytes = process.readAllStandardError();
        if (auto stderrAsString = QString::fromUtf8(stderrAsBytes);
            !stderrAsString.contains(QStringLiteral("Unknown option"))) {
            goodOptions.append(option);
        }
    }
    return goodOptions;
}

static void setupF3D()
{
    std::lock_guard<std::mutex> guard(mutex);
    if (f3d.initialized) {
        return;
    }

    // This method makes repeated blocking calls to f3d (both directly, the call below, and
    // indirectly, by calling getF3DOptions). By holding the mutex above, it ensures that these
    // calls complete before any process can attempt to make a real call to f3d to create thumbnail
    // data. ThumbnailSource is run in its own thread, so blocking here is appropriate and will not
    // affect any other part of the program.

    f3d.initialized = true;  // Set immediately so we can use early-return below
    const auto f3dPath = QString::fromStdString(getF3dPath());
    const QStringList args {QStringLiteral("--version")};
    QProcess process;
    process.start(f3dPath, args);
    if (!process.waitForFinished()) {
        process.kill();
    }
    if (process.exitCode() != 0) {
        return;
    }
    const QByteArray stdoutBytes = process.readAllStandardOutput();
    const auto version = extractF3DVersion({stdoutBytes.data(), size_t(stdoutBytes.size())});
    if (version[0] == 0 && version[1] == 0 && version[2] == 0) {
        Base::Console().log("Could not determine F3D version, disabling thumbnail generation\n");
    }
    else {
        f3d.major = version[0];
        f3d.minor = version[1];
        if (f3d.major >= 2) {
            f3d.baseArgs = getF3DOptions(f3dPath);
        }
        Base::Console().log("Running f3d version %d.%d.%d\n", f3d.major, f3d.minor, version[2]);
    }
}

constexpr InfoSource::Type F3DInfoSource::type {
    .makeSource = [](QString filePath, int) -> InfoSource* {
        return new F3DInfoSource(std::move(filePath));
    },
    .handlesFile = [](const QFileInfo& qfi) -> bool {
        static const QStringList ignoredExtensions {
            QStringLiteral("fcstd"),
            QStringLiteral("fcmacro"),
            QStringLiteral("py"),
            QStringLiteral("pyi"),
            QStringLiteral("csv"),
            QStringLiteral("txt"),
            QStringLiteral("tiff"),
            QStringLiteral("tif"),
            QStringLiteral("png"),
            QStringLiteral("jpeg"),
            QStringLiteral("jpg"),
            QStringLiteral("bmp"),
            QStringLiteral("tga"),
        };
        return !ignoredExtensions.contains(qfi.suffix(), Qt::CaseSensitivity::CaseInsensitive);
    },
};

F3DInfoSource::F3DInfoSource(QString filePath)
    : filePath(std::move(filePath))
{}

void F3DInfoSource::run()
{
    QString thumbnailPath = getPathToCachedThumbnail(filePath);
    if (!useCachedThumbnail(thumbnailPath, filePath)) {
        // Go through the mutex to ensure data is not stale.
        // Contention on the lock is diminished because of first checking the cache.
        setupF3D();
        if (f3d.major < 2) {
            return;
        }
        const auto f3dPath = QString::fromStdString(getF3dPath());
        QStringList args(f3d.baseArgs);
        args << QStringLiteral("--output=") + thumbnailPath << filePath;

        Base::Console().log("Creating thumbnail for %s...\n", filePath.toStdString());
        QProcess process;
        process.start(f3dPath, args);
        if (!process.waitForFinished()) {
            process.kill();
            Base::Console().log("Creating thumbnail for %s timed out\n", filePath.toStdString());
            return;
        }
        if (process.exitStatus() == QProcess::CrashExit) {
            Base::Console().log("Creating thumbnail for %s crashed\n", filePath.toStdString());
            return;
        }
        if (process.exitCode() != 0) {
            Base::Console().log(
                "Creating thumbnail for %s failed: f3d exited with code %d\n",
                filePath.toStdString(),
                process.exitCode()
            );
            return;
        }
        Base::Console().log(
            "Creating thumbnail for %s succeeded, wrote to %s\n",
            filePath.toStdString(),
            thumbnailPath.toStdString()
        );
    }
    if (QFile thumbnailFile(thumbnailPath); thumbnailFile.open(QIODevice::OpenModeFlag::ReadOnly)) {
        Q_EMIT signals.infoAvailable(filePath, {}, thumbnailFile.readAll(), thumbnailPath);
    }
}
