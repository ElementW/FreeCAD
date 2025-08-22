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

#ifndef FORMATS_H
#define FORMATS_H

#include <map>
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

struct MimeTypes final
{
    // FreeCAD formats
    // Subject to change. See #17727
    static constexpr const char* FreecadDocument = "application/vnd.freecad.std";

    // Text document formats
    static constexpr const char* Txt = "text/plain";
    static constexpr const char* Pdf = "application/pdf";
    static constexpr const char* Rtf = "application/rtf";

    // Image formats
    static constexpr const char* Png = "image/png";
    static constexpr const char* Gif = "image/gif";
    static constexpr const char* Jpeg = "image/jpeg";
};

class AppExport Format final
{
public:
    Format() = default;
    Format(std::string translatableName, MimeType mimeType, FileNamePatternList fileNamePatterns, MimeTypeList secondaryMimeTypes = {}) :
        translatableName(std::move(translatableName)), mimeType(std::move(mimeType)),
        secondaryMimeTypes(std::move(secondaryMimeTypes)), fileNamePatterns(std::move(fileNamePatterns)) {}

    /// Translated name for the format.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string name() const;

#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Application;
    static Format fromLegacyFilter(const std::string_view& filter);
    std::vector<std::string> getLegacyFileTypes() const;
    std::string getLegacyFileFilter() const;
    bool matchesLegacyFileType(const std::string_view& fileType) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING

public:
    /// Translatable (with context "Formats") name for the format.
    std::string translatableName;
    /// IANA-registered (or most popular failing that) MIME type.
    MimeType mimeType;
    /// Additional/alias MIME types.
    MimeTypeList secondaryMimeTypes;
    /// `glob()`-style file name patterns to aid in file type detection, including in
    /// open/save dialog boxes. Note those patterns match more than just file extensions and should
    /// *never* be reduced to extensions only.
    FileNamePatternList fileNamePatterns;
};

struct AppExport Translator
{
    /// Translated text for the supported formats collectively.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string supportedFormatsText() const;
    bool handlesMimeType(const std::string_view& mimeType) const;

    FileNamePatternList getFileNamePatterns(const Formats& formats) const;

    /// Python module name to call to run this importer/exporter.
    std::string moduleName;
    /// File type MIME identifiers this importer/exporter supports.
    /// File name patterns are derived automatically from this and the list of registered file types.
    MimeTypeList fileMimeTypes;
    /// Translatable (with context "Formats") text for the supported formats collectively.
    /// Used in open/save dialog filters.
    /// @example "Vector images"
    std::string translatableSupportedFormatsText;

    // TODO options here, like those of the STEP importer
    // Could be embedded straight into the file modals
};

struct AppExport Importer final : public Translator
{
    /// Translated text for the import action itself.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string importActionText() const;
    /// Translated text for importing @p n files.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string importFilesText(int n) const;

    /// Translatable (with context "Formats") text for the import action itself, with singular subject.
    /// @example "Import vector file as geometry"
    std::string translatableImportActionText;
    /// Translatable (with context "Formats") text for importing N files.
    /// Number of files substitutes "%1".
    /// Translations must support plural forms (if applicable).
    /// @example "Import %1 vector file(s) as geometry"
    std::string translatableImportFilesText;

    /// Do not use in new code. Support for string-based filter strings will be removed.
    std::string getFileDialogFilter(const Formats& formats) const;
#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Application;
    std::string getLegacyFileFilter(const Formats& formats) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING
};

struct AppExport Exporter final : public Translator
{
    /// Translated text for the export action itself.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string exportActionText() const;
    /// Translated text for exporting @p n files.
    /// FreeCAD branding is automatically replaced.
    /// Only use for display purposes.
    std::string exportFilesText(int n) const;

    /// Translatable (with context "Formats") text for the export action itself, with singular subject.
    /// @example "Export body as mesh"
    std::string translatableExportActionText;
    /// Translatable (with context "Formats") text for exporting N files.
    /// Number of files substitutes "%1".
    /// Translations must support plural forms (if applicable).
    /// @example "Export %1 body/ies as meshe(s)"
    std::string translatableExportFilesText;

    /// Do not use in new code. Support for string-based filter strings will be removed.
    std::string getFileDialogFilter(const Formats& formats) const;
#ifndef FC_NO_LEGACY_FORMAT_HANDLING
private:
    friend Application;
    std::string getLegacyFileFilter(const Formats& formats) const;
#endif  // FC_NO_LEGACY_FORMAT_HANDLING
};

class AppExport Formats final
{
public:
    Formats();

    /// @name Formats
    /// @{

    /// Register a file format.
    /// @returns `true` if the format was added, `false` if it was already known.
    /// @throws Base::ParserError if a file name pattern of the format is invalid.
    bool addFormat(const Format& format);
    /// Get all known formats.
    std::vector<gsl::not_null<const Format*>> getFormats() const;
    /// Find the format with the given MIME type.
    /// @return Pointer to format, or `nullptr` if no such format is known.
    const Format* getFormatByMimeType(const MimeType& mimeType) const;
    /// Get formats matching the given file name.
    /// @param name Full name (not path) of the file to figure out potential formats of.
    ///        *Never* pass a truncated file name or extension only; as this will fail to match formats.
    std::vector<gsl::not_null<const Format*>> getFormatsForFileName(const std::string_view& name) const;

    /// @}

    /// @name Importers
    /// @{

    /// Register an importer, associating one or more file formats to a Python module name.
    void addImporter(const Importer& importer);
    std::vector<gsl::not_null<const Importer*>> getImporters() const;
    std::vector<gsl::not_null<const Importer*>> getImportersForMimeType(const MimeType& mimeType) const;
    inline std::vector<gsl::not_null<const Importer*>> getImportersForFormat(const Format& format) const
    {
        return getImportersForMimeType(format.mimeType);
    }
    /// Get importers able to import a file with the given file name.
    /// @param name Full name (not path) of the file to be imported.
    ///        *Never* pass a truncated file name or extension only; as this will fail to match importers.
    std::vector<gsl::not_null<const Importer*>> getImportersForFileName(const std::string_view& name) const;
    std::vector<gsl::not_null<const Format*>> getSupportedImportFormats() const;
    /// Get the importers with a given Python module name.
    std::vector<gsl::not_null<const Importer*>> getImportersByModule(const std::string_view& module) const;

    /// @}

    /// @name Exporters
    /// @{

    /// Register an exporter, associating one or more file formats to a Python module name.
    void addExporter(const Exporter& exporter);
    std::vector<gsl::not_null<const Exporter*>> getExporters() const;
    std::vector<gsl::not_null<const Exporter*>> getExportersForMimeType(const MimeType& mimeType) const;
    inline std::vector<gsl::not_null<const Exporter*>> getExportersForFormat(const Format& format) const
    {
        return getExportersForMimeType(format.mimeType);
    }
    /// Get exporters able to export to a file with the given file name.
    /// @param name Full name (not path) of the file to be exported to.
    ///        *Never* pass a truncated file name or extension only; as this will fail to match exporters.
    std::vector<gsl::not_null<const Exporter*>> getExportersForFileName(const std::string_view& name) const;
    std::vector<gsl::not_null<const Format*>> getSupportedExportFormats() const;
    /// Get the exporters with a given Python module name.
    std::vector<gsl::not_null<const Exporter*>> getExportersByModule(const std::string_view& module) const;

    /// @}

private:
    struct FormatInfo {
        Format format;
        std::regex fileNameRegex;
    };
    std::map<MimeType, FormatInfo> _formatInfos;
    std::vector<Importer> _importers;
    std::vector<Exporter> _exporters;
};

}  // namespace App

#endif /* end of include guard: FORMATS_H */
