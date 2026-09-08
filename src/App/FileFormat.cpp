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

#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/Translation.h>

#include "Application.h"
#include "FileFormat.h"


FC_LOG_LEVEL_INIT("FileFormat", true)

using namespace std::string_view_literals;

namespace App
{

// Exists as macro for use within QT_TRANSLATE_NOOP
// NOLINTNEXTLINE
#define TRANSLATE_CONTEXT "FileFormat"
static constexpr auto TranslateContext = "FileFormat"sv;

/// FreeCAD branding string to substitute with branding in config.
/// @private
static const std::regex BrandRegex("FreeCAD");

std::string FileFormat::displayName() const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableName),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

std::string FileAdapter::supportedFormatsText() const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableSupportedFormatsText),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

bool FileAdapter::handlesMimeType(std::string_view mimeType) const
{
    return std::ranges::find(fileMimeTypes, mimeType) != fileMimeTypes.cend();
}

FileNamePatternList FileAdapter::getFileNamePatterns(const Formats& formats) const
{
    std::set<FileNamePattern> patterns;
    for (const auto& mimeType : fileMimeTypes) {
        const auto format = formats.formatByMimeType(mimeType);
        patterns.insert(format->fileNamePatterns.cbegin(), format->fileNamePatterns.cend());
    }
    return {patterns.cbegin(), patterns.cend()};
}

std::string FileImporter::importActionText() const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableImportActionText),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

std::string FileImporter::importFilesText(int n) const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableImportFilesText, {}, n),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

std::string FileExporter::exportActionText() const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableExportActionText),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

std::string FileExporter::exportFilesText(int n) const
{
    return std::regex_replace(
        Base::Translation::translate(TranslateContext, translatableExportFilesText, {}, n),
        BrandRegex,
        Application::Config()["ExeName"]
    );
}

/// @private
static void addDefaultFormats(Formats& r)
{
    // FreeCAD formats
    r.addFormat(
        {QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "FreeCAD Document"),
         MimeTypes::FreecadDocument,
         {"*.FCStd"},
         // Stil subject to change. See #17727
         {"application/x-extension-fcstd",
          "application/freecad",
          "application/x-freecad",
          "application/vnd.freecad"}}
    );

    // Text document formats
    r.addFormat({QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "Text Document"), MimeTypes::Txt, {"*.txt"}});
    r.addFormat(
        {QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "PDF Document"),
         MimeTypes::Pdf,
         {"*.pdf"},
         {"application/x-pdf", "image/pdf", "application/acrobat", "application/nappdf"}}
    );
    r.addFormat(
        {QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "RTF Document"), MimeTypes::Rtf, {"*.rtf"}, {"text/rtf"}}
    );

    // Image formats
    r.addFormat({QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "PNG Image"), MimeTypes::Png, {"*.png"}});
    r.addFormat({QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "GIF Image"), MimeTypes::Gif, {"*.gif"}});
    r.addFormat(
        {QT_TRANSLATE_NOOP(TRANSLATE_CONTEXT, "JPEG Image"),
         MimeTypes::Jpeg,
         {"*.jpg", "*.jpeg", "*.jpe", "*.jfif"}}
    );
}

Formats::Formats(bool populateDefaultFormats)
{
    if (populateDefaultFormats) {
        addDefaultFormats(*this);
    }
}

// NOLINTBEGIN(*-magic-numbers)
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
            // "(c & 0x80) != 0" checks for bit 7 being set, allowing for non-ASCII UTF-8
            // codepoints to be expressed in patterns.
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || (c & 0x80) != 0) {
                regex.push_back(c);
            }
            else if (c == '.') {
                regex.append("\\.");
            }
            else if (c == '*') {
                regex.append(".*");
            }
            else {
                throw Base::ParserError(
                    fmt::format(
                        "Character '{}' at position {} of file name pattern #{} \"{}\" is invalid",
                        c,
                        it - patternIt->cbegin(),
                        patternIt - patterns.cbegin(),
                        *patternIt
                    )
                );
            }
        }
    }
    return std::regex {regex, std::regex::ECMAScript | std::regex::icase};
}
// NOLINTEND(*-magic-numbers)

void Formats::mergeFormat(FormatInfoIndex existingFormatIndex, FileFormat& newFormat)
{
    auto& existingFormatInfo = _formatInfos[existingFormatIndex];
    auto& existingFormat = existingFormatInfo->format;
    FC_TRACE(fmt::format("Merging format {}", existingFormat.mimeType));
    for (auto& newMimeType : newFormat.secondaryMimeTypes) {
        if (std::ranges::find(existingFormat.secondaryMimeTypes, newMimeType)
            == existingFormat.secondaryMimeTypes.end()) {
            _mimeToInfoIndex.emplace(newMimeType, existingFormatIndex);
            existingFormat.secondaryMimeTypes.push_back(std::move(newMimeType));
        }
    }
    for (auto& newPattern : newFormat.fileNamePatterns) {
        if (std::ranges::find(existingFormat.fileNamePatterns, newPattern)
            == existingFormat.fileNamePatterns.end()) {
            existingFormat.fileNamePatterns.push_back(std::move(newPattern));
        }
    }
    existingFormatInfo->fileNameRegex = patternsToRegex(existingFormat.fileNamePatterns);
}

/**
 * This function pulls double duty by checking any new format being registered does not clash
 * with the existing known MIME types, and returning the info index of the format to merge with
 * if applicable.
 */
std::optional<Formats::FormatInfoIndex> Formats::checkMergeNoMimeClash(const FileFormat& newFormat)
{
    std::optional<Formats::FormatInfoIndex> mergeWithIndex(std::nullopt);
    const FileFormat* mergeWith = nullptr;
    if (const auto formatInfoIndexIt = _mimeToInfoIndex.find(newFormat.mimeType);
        formatInfoIndexIt != _mimeToInfoIndex.end()) {
        auto& existingFormat = _formatInfos[formatInfoIndexIt->second]->format;
        if (newFormat.mimeType != existingFormat.mimeType) {
            throw std::invalid_argument(
                fmt::format(
                    "Cannot register new format whose main MIME type {} is a secondary type of {}",
                    newFormat.mimeType,
                    existingFormat.mimeType
                )
            );
        }
        mergeWithIndex = formatInfoIndexIt->second;
        mergeWith = &existingFormat;
    }
    for (const auto& secondaryMime : newFormat.secondaryMimeTypes) {
        if (const auto* existingFormat = formatByMimeType(secondaryMime);
            existingFormat != nullptr && existingFormat != mergeWith) {
            throw std::invalid_argument(
                fmt::format(
                    "Cannot register new format whose secondary MIME type {} is {} type of {}",
                    secondaryMime,
                    secondaryMime == existingFormat->mimeType ? "the primary" : "a secondary",
                    existingFormat->mimeType
                )
            );
        }
    }
    return mergeWithIndex;
}

bool Formats::addFormat(FileFormat format)
{
    if (const auto mergeIndex = checkMergeNoMimeClash(format)) {
        mergeFormat(mergeIndex.value(), format);
        return false;
    }
    if (_formatInfos.size() >= std::numeric_limits<FormatInfoIndex>::max()) {
        throw std::overflow_error("Too many formats registered");
    }
    FC_TRACE(fmt::format("Adding format {} \"{}\"", format.mimeType, format.translatableName));
    // Can't do this inside the braced init as `format` will already have moved by then
    auto fileNameRegex = patternsToRegex(format.fileNamePatterns);
    _formatInfos.emplace_back(new FormatInfo {
        .format = std::move(format),
        .fileNameRegex = std::move(fileNameRegex),
    });
    // Do this *after* inserting the format into _formatInfos just to keep
    // consistency should an std::bad_alloc happen.
    const auto& movedFormat = _formatInfos.back()->format;
    const auto infoIndex = static_cast<FormatInfoIndex>(_formatInfos.size() - 1);
    _mimeToInfoIndex.emplace(movedFormat.mimeType, infoIndex);
    for (const auto& secondaryMimeType : movedFormat.secondaryMimeTypes) {
        _mimeToInfoIndex.emplace(secondaryMimeType, infoIndex);
    }
    return true;
}

std::vector<gsl::not_null<const FileFormat*>> Formats::formats() const
{
    std::vector<gsl::not_null<const FileFormat*>> formats;
    formats.reserve(_formatInfos.size());
    for (const auto& formatInfo : _formatInfos) {
        formats.emplace_back(&formatInfo->format);
    }
    return formats;
}

const FileFormat* Formats::formatByMimeType(const MimeType& mimeType) const
{
    if (const auto formatInfoIndexIt = _mimeToInfoIndex.find(mimeType);
        formatInfoIndexIt != _mimeToInfoIndex.cend()) {
        return &_formatInfos[formatInfoIndexIt->second]->format;
    }
    return nullptr;
}

std::vector<gsl::not_null<const FileFormat*>> Formats::formatsForFileName(std::string_view name) const
{
    std::vector<gsl::not_null<const FileFormat*>> formats;
    for (const auto& formatInfo : _formatInfos) {
        if (std::regex_match(name.cbegin(), name.cend(), formatInfo->fileNameRegex)) {
            formats.emplace_back(&formatInfo->format);
        }
    }
    return formats;
}

void Formats::addImporter(FileImporter importer)
{
    for (const auto& mimeType : importer.fileMimeTypes) {
        const auto* format = formatByMimeType(mimeType);
        if (format == nullptr) {
            throw std::invalid_argument(
                fmt::format(
                    "Can't register importer for unknown format with MIME type {}, "
                    "did you forget to addFormat() first?",
                    mimeType
                )
            );
        }
        if (format->mimeType != mimeType) {
            throw std::invalid_argument(
                fmt::format(
                    "Importer supports {} which is not the primary MIME "
                    "type for that format; expected {}",
                    mimeType,
                    format->mimeType
                )
            );
        }
    }
    FC_TRACE(
        fmt::format(
            "Adding importer with module {} for [{}]",
            importer.moduleName,
            fmt::join(importer.fileMimeTypes, ", ")
        )
    );
    _importers.emplace_back(new FileImporter(std::move(importer)));
}

std::vector<gsl::not_null<const FileImporter*>> Formats::importers() const
{
    std::vector<gsl::not_null<const FileImporter*>> importers;
    importers.reserve(_importers.size());
    for (const auto& importer : _importers) {
        importers.emplace_back(importer.get());
    }
    return importers;
}

std::vector<gsl::not_null<const FileImporter*>> Formats::importersForMimeType(
    const MimeType& mimeType
) const
{
    std::vector<gsl::not_null<const FileImporter*>> importers;
    for (const auto& importer : _importers) {
        if (importer->handlesMimeType(mimeType)) {
            importers.emplace_back(importer.get());
        }
    }
    return importers;
}

std::vector<gsl::not_null<const FileImporter*>> Formats::importersForFileName(std::string_view name) const
{
    std::vector<gsl::not_null<const FileImporter*>> importers;
    for (const auto& importer : _importers) {
        for (const auto& mimeType : importer->fileMimeTypes) {
            const auto infoIndex = _mimeToInfoIndex.at(mimeType);
            const auto& formatInfo = _formatInfos[infoIndex];
            if (std::regex_match(name.cbegin(), name.cend(), formatInfo->fileNameRegex)) {
                importers.emplace_back(importer.get());
            }
        }
    }
    return importers;
}

std::vector<gsl::not_null<const FileFormat*>> Formats::supportedImportFormats() const
{
    std::set<gsl::not_null<const FileFormat*>> formats;
    for (const auto& importer : _importers) {
        for (const auto& mimeType : importer->fileMimeTypes) {
            // Existence of format is guaranteed by checks in addImporter()
            const auto infoIndex = _mimeToInfoIndex.at(mimeType);
            formats.emplace(&_formatInfos[infoIndex]->format);
        }
    }
    return {formats.cbegin(), formats.cend()};
}

std::vector<gsl::not_null<const FileImporter*>> Formats::importersByModule(std::string_view module) const
{
    std::vector<gsl::not_null<const FileImporter*>> importers;
    for (const auto& importer : _importers) {
        if (importer->moduleName == module) {
            importers.emplace_back(importer.get());
        }
    }
    return importers;
}

void Formats::addExporter(FileExporter exporter)
{
    for (const auto& mimeType : exporter.fileMimeTypes) {
        const auto* format = formatByMimeType(mimeType);
        if (format == nullptr) {
            throw std::invalid_argument(
                fmt::format(
                    "Can't register exporter for unknown format with MIME type {}, "
                    "did you forget to addFormat() first?",
                    mimeType
                )
            );
        }
        if (format->mimeType != mimeType) {
            throw std::invalid_argument(
                fmt::format(
                    "Exporter supports {} which is not the primary MIME "
                    "type for that format; expected {}",
                    mimeType,
                    format->mimeType
                )
            );
        }
    }
    FC_TRACE(
        fmt::format(
            "Adding exporter with module {} for [{}]",
            exporter.moduleName,
            fmt::join(exporter.fileMimeTypes, ", ")
        )
    );
    _exporters.emplace_back(new FileExporter(std::move(exporter)));
}

std::vector<gsl::not_null<const FileExporter*>> Formats::exporters() const
{
    std::vector<gsl::not_null<const FileExporter*>> exporters;
    exporters.reserve(_exporters.size());
    for (const auto& exporter : _exporters) {
        exporters.emplace_back(exporter.get());
    }
    return exporters;
}

std::vector<gsl::not_null<const FileExporter*>> Formats::exportersForMimeType(
    const MimeType& mimeType
) const
{
    std::vector<gsl::not_null<const FileExporter*>> exporters;
    for (const auto& exporter : _exporters) {
        if (exporter->handlesMimeType(mimeType)) {
            exporters.emplace_back(exporter.get());
        }
    }
    return exporters;
}

std::vector<gsl::not_null<const FileExporter*>> Formats::exportersForFileName(std::string_view name) const
{
    std::vector<gsl::not_null<const FileExporter*>> exporters;
    for (const auto& exporter : _exporters) {
        for (const auto& mimeType : exporter->fileMimeTypes) {
            const auto infoIndex = _mimeToInfoIndex.at(mimeType);
            const auto& formatInfo = _formatInfos[infoIndex];
            if (std::regex_match(name.cbegin(), name.cend(), formatInfo->fileNameRegex)) {
                exporters.emplace_back(exporter.get());
            }
        }
    }
    return exporters;
}

std::vector<gsl::not_null<const FileFormat*>> Formats::supportedExportFormats() const
{
    std::set<gsl::not_null<const FileFormat*>> formats;
    for (const auto& exporter : _exporters) {
        for (const auto& mimeType : exporter->fileMimeTypes) {
            // Existence of format is guaranteed by checks in addImporter()
            const auto infoIndex = _mimeToInfoIndex.at(mimeType);
            formats.emplace(&_formatInfos[infoIndex]->format);
        }
    }
    return {formats.cbegin(), formats.cend()};
}

std::vector<gsl::not_null<const FileExporter*>> Formats::exportersByModule(std::string_view module) const
{
    std::vector<gsl::not_null<const FileExporter*>> exporters;
    for (const auto& exporter : _exporters) {
        if (exporter->moduleName == module) {
            exporters.emplace_back(exporter.get());
        }
    }
    return exporters;
}

}  // namespace App
