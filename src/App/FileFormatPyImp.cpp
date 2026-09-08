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

#include "FileFormatPy.h"
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "FileFormatPy.cpp"


using namespace App;


std::string FileFormatPy::representation() const
{
    return {"<FileFormat object>"};
}

Py::String FileFormatPy::getDisplayName() const
{
    //return Py::String();
    throw Py::AttributeError("Not yet implemented");
}

Py::String FileFormatPy::getTranslatableName() const
{
    //return Py::String();
    throw Py::AttributeError("Not yet implemented");
}

Py::Object FileFormatPy::getMimeType() const
{
    //return Py::Object();
    throw Py::AttributeError("Not yet implemented");
}

Py::Object FileFormatPy::getSecondaryMimeTypes() const
{
    //return Py::Object();
    throw Py::AttributeError("Not yet implemented");
}

Py::Object FileFormatPy::getFileNamePatterns() const
{
    //return Py::Object();
    throw Py::AttributeError("Not yet implemented");
}

PyObject *FileFormatPy::getCustomAttributes(const char* /*attr*/) const
{
    return nullptr;
}

int FileFormatPy::setCustomAttributes(const char* /*attr*/, PyObject* /*obj*/)
{
    return 0;
}
