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
    DisplayName: Final[str] = ""

    TranslatableName: Final[str] = ""

    MimeType: Final[MimeType] = ""

    SecondaryMimeTypes: Final[Sequence[MimeType]] = []

    FileNamePatterns: Final[Sequence[FileNamePattern]] = []


@export(
    Constructor=False,
    Delete=True,
)
class Formats(PyObjectBase):
    @constmethod
    def addFormat(
        self,
        *,
        translatableName: str,
        mimeType: MimeType,
        secondaryMimeTypes: Iterable[MimeType],
        fileNamePatterns: Iterable[FileNamePattern],
    ) -> bool:
        ...

    @constmethod
    def getFormats(self) -> Sequence[FileFormat]:
        ...

    @constmethod
    def getFormatByMimeType(self, mimeType: MimeType) -> Optional[FileFormat]:
        ...

    @constmethod
    def getFormatsForFileName(self, name: str) -> Sequence[FileFormat]:
        ...
