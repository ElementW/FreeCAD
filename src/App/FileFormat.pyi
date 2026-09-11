# SPDX-License-Identifier: LGPL-2.1-or-later
# SPDX-FileCopyrightText: 2026 Céleste Wouters <foss@elementw.net>
# SPDX-FileNotice: Part of the FreeCAD project.

from __future__ import annotations

from collections.abc import Sequence, Iterable
from typing import Final

from Base.Metadata import export
from Base.PyObjectBase import PyObjectBase

MimeType = str

FileNamePattern = str

@export(
    Constructor=False,
    Delete=True,
)
class FileFormat(PyObjectBase):
    @constmethod
    def getDisplayName(self) -> str:
        """
        Translated text for the supported formats collectively.
        FreeCAD branding is automatically replaced.
        Only use for display purposes, do not store or use as map keys.
        """
        ...

    TranslatableName: Final[str] = ""
    """Translatable (with context "FileFormat") name for the format."""

    MimeType: Final[MimeType] = ""
    """IANA-registered (or most popular failing that) MIME type."""

    SecondaryMimeTypes: Final[Sequence[MimeType]] = []
    """Additional/alias MIME types."""

    FileNamePatterns: Final[Sequence[FileNamePattern]] = []
    """
    Case-insensitive ``glob()``-style file name patterns to aid in file type detection, including in
    open/save dialog boxes. Note those patterns match more than just file extensions and should
    *never* be reduced to extensions only.

    Example:
        ["*.abc", "result_*.txt"]
    """

@export(
    Constructor=False,
    Delete=True,
)
class FileAdapter(PyObjectBase):
    @constmethod
    def getFileNamePatterns(self, formats: Formats) -> Sequence[FileNamePattern]: ...

    @constmethod
    def getSupportedFormatsText(self) -> str:
        """
        Translated text for the supported formats collectively.
        FreeCAD branding is automatically replaced.
        Only use for display purposes, do not store or use as map keys.
        """
        ...

    @constmethod
    def getActionText(self) -> str:
        """
        Translated text for the import/export action itself.
        FreeCAD branding is automatically replaced.
        Only use for display purposes, do not store or use as map keys.
        """
        ...

    @constmethod
    def getFilesText(self, n: int) -> str:
        """
        Translated text for importing @p n files.
        FreeCAD branding is automatically replaced.
        Only use for display purposes, do not store or use as map keys.
        """
        ...

    SortKey: Final[str] = ""
    """
    Arbitrary key that *can* (but is not guaranteed to) be used alphabetically on when
    displaying a list of file importers/exporters to the user, e.g. in file dialogs, to
    provide a more consistent and permanent order including across languages.
    When unspecified, the context-sensitive translated name will be used.
    Specify this **if and only if** there is one overarching file extension that's handled by
    this importer/exporter that's consistent across all languages.

    Example:
        "xlsx" for "Excel 2007 Spreadsheet" (en) & "Tableur Excel 2007" (fr),
        "step+color" for "STEP with colors" (en) & "STEP avec couleurs" (fr),
        but unset for "Supported formats" (en) & "Formats supportés" (fr) as
        "supported" is only logical in English.
    """

    ModuleName: Final[str] = ""
    """Python module name to call to run this importer/exporter."""

    FileMimeTypes: Final[Sequence[MimeType]] = []
    """
    File MIME types this importer/exporter supports.
    File name patterns are derived automatically from this and the list of registered file types.
    """

    TranslatableSupportedFormatsText: Final[str] = ""
    """
    Translatable (with context "FileFormat") text for the supported formats collectively.
    Used in open/save dialog filters.

    Example:
        "Vector images" (en)
    """

    TranslatableActionText: Final[str] = ""
    """
    Translatable (with context "FileFormat") text for the general import/export action itself,
    to be used in contexts not concerning specific files, e.g. the toolbar editor.
    In languages that use them, sentences that use indefinite articles.

    Example:
        "Import FEM simulation results" (en),
        "Importer des résultats de simulation FEM" (fr),
        "Export bodies as meshes" (en)
    """

    TranslatableFilesText: Final[str] = ""
    """
    Translatable (with context "FileFormat") text for importing/exporting N files,
    to be used in contexts where files are involved, e.g. file dialogs or the drag & drop
    import type disambiguation dialog.
    In languages that use them, sentences that use definite articles.
    Translations must support plural forms if applicable; "%1" is replaced by number of files.

    Example:
        "Import %1 FEM simulation result file(s)" (en),
        "Importer %1 fichiers de résultat de simulation FEM" (fr),
        "Export %1 body/bodies as mesh(es)" (en)
    """

FileImporter = FileAdapter
FileExporter = FileAdapter

@export(
    Constructor=False,
    Delete=True,
)
class Formats(PyObjectBase):
    def addFormat(
        self,
        *,
        translatableName: str,
        mimeType: MimeType,
        secondaryMimeTypes: Iterable[MimeType] = [],
        fileNamePatterns: Iterable[FileNamePattern],
    ) -> bool:
        """
        Register a file format.
        If the format is already known, the properties passed will be merged with the existing entry.

        Returns:
            ``True`` if the format was new and added, ``False`` if it was already known.

        Raises:
            ParserError: if a file name pattern of the format is invalid.
            IndexError: if too many formats are registered.
            ValueError: if the main MIME type of the format is a secondary
                        type of another known format.
            ValueError: if any secondary MIME type of the format is the primary
                        or a secondary type of another known format.
        """
        ...

    @constmethod
    def getFormats(self) -> Sequence[FileFormat]:
        """Get all known formats."""
        ...

    @constmethod
    def getFormatByMimeType(self, mimeType: MimeType, /) -> Optional[FileFormat]:
        """Find the format with the given MIME type."""
        ...

    @constmethod
    def getFormatsForFileName(self, name: str, /) -> Sequence[FileFormat]:
        """
        Get formats matching the given file name.

        Args:
            name: Full name (not path) of the file to figure out potential formats of.
                  *Never* pass a truncated file name or extension only; format matching will fail.
        """
        ...


    def addImporter(
        self,
        *,
        sortKey: str = "",
        moduleName: str,
        fileMimeTypes: Sequence[MimeType],
        translatableSupportedFormatsText: str,
        translatableActionText: str,
        translatableFilesText: str,
    ) -> None:
        """
        Register an importer, associating one or more file formats to a Python module name.

        Raises:
            ValueError: if any of the handled MIME types matches no known FileFormat.
        """
        ...

    @constmethod
    def getImporters() -> Sequence[FileImporter]:
        """List all known importers."""
        ...

    @constmethod
    def getImportersForMimeType(mimeType: MimeType, /) -> Sequence[FileImporter]:
        """List all known importers able to handle a given MIME type."""
        ...

    @constmethod
    def getImportersForFormat(format: FileFormat, /) -> Sequence[FileImporter]:
        """List all known importers able to handle a given ``FileFormat``."""
        ...

    @constmethod
    def getImportersForFileName(self, name: str, /) -> Sequence[FileImporter]:
        """
        Get importers able to import a file with the given file name.

        Args:
            name: Full name (not path) of the file to be imported.
                  *Never* pass a truncated file name or extension only; importer matching will fail.
        """
        ...

    @constmethod
    def getSupportedImportFormats(self) -> Sequence[FileFormat]:
        """Get the importers with a given Python module name."""
        ...

    @constmethod
    def getImportersByModule(self, module: str, /) -> Sequence[FileFormat]:
        """Get the importers with a given Python module name."""
        ...


    def addExporter(
        self,
        *,
        sortKey: str = "",
        moduleName: str,
        fileMimeTypes: Sequence[MimeType],
        translatableSupportedFormatsText: str,
        translatableActionText: str,
        translatableFilesText: str,
    ) -> None:
        """
        Register an exporter, associating one or more file formats to a Python module name.

        Raises:
            ValueError: if any of the handled MIME types matches no known FileFormat.
        """
        ...

    @constmethod
    def getExporters() -> Sequence[FileExporter]:
        """List all known exporters."""
        ...

    @constmethod
    def getExportersForMimeType(mimeType: MimeType, /) -> Sequence[FileExporter]:
        """List all known exporters able to handle a given MIME type."""
        ...

    @constmethod
    def getExportersForFormat(format: FileFormat, /) -> Sequence[FileExporter]:
        """List all known exporters able to handle a given ``FileFormat``."""
        ...

    @constmethod
    def getExportersForFileName(self, name: str, /) -> Sequence[FileExporter]:
        """
        Get exporters able to export a file with the given file name.

        Args:
            name: Full name (not path) of the file to be exported.
                  *Never* pass a truncated file name or extension only; exporter matching will fail.
        """
        ...

    @constmethod
    def getSupportedExportFormats(self) -> Sequence[FileFormat]:
        """Get the exporters with a given Python module name."""
        ...

    @constmethod
    def getExportersByModule(self, module: str, /) -> Sequence[FileFormat]:
        """Get the exporters with a given Python module name."""
        ...
