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

#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <App/FileFormat.h>

#include "FileFormat.h"


using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

class FileFormatTest: public ::testing::Test
{
protected:
    App::Formats formats {false};
};

TEST_F(FileFormatTest, addNewFormat)
{
    EXPECT_THAT(formats.formats(), Not(Contains(HasMainMimeOf("application/x-fc-new-fmt-test"))));
    EXPECT_THAT(formats.formatByMimeType("application/x-fc-new-fmt-test"), IsNull());
    EXPECT_THAT(formats.formatsForFileName("file.new"), IsEmpty());
    EXPECT_THAT(formats.addFormat({"New format", "application/x-fc-new-fmt-test", {"*.new"}}), Eq(true));
    EXPECT_THAT(formats.formats(), Contains(HasMainMimeOf("application/x-fc-new-fmt-test")));
    EXPECT_THAT(formats.formatByMimeType("application/x-fc-new-fmt-test"), NotNull());
    EXPECT_THAT(
        formats.formatsForFileName("file.new"),
        Contains(HasMainMimeOf("application/x-fc-new-fmt-test"))
    );
    // Test merging of MIME types and file name patterns
    EXPECT_THAT(
        formats.addFormat(
            {"Novo formato", "application/x-fc-new-fmt-test", {"*.novo"}, {"application/x-fc-novo-fmt-test"}}
        ),
        Eq(false)
    );
    EXPECT_THAT(
        formats.formatByMimeType("application/x-fc-novo-fmt-test"),
        Eq(formats.formatByMimeType("application/x-fc-new-fmt-test"))
    );
    EXPECT_THAT(
        formats.formatsForFileName("file.novo"),
        Contains(HasMainMimeOf("application/x-fc-new-fmt-test"))
    );
}

TEST_F(FileFormatTest, addNewFormatClash)
{
    EXPECT_THAT(formats.formatByMimeType("test-clash/aaa"), IsNull());
    EXPECT_THAT(
        formats.addFormat({"Clash AAA", "test-clash/aaa", {"*.aaa"}, {"test-clash/aaa-secondary"}}),
        Eq(true)
    );
    // Try registering a format whose secondary is an existing format's primary
    EXPECT_THROW(
        formats.addFormat({"Clash BBB", "test-clash/bbb", {"*.bbb"}, {"test-clash/aaa"}}),
        std::invalid_argument
    );
    // Try registering a format whose primary is an existing format's secondary
    EXPECT_THROW(
        formats.addFormat({"Clash AAA 2", "test-clash/aaa-secondary", {"*.aaa"}}),
        std::invalid_argument
    );
    // Try registering a format whose secondary is an existing unrelated format's secondary
    EXPECT_THROW(
        formats.addFormat({"Clash BBB", "test-clash/bbb", {"*.bbb"}, {"test-clash/aaa-secondary"}}),
        std::invalid_argument
    );
}

TEST_F(FileFormatTest, addImporters)
{
    formats.addFormat({"Test ABC, JSON", "application/x-fc-abc-test+json", {"*.abc"}});
    formats.addFormat({"Test ABC, Binary", "application/x-fc-abc-test+bin", {"*.abc"}});
    EXPECT_THAT(formats.importers(), Not(Contains(HandlesMime("application/x-fc-abc-test+json"))));
    EXPECT_THAT(formats.importersForMimeType("application/x-fc-abc-test+json"), IsEmpty());
    EXPECT_THAT(formats.importersForFileName("test.abc"), IsEmpty());

    formats.addImporter(
        {"JsonABCImporter",
         {"application/x-fc-abc-test+json"},
         "Test ABC",
         "Import ABC file",
         "Import %1 ABC files"}
    );
    EXPECT_THAT(formats.importers(), Contains(HandlesMime("application/x-fc-abc-test+json")));
    EXPECT_THAT(formats.importers(), Not(Contains(HandlesMime("application/x-fc-abc-test+bin"))));
    EXPECT_THAT(
        formats.importersForMimeType("application/x-fc-abc-test+json"),
        Contains(AdapterWithModule("JsonABCImporter"))
    );
    EXPECT_THAT(
        formats.importersForFileName("test.abc"),
        Contains(AdapterWithModule("JsonABCImporter"))
    );

    formats.addImporter(
        {"LibABCImporter",
         {"application/x-fc-abc-test+json", "application/x-fc-abc-test+bin"},
         "Test ABC",
         "Import ABC file",
         "Import %1 ABC files"}
    );
    EXPECT_THAT(formats.importers(), Contains(HandlesMime("application/x-fc-abc-test+json")));
    EXPECT_THAT(formats.importers(), Contains(HandlesMime("application/x-fc-abc-test+bin")));
    // JSON variant handled by both
    EXPECT_THAT(
        formats.importersForMimeType("application/x-fc-abc-test+json"),
        AllOf(
            Contains(AdapterWithModule("JsonABCImporter")),
            Contains(AdapterWithModule("LibABCImporter"))
        )
    );
    // Binary variant only handled by LibABC
    EXPECT_THAT(
        formats.importersForMimeType("application/x-fc-abc-test+bin"),
        AllOf(
            Not(Contains(AdapterWithModule("JsonABCImporter"))),
            Contains(AdapterWithModule("LibABCImporter"))
        )
    );
    // Multiple formats for a given file name, both importers should be available
    EXPECT_THAT(
        formats.importersForFileName("test.abc"),
        AllOf(
            Contains(AdapterWithModule("JsonABCImporter")),
            Contains(AdapterWithModule("LibABCImporter"))
        )
    );
}

TEST_F(FileFormatTest, twoImportersOneMime)
{
    formats.addFormat({"Test IJK", "application/x-fc-ijk-test", {"*.ijk"}});
    formats.addImporter(
        {"IJKAsGeometry",
         {"application/x-fc-ijk-test"},
         "Test IJK",
         "Import IJK file as geometry",
         "Export %1 IJK files as geometry"}
    );
    formats.addImporter(
        {"IJKAsImage",
         {"application/x-fc-ijk-test"},
         "Test IJK",
         "Import IJK file as image",
         "Import %1 IJK files as image"}
    );
    EXPECT_THAT(
        formats.importersForMimeType("application/x-fc-ijk-test"),
        AllOf(Contains(AdapterWithModule("IJKAsGeometry")), Contains(AdapterWithModule("IJKAsImage")))
    );
    EXPECT_THAT(
        formats.importersForFileName("test.ijk"),
        AllOf(Contains(AdapterWithModule("IJKAsGeometry")), Contains(AdapterWithModule("IJKAsImage")))
    );
}

TEST_F(FileFormatTest, addImporterUnknownFormat)
{
    EXPECT_THROW(
        formats.addImporter(  // NOLINT
            {"UnknownFormatImporter",
             {"non/existent-mime"},
             "Unknown",
             "Import unknown file",
             "Import %1 unknown files"}
        ),
        std::invalid_argument
    );
}

TEST_F(FileFormatTest, addImporterSecondaryMime)
{
    formats.addFormat(
        {"Import dual", "application/x-import-dual", {"*.idl"}, {"secondary/x-import-dual"}}
    );
    EXPECT_THROW(
        formats.addImporter(  // NOLINT
            {"SecondaryMimeFormatImporter",
             {"secondary/x-import-dual"},
             "Secondary",
             "Import secondary MIME file",
             "Import %1 secondary MIME files"}
        ),
        std::invalid_argument
    );
}

TEST_F(FileFormatTest, addExporters)
{
    formats.addFormat({"Test ABC, JSON", "application/x-fc-abc-test+json", {"*.abc"}});
    formats.addFormat({"Test ABC, Binary", "application/x-fc-abc-test+bin", {"*.abc"}});
    EXPECT_THAT(formats.exporters(), Not(Contains(HandlesMime("application/x-fc-abc-test+json"))));
    EXPECT_THAT(formats.exportersForMimeType("application/x-fc-abc-test+json"), IsEmpty());
    EXPECT_THAT(formats.exportersForFileName("test.abc"), IsEmpty());
    formats.addExporter(
        {"JsonABCExporter",
         {"application/x-fc-abc-test+json"},
         "Test ABC",
         "Export ABC file",
         "Export %1 ABC files"}
    );

    EXPECT_THAT(formats.exporters(), Contains(HandlesMime("application/x-fc-abc-test+json")));
    EXPECT_THAT(formats.exporters(), Not(Contains(HandlesMime("application/x-fc-abc-test+bin"))));
    EXPECT_THAT(
        formats.exportersForMimeType("application/x-fc-abc-test+json"),
        Contains(AdapterWithModule("JsonABCExporter"))
    );
    EXPECT_THAT(
        formats.exportersForFileName("test.abc"),
        Contains(AdapterWithModule("JsonABCExporter"))
    );

    formats.addExporter(
        {"LibABCExporter",
         {"application/x-fc-abc-test+json", "application/x-fc-abc-test+bin"},
         "Test ABC",
         "Import ABC file",
         "Import %1 ABC files"}
    );
    EXPECT_THAT(formats.exporters(), Contains(HandlesMime("application/x-fc-abc-test+json")));
    EXPECT_THAT(formats.exporters(), Contains(HandlesMime("application/x-fc-abc-test+bin")));
    // JSON variant handled by both
    EXPECT_THAT(
        formats.exportersForMimeType("application/x-fc-abc-test+json"),
        AllOf(
            Contains(AdapterWithModule("JsonABCExporter")),
            Contains(AdapterWithModule("LibABCExporter"))
        )
    );
    // Binary variant only handled by LibABC
    EXPECT_THAT(
        formats.exportersForMimeType("application/x-fc-abc-test+bin"),
        AllOf(
            Not(Contains(AdapterWithModule("JsonABCExporter"))),
            Contains(AdapterWithModule("LibABCExporter"))
        )
    );
    // Multiple formats for a given file name, both Exporters should be available
    EXPECT_THAT(
        formats.exportersForFileName("test.abc"),
        AllOf(
            Contains(AdapterWithModule("JsonABCExporter")),
            Contains(AdapterWithModule("LibABCExporter"))
        )
    );
}

TEST_F(FileFormatTest, addExporterUnknownFormat)
{
    EXPECT_THROW(
        formats.addExporter(  // NOLINT
            {"UnknownFormatExporter",
             {"non/existent-mime"},
             "Unknown",
             "Export unknown file",
             "Export %1 unknown files"}
        ),
        std::invalid_argument
    );
}

TEST_F(FileFormatTest, addExporterSecondaryMime)
{
    formats.addFormat(
        {"Export dual", "application/x-export-dual", {"*.edl"}, {"secondary/x-export-dual"}}
    );
    EXPECT_THROW(
        formats.addExporter(  // NOLINT
            {"SecondaryMimeFormatExporter",
             {"secondary/x-export-dual"},
             "Secondary",
             "Export secondary MIME file",
             "Export %1 secondary MIME files"}
        ),
        std::invalid_argument
    );
}
