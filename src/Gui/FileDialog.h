/***************************************************************************
 *   Copyright (c) 2004 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#ifndef GUI_FILEDIALOG_H
#define GUI_FILEDIALOG_H

#include <utility>
#include <variant>

#include <QCompleter>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QPointer>
#include <FCGlobal.h>

class QButtonGroup;
class QDialogButtonBox;
class QGridLayout;
class QGroupBox;
class QHBoxLayout;
class QLineEdit;
class QSpacerItem;

namespace App
{
class Importer;
class Exporter;
class Formats;
} // namespace App

namespace Gui {

/*!
 * \brief The DialogOptions class
 * Helper class to control whether to use native or Qt dialogs.
 */
class GuiExport DialogOptions
{
public:
    static bool dontUseNativeFileDialog();
    static bool dontUseNativeColorDialog();
};

class GuiExport FileFilter
{
public:
    static const FileFilter AllFiles;

    QString filterName;
    QStringList patterns;
    QStringList commonPatterns;
    QString userData;

protected:
    std::variant<const App::Importer*, const App::Exporter*> translator;

public:
    // QList<T> requires default-constructible elements.
    FileFilter() = default;

    FileFilter(QString filterName, QStringList patterns, QString userData = {}) :
        filterName(std::move(filterName)), patterns(std::move(patterns)), userData(std::move(userData)) {}
    FileFilter(QString filterName, QStringList patterns, const char* userData) :
        FileFilter(std::move(filterName), std::move(patterns), QString::fromUtf8(userData)) {}

    FileFilter(QString filterName, const QList<QStringView>& patterns, QString userData = {}) :
        filterName(filterName), userData(std::move(userData))
    {
        this->patterns.reserve(patterns.size());
        for (const auto &pattern : patterns) {
            this->patterns << pattern.toString();
        }
    }
    FileFilter(QString filterName, const QList<QStringView>& patterns, const char* userData) :
        FileFilter(std::move(filterName), patterns, QString::fromUtf8(userData)) {}

    FileFilter(QString filterName, const std::initializer_list<const char*>& patterns, QString userData = {}) :
        filterName(std::move(filterName)), userData(std::move(userData))
    {
        this->patterns.reserve(patterns.size());
        for (const auto &pattern : patterns) {
            this->patterns << QString::fromUtf8(pattern);
        }
    }
    FileFilter(QString filterName, const std::initializer_list<const char*>& patterns, const char* userData) :
        FileFilter(std::move(filterName), patterns, QString::fromUtf8(userData)) {}

    bool operator==(const FileFilter& other) const
    {
        return filterName == other.filterName && patterns == other.patterns &&
                commonPatterns == other.commonPatterns && translator == other.translator;
    }

    static FileFilter fromImporter(const App::Importer&, const App::Formats&);
    const App::Importer* getImporter() const;
    static FileFilter fromExporter(const App::Exporter&, const App::Formats&);
    const App::Exporter* getExporter() const;

private:
    friend class FileDialog;
    friend class FileFilterList;
    static FileFilter fromFilterString(const QStringView&);
    QString asFilterString() const;
    QString asFancyFilterString() const;
};

class GuiExport FileFilterList : public QList<FileFilter>
{
private:
    friend class FileDialog;
    static FileFilterList fromFilterStringList(const QStringList&);
    static FileFilterList fromFiltersString(const QString&);
    QStringList asFancyFiltersStringList() const;
    //QString asFancyFiltersString() const;
};

/**
 * The FileDialog class provides dialogs that allow users to select files or directories.
 * \author Werner Mayer
 */
class GuiExport FileDialog : public QFileDialog
{
    Q_OBJECT

public:
    static QString getOpenFileName(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                   const FileFilterList& filters = {}, qsizetype* selectedFilterIndex = nullptr, Options options = {});
    [[deprecated("Use getOpenFileName with a FileFilterList instead")]]
    static QString getOpenFileName(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                   const QString& filter = {}, QString* selectedFilter = nullptr, Options options = {});
    static QString getSaveFileName(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                   const FileFilterList& filters = {}, qsizetype* selectedFilterIndex = nullptr, Options options = {});
    [[deprecated("Use getSaveFileName with a FileFilterList instead")]]
    static QString getSaveFileName(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                   const QString& filter = {}, QString * selectedFilter = nullptr, Options options = {});
    static QString getExistingDirectory(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                        Options options = ShowDirsOnly );
    static QStringList getOpenFileNames(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                        const FileFilterList& filters = {}, qsizetype* selectedFilterIndex = nullptr, Options options = {});
    [[deprecated("Use getOpenFileNames with a FileFilterList instead")]]
    static QStringList getOpenFileNames(QWidget* parent = nullptr, const QString& caption = {}, const QString& dir = {},
                                        const QString& filter = {}, QString* selectedFilter = nullptr, Options options = {});

    /*! Return the last directory a file was read from or saved to. */
    static QString getWorkingDirectory();
    /*! Set the directory a file was read from or saved to. */
    static void setWorkingDirectory( const QString& );
    static QString restoreLocation();
    static void saveLocation(const QString&);

    explicit FileDialog(QWidget * parent = nullptr);
    ~FileDialog() override;

    void accept() override;

private Q_SLOTS:
    void onSelectedFilter(const QString&);

private:
    bool hasSuffix(const QString&) const;
    static QList<QUrl> fetchSidebarUrls();
    static QString workingDirectory;
};

// ----------------------------------------------------------------------

/**
 * The FileOptionsDialog class provides an extensible file dialog with an additional widget either at the right
 * or at the bottom, that can be shown or hidden with the 'Extended' button.
 * @author Werner Mayer
 */
class GuiExport FileOptionsDialog : public QFileDialog
{
    Q_OBJECT

public:
    enum ExtensionPosition {
        ExtensionRight    = 0,
        ExtensionBottom   = 1
    };

    FileOptionsDialog ( QWidget* parent, Qt::WindowFlags );
    ~FileOptionsDialog() override;

    void accept() override;

    void setOptionsWidget( ExtensionPosition pos , QWidget*, bool show = false );
    QWidget* getOptionsWidget() const;

protected Q_SLOTS:
    void toggleExtension();

private:
    QSize oldSize;
    ExtensionPosition extensionPos;
    QPushButton* extensionButton;
    QPointer<QWidget> extensionWidget;
};

// ----------------------------------------------------------------------

/**
 * The FileIconProvider class provides icons for FileDialog to use.
 * \author Werner Mayer
 */
class FileIconProvider : public QFileIconProvider
{
public:
    FileIconProvider();
    ~FileIconProvider() override;

    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo & info) const override;
    QString type(const QFileInfo & info) const override;
};

// ----------------------------------------------------------------------

/**
 * The FileChooser class provides a lineedit with a button on the right side
 * to specify a file or directory.
 * \author Werner Mayer
 */
class GuiExport FileChooser : public QWidget
{
    Q_OBJECT

public:
    enum Mode { File, Directory };
    enum AcceptMode { AcceptOpen, AcceptSave };

    Q_ENUM( Mode )
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_ENUM( AcceptMode )
    Q_PROPERTY(AcceptMode acceptMode READ acceptMode WRITE setAcceptMode NOTIFY acceptModeChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString buttonText READ buttonText WRITE setButtonText NOTIFY buttonTextChanged)

public:
    explicit FileChooser ( QWidget * parent = nullptr );
    ~FileChooser() override;

    /**
    * Returns the set filter.
    */
    QString filter() const;

    /**
    * Returns the filename.
    */
    QString fileName() const;

    /**
    * Returns true if this widgets is set to choose a file, if it is
    * set to choose false is returned.
    */
    Mode mode() const;

    /**
    * Returns the button's text.
    */
    QString buttonText() const;

    /**
     * Sets the accept mode.
     */
    void setAcceptMode(AcceptMode mode);
    /**
     * Returns the accept mode.
     */
    AcceptMode acceptMode() const {
        return accMode;
    }

public Q_SLOTS:
    virtual void setFileName( const QString &fn );
    virtual void setMode( Gui::FileChooser::Mode m );
    virtual void setFilter ( const QString & );
    virtual void setButtonText ( const QString & );

Q_SIGNALS:
    void fileNameChanged( const QString & );
    void fileNameSelected( const QString & );
    void filterChanged(const QString&);
    void buttonTextChanged(const QString&);
    void modeChanged(Gui::FileChooser::Mode);
    void acceptModeChanged(Gui::FileChooser::AcceptMode);

private Q_SLOTS:
    void chooseFile();
    void editingFinished();

protected:
    void resizeEvent(QResizeEvent*) override;

private:
    QLineEdit *lineEdit;
    QCompleter *completer;
    QFileSystemModel *fs_model;
    QPushButton *button;
    Mode md;
    AcceptMode accMode;
    QString _filter;
};

// ----------------------------------------------------------------------

/**
 * The SelectModule class provides a list of radio buttons to choose
 * the module that should handle a certain file type.
 * @author Werner Mayer
 */
class GuiExport SelectModule : public QDialog
{
    Q_OBJECT

public:
    using Dict = QMap<QString, QString>;

    SelectModule (const QString& type, const Dict&, QWidget* parent);
    ~SelectModule() override;
    QString getModule() const;

    /** @name Import/Export handler
     * These methods accepts a file name or a list of file names and return
     * a map of file names with the associated Python module that should open
     * the file.
     */
    //@{
    static Dict exportHandler(const QString& fileName, const FileFilter& filter = FileFilter::AllFiles);
    static Dict exportHandler(const QStringList& fileNames, const FileFilter& filter = FileFilter::AllFiles);
    static Dict importHandler(const QString& fileName, const FileFilter& filter = FileFilter::AllFiles);
    static Dict importHandler(const QStringList& fileNames, const FileFilter& filter = FileFilter::AllFiles);
    //@}

    void accept() override;
    void reject() override;

private Q_SLOTS:
    void onButtonClicked();

private:
    QDialogButtonBox *buttonBox;
    QButtonGroup* group;
    QGridLayout *gridLayout;
    QHBoxLayout *hboxLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayout1;
    QSpacerItem *spacerItem;
    QSpacerItem *spacerItem1;
};

} // namespace Gui

#endif // GUI_FILEDIALOG_H
