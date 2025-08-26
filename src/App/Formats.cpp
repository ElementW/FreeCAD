/***************************************************************************
 *   Copyright (c) 2025 Céleste Wouters <foss@elementw.net>                *
 *                                                                         *
 *   This file is part of FreeCAD.                                         *
 *                                                                         *
 *   FreeCAD is free software: you can redistribute it and/or modify it    *
 *   under the terms of the GNU Lesser General Public License as           *
 *   published by the Free Software Foundation, either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful, but        *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with FreeCAD. If not, see                               *
 *   <https://www.gnu.org/licenses/>.                                      *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#include <set>
#ifndef FC_NO_LEGACY_FORMAT_HANDLING
# include <utility>
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <QCoreApplication>
#include <QTranslator>

#include <Base/Console.h>
#include <Base/Exception.h>

#include "Application.h"
#include "Formats.h"


FC_LOG_LEVEL_INIT("Formats", true)

namespace App
{

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
/// Create a Format from a legacy format filter string.
/// The created MIME type is completely synthetic.
Format Format::fromLegacyFilter(const std::string_view& filter)
{
    FileNamePatternList fileNamePatterns;
    const std::string_view f{filter};
    const auto fileNamePatternsStart = f.find_last_of('(');
    if (fileNamePatternsStart == std::string_view::npos) {
        throw Base::ValueError("No pattern list opening parens found");
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

std::vector<std::string> Format::getLegacyFileTypes() const
{
    std::vector<std::string> formats;
    for (const auto& fileNamePattern : fileNamePatterns) {
        if (const auto pos = fileNamePattern.find("*."); pos != FileNamePattern::npos) {
            formats.emplace_back(fileNamePattern.begin() + pos + 2, fileNamePattern.end());
        }
    }
    return formats;
}

std::string Format::getLegacyFileFilter() const
{
    return fmt::format("{} ({})", name(), fmt::join(fileNamePatterns, " "));
}

bool Format::matchesLegacyFileType(const std::string_view& fileType) const
{
    for (const auto& fileNamePattern : fileNamePatterns) {
        if (const auto pos = fileNamePattern.find("*.");
                pos != FileNamePattern::npos && std::string_view(fileNamePattern).substr(pos) == fileType) {
            return true;
        }
    }
    return false;
}
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

/// FreeCAD branding string to substitute with branding in config.
/// @private
static const QString FcName = QStringLiteral("FreeCAD");

std::string Format::name() const
{
    return QCoreApplication::translate("Format", translatableName.c_str())
        .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

std::string Translator::supportedFormatsText() const
{
    return QCoreApplication::translate("Format", translatableSupportedFormatsText.c_str())
            .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

bool Translator::handlesMimeType(const std::string_view& mimeType) const
{
    return std::find(fileMimeTypes.cbegin(), fileMimeTypes.cend(), mimeType) != fileMimeTypes.cend();
}

FileNamePatternList Translator::getFileNamePatterns(const Formats& formats) const
{
    std::set<FileNamePattern> patterns;
    for (const auto& mimeType : fileMimeTypes) {
        const auto format = formats.getFormatByMimeType(mimeType);
        patterns.insert(format->fileNamePatterns.cbegin(), format->fileNamePatterns.cend());
    }
    return {patterns.cbegin(), patterns.cend()};
}

std::string Importer::importActionText() const
{
    return QCoreApplication::translate("Format", translatableImportActionText.c_str())
            .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

std::string Importer::importFilesText(int n) const
{
    return QCoreApplication::translate("Format", translatableImportFilesText.c_str(), nullptr, n)
            .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

/// @private
static void writeLegacyFileFilterExtensions(
        const Translator& translator, const Formats& formats, std::string& filter)
{
    filter += " (";
    for (const auto& mime : translator.fileMimeTypes) {
        if (const auto format = formats.getFormatByMimeType(mime)) {
            for (const auto& fileNamePattern : format->fileNamePatterns) {
                filter += fileNamePattern;
                filter += ' ';
            }
        }
    }
    *(filter.end() - 1) = ')';
}

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
std::string Importer::getLegacyFileFilter(const Formats& formats) const
{
    std::string filter{supportedFormatsText()};
    writeLegacyFileFilterExtensions(*this, formats, filter);
    return filter;
}
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

std::string Exporter::exportActionText() const
{
    return QCoreApplication::translate("Format", translatableExportActionText.c_str())
            .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

std::string Exporter::exportFilesText(int n) const
{
    return QCoreApplication::translate("Format", translatableExportFilesText.c_str(), nullptr, n)
            .replace(FcName, QString::fromStdString(Application::Config()["ExeName"])).toStdString();
}

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
std::string Exporter::getLegacyFileFilter(const Formats& formats) const
{
    std::string filter{supportedFormatsText()};
    writeLegacyFileFilterExtensions(*this, formats, filter);
    return filter;
}
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

// Using a macro here can't really be avoided as the goal is reducing repetition of
// QT_TRANSLATE_NOOP, which expects macro expansion of its arguments to build the language files.
#define F(name, mime, globs) r.addFormat({QT_TRANSLATE_NOOP("Format", name), MimeTypes::mime, globs}) // NOLINT
/// @private
static void addDefaultFormats(Formats &r)
{
    // FreeCAD formats
    F("FreeCAD Document", FreecadDocument, {"*.FCStd"});

    // Text document formats
    F("Text Document", Txt, {"*.txt"});
    F("PDF Document", Pdf, {"*.pdf"});
    F("RTF Document", Rtf, {"*.rtf"});

    // Image formats
    F("PNG Image", Png, {"*.png"});
    F("GIF Image", Gif, {"*.gif"});
    F("JPEG Image", Jpeg, {"*.jpeg"});
}

Formats::Formats()
{
    addDefaultFormats(*this);
}

/// @private
static std::regex patternsToRegex(const FileNamePatternList& patterns)
{
    // Maximum expected length for the regex translation of a single file name pattern.
    // Based on a fictional worst-case pattern translating to ".*i1\.FCScript|".
    constexpr size_t regexBufferPerFileNamePattern = 16;
    std::string regex;
    regex.reserve(patterns.size() * regexBufferPerFileNamePattern);
    for (auto patternIt = patterns.cbegin(); patternIt != patterns.cend(); ++patternIt) {
        if (patternIt != patterns.cbegin()) {
            regex.push_back('|');
        }
        for (auto it = patternIt->cbegin(); it != patternIt->cend(); ++it) {
            const char c = *it;
            // "(c & 0x80) != 0" checks for bit 7 being set, allowing for UTF-8 codepoints to be
            // expressed in patterns.
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c & 0x80) != 0) {
                regex.push_back(c);
            } else if (c == '.') {
                regex.append("\\.");
            } else if (c == '*') {
                regex.append(".*");
            } else {
                throw Base::ParserError(
                    fmt::format(
                        "Character '{}' at position {} of file name pattern #{} \"{}\" is invalid",
                        c, it - patternIt->cbegin(), patternIt - patterns.cbegin(), *patternIt
                    )
                );
            }
        }
    }
    return std::regex{regex, std::regex::ECMAScript | std::regex::icase};
}

bool Formats::addFormat(const Format& format)
{
    if (_formatInfos.find(format.mimeType) != _formatInfos.cend()) {
        return false;
    }
    _formatInfos.emplace(format.mimeType, FormatInfo {
                             .format = format,
                             .fileNameRegex = patternsToRegex(format.fileNamePatterns),
                         });
    FC_TRACE(fmt::format("Added format {} \"{}\"", format.mimeType, format.translatableName));
    return true;
}

std::vector<gsl::not_null<const Format*>> Formats::getFormats() const
{
    std::vector<gsl::not_null<const Format*>> formats;
    formats.reserve(_formatInfos.size());
    for (const auto& formatInfo : _formatInfos) {
        formats.emplace_back(&formatInfo.second.format);
    }
    return formats;
}

const Format* Formats::getFormatByMimeType(const MimeType& mimeType) const
{
    if (const auto formatInfo = _formatInfos.find(mimeType); formatInfo != _formatInfos.cend()) {
        return &formatInfo->second.format;
    }
    return nullptr;
}

std::vector<gsl::not_null<const Format*>> Formats::getFormatsForFileName(const std::string_view& name) const
{
    std::vector<gsl::not_null<const Format*>> formats;
    for (const auto& formatInfo : _formatInfos) {
        if (std::regex_match(name.cbegin(), name.cend(), formatInfo.second.fileNameRegex)) {
            formats.emplace_back(&formatInfo.second.format);
        }
    }
    return formats;
}

void Formats::addImporter(const Importer& importer)
{
    // TODO check MIMEs are registered
    _importers.emplace_back(importer);
    FC_TRACE(fmt::format("Added importer with module {} for [{}]",
                         importer.moduleName, fmt::join(importer.fileMimeTypes, ", ")));
}

std::vector<gsl::not_null<const Importer*>> Formats::getImporters() const
{
    std::vector<gsl::not_null<const Importer*>> importers;
    importers.reserve(_importers.size());
    for (const auto& importer : _importers) {
        importers.emplace_back(&importer);
    }
    return importers;
}

std::vector<gsl::not_null<const Importer*>> Formats::getImportersForMimeType(const MimeType& mimeType) const
{
    std::vector<gsl::not_null<const Importer*>> importers;
    for (const auto& importer : _importers) {
        if (importer.handlesMimeType(mimeType)) {
            importers.emplace_back(&importer);
        }
    }
    return importers;
}

std::vector<gsl::not_null<const Importer*>> Formats::getImportersForFileName(const std::string_view& name) const
{
    std::vector<gsl::not_null<const Importer*>> importers;
    for (const auto& importer : _importers) {
        for (const auto& type : importer.fileMimeTypes) {
            const auto& formatInfo = _formatInfos.at(type);
            if (std::regex_match(name.cbegin(), name.cend(), formatInfo.fileNameRegex)) {
                importers.emplace_back(&importer);
            }
        }
    }
    return importers;
}

std::vector<gsl::not_null<const Format*>> Formats::getSupportedImportFormats() const
{
    std::set<gsl::not_null<const Format*>> formats;
    for (const auto& importer : _importers) {
        for (const auto& importerFormat : importer.fileMimeTypes) {
            // Existence of format is guaranteed by checks in addImporter()
            formats.emplace(&_formatInfos.at(importerFormat).format);
        }
    }
    return {formats.cbegin(), formats.cend()};
}

std::vector<gsl::not_null<const Importer*>> Formats::getImportersByModule(const std::string_view& module) const
{
    std::vector<gsl::not_null<const Importer*>> importers;
    for (const auto& importer : _importers) {
        if (importer.moduleName == module) {
            importers.emplace_back(&importer);
        }
    }
    return importers;
}

void Formats::addExporter(const Exporter& exporter)
{
    // TODO check MIMEs are registered
    _exporters.emplace_back(exporter);
    FC_TRACE(fmt::format("Added exporter with module {} for [{}]",
                         exporter.moduleName, fmt::join(exporter.fileMimeTypes, ", ")));
}

std::vector<gsl::not_null<const Exporter*>> Formats::getExporters() const
{
    std::vector<gsl::not_null<const Exporter*>> exporters;
    exporters.reserve(_exporters.size());
    for (const auto& exporter : _exporters) {
        exporters.emplace_back(&exporter);
    }
    return exporters;
}

std::vector<gsl::not_null<const Exporter*>> Formats::getExportersForMimeType(const MimeType& mimeType) const
{
    std::vector<gsl::not_null<const Exporter*>> exporters;
    for (const auto& exporter : _exporters) {
        if (exporter.handlesMimeType(mimeType)) {
            exporters.emplace_back(&exporter);
        }
    }
    return exporters;
}

std::vector<gsl::not_null<const Exporter*>> Formats::getExportersForFileName(const std::string_view& name) const
{
    std::vector<gsl::not_null<const Exporter*>> exporters;
    for (const auto& exporter : _exporters) {
        for (const auto& type : exporter.fileMimeTypes) {
            const auto& formatInfo = _formatInfos.at(type);
            if (std::regex_match(name.cbegin(), name.cend(), formatInfo.fileNameRegex)) {
                exporters.emplace_back(&exporter);
            }
        }
    }
    return exporters;
}

std::vector<gsl::not_null<const Format*>> Formats::getSupportedExportFormats() const
{
    std::set<gsl::not_null<const Format*>> formats;
    for (const auto& exporter : _exporters) {
        for (const auto& exporterFormat : exporter.fileMimeTypes) {
            // Existence of format is guaranteed by checks in addExporter()
            formats.emplace(&_formatInfos.at(exporterFormat).format);
        }
    }
    return {formats.cbegin(), formats.cend()};
}

std::vector<gsl::not_null<const Exporter*>> Formats::getExportersByModule(const std::string_view& module) const
{
    std::vector<gsl::not_null<const Exporter*>> exporters;
    for (const auto& exporter : _exporters) {
        if (exporter.moduleName == module) {
            exporters.emplace_back(&exporter);
        }
    }
    return exporters;
}

}  // namespace App
