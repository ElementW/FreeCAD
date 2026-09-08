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

#include <algorithm>

#include <gmock/gmock.h>


MATCHER_P(HasTranslatableNameOf, translatableName, "")
{
    *result_listener << "has a translatable name of " << translatableName;
    return arg->translatableName == translatableName;
}

MATCHER_P(HasMainMimeOf, mime, "")
{
    *result_listener << "has a main MIME type of " << mime;
    return arg->mimeType == mime;
}

MATCHER_P(HandlesMime, mime, "")
{
    *result_listener << "handles files with MIME type of " << mime;
    return std::ranges::find(arg->fileMimeTypes, mime) != std::ranges::end(arg->fileMimeTypes);
}

MATCHER_P(AdapterWithModule, module, "")
{
    *result_listener << "is an adapter with a module of " << module;
    return arg->moduleName == module;
}
