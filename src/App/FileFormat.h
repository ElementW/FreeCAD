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

#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <gsl/pointers>

#include <FCGlobal.h>


namespace App
{

class Application;
class Formats;

using MimeType = std::string;
using MimeTypeList = std::vector<MimeType>;

using FileNamePattern = std::string;
using FileNamePatternList = std::vector<FileNamePattern>;

namespace MimeTypes
{
// The following MIME types are kept as string literals and not std::string_views
// as the latter doesn't implicitly convert to std::string, which is inconvenient.

// FreeCAD formats
// Subject to change. See #17727
constexpr const auto FreecadDocument = "application/vnd.freecad.std";

// Text document formats
constexpr const auto Txt = "text/plain";
constexpr const auto Pdf = "application/pdf";
constexpr const auto Rtf = "application/rtf";

// Image formats
constexpr const auto Png = "image/png";
constexpr const auto Gif = "image/gif";
constexpr const auto Jpeg = "image/jpeg";
};  // namespace MimeTypes

// Disable clang-tidy warning about exposed data members: these classes *are* mere data classes
// and the presence of private members is only supporting code for legacy API compatibility.
// The silencing of those warnings can be removed at the same time this support code will be.
// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)

class AppExport FileFormat final
{
public:
    FileFormat() = default;
    FileFormat(
        std::string translatableName,
        MimeType mimeType,
        FileNamePatternList fileNamePatterns,
        MimeTypeList secondaryMimeTypes = {}
    )
        : translatableName(std::move(translatableName))
        , mimeType(std::move(mimeType))
        , secondaryMimeTypes(std::move(secondaryMimeTypes))
        , fileNamePatterns(std::move(fileNamePatterns))
    {}

    /**
     * @brief Translated name for the format.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string displayName() const;

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Formats;
    static FileFormat fromLegacyFilter(std::string_view filter);
    std::vector<std::string> getLegacyFileExtensions() const;
    bool matchesLegacyFileExtension(std::string_view extension) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

public:
    /** Translatable (with context "FileFormat") name for the format. */
    std::string translatableName;
    /** IANA-registered (or most popular failing that) MIME type. */
    MimeType mimeType;
    /** Additional/alias MIME types. */
    MimeTypeList secondaryMimeTypes;
    /**
     * `glob()`-style file name patterns to aid in file type detection, including in
     * open/save dialog boxes. Note those patterns match more than just file extensions and should
     * *never* be reduced to extensions only.
     */
    FileNamePatternList fileNamePatterns;

    // Can be extended with e.g. format detection std::function<>, icon path, etc
};

struct AppExport FileAdapter
{
    constexpr FileAdapter() = default;
    FileAdapter(
        std::string moduleName,
        MimeTypeList fileMimeTypes,
        std::string translatableSupportedFormatsText
    )
        : moduleName(std::move(moduleName))
        , fileMimeTypes(std::move(fileMimeTypes))
        , translatableSupportedFormatsText(std::move(translatableSupportedFormatsText))
    {}

    /**
     * @brief Translated text for the supported formats collectively.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string supportedFormatsText() const;
    [[nodiscard]] bool handlesMimeType(std::string_view mimeType) const;

    [[nodiscard]] FileNamePatternList getFileNamePatterns(const Formats& formats) const;

    /** Python module name to call to run this importer/exporter. */
    std::string moduleName;
    /**
     * File type MIME identifiers this importer/exporter supports.
     * File name patterns are derived automatically from this and the list of registered file types.
     */
    MimeTypeList fileMimeTypes;
    /**
     * Translatable (with context "FileFormat") text for the supported formats collectively.
     * Used in open/save dialog filters.
     * @example "Vector images"
     */
    std::string translatableSupportedFormatsText;

    // TODO: Options here, like those of the STEP importer
    // Could be embedded straight into the file modals

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Formats;
    std::string getLegacyFileFilter(const Formats& formats) const;
    std::string originalLegacyFileFilter;
    bool createdFromLegacy = false;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING
};

struct AppExport FileImporter final: public FileAdapter
{
    constexpr FileImporter() = default;
    FileImporter(
        std::string moduleName,
        MimeTypeList fileMimeTypes,
        std::string translatableSupportedFormatsText,
        std::string translatableImportActionText,
        std::string translatableImportFilesText
    )
        : FileAdapter(
              std::move(moduleName),
              std::move(fileMimeTypes),
              std::move(translatableSupportedFormatsText)
          )
        , translatableImportActionText(std::move(translatableImportActionText))
        , translatableImportFilesText(std::move(translatableImportFilesText))
    {}

    /**
     * @brief Translated text for the import action itself.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string importActionText() const;
    /**
     * @brief Translated text for importing @p n files.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string importFilesText(int n) const;

    /**
     * Translatable (with context "FileFormat") text for the general import action itself.
     * In languages that use them, sentences that use indefinite articles.
     * @example "Import FEM simulation results" (en),
     *          "Importer des résultats de simulation FEM" (fr)
     */
    std::string translatableImportActionText;
    /**
     * Translatable (with context "FileFormat") text for importing N files.
     * In languages that use them, sentences that use definite articles.
     * Translations must support plural forms if applicable; "%1" is replaced by number of files.
     * @example "Import %1 FEM simulation result file(s)" (en)
     *          "Importer %1 fichiers de résultat de simulation FEM" (fr)
     */
    std::string translatableImportFilesText;
};

struct AppExport FileExporter final: public FileAdapter
{
    constexpr FileExporter() = default;
    FileExporter(
        std::string moduleName,
        MimeTypeList fileMimeTypes,
        std::string translatableSupportedFormatsText,
        std::string translatableExportActionText,
        std::string translatableExportFilesText
    )
        : FileAdapter(
              std::move(moduleName),
              std::move(fileMimeTypes),
              std::move(translatableSupportedFormatsText)
          )
        , translatableExportActionText(std::move(translatableExportActionText))
        , translatableExportFilesText(std::move(translatableExportFilesText))
    {}

    /**
     * @brief Translated text for the export action itself.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string exportActionText() const;
    /**
     * @brief Translated text for exporting @p n files.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string exportFilesText(int n) const;

    /**
     * Translatable (with context "FileFormat") text for the general export action itself.
     * In languages that use them, sentences that use indefinite articles.
     * @example "Export body as mesh"
     */
    std::string translatableExportActionText;
    /**
     * Translatable (with context "FileFormat") text for exporting N files.
     * In languages that use them, sentences that use definite articles.
     * Translations must support plural forms if applicable; "%1" is replaced by number of files.
     * @example "Export %1 body/ies as meshe(s)"
     */
    std::string translatableExportFilesText;
};

// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

class AppExport Formats final
{
public:
    Formats()
        : Formats(true)
    {}
    explicit Formats(bool populateDefaultFormats);

    /// @name Formats
    /// @{

    /**
     * @brief Register a file format.
     * If the format is already known, the properties passed will be merged with the existing entry.
     * @returns `true` if the format was new and added, `false` if it was already known.
     * @throws Base::ParserError if a file name pattern of the format is invalid.
     */
    bool addFormat(FileFormat format);
    /**
     * @brief Get all known formats.
     * Lifetime: elements remain valid until destruction of the parents `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> formats() const;
    /**
     * @brief Find the format with the given MIME type.
     * Lifetime: elements remain valid until destruction of the parents `Formats`.
     * @return Pointer to format, or `nullptr` if no such format is known.
     */
    [[nodiscard]] const FileFormat* formatByMimeType(const MimeType& mimeType) const;
    /**
     * @brief Get formats matching the given file name.
     * Lifetime: elements remain valid until destruction of the parents `Formats`.
     * @param name Full name (not path) of the file to figure out potential formats of.
     *        *Never* pass a truncated file name or extension only; as this will fail to match
     * formats.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> formatsForFileName(
        std::string_view name
    ) const;

    /// @}

    /// @name Importers
    /// @{

    /**
     * @brief Register an importer, associating one or more file formats to a Python module name.
     * @throws std::invalid_argument if any of the importer's handled MIME types matches no known
     *         FileFormat.
     */
    void addImporter(FileImporter importer);
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importers() const;
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForMimeType(
        const MimeType& mimeType
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForFormat(
        const FileFormat& format
    ) const
    {
        return importersForMimeType(format.mimeType);
    }
    /**
     * @brief Get importers able to import a file with the given file name.
     * Lifetime: elements remain valid until destruction of the parents `Formats`.
     * @param name Full name (not path) of the file to be imported.
     *        *Never* pass a truncated file name or extension only; as this will fail to match
     *        importers.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForFileName(
        std::string_view name
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> supportedImportFormats() const;
    /**
     * @brief Get the importers with a given Python module name.
     * Lifetime: invalidated when addFormat(), addImporter(), or addExporter() is called.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersByModule(
        std::string_view module
    ) const;

    /// @}

    /// @name Exporters
    /// @{

    /**
     * @brief Register an exporter, associating one or more file formats to a Python module name.
     * @throws std::invalid_argument if any of the importer's handled MIME types matches no known
     *         FileFormat.
     */
    void addExporter(FileExporter exporter);
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exporters() const;
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForMimeType(
        const MimeType& mimeType
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForFormat(
        const FileFormat& format
    ) const
    {
        return exportersForMimeType(format.mimeType);
    }
    /**
     * @brief Get exporters able to export to a file with the given file name.
     * Lifetime: elements remain valid until destruction of the parents `Formats`.
     * @param name Full name (not path) of the file to be exported to.
     *        *Never* pass a truncated file name or extension only; as this will fail to match
     *        exporters.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForFileName(
        std::string_view name
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> supportedExportFormats() const;
    /**
     * @brief Get the exporters with a given Python module name.
     * Lifetime: invalidated when addFormat(), addImporter(), or addExporter() is called.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersByModule(
        std::string_view module
    ) const;

    /// @}

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Application;
    void addImportType(const char* filter, const char* moduleName);
    void changeImportModule(const char* filter, const char* oldModuleName, const char* newModuleName);
    std::vector<std::string> getImportModules(const std::string& extension) const;
    std::vector<std::string> getImportModules() const;
    std::vector<std::string> getImportTypes(const std::string& module) const;
    std::vector<std::string> getImportTypes() const;
    std::map<std::string, std::string> getImportFilters(const std::string& extension) const;
    std::map<std::string, std::string> getImportFilters() const;
    void addExportType(const char* filter, const char* moduleName);
    void addTranslatableExportType(
        const std::string& description,
        const std::vector<std::string>& extensions,
        const std::string& moduleName
    );
    void changeExportModule(const char* filter, const char* oldModuleName, const char* newModuleName);
    std::vector<std::string> getExportModules(const std::string& extension) const;
    std::vector<std::string> getExportModules() const;
    std::vector<std::string> getExportTypes(const std::string& module) const;
    std::vector<std::string> getExportTypes() const;
    std::map<std::string, std::string> getExportFilters(const std::string& extension) const;
    std::map<std::string, std::string> getExportFilters() const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

private:
    using FormatInfoIndex = uint16_t;
    struct FormatInfo
    {
        FileFormat format;
        std::regex fileNameRegex;
    };
    std::vector<std::unique_ptr<FormatInfo>> _formatInfos;
    std::map<MimeType, FormatInfoIndex> _mimeToInfoIndex;
    std::vector<std::unique_ptr<FileImporter>> _importers;
    std::vector<std::unique_ptr<FileExporter>> _exporters;

    void mergeFormat(FormatInfoIndex, FileFormat&);
    std::optional<FormatInfoIndex> checkMergeNoMimeClash(const FileFormat&);
};

}  // namespace App
