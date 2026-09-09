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

#ifndef FC_NO_LEGACY_FORMAT_HANDLING

// Silence the deprecation warnings for legacy file format functions in App::Application
# define FC_LEGACY_FORMAT_DEPRECATED(x)
# define fcApp App::GetApplication()

# include <gtest/gtest.h>

# include <App/Application.h>
# include <App/FileFormat.h>

# include "FileFormat.h"
# include "InitApplication.h"


using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

class FileFormatLegacyTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        tests::initApplication();
    }
};

TEST_F(FileFormatLegacyTest, addImportType)
{
    EXPECT_THAT(fcApp.getImportFilters("abc"), IsEmpty());
    EXPECT_THAT(fcApp.getImportModules("abc"), IsEmpty());
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.abc"), IsEmpty());
    EXPECT_THAT(fcApp.getFormats().importersForFileName("test.abc"), IsEmpty());
    fcApp.addImportType("Import ABC file (*.abc)", "JsonABCImporter");
    EXPECT_THAT(
        fcApp.getImportFilters("abc"),
        AllOf(SizeIs(1), Contains(Key(HasSubstr("Import ABC file"))))
    );
    EXPECT_THAT(fcApp.getImportModules("abc"), AllOf(SizeIs(1), Contains(Eq("JsonABCImporter"))));
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.abc"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().importersForFileName("test.abc"),
        Contains(AdapterWithModule("JsonABCImporter"))
    );
};

TEST_F(FileFormatLegacyTest, noMergeModernAndLegcyImportType)
{
    // Modern and legacy-inferred formats can't be merged because there is no MIME type
    // to uniquely tie them together, and file patterns aren't unique to a specific format,
    // e.g. both Z88 meshes and their displacement result files use `*.txt`.
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.timl"), IsEmpty());
    fcApp.getFormats().addFormat({
        .translatableName = "Test Import Modern-Legacy",
        .mimeType = "application/x-test-timl",
        .fileNamePatterns = {"*.timl"},
    });
    fcApp.addImportType("Import TIML file (*.timl)", "JsonTIMLImporter");
    EXPECT_THAT(
        fcApp.getFormats().formatsForFileName("test.timl"),
        AllOf(
            SizeIs(2),
            Contains(HasTranslatableNameOf("Test Import Modern-Legacy")),
            Contains(HasTranslatableNameOf("Import TIML file"))
        )
    );
};

TEST_F(FileFormatLegacyTest, changeImportModule)
{
    fcApp.addImportType("Import OLD file (*.old)", "OLDImporter");
    EXPECT_THAT(fcApp.getImportFilters("old"), SizeIs(1));
    EXPECT_THAT(fcApp.getImportFilters("old"), Contains(Key(HasSubstr("Import OLD file"))));
    EXPECT_THAT(fcApp.getImportModules("old"), SizeIs(1));
    EXPECT_THAT(fcApp.getImportModules("old"), Contains(Eq("OLDImporter")));
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.old"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().importersForFileName("test.old"),
        AllOf(SizeIs(1), Contains(AdapterWithModule("OLDImporter")))
    );
    fcApp.changeImportModule("Import OLD file (*.old)", "OLDImporter", "NewImporter");
    EXPECT_THAT(
        fcApp.getImportFilters("old"),
        AllOf(SizeIs(1), Contains(Key(HasSubstr("Import OLD file"))))
    );
    EXPECT_THAT(
        fcApp.getImportModules("old"),
        AllOf(SizeIs(1), Not(Contains(Eq("OLDImporter"))), Contains(Eq("NewImporter")))
    );
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.old"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().importersForFileName("test.old"),
        AllOf(Not(Contains(AdapterWithModule("OLDImporter"))), Contains(AdapterWithModule("NewImporter")))
    );
};

TEST_F(FileFormatLegacyTest, addExportType)
{
    EXPECT_THAT(fcApp.getExportFilters("def"), IsEmpty());
    EXPECT_THAT(fcApp.getExportModules("def"), IsEmpty());
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.def"), IsEmpty());
    EXPECT_THAT(fcApp.getFormats().exportersForFileName("test.def"), IsEmpty());
    fcApp.addExportType("Export DEF file (*.def)", "JsonDEFExporter");
    EXPECT_THAT(fcApp.getExportFilters("def"), SizeIs(1));
    EXPECT_THAT(fcApp.getExportFilters("def"), Contains(Key(HasSubstr("Export DEF file"))));
    EXPECT_THAT(fcApp.getExportModules("def"), SizeIs(1));
    EXPECT_THAT(fcApp.getExportModules("def"), Contains(Eq("JsonDEFExporter")));
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.def"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().exportersForFileName("test.def"),
        Contains(AdapterWithModule("JsonDEFExporter"))
    );
};

TEST_F(FileFormatLegacyTest, noMergeModernAndLegcyExportType)
{
    // Modern and legacy-inferred formats can't be merged because there is no MIME type
    // to uniquely tie them together, and file patterns aren't unique to a specific format,
    // e.g. both FEM JSON meshes and FreeCAD simple mesh exports use `*.json`.
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.teml"), IsEmpty());
    fcApp.getFormats().addFormat({
        .translatableName = "Test Export Modern-Legacy",
        .mimeType = "application/x-test-teml",
        .fileNamePatterns = {"*.teml"},
    });
    fcApp.addImportType("Export TEML file (*.teml)", "JsonTEMLExporter");
    EXPECT_THAT(
        fcApp.getFormats().formatsForFileName("test.teml"),
        AllOf(
            SizeIs(2),
            Contains(HasTranslatableNameOf("Test Export Modern-Legacy")),
            Contains(HasTranslatableNameOf("Export TEML file"))
        )
    );
};

TEST_F(FileFormatLegacyTest, changeExportModule)
{
    fcApp.addExportType("Export OLD file (*.old)", "OLDExporter");
    EXPECT_THAT(
        fcApp.getExportFilters("old"),
        AllOf(SizeIs(1), Contains(Key(HasSubstr("Export OLD file"))))
    );
    EXPECT_THAT(fcApp.getExportModules("old"), AllOf(SizeIs(1), Contains(Eq("OLDExporter"))));
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.old"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().exportersForFileName("test.old"),
        AllOf(SizeIs(1), Contains(AdapterWithModule("OLDExporter")))
    );
    fcApp.changeExportModule("Export OLD file (*.old)", "OLDExporter", "NewExporter");
    EXPECT_THAT(
        fcApp.getExportFilters("old"),
        AllOf(SizeIs(1), Contains(Key(HasSubstr("Export OLD file"))))
    );
    EXPECT_THAT(
        fcApp.getExportModules("old"),
        AllOf(SizeIs(1), Not(Contains(Eq("OLDExporter"))), Contains(Eq("NewExporter")))
    );
    EXPECT_THAT(fcApp.getFormats().formatsForFileName("test.old"), Not(IsEmpty()));
    EXPECT_THAT(
        fcApp.getFormats().exportersForFileName("test.old"),
        AllOf(Not(Contains(AdapterWithModule("OLDExporter"))), Contains(AdapterWithModule("NewExporter")))
    );
};

#endif  // FC_NO_LEGACY_FORMAT_HANDLING
