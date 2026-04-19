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

#include "CustomFolderModel.h"

#include <filesystem>
#include <ranges>
#include <string_view>

#include <App/Application.h>


using namespace Start;
using namespace std::string_view_literals;
namespace fs = std::filesystem;


CustomFolderModel::CustomFolderModel(QObject* parent, int thumbnailSizeHint)
    : DisplayedFilesModel(parent, thumbnailSizeHint)
{

    Base::Reference<ParameterGrp> parameterGroup = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );

    customFolderPathSpec = parameterGroup->GetASCII("CustomFolder", "");

    showOnlyFCStd = parameterGroup->GetBool("ShowOnlyFCStd", false);
}

/// If the custom folder path contains multiple paths separated by ';;', split them into individual
/// paths. This is used to allow the user to specify multiple paths in the preferences dialog.
/// We use ';;' as a separator because ';' is a valid character in a file path (e.g. NTFS on
/// Windows).
void CustomFolderModel::loadCustomFolder()
{
    beginResetModel();
    clear();
    constexpr auto pathDelimiter = ";;"sv;

    std::vector<fs::directory_entry> entries;
    for (const auto path : std::views::split(customFolderPathSpec, pathDelimiter)) {
        const fs::path customFolderDirectory(path.begin(), path.end());
        entries.clear();
        try {
            for (auto file : std::filesystem::directory_iterator {customFolderDirectory}) {
                if (!showOnlyFCStd || file.path().extension().compare(".FCStd"sv) == 0) {
                    entries.emplace_back(std::move(file));
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            Base::Console().warning(
                "BaseApp/Preferences/Mod/Start/CustomFolder: cannot read custom folder %s: %s\n",
                customFolderDirectory.string(),
                e.code().message()
            );
        }
        std::ranges::sort(entries);
        for (const auto& entry : entries) {
            addFile(QString::fromStdString(entry.path().string()));
        }
    }

    endResetModel();
}
