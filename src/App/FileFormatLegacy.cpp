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

#include "FileFormat.h"

#include <algorithm>
#include <stdexcept>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <Base/Exception.h>


#ifndef FC_NO_LEGACY_FORMAT_HANDLING

namespace App
{

/// Create a Format from a legacy format filter string.
/// The created MIME type is completely synthetic.
FileFormat FileFormat::fromLegacyFilter(std::string_view filter)
{
    FileNamePatternList fileNamePatterns;
    const std::string_view f {filter};
    const auto fileNamePatternsStart = f.find_last_of('(');
    if (fileNamePatternsStart == std::string_view::npos) {
        throw std::invalid_argument("No pattern list opening parens found");
    }
    auto pos = fileNamePatternsStart;
    while (true) {
        const auto next = f.find_first_of(" )", pos + 1);
        if (next == std::string_view::npos) {
            break;
        }
        const auto len = next - pos - 1;
        fileNamePatterns.emplace_back(f.substr(pos + 1, len));
        pos = next;
    }
    const auto name = f.substr(0, fileNamePatternsStart - 1);
    return {std::string(name), "x-fc-synthetic/" + std::string(name), std::move(fileNamePatterns)};
}

std::vector<std::string> FileFormat::getLegacyFileExtensions() const
{
    std::vector<std::string> formats;
    for (const auto& fileNamePattern : fileNamePatterns) {
        if (fileNamePattern.starts_with("*.")) {
            formats.emplace_back(fileNamePattern.begin() + 2, fileNamePattern.end());
        }
    }
    return formats;
}

bool FileFormat::matchesLegacyFileExtension(std::string_view extension) const
{
    return std::ranges::any_of(fileNamePatterns, [&extension](const auto& fileNamePattern) {
        return fileNamePattern.starts_with("*.")
            && std::ranges::equal(
                   extension,
                   std::string_view(fileNamePattern).substr(2),
                   [](char a, char b) -> bool { return std::tolower(a) == std::tolower(b); }
            );
    });
}

std::string FileAdapter::getLegacyFileFilter(const Formats& formats) const
{
    if (!originalLegacyFileFilter.empty()) {
        return originalLegacyFileFilter;
    }
    std::string filter {supportedFormatsText()};
    filter += " (";
    for (const auto& mime : fileMimeTypes) {
        if (const auto format = formats.formatByMimeType(mime)) {
            for (const auto& fileNamePattern : format->fileNamePatterns) {
                filter += fileNamePattern;
                filter += ' ';
            }
        }
    }
    *(filter.end() - 1) = ')';
    return filter;
}

void Formats::addImportType(const char* filter, const char* moduleName)
{
    auto format = FileFormat::fromLegacyFilter(filter);

    FileImporter importer;
    importer.createdFromLegacy = true;
    importer.moduleName = moduleName;
    importer.fileMimeTypes.emplace_back(format.mimeType);
    importer.translatableSupportedFormatsText = format.translatableName;
    importer.translatableImportActionText = format.translatableName;
    importer.translatableImportFilesText = format.translatableName;
    importer.originalLegacyFileFilter = filter;

    addFormat(std::move(format));
    addImporter(std::move(importer));
}

void Formats::changeImportModule(const char* filter, const char* oldModuleName, const char* newModuleName)
{
    auto format = FileFormat::fromLegacyFilter(filter);
    if (formatByMimeType(format.mimeType) == nullptr) {
        throw std::invalid_argument(fmt::format("No previously registered \"{}\" filter", filter));
    }

    for (auto& oldImporter : _importers) {
        if (oldImporter->moduleName == oldModuleName) {
            const auto [first, last] = std::ranges::remove_if(
                oldImporter->fileMimeTypes,
                [&format](const std::string& mime) { return mime == format.mimeType; }
            );
            oldImporter->fileMimeTypes.erase(first, last);
        }
    }

    FileImporter newImporter;
    newImporter.createdFromLegacy = true;
    newImporter.moduleName = newModuleName;
    newImporter.fileMimeTypes.emplace_back(format.mimeType);
    newImporter.translatableSupportedFormatsText = format.translatableName;
    newImporter.translatableImportActionText = format.translatableName;
    newImporter.translatableImportFilesText = format.translatableName;
    newImporter.originalLegacyFileFilter = filter;
    addImporter(std::move(newImporter));
}

std::vector<std::string> Formats::getImportModules(const std::string& extension) const
{
    const auto importers = this->importers();
    std::vector<std::string> modules;
    for (const auto& importer : importers) {
        for (const auto& mime : importer->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime);
                format && format->matchesLegacyFileExtension(extension)) {
                modules.push_back(importer->moduleName);
            }
        }
    }
    return modules;
}

std::vector<std::string> Formats::getImportModules() const
{
    const auto importers = this->importers();
    std::vector<std::string> modules;
    modules.reserve(importers.size());
    for (const auto& importer : importers) {
        modules.push_back(importer->moduleName);
    }

    std::ranges::sort(modules);
    const auto [first, last] = std::ranges::unique(modules);
    modules.erase(first, last);

    return modules;
}

std::vector<std::string> Formats::getImportTypes(const std::string& module) const
{
    std::vector<std::string> types;
    const auto importers = importersByModule(module);
    for (const auto& importer : importers) {
        for (const auto& mime : importer->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime); format) {
                const auto legacy = format->getLegacyFileExtensions();
                types.insert(types.end(), legacy.begin(), legacy.end());
            }
        }
    }
    return types;
}

std::vector<std::string> Formats::getImportTypes() const
{
    std::vector<std::string> types;
    for (const auto& importer : importers()) {
        for (const auto& mime : importer->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime); format) {
                const auto legacy = format->getLegacyFileExtensions();
                types.insert(types.end(), legacy.begin(), legacy.end());
            }
        }
    }

    std::ranges::sort(types);
    const auto [first, last] = std::ranges::unique(types);
    types.erase(first, last);

    return types;
}

std::map<std::string, std::string> Formats::getImportFilters(const std::string& extension) const
{
    std::map<std::string, std::string> moduleFilter;
    for (const auto& importer : importers()) {
        bool matchesType = false;
        for (const auto& mime : importer->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime);
                format && format->matchesLegacyFileExtension(extension)) {
                matchesType = true;
                break;
            }
        }
        if (matchesType) {
            moduleFilter[importer->getLegacyFileFilter(*this)] = importer->moduleName;
        }
    }
    return moduleFilter;
}

std::map<std::string, std::string> Formats::getImportFilters() const
{
    std::map<std::string, std::string> filter;
    for (const auto& importer : importers()) {
        filter[importer->getLegacyFileFilter(*this)] = importer->moduleName;
    }
    return filter;
}

void Formats::addExportType(const char* filter, const char* moduleName)
{
    auto format = FileFormat::fromLegacyFilter(filter);

    FileExporter exporter;
    exporter.createdFromLegacy = true;
    exporter.moduleName = moduleName;
    exporter.fileMimeTypes.emplace_back(format.mimeType);
    exporter.translatableSupportedFormatsText = format.translatableName;
    exporter.translatableExportActionText = format.translatableName;
    exporter.translatableExportFilesText = format.translatableName;
    exporter.originalLegacyFileFilter = filter;

    addFormat(std::move(format));
    addExporter(std::move(exporter));
}

void Formats::addTranslatableExportType(
    const std::string& description,
    const std::vector<std::string>& extensions,
    const std::string& moduleName
)
{
    FileFormat format {description, "x-fc-synthetic/" + description, {}};
    for (const auto& extension : extensions) {
        format.fileNamePatterns.emplace_back("*." + extension);
    }

    FileExporter exporter;
    exporter.createdFromLegacy = true;
    exporter.moduleName = moduleName;
    exporter.fileMimeTypes.emplace_back(format.mimeType);
    exporter.translatableSupportedFormatsText = description;
    exporter.translatableExportActionText = description;
    exporter.translatableExportFilesText = description;

    addFormat(std::move(format));
    addExporter(std::move(exporter));
}

void Formats::changeExportModule(const char* filter, const char* oldModuleName, const char* newModuleName)
{
    auto format = FileFormat::fromLegacyFilter(filter);
    if (formatByMimeType(format.mimeType) == nullptr) {
        throw std::invalid_argument(fmt::format("No previously registered \"{}\" filter", filter));
    }

    for (auto& oldExporter : _exporters) {
        if (oldExporter->moduleName == oldModuleName) {
            const auto [first, last] = std::ranges::remove_if(
                oldExporter->fileMimeTypes,
                [&format](const std::string& mime) { return mime == format.mimeType; }
            );
            oldExporter->fileMimeTypes.erase(first, last);
        }
    }

    FileExporter newExporter;
    newExporter.createdFromLegacy = true;
    newExporter.moduleName = newModuleName;
    newExporter.fileMimeTypes.emplace_back(format.mimeType);
    newExporter.translatableSupportedFormatsText = format.translatableName;
    newExporter.translatableExportActionText = format.translatableName;
    newExporter.translatableExportFilesText = format.translatableName;
    newExporter.originalLegacyFileFilter = filter;
    addExporter(std::move(newExporter));
}

std::vector<std::string> Formats::getExportModules(const std::string& extension) const
{
    const auto exporters = this->exporters();
    std::vector<std::string> modules;
    for (const auto& exporter : exporters) {
        for (const auto& mime : exporter->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime);
                format && format->matchesLegacyFileExtension(extension)) {
                modules.push_back(exporter->moduleName);
            }
        }
    }
    return modules;
}

std::vector<std::string> Formats::getExportModules() const
{
    const auto exporters = this->exporters();
    std::vector<std::string> modules;
    modules.reserve(exporters.size());
    for (const auto& exporter : exporters) {
        modules.push_back(exporter->moduleName);
    }
    std::ranges::sort(modules);
    const auto [first, last] = std::ranges::unique(modules);
    modules.erase(first, last);
    return modules;
}

std::vector<std::string> Formats::getExportTypes(const std::string& module) const
{
    std::vector<std::string> types;
    const auto exporters = exportersByModule(module);
    for (const auto& exporter : exporters) {
        for (const auto& mime : exporter->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime); format) {
                const auto legacy = format->getLegacyFileExtensions();
                types.insert(types.end(), legacy.begin(), legacy.end());
            }
        }
    }
    return types;
}

std::vector<std::string> Formats::getExportTypes() const
{
    std::vector<std::string> types;
    for (const auto& exporter : exporters()) {
        for (const auto& mime : exporter->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime); format) {
                const auto legacy = format->getLegacyFileExtensions();
                types.insert(types.end(), legacy.begin(), legacy.end());
            }
        }
    }

    std::ranges::sort(types);
    const auto [first, last] = std::ranges::unique(types);
    types.erase(first, last);

    return types;
}

std::map<std::string, std::string> Formats::getExportFilters(const std::string& extension) const
{
    std::map<std::string, std::string> moduleFilter;
    for (const auto& exporter : exporters()) {
        bool matchesType = false;
        for (const auto& mime : exporter->fileMimeTypes) {
            if (const auto format = formatByMimeType(mime);
                format && format->matchesLegacyFileExtension(extension)) {
                matchesType = true;
                break;
            }
        }
        if (matchesType) {
            moduleFilter[exporter->getLegacyFileFilter(*this)] = exporter->moduleName;
        }
    }
    return moduleFilter;
}

std::map<std::string, std::string> Formats::getExportFilters() const
{
    std::map<std::string, std::string> filter;
    for (const auto& exporter : exporters()) {
        filter[exporter->getLegacyFileFilter(*this)] = exporter->moduleName;
    }
    return filter;
}

}  // namespace App

#endif  // FC_NO_LEGACY_FORMAT_HANDLING
