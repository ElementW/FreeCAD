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

// NOLINTBEGIN(readability-redundant-member-init) Silence missing-designated-field-initializers

struct AppExport FileFormat final
{
    /**
     * @brief Translated name for the format.
     * FreeCAD branding is automatically replaced. Only use for display purposes.
     */
    [[nodiscard]] std::string displayName() const;

    /** Translatable (with context "FileFormat") name for the format. */
    std::string translatableName;
    /** IANA-registered (or most popular failing that) MIME type. */
    MimeType mimeType;
    /** Additional/alias MIME types. */
    MimeTypeList secondaryMimeTypes = {};
    /**
     * Case-insensitive `glob()`-style file name patterns to aid in file type detection, including
     * in open/save dialog boxes. Note those patterns match more than just file extensions and
     * should *never* be reduced to extensions only.
     * @example {"*.abc", "result_*.txt"}
     */
    FileNamePatternList fileNamePatterns;

    // TODO: Can be extended with e.g.
    // * format detection std::function<> (limited to 4k head and 4k tail data for performance)
    //   to finally enable imports to not be extension-sensitive, as is expected of Linux software
    // * QIcon holder (derived from QFileIconProvider or FreeCAD overrides)
    // * etc

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Formats;
    static FileFormat fromLegacyFilter(std::string_view filter);
    std::vector<std::string> getLegacyFileExtensions() const;
    bool matchesLegacyFileExtension(std::string_view extension) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING
};

struct AppExport FileAdapter final
{
    [[nodiscard]] FileNamePatternList getFileNamePatterns(const Formats& formats) const;

    /**
     * @brief Translated text for the supported formats collectively.
     * FreeCAD branding is automatically replaced.
     * Only use for display purposes, do not store or use as map keys.
     */
    [[nodiscard]] std::string supportedFormatsText() const;
    /**
     * @brief Translated text for the import/export action itself.
     * FreeCAD branding is automatically replaced.
     * Only use for display purposes, do not store or use as map keys.
     */
    [[nodiscard]] std::string actionText() const;
    /**
     * @brief Translated text for importing @p n files.
     * FreeCAD branding is automatically replaced.
     * Only use for display purposes, do not store or use as map keys.
     */
    [[nodiscard]] std::string filesText(int n) const;

    /**
     * Arbitrary key that *can* (but is not guaranteed to) be used alphabetically on when
     * displaying a list of file importers/exporters to the user, e.g. in file dialogs, to
     * provide a more consistent and permanent order including across languages.
     * When unspecified, the context-sensitive translated name will be used.
     * Specify this **if and only if** there is one overarching file extension that's handled by
     * this importer/exporter that's consistent across all languages.
     * @example "xlsx" for "Excel 2007 Spreadsheet" (en) & "Tableur Excel 2007" (fr),
     *          "step+color" for "STEP with colors" (en) & "STEP avec couleurs" (fr),
     *          but unset for "Supported formats" (en) & "Formats supportés" (fr) as
     *          "supported" is only logical in English.
     */
    std::string sortKey = {};
    /** Python module name to call to run this importer/exporter. */
    std::string moduleName;
    /**
     * File MIME types this importer/exporter supports.
     * File name patterns are derived automatically from this and the list of registered file types.
     */
    MimeTypeList fileMimeTypes;

    /**
     * Translatable (with context "FileFormat") text for the supported formats collectively.
     * Used in open/save dialog filters.
     * @example "Vector images" (en)
     */
    std::string translatableSupportedFormatsText;
    /**
     * Translatable (with context "FileFormat") text for the general import/export action itself,
     * to be used in contexts not concerning specific files, e.g. the toolbar editor.
     * In languages that use them, sentences that use indefinite articles.
     * @example "Import FEM simulation results" (en),
     *          "Importer des résultats de simulation FEM" (fr),
     *          "Export bodies as meshes" (en)
     */
    std::string translatableActionText;
    /**
     * Translatable (with context "FileFormat") text for importing/exporting N files,
     * to be used in contexts where files are involved, e.g. file dialogs or the drag & drop
     * import type disambiguation dialog.
     * In languages that use them, sentences that use definite articles.
     * Translations must support plural forms if applicable; "%1" is replaced by number of files.
     * @example "Import %1 FEM simulation result file(s)" (en),
     *          "Importer %1 fichiers de résultat de simulation FEM" (fr),
     *          "Export %1 body/bodies as mesh(es)" (en)
     */
    std::string translatableFilesText;

    // TODO: Options here, like those of the STEP importer
    // Could be embedded straight into the file modals on
    // Win32 (IFileDialogCustomize) & non-native dialogs

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
public:
    // Making this private would make FileAdapter a non-aggregate,
    // therefore unable to be constructed with designated initializers.
    std::string originalLegacyFileFilter = {};

private:
    friend Formats;
    std::string getLegacyFileFilter(const Formats& formats) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING
};

using FileImporter = FileAdapter;
using FileExporter = FileAdapter;

// NOLINTEND(readability-redundant-member-init)

class AppExport Formats final
{
public:
    Formats()
        : Formats(true)
    {}
    explicit Formats(bool populateDefaultFormats);
    ~Formats();
    FC_DISABLE_COPY_MOVE(Formats);

    /// @name Formats
    /// @{

    /**
     * @brief Register a file format.
     * If the format is already known, the properties passed will be merged with the existing entry.
     * @returns `true` if the format was new and added, `false` if it was already known.
     * @throws Base::ParserError if a file name pattern of the format is invalid.
     * @throws Base::IndexError if too many formats are registered.
     * @throws Base::ValueError if the main MIME type of the format is a secondary
     *         type of another known format.
     * @throws Base::ValueError if any secondary MIME type of the format is the primary
     *         or a secondary type of another known format.
     */
    bool addFormat(FileFormat format);
    /**
     * @brief Get all known formats.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> formats() const;
    /**
     * @brief Find the format with the given MIME type.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     * @return Pointer to format, or `nullptr` if no such format is known.
     */
    [[nodiscard]] const FileFormat* formatByMimeType(const MimeType& mimeType) const noexcept;
    /**
     * @brief Get formats matching the given file name.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     * @param name Full name (not path) of the file to figure out potential formats of.
     *        *Never* pass a truncated file name or extension only; format matching will fail.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> formatsForFileName(
        std::string_view name
    ) const;

    /// @}

    /// @name Importers
    /// @{

    /**
     * @brief Register an importer, associating one or more file formats to a Python module name.
     * @throws Base::ValueError if any of the handled MIME types matches no known FileFormat.
     */
    void addImporter(FileImporter importer);
    /**
     * @brief List all known importers.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importers() const;
    /**
     * @brief List all known importers able to handle a given MIME type.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForMimeType(
        const MimeType& mimeType
    ) const;
    /**
     * @brief List all known importers able to handle a given `FileFormat`.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForFormat(
        const FileFormat& format
    ) const
    {
        return importersForMimeType(format.mimeType);
    }
    /**
     * @brief Get importers able to import a file with the given file name.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     * @param name Full name (not path) of the file to be imported.
     *        *Never* pass a truncated file name or extension only; importer matching will fail.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersForFileName(
        std::string_view name
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> supportedImportFormats() const;
    /**
     * @brief Get the importers with a given Python module name.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileImporter*>> importersByModule(
        std::string_view module
    ) const;

    /// @}

    /// @name Exporters
    /// @{

    /**
     * @brief Register an exporter, associating one or more file formats to a Python module name.
     * @throws Base::ValueError if any of the handled MIME types matches no known FileFormat.
     */
    void addExporter(FileExporter exporter);
    /**
     * @brief List all known exporters.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     *
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exporters() const;
    /**
     * @brief List all known exporters able to handle a given MIME type.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForMimeType(
        const MimeType& mimeType
    ) const;
    /**
     * @brief List all known exporters able to handle a given `FileFormat`.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForFormat(
        const FileFormat& format
    ) const
    {
        return exportersForMimeType(format.mimeType);
    }
    /**
     * @brief Get exporters able to export to a file with the given file name.
     * Lifetime: elements remain valid until destruction of this `Formats`.
     * @param name Full name (not path) of the file to be exported to.
     *        *Never* pass a truncated file name or extension only; exporter matching will fail.
     */
    [[nodiscard]] std::vector<gsl::not_null<const FileExporter*>> exportersForFileName(
        std::string_view name
    ) const;
    [[nodiscard]] std::vector<gsl::not_null<const FileFormat*>> supportedExportFormats() const;
    /**
     * @brief Get the exporters with a given Python module name.
     * Lifetime: elements remain valid until destruction of this `Formats`.
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
    struct FormatInfo;
    std::vector<std::unique_ptr<FormatInfo>> _formatInfos;
    std::map<MimeType, FormatInfoIndex> _mimeToInfoIndex;
    std::vector<std::unique_ptr<FileImporter>> _importers;
    std::vector<std::unique_ptr<FileExporter>> _exporters;

    void mergeFormat(FormatInfoIndex, FileFormat&);
    std::optional<FormatInfoIndex> checkMergeNoMimeClash(const FileFormat&);
};

}  // namespace App
