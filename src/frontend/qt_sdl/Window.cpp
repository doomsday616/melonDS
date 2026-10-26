/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "NDS.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <QProcess>
#include <QApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QMimeDatabase>
#include <QFileDialog>
#include <QInputDialog>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QMimeData>
#include <QVector>
#include <QCommandLineParser>
#include <QDesktopServices>

#include "main.h"
#include "CheatsDialog.h"
#include "DateTimeDialog.h"
#include "EmuSettingsDialog.h"
#include "InputConfig/InputConfigDialog.h"
#include "VideoSettingsDialog.h"
#include "CameraSettingsDialog.h"
#include "AudioSettingsDialog.h"
#include "FirmwareSettingsDialog.h"
#include "PathSettingsDialog.h"
#include "MPSettingsDialog.h"
#include "WifiSettingsDialog.h"
#include "InterfaceSettingsDialog.h"
#include "ROMInfoDialog.h"
#include "RAMInfoDialog.h"
#include "TitleManagerDialog.h"
#include "PowerManagement/PowerManagementDialog.h"

#include "Platform.h"
#include "Config.h"
#include "version.h"
#include "Savestate.h"
#include "MPInterface.h"
#include "LANDialog.h"

//#include "main_shaders.h"

#include "EmuInstance.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"
#include "Window.h"
#include "AboutDialog.h"

using namespace melonDS;




extern CameraManager* camManager[2];
extern bool camStarted[2];


QString NdsRomMimeType = "application/x-nintendo-ds-rom";
QStringList NdsRomExtensions { ".nds", ".srl", ".dsi", ".ids" };

QString GbaRomMimeType = "application/x-gba-rom";
QStringList GbaRomExtensions { ".gba", ".agb" };


// This list of supported archive formats is based on libarchive(3) version 3.6.2 (2022-12-09).
QStringList ArchiveMimeTypes
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    "application/zip",
    "application/x-7z-compressed",
    "application/vnd.rar", // *.rar
    "application/x-tar",

    "application/x-compressed-tar", // *.tar.gz
    "application/x-xz-compressed-tar",
    "application/x-bzip-compressed-tar",
    "application/x-lz4-compressed-tar",
    "application/x-zstd-compressed-tar",

    "application/x-tarz", // *.tar.Z
    "application/x-lzip-compressed-tar",
    "application/x-lzma-compressed-tar",
    "application/x-lrzip-compressed-tar",
    "application/x-tzo", // *.tar.lzo
#endif
};

QStringList ArchiveExtensions
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    ".zip", ".7z", ".rar", ".tar",

    ".tar.gz", ".tgz",
    ".tar.xz", ".txz",
    ".tar.bz2", ".tbz2",
    ".tar.lz4", ".tlz4",
    ".tar.zst", ".tzst",

    ".tar.Z", ".taz",
    ".tar.lz",
    ".tar.lzma", ".tlz",
    ".tar.lrz", ".tlrz",
    ".tar.lzo", ".tzo"
#endif
};

// AAAAAAA
static bool FileExtensionInList(const QString& filename, const QStringList& extensions, Qt::CaseSensitivity cs = Qt::CaseInsensitive)
{
    return std::any_of(extensions.cbegin(), extensions.cend(), [&](const auto& ext) {
        return filename.endsWith(ext, cs);
    });
}

static bool MimeTypeInList(const QMimeType& mimetype, const QStringList& superTypeNames)
{
    return std::any_of(superTypeNames.cbegin(), superTypeNames.cend(), [&](const auto& superTypeName) {
        return mimetype.inherits(superTypeName);
    });
}


static bool NdsRomByExtension(const QString& filename)
{
    return FileExtensionInList(filename, NdsRomExtensions);
}

static bool GbaRomByExtension(const QString& filename)
{
    return FileExtensionInList(filename, GbaRomExtensions);
}

static bool SupportedArchiveByExtension(const QString& filename)
{
    return FileExtensionInList(filename, ArchiveExtensions);
}


static bool NdsRomByMimetype(const QMimeType& mimetype)
{
    return mimetype.inherits(NdsRomMimeType);
}

static bool GbaRomByMimetype(const QMimeType& mimetype)
{
    return mimetype.inherits(GbaRomMimeType);
}

static bool SupportedArchiveByMimetype(const QMimeType& mimetype)
{
    return MimeTypeInList(mimetype, ArchiveMimeTypes);
}

static bool ZstdNdsRomByExtension(const QString& filename)
{
    return filename.endsWith(".zst", Qt::CaseInsensitive) &&
        NdsRomByExtension(filename.left(filename.size() - 4));
}

static bool ZstdGbaRomByExtension(const QString& filename)
{
    return filename.endsWith(".zst", Qt::CaseInsensitive) &&
        GbaRomByExtension(filename.left(filename.size() - 4));
}

static bool FileIsSupportedFiletype(const QString& filename, bool insideArchive = false)
{
    if (ZstdNdsRomByExtension(filename) || ZstdGbaRomByExtension(filename))
        return true;

    if (NdsRomByExtension(filename) || GbaRomByExtension(filename) || SupportedArchiveByExtension(filename))
        return true;

    const auto matchmode = insideArchive ? QMimeDatabase::MatchExtension : QMimeDatabase::MatchDefault;
    const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, matchmode);
    return NdsRomByMimetype(mimetype) || GbaRomByMimetype(mimetype) || SupportedArchiveByMimetype(mimetype);
}



MainWindow::MainWindow(int id, EmuInstance* inst, QWidget* parent) :
    QMainWindow(parent),
    windowID(id),
    emuInstance(inst),
    globalCfg(inst->globalCfg),
    localCfg(inst->localCfg),
    windowCfg(localCfg.GetTable("Window"+std::to_string(id), "Window0")),
    emuThread(inst->getEmuThread()),
    enabledSaved(false),
    focused(true),
    wasMaximized(false),
    confirmPending(false)
{

    showOSD = windowCfg.GetBool("ShowOSD");

    setWindowTitle("melonDS " MELONDS_VERSION);
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);

#if QT_VERSION_MAJOR == 6 && WIN32
    // The "windows11" theme has pretty massive padding around menubar items, this makes Config and Help not fit in a window at 1x screen sizing
    // So let's reduce the padding a bit.
    if (QApplication::style()->name() == "windows11")
        setStyleSheet("QMenuBar::item { padding: 4px 8px; }");
#endif

    //hasMenu = (!parent);
    hasMenu = true;

    if (hasMenu)
    {
        // The menus are built with their English source texts: this local
        // tr() shadows MainWindow::tr() (lupdate still extracts the strings
        // under the MainWindow context). Each action's source text is then
        // stored and retranslateMenus() applies the active language, which
        // is also what happens on a live language switch.
        auto tr = [](const char* text) { return QString::fromUtf8(text); };

        QMenuBar * menubar = new QMenuBar();
        {
            QMenu * menu = menubar->addMenu(tr("File"));

            actOpenROM = menu->addAction(tr("Open ROM..."));
            connect(actOpenROM, &QAction::triggered, this, &MainWindow::onOpenFile);
            actOpenROM->setShortcut(QKeySequence(QKeySequence::StandardKey::Open));

            /*actOpenROMArchive = menu->addAction("Open ROM inside archive...");
            connect(actOpenROMArchive, &QAction::triggered, this, &MainWindow::onOpenFileArchive);
            actOpenROMArchive->setShortcut(QKeySequence(Qt::Key_O | Qt::CTRL | Qt::SHIFT));*/

            recentMenu = menu->addMenu(tr("Open recent"));
            loadRecentFilesMenu(true);

            //actBootFirmware = menu->addAction("Launch DS menu");
            actBootFirmware = menu->addAction(tr("Boot firmware"));
            connect(actBootFirmware, &QAction::triggered, this, &MainWindow::onBootFirmware);

            menu->addSeparator();

            actCurrentCart = menu->addAction(tr("DS slot: %1").arg(emuInstance->cartLabel()));
            actCurrentCart->setEnabled(false);

            actInsertCart = menu->addAction(tr("Insert cart..."));
            connect(actInsertCart, &QAction::triggered, this, &MainWindow::onInsertCart);

            actEjectCart = menu->addAction(tr("Eject cart"));
            connect(actEjectCart, &QAction::triggered, this, &MainWindow::onEjectCart);

            menu->addSeparator();

            actCurrentGBACart = menu->addAction(tr("GBA slot: %1").arg(emuInstance->gbaCartLabel()));
            actCurrentGBACart->setEnabled(false);

            actInsertGBACart = menu->addAction(tr("Insert ROM cart..."));
            connect(actInsertGBACart, &QAction::triggered, this, &MainWindow::onInsertGBACart);

            {
                QMenu * submenu = menu->addMenu(tr("Insert add-on cart"));
                QAction *act;

                int addons[] = {
                    GBAAddon_RAMExpansion,
                    GBAAddon_RumblePak,
                    GBAAddon_SolarSensorBoktai1,
                    GBAAddon_SolarSensorBoktai2,
                    GBAAddon_SolarSensorBoktai3,
                    GBAAddon_MotionPakHomebrew,
                    GBAAddon_MotionPakRetail,
                    GBAAddon_GuitarGrip,
                    -1
                };

                for (int i = 0; addons[i] != -1; i++)
                {
                    int addon = addons[i];
                    act = submenu->addAction(emuInstance->gbaAddonName(addon));
                    act->setData(QVariant(addon));
                    connect(act, &QAction::triggered, this, &MainWindow::onInsertGBAAddon);
                    actInsertGBAAddon.append(act);
                }
            }

            actEjectGBACart = menu->addAction(tr("Eject cart"));
            connect(actEjectGBACart, &QAction::triggered, this, &MainWindow::onEjectGBACart);

            menu->addSeparator();

            actImportSavefile = menu->addAction(tr("Import savefile"));
            connect(actImportSavefile, &QAction::triggered, this, &MainWindow::onImportSavefile);

            menu->addSeparator();

            {
                QMenu * submenu = menu->addMenu(tr("Save state"));

                for (int i = 1; i < 9; i++)
                {
                    actSaveState[i] = submenu->addAction(QString("%1").arg(i));
                    actSaveState[i]->setShortcut(QKeySequence(Qt::ShiftModifier | (Qt::Key_F1 + i - 1)));
                    actSaveState[i]->setData(QVariant(i));
                    connect(actSaveState[i], &QAction::triggered, this, &MainWindow::onSaveState);
                }

                actSaveState[0] = submenu->addAction(tr("File..."));
                actSaveState[0]->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_F9));
                actSaveState[0]->setData(QVariant(0));
                connect(actSaveState[0], &QAction::triggered, this, &MainWindow::onSaveState);
            }
            {
                QMenu * submenu = menu->addMenu(tr("Load state"));

                for (int i = 1; i < 9; i++)
                {
                    actLoadState[i] = submenu->addAction(QString("%1").arg(i));
                    actLoadState[i]->setShortcut(QKeySequence(Qt::Key_F1 + i - 1));
                    actLoadState[i]->setData(QVariant(i));
                    connect(actLoadState[i], &QAction::triggered, this, &MainWindow::onLoadState);
                }

                actLoadState[0] = submenu->addAction(tr("File..."));
                actLoadState[0]->setShortcut(QKeySequence(Qt::Key_F9));
                actLoadState[0]->setData(QVariant(0));
                connect(actLoadState[0], &QAction::triggered, this, &MainWindow::onLoadState);
            }

            actUndoStateLoad = menu->addAction(tr("Undo state load"));
            actUndoStateLoad->setShortcut(QKeySequence(Qt::Key_F12));
            connect(actUndoStateLoad, &QAction::triggered, this, &MainWindow::onUndoStateLoad);

            menu->addSeparator();
            actOpenConfig = menu->addAction(tr("Open melonDS directory"));
            connect(actOpenConfig, &QAction::triggered, this, [&]()
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(emuDirectory));
            });

            menu->addSeparator();

            actQuit = menu->addAction(tr("Quit"));
            connect(actQuit, &QAction::triggered, this, &MainWindow::onQuit);
            actQuit->setShortcut(QKeySequence(QKeySequence::StandardKey::Quit));
        }
        {
            QMenu * menu = menubar->addMenu(tr("System"));

            actPause = menu->addAction(tr("Pause"));
            actPause->setCheckable(true);
            connect(actPause, &QAction::triggered, this, &MainWindow::onPause);

            actReset = menu->addAction(tr("Reset"));
            connect(actReset, &QAction::triggered, this, &MainWindow::requestReset);

            actStop = menu->addAction(tr("Stop"));
            connect(actStop, &QAction::triggered, this, &MainWindow::onStop);

            actFrameStep = menu->addAction(tr("Frame step"));
            connect(actFrameStep, &QAction::triggered, this, &MainWindow::onFrameStep);

            menu->addSeparator();

            actPowerManagement = menu->addAction(tr("Power management"));
            connect(actPowerManagement, &QAction::triggered, this, &MainWindow::onOpenPowerManagement);

            actDateTime = menu->addAction(tr("Date and time"));
            connect(actDateTime, &QAction::triggered, this, &MainWindow::onOpenDateTime);

            menu->addSeparator();

            actEnableCheats = menu->addAction(tr("Enable cheats"));
            actEnableCheats->setCheckable(true);
            connect(actEnableCheats, &QAction::triggered, this, &MainWindow::onEnableCheats);

            //if (inst == 0)
            {
                actSetupCheats = menu->addAction(tr("Setup cheat codes"));
                actSetupCheats->setMenuRole(QAction::NoRole);
                connect(actSetupCheats, &QAction::triggered, this, &MainWindow::onSetupCheats);

                menu->addSeparator();
                actROMInfo = menu->addAction(tr("ROM info"));
                connect(actROMInfo, &QAction::triggered, this, &MainWindow::onROMInfo);

                actRAMInfo = menu->addAction(tr("RAM search"));
                connect(actRAMInfo, &QAction::triggered, this, &MainWindow::onRAMInfo);

                actTitleManager = menu->addAction(tr("Manage DSi titles"));
                connect(actTitleManager, &QAction::triggered, this, &MainWindow::onOpenTitleManager);
            }

            {
                menu->addSeparator();
                QMenu * submenu = menu->addMenu(tr("Multiplayer"));

                actMPNewInstance = submenu->addAction(tr("Launch new instance"));
                connect(actMPNewInstance, &QAction::triggered, this, &MainWindow::onMPNewInstance);

                submenu->addSeparator();

                actLANStartHost = submenu->addAction(tr("Host LAN game"));
                connect(actLANStartHost, &QAction::triggered, this, &MainWindow::onLANStartHost);

                actLANStartClient = submenu->addAction(tr("Join LAN game"));
                connect(actLANStartClient, &QAction::triggered, this, &MainWindow::onLANStartClient);

                /*submenu->addSeparator();

                actNPStartHost = submenu->addAction("NETPLAY HOST");
                connect(actNPStartHost, &QAction::triggered, this, &MainWindow::onNPStartHost);

                actNPStartClient = submenu->addAction("NETPLAY CLIENT");
                connect(actNPStartClient, &QAction::triggered, this, &MainWindow::onNPStartClient);

                actNPTest = submenu->addAction("NETPLAY GO");
                connect(actNPTest, &QAction::triggered, this, &MainWindow::onNPTest);*/
            }
        }
        {
            QMenu * menu = menubar->addMenu(tr("View"));

            {
                QMenu * submenu = menu->addMenu(tr("Screen size"));

                for (int i = 0; i < 4; i++)
                {
                    int data = i + 1;
                    actScreenSize[i] = submenu->addAction(QString("%1x").arg(data));
                    actScreenSize[i]->setData(QVariant(data));
                    connect(actScreenSize[i], &QAction::triggered, this, &MainWindow::onChangeScreenSize);
                }
            }
            {
                QMenu * submenu = menu->addMenu(tr("Screen rotation"));
                grpScreenRotation = new QActionGroup(submenu);

                for (int i = 0; i < screenRot_MAX; i++)
                {
                    int data = i * 90;
                    actScreenRotation[i] = submenu->addAction(QString("%1°").arg(data));
                    actScreenRotation[i]->setActionGroup(grpScreenRotation);
                    actScreenRotation[i]->setData(QVariant(i));
                    actScreenRotation[i]->setCheckable(true);
                }

                connect(grpScreenRotation, &QActionGroup::triggered, this, &MainWindow::onChangeScreenRotation);
            }
            {
                QMenu * submenu = menu->addMenu(tr("Screen gap"));
                grpScreenGap = new QActionGroup(submenu);

                const int screengap[] = {0, 1, 8, 64, 90, 128};

                for (int i = 0; i < 6; i++)
                {
                    int data = screengap[i];
                    actScreenGap[i] = submenu->addAction(QString("%1 px").arg(data));
                    actScreenGap[i]->setActionGroup(grpScreenGap);
                    actScreenGap[i]->setData(QVariant(data));
                    actScreenGap[i]->setCheckable(true);
                }

                submenu->addSeparator();

                actScreenGapCustom = submenu->addAction(tr("Custom..."));
                actScreenGapCustom->setActionGroup(grpScreenGap);
                actScreenGapCustom->setCheckable(true);

                connect(grpScreenGap, &QActionGroup::triggered, this, &MainWindow::onChangeScreenGap);
            }
            {
                QMenu * submenu = menu->addMenu(tr("Screen layout"));
                grpScreenLayout = new QActionGroup(submenu);

                const char *screenlayout[] = {QT_TR_NOOP("Natural"), QT_TR_NOOP("Vertical"), QT_TR_NOOP("Horizontal"), QT_TR_NOOP("Hybrid")};

                for (int i = 0; i < screenLayout_MAX; i++)
                {
                    actScreenLayout[i] = submenu->addAction(tr(screenlayout[i]));
                    actScreenLayout[i]->setActionGroup(grpScreenLayout);
                    actScreenLayout[i]->setData(QVariant(i));
                    actScreenLayout[i]->setCheckable(true);
                }

                connect(grpScreenLayout, &QActionGroup::triggered, this, &MainWindow::onChangeScreenLayout);

                submenu->addSeparator();

                actScreenSwap = submenu->addAction(tr("Swap screens"));
                actScreenSwap->setCheckable(true);
                connect(actScreenSwap, &QAction::triggered, this, &MainWindow::onChangeScreenSwap);
            }
            {
                QMenu * submenu = menu->addMenu(tr("Screen sizing"));
                grpScreenSizing = new QActionGroup(submenu);

                const char *screensizing[] = {QT_TR_NOOP("Even"), QT_TR_NOOP("Emphasize top"), QT_TR_NOOP("Emphasize bottom"),
                                              QT_TR_NOOP("Auto"), QT_TR_NOOP("Top only"), QT_TR_NOOP("Bottom only")};

                for (int i = 0; i < screenSizing_MAX; i++)
                {
                    actScreenSizing[i] = submenu->addAction(tr(screensizing[i]));
                    actScreenSizing[i]->setActionGroup(grpScreenSizing);
                    actScreenSizing[i]->setData(QVariant(i));
                    actScreenSizing[i]->setCheckable(true);
                }

                connect(grpScreenSizing, &QActionGroup::triggered, this, &MainWindow::onChangeScreenSizing);

                submenu->addSeparator();

                actIntegerScaling = submenu->addAction(tr("Force integer scaling"));
                actIntegerScaling->setCheckable(true);
                connect(actIntegerScaling, &QAction::triggered, this, &MainWindow::onChangeIntegerScaling);
            }
            {
                QMenu * submenu = menu->addMenu(tr("Aspect ratio"));
                grpScreenAspectTop = new QActionGroup(submenu);
                grpScreenAspectBot = new QActionGroup(submenu);
                actScreenAspectTop = new QAction *[AspectRatiosNum];
                actScreenAspectBot = new QAction *[AspectRatiosNum];

                for (int i = 0; i < 2; i++)
                {
                    QActionGroup * group = grpScreenAspectTop;
                    QAction **actions = actScreenAspectTop;

                    if (i == 1)
                    {
                        group = grpScreenAspectBot;
                        submenu->addSeparator();
                        actions = actScreenAspectBot;
                    }

                    for (int j = 0; j < AspectRatiosNum; j++)
                    {
                        auto ratio = aspectRatios[j];
                        QString label = (i ? tr("Bottom %1") : tr("Top %1")).arg(tr(ratio.label));
                        actions[j] = submenu->addAction(label);
                        actions[j]->setActionGroup(group);
                        actions[j]->setData(QVariant(ratio.id));
                        actions[j]->setCheckable(true);
                    }

                    connect(group, &QActionGroup::triggered, this, &MainWindow::onChangeScreenAspect);
                }
            }

            menu->addSeparator();

            actNewWindow = menu->addAction(tr("Open new window"));
            connect(actNewWindow, &QAction::triggered, this, &MainWindow::onOpenNewWindow);

            menu->addSeparator();

            actScreenFiltering = menu->addAction(tr("Screen filtering"));
            actScreenFiltering->setCheckable(true);
            connect(actScreenFiltering, &QAction::triggered, this, &MainWindow::onChangeScreenFiltering);

            actShowOSD = menu->addAction(tr("Show OSD"));
            actShowOSD->setCheckable(true);
            connect(actShowOSD, &QAction::triggered, this, &MainWindow::onChangeShowOSD);
        }
        {
            QMenu * menu = menubar->addMenu(tr("Config"));

            actEmuSettings = menu->addAction(tr("Emu settings"));
            connect(actEmuSettings, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);

#ifdef __APPLE__
            actPreferences = menu->addAction(tr("Preferences..."));
            connect(actPreferences, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);
            actPreferences->setMenuRole(QAction::PreferencesRole);
#endif

            actInputConfig = menu->addAction(tr("Input and hotkeys"));
            connect(actInputConfig, &QAction::triggered, this, &MainWindow::onOpenInputConfig);

            actVideoSettings = menu->addAction(tr("Video settings"));
            connect(actVideoSettings, &QAction::triggered, this, &MainWindow::onOpenVideoSettings);

            actCameraSettings = menu->addAction(tr("Camera settings"));
            connect(actCameraSettings, &QAction::triggered, this, &MainWindow::onOpenCameraSettings);

            actAudioSettings = menu->addAction(tr("Audio settings"));
            connect(actAudioSettings, &QAction::triggered, this, &MainWindow::onOpenAudioSettings);

            actMPSettings = menu->addAction(tr("Multiplayer settings"));
            connect(actMPSettings, &QAction::triggered, this, &MainWindow::onOpenMPSettings);

            actWifiSettings = menu->addAction(tr("Wifi settings"));
            connect(actWifiSettings, &QAction::triggered, this, &MainWindow::onOpenWifiSettings);

            actFirmwareSettings = menu->addAction(tr("Firmware settings"));
            connect(actFirmwareSettings, &QAction::triggered, this, &MainWindow::onOpenFirmwareSettings);

            actInterfaceSettings = menu->addAction(tr("Interface settings"));
            connect(actInterfaceSettings, &QAction::triggered, this, &MainWindow::onOpenInterfaceSettings);

            actPathSettings = menu->addAction(tr("Path settings"));
            connect(actPathSettings, &QAction::triggered, this, &MainWindow::onOpenPathSettings);

            menu->addSeparator();

            actLimitFramerate = menu->addAction(tr("Limit framerate"));
            actLimitFramerate->setCheckable(true);
            connect(actLimitFramerate, &QAction::triggered, this, &MainWindow::onChangeLimitFramerate);

            actAudioSync = menu->addAction(tr("Audio sync"));
            actAudioSync->setCheckable(true);
            connect(actAudioSync, &QAction::triggered, this, &MainWindow::onChangeAudioSync);
        }
        {
            // Top-level so it is easy to find whatever the current language is.
            // Language names are shown in their own language and never translated.
            languageMenu = menubar->addMenu(tr("Language"));
            languageMenu->menuAction()->setProperty("melonNoTr", true); // see retranslateMenus()
            grpLanguage = new QActionGroup(languageMenu);

            actLanguageAuto = languageMenu->addAction(tr("Automatic (system language)"));
            actLanguageAuto->setData(QString("auto"));
            actLanguageAuto->setCheckable(true);
            actLanguageAuto->setActionGroup(grpLanguage);

            languageMenu->addSeparator();

            for (int i = 0; i < uiLanguagesNum; i++)
            {
                QAction* act = languageMenu->addAction(QString::fromUtf8(uiLanguages[i].nativeName));
                act->setData(QString(uiLanguages[i].code));
                act->setCheckable(true);
                act->setActionGroup(grpLanguage);
                act->setProperty("melonNoTr", true);
                actLanguage.append(act);
            }

            connect(grpLanguage, &QActionGroup::triggered, this, [](QAction* act)
            {
                setUILanguage(act->data().toString());
            });
        }
        {
            QMenu * menu = menubar->addMenu(tr("Help"));
            actAbout = menu->addAction(tr("About..."));
            connect(actAbout, &QAction::triggered, this, [&]
            {
                auto dialog = AboutDialog(this);
                dialog.exec();
            });
        }

        setMenuBar(menubar);

        // Remember the source text of every fixed menu entry. Entries whose
        // text is built at runtime are refreshed separately in retranslateMenus().
        actCurrentCart->setProperty("melonNoTr", true);
        actCurrentGBACart->setProperty("melonNoTr", true);
        for (QAction* act : actInsertGBAAddon)
            act->setProperty("melonNoTr", true);
        for (int i = 0; i < AspectRatiosNum; i++)
        {
            actScreenAspectTop[i]->setProperty("melonNoTr", true);
            actScreenAspectBot[i]->setProperty("melonNoTr", true);
        }
        auto storeSourceText = [](QAction* act)
        {
            if (act->isSeparator() || act->property("melonNoTr").toBool()
                || act->property("melonTrSource").isValid())
                return;
            act->setProperty("melonTrSource", act->text());
        };
        for (QMenu* m : menubar->findChildren<QMenu*>())
        {
            storeSourceText(m->menuAction());
            if (m == recentMenu) continue; // rebuilt by loadRecentFilesMenu()
            for (QAction* act : m->actions())
                storeSourceText(act);
        }

        retranslateMenus();
        updateLanguageMenu();
    }

#ifdef Q_OS_MAC
    QPoint screenCenter = screen()->availableGeometry().center();
    QRect frameGeo = frameGeometry();
    frameGeo.moveCenter(screenCenter);
    move(frameGeo.topLeft());
#endif

    std::string geom = windowCfg.GetString("Geometry");
    if (!geom.empty())
    {
        QByteArray raw = QByteArray::fromStdString(geom);
        QByteArray dec = QByteArray::fromBase64(raw, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!dec.isEmpty())
            restoreGeometry(dec);
        // if the window was closed in fullscreen do not restore this
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
    show();

    panel = nullptr;
    createScreenPanel();

    if (hasMenu)
    {
        actEjectCart->setEnabled(false);
        actEjectGBACart->setEnabled(false);

        if (globalCfg.GetInt("Emu.ConsoleType") == 1)
        {
            actInsertGBACart->setEnabled(false);
            for (auto act: actInsertGBAAddon)
                act->setEnabled(false);
        }

        for (int i = 0; i < 9; i++)
        {
            actSaveState[i]->setEnabled(false);
            actLoadState[i]->setEnabled(false);
        }
        actUndoStateLoad->setEnabled(false);
        actImportSavefile->setEnabled(false);

        actPause->setEnabled(false);
        actReset->setEnabled(false);
        actStop->setEnabled(false);
        actFrameStep->setEnabled(false);

        //actDateTime->setEnabled(true);
        actPowerManagement->setEnabled(false);

        actEnableCheats->setEnabled(false);
        actSetupCheats->setEnabled(false);
        actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());

        actEnableCheats->setChecked(localCfg.GetBool("EnableCheats"));

        actROMInfo->setEnabled(false);
        actRAMInfo->setEnabled(false);

        actScreenRotation[windowCfg.GetInt("ScreenRotation")]->setChecked(true);

        updateScreenGapMenu();

        actScreenLayout[windowCfg.GetInt("ScreenLayout")]->setChecked(true);
        actScreenSizing[windowCfg.GetInt("ScreenSizing")]->setChecked(true);
        actIntegerScaling->setChecked(windowCfg.GetBool("IntegerScaling"));

        actScreenSwap->setChecked(windowCfg.GetBool("ScreenSwap"));

        int aspectTop = windowCfg.GetInt("ScreenAspectTop");
        int aspectBot = windowCfg.GetInt("ScreenAspectBot");
        for (int i = 0; i < AspectRatiosNum; i++)
        {
            if (aspectTop == aspectRatios[i].id)
                actScreenAspectTop[i]->setChecked(true);
            if (aspectBot == aspectRatios[i].id)
                actScreenAspectBot[i]->setChecked(true);
        }

        actScreenFiltering->setChecked(windowCfg.GetBool("ScreenFilter"));
        actShowOSD->setChecked(showOSD);

        actLimitFramerate->setChecked(emuInstance->doLimitFPS);
        actAudioSync->setChecked(emuInstance->doAudioSync);

        if (emuInstance->instanceID > 0)
        {
            actEmuSettings->setEnabled(false);
            actVideoSettings->setEnabled(false);
            actMPSettings->setEnabled(false);
            actWifiSettings->setEnabled(false);
            actInterfaceSettings->setEnabled(false);

#ifdef __APPLE__
            actPreferences->setEnabled(false);
#endif // __APPLE__
        }

        if (emuThread->emuIsActive())
            onEmuStart();
    }

    QObject::connect(qApp, &QApplication::applicationStateChanged, this, &MainWindow::onAppStateChanged);
    onUpdateInterfaceSettings();

    updateMPInterface(MPInterface::GetType());
}

MainWindow::~MainWindow()
{
    if (hasMenu)
    {
        delete[] actScreenAspectTop;
        delete[] actScreenAspectBot;
    }
}

void MainWindow::osdAddMessage(unsigned int color, const char* msg)
{
    if (!showOSD) return;
    panel->osdAddMessage(color, msg);
}

void MainWindow::saveEnabled(bool enabled)
{
    if (enabledSaved) return;
    windowCfg.SetBool("Enabled", enabled);
    enabledSaved = true;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (emuInstance)
    {
        if (windowID == 0)
            emuInstance->saveEnabledWindows();
        else
            saveEnabled(false);
    }

    // explicitly close children windows, so the OpenGL contexts get closed properly
    auto childwins = findChildren<MainWindow *>(nullptr, Qt::FindDirectChildrenOnly);
    for (auto child : childwins)
        child->close();

    if (!emuInstance) return;

    QByteArray geom = saveGeometry();
    QByteArray enc = geom.toBase64(QByteArray::Base64Encoding);
    windowCfg.SetString("Geometry", enc.toStdString());
    Config::Save();

    emuInstance->deleteWindow(windowID, false);

    // emuInstance may be deleted
    // prevent use after free from us
    emuInstance = nullptr;
    QMainWindow::closeEvent(event);
}

void MainWindow::createScreenPanel()
{
    auto oldpanel = panel;
    panel = nullptr;
    if (oldpanel) delete oldpanel;

    hasOGL = globalCfg.GetBool("Screen.UseGL") ||
            (globalCfg.GetInt("3D.Renderer") != renderer3D_Software);

    if (hasOGL)
    {
        ScreenPanelGL* panelGL = new ScreenPanelGL(this);
        panelGL->show();

        // make sure no GL context is in use by the emu thread
        // otherwise we may fail to create a shared context
        if (windowID != 0)
            emuThread->borrowGL();

        // Check that creating the context hasn't failed
        if (panelGL->createContext() == false)
        {
            Log(Platform::LogLevel::Error, "Failed to create OpenGL context, falling back to Software Renderer.\n");
            hasOGL = false;

            globalCfg.SetBool("Screen.UseGL", false);
            globalCfg.SetInt("3D.Renderer", renderer3D_Software);

            delete panelGL;
            panelGL = nullptr;
        }

        if (windowID != 0)
            emuThread->returnGL();

        panel = panelGL;
    }

    if (!hasOGL)
    {
        ScreenPanelNative* panelNative = new ScreenPanelNative(this);
        panel = panelNative;
        panel->show();
    }
    setCentralWidget(panel);

    if (hasMenu)
        actScreenFiltering->setEnabled(hasOGL);
    panel->osdSetEnabled(showOSD);

    connect(emuThread, SIGNAL(windowUpdate()), panel, SLOT(repaint()));

    connect(this, SIGNAL(screenLayoutChange()), panel, SLOT(onScreenLayoutChanged()));
    emit screenLayoutChange();
}

GL::Context* MainWindow::getOGLContext()
{
    if (!hasOGL) return nullptr;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->getContext();
}

void MainWindow::initOpenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->initOpenGL();
}

void MainWindow::deinitOpenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->deinitOpenGL();
}

void MainWindow::setGLSwapInterval(int intv)
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    if (!glpanel) return;
    return glpanel->setSwapInterval(intv);
}

void MainWindow::makeCurrentGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    if (!glpanel) return;
    return glpanel->makeCurrentGL();
}

void MainWindow::releaseGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    if (!glpanel) return;
    return glpanel->releaseGL();
}

void MainWindow::drawScreen()
{
    if (!panel) return;
    return panel->drawScreen();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    // TODO!! REMOVE ME IN RELEASE BUILDS!!
    //if (event->key() == Qt::Key_F11) emuInstance->getNDS()->debug(0);

    emuInstance->onKeyPress(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    emuInstance->onKeyRelease(event);
}


void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.count() > 1) return; // not handling more than one file at once

    QString filename = urls.at(0).toLocalFile();

    if (FileIsSupportedFiletype(filename))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.count() > 1) return; // not handling more than one file at once

    if (!verifySetup())
        return;

    const QStringList file = splitArchivePath(urls.at(0).toLocalFile(), false);
    if (file.isEmpty())
        return;

    const QString filename = file.last();
    const bool romInsideArchive = file.size() > 1;
    const auto matchMode = romInsideArchive ? QMimeDatabase::MatchExtension : QMimeDatabase::MatchDefault;
    const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, matchMode);

    bool isNdsRom = NdsRomByExtension(filename) || NdsRomByMimetype(mimetype);
    bool isGbaRom = GbaRomByExtension(filename) || GbaRomByMimetype(mimetype);
    isNdsRom |= ZstdNdsRomByExtension(filename);
    isGbaRom |= ZstdGbaRomByExtension(filename);

    QString errorstr;
    if (isNdsRom)
    {
        if (!emuThread->bootROM(file, errorstr))
        {
            QMessageBox::critical(this, "melonDS", errorstr);
            return;
        }

        const QString barredFilename = file.join('|');
        recentFileList.removeAll(barredFilename);
        recentFileList.prepend(barredFilename);
        updateRecentFilesMenu();

        updateCartInserted(false);
    }
    else if (isGbaRom)
    {
        if (!emuThread->insertCart(file, true, errorstr))
        {
            QMessageBox::critical(this, "melonDS", errorstr);
            return;
        }

        updateCartInserted(true);
    }
    else
    {
        QMessageBox::critical(this, "melonDS", tr("The file could not be recognized as a DS or GBA ROM."));
        return;
    }
}

void MainWindow::focusInEvent(QFocusEvent* event)
{
    onFocusIn();
}

void MainWindow::focusOutEvent(QFocusEvent* event)
{
    onFocusOut();
}

void MainWindow::onFocusIn()
{
    focused = true;
    if (emuInstance)
        emuInstance->updateAudioMuteByWindowFocus();
}

void MainWindow::onFocusOut()
{
    // focusOutEvent is called through the window close event handler
    // prevent use after free
    focused = false;
    if (emuInstance)
        emuInstance->updateAudioMuteByWindowFocus();
}

void MainWindow::onAppStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationInactive)
    {
        emuInstance->keyReleaseAll();
        if (pauseOnLostFocus && emuThread->emuIsRunning())
            emuThread->emuPause();
    }
    else if (state == Qt::ApplicationActive)
    {
        if (pauseOnLostFocus && !pausedManually)
            emuThread->emuUnpause();
    }
}

bool MainWindow::verifySetup()
{
    QString res = emuInstance->verifySetup();
    if (!res.isEmpty())
    {
         QMessageBox::critical(this, "melonDS", res);
         return false;
    }

    return true;
}

bool MainWindow::preloadROMs(QStringList file, QStringList gbafile, bool boot)
{
    QString errorstr;

    if (file.isEmpty() && gbafile.isEmpty() && !boot)
        return false;

    if (!verifySetup())
    {
        return false;
    }

    bool gbaloaded = false;
    if (!gbafile.isEmpty())
    {
        if (!emuThread->insertCart(gbafile, true, errorstr))
        {
            QMessageBox::critical(this, "melonDS", errorstr);
            return false;
        }

        gbaloaded = true;
    }

    bool ndsloaded = false;
    if (!file.isEmpty())
    {
        if (boot)
        {
            if (!emuThread->bootROM(file, errorstr))
            {
                QMessageBox::critical(this, "melonDS", errorstr);
                return false;
            }
        }
        else
        {
            if (!emuThread->insertCart(file, false, errorstr))
            {
                QMessageBox::critical(this, "melonDS", errorstr);
                return false;
            }
        }

        recentFileList.removeAll(file.join("|"));
        recentFileList.prepend(file.join("|"));
        updateRecentFilesMenu();
        ndsloaded = true;
    }
    else if (boot)
    {
        if (!emuThread->bootFirmware(errorstr))
        {
            QMessageBox::critical(this, "melonDS", errorstr);
            return false;
        }
    }

    updateCartInserted(false);
    if (gbaloaded)
        updateCartInserted(true);

    return true;
}

QStringList MainWindow::splitArchivePath(const QString& filename, bool useMemberSyntax)
{
    if (filename.isEmpty()) return {};

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (useMemberSyntax)
    {
        const QStringList filenameParts = filename.split('|');
        if (filenameParts.size() > 2)
        {
            QMessageBox::warning(this, "melonDS", tr("This path contains too many '|'."));
            return {};
        }

        if (filenameParts.size() == 2)
        {
            const QString archive = filenameParts.at(0);
            if (!QFileInfo(archive).exists())
            {
                QMessageBox::warning(this, "melonDS", tr("This archive does not exist."));
                return {};
            }

            const QString subfile = filenameParts.at(1);
            if (!Archive::ListArchive(archive).contains(subfile))
            {
                QMessageBox::warning(this, "melonDS", tr("This archive does not contain the desired file."));
                return {};
            }

            return filenameParts;
        }
    }
#endif

    if (!QFileInfo(filename).exists())
    {
        QMessageBox::warning(this, "melonDS", tr("This ROM file does not exist."));
        return {};
    }

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (SupportedArchiveByExtension(filename)
        || SupportedArchiveByMimetype(QMimeDatabase().mimeTypeForFile(filename)))
    {
        const QString subfile = pickFileFromArchive(filename);
        if (subfile.isEmpty())
            return {};

        return { filename, subfile };
    }
#endif

    return { filename };
}

QString MainWindow::pickFileFromArchive(QString archiveFileName)
{
    QVector<QString> archiveROMList = Archive::ListArchive(archiveFileName);

    if (archiveROMList.size() <= 1)
    {
        if (!archiveROMList.isEmpty() && archiveROMList.at(0) == "OK")
            QMessageBox::warning(this, "melonDS", tr("This archive is empty."));
        else
            QMessageBox::critical(this, "melonDS", tr("This archive could not be read. It may be corrupt or you don't have the permissions."));
        return QString();
    }

    archiveROMList.removeFirst();

    const auto notSupportedRom = [&](const auto& filename){
        if (NdsRomByExtension(filename) || GbaRomByExtension(filename))
            return false;
        const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, QMimeDatabase::MatchExtension);
        return !(NdsRomByMimetype(mimetype) || GbaRomByMimetype(mimetype));
    };

    archiveROMList.erase(std::remove_if(archiveROMList.begin(), archiveROMList.end(), notSupportedRom),
                         archiveROMList.end());

    if (archiveROMList.isEmpty())
    {
        QMessageBox::warning(this, "melonDS", tr("This archive does not contain any supported ROMs."));
        return QString();
    }

    if (archiveROMList.size() == 1)
        return archiveROMList.first();

    bool ok;
    const QString toLoad = QInputDialog::getItem(
        this, "melonDS",
        tr("This archive contains multiple files. Select which ROM you want to load."),
        archiveROMList.toList(), 0, false, &ok
    );

    if (ok) return toLoad;

    // User clicked on cancel

    return QString();
}

QStringList MainWindow::pickROM(bool gba)
{
    emuThread->emuPause();

    const QString console = gba ? "GBA" : "DS";
    const QStringList& romexts = gba ? GbaRomExtensions : NdsRomExtensions;

    QString rawROMs = romexts.join(" *");
    QString extraFilters = ";;" + tr("%1 ROMs").arg(console) + " (*" + rawROMs;
    QString allROMs = rawROMs;

    QString zstdROMs = "*" + romexts.join(".zst *") + ".zst";
    extraFilters += ");;" + tr("Zstandard-compressed %1 ROMs").arg(console) + " (" + zstdROMs + ")";
    allROMs += " " + zstdROMs;

#ifdef ARCHIVE_SUPPORT_ENABLED
    QString archives = "*" + ArchiveExtensions.join(" *");
    extraFilters += ";;" + tr("Archives") + " (" + archives + ")";
    allROMs += " " + archives;
#endif
    extraFilters += ";;" + tr("All files") + " (*.*)";

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open %1 ROM").arg(console),
        globalCfg.GetQString("LastROMFolder"),
        tr("All supported files") + " (*" + allROMs + ")" + extraFilters
    );

    if (filename.isEmpty())
    {
        emuThread->emuUnpause();
        return {};
    }

    globalCfg.SetQString("LastROMFolder", QFileInfo(filename).dir().path());
    auto ret = splitArchivePath(filename, false);
    emuThread->emuUnpause();
    return ret;
}

void MainWindow::updateCartInserted(bool gba)
{
    bool inserted;
    QString label;
    if (gba)
    {
        inserted = emuInstance->gbaCartInserted() && (emuInstance->getConsoleType() == 0);
        label = tr("GBA slot: %1").arg(emuInstance->gbaCartLabel());

        emuInstance->doOnAllWindows([=](MainWindow* win)
        {
            if (!win->hasMenu) return;
            win->actCurrentGBACart->setText(label);
            win->actEjectGBACart->setEnabled(inserted);
        });
    }
    else
    {
        inserted = emuInstance->cartInserted();
        label = tr("DS slot: %1").arg(emuInstance->cartLabel());

        emuInstance->doOnAllWindows([=](MainWindow* win)
        {
            if (!win->hasMenu) return;
            win->actCurrentCart->setText(label);
            win->actEjectCart->setEnabled(inserted);
            win->actImportSavefile->setEnabled(inserted);
            win->actEnableCheats->setEnabled(inserted);
            win->actSetupCheats->setEnabled(inserted);
            win->actROMInfo->setEnabled(inserted);
            win->actRAMInfo->setEnabled(inserted);
        });
    }
}

void MainWindow::onOpenFile()
{
    if (!verifySetup())
        return;

    QStringList file = pickROM(false);
    if (file.isEmpty())
        return;

    QString errorstr;
    if (!emuThread->bootROM(file, errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }

    QString filename = file.join('|');
    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    updateCartInserted(false);
}

void MainWindow::onClearRecentFiles()
{
    recentFileList.clear();
    globalCfg.GetArray("RecentROM").Clear();
    updateRecentFilesMenu();
}

void MainWindow::loadRecentFilesMenu(bool loadcfg)
{
    if (loadcfg)
    {
        recentFileList.clear();

        Config::Array recentROMs = globalCfg.GetArray("RecentROM");
        int numrecent = std::min(kMaxRecentROMs, (int) recentROMs.Size());
        for (int i = 0; i < numrecent; ++i)
        {
            std::string item = recentROMs.GetString(i);
            if (!item.empty())
                recentFileList.push_back(QString::fromStdString(item));
        }
    }

    recentMenu->clear();

    for (int i = 0; i < recentFileList.size(); ++i)
    {
        if (i >= kMaxRecentROMs) break;

        QString item_full = recentFileList.at(i);
        QString item_display = item_full;
        int itemlen = item_full.length();
        const int maxlen = 100;
        if (itemlen > maxlen)
        {
            int cut_start = 0;
            while (item_full[cut_start] != '/' && item_full[cut_start] != '\\' &&
                   cut_start < itemlen)
                cut_start++;

            int cut_end = itemlen-1;
            while (((item_full[cut_end] != '/' && item_full[cut_end] != '\\') ||
                    (cut_start+4+(itemlen-cut_end) < maxlen)) &&
                   cut_end > 0)
                cut_end--;

            item_display.truncate(cut_start+1);
            item_display += "...";
            item_display += QString(item_full).remove(0, cut_end);
        }

        QAction *actRecentFile_i = recentMenu->addAction(QString("%1.  %2").arg(i+1).arg(item_display));
        actRecentFile_i->setData(item_full);
        connect(actRecentFile_i, &QAction::triggered, this, &MainWindow::onClickRecentFile);
    }

    while (recentFileList.size() > 10)
        recentFileList.removeLast();

    recentMenu->addSeparator();

    QAction *actClearRecentList = recentMenu->addAction(tr("Clear"));
    connect(actClearRecentList, &QAction::triggered, this, &MainWindow::onClearRecentFiles);

    if (recentFileList.empty())
        actClearRecentList->setEnabled(false);
}

void MainWindow::updateRecentFilesMenu()
{
    Config::Array recentroms = globalCfg.GetArray("RecentROM");
    recentroms.Clear();

    for (int i = 0; i < recentFileList.size(); ++i)
    {
        if (i >= kMaxRecentROMs) break;

        recentroms.SetQString(i, recentFileList.at(i));
    }

    Config::Save();
    loadRecentFilesMenu(false);

    emuInstance->broadcastCommand(InstCmd_UpdateRecentFiles);
}

void MainWindow::onClickRecentFile()
{
    QAction *act = (QAction *)sender();
    QString filename = act->data().toString();

    if (!verifySetup())
        return;

    const QStringList file = splitArchivePath(filename, true);
    if (file.isEmpty())
        return;

    QString errorstr;
    if (!emuThread->bootROM(file, errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }

    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    updateCartInserted(false);
}

void MainWindow::onBootFirmware()
{
    if (!verifySetup())
        return;

    QString errorstr;
    if (!emuThread->bootFirmware(errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }
}

void MainWindow::onInsertCart()
{
    QStringList file = pickROM(false);
    if (file.isEmpty())
        return;

    QString errorstr;
    if (!emuThread->insertCart(file, false, errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }

    updateCartInserted(false);
}

void MainWindow::onEjectCart()
{
    emuThread->ejectCart(false);
    updateCartInserted(false);
}

void MainWindow::onInsertGBACart()
{
    QStringList file = pickROM(true);
    if (file.isEmpty())
        return;

    QString errorstr;
    if (!emuThread->insertCart(file, true, errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }

    updateCartInserted(true);
}

void MainWindow::onInsertGBAAddon()
{
    QAction* act = (QAction*)sender();
    int type = act->data().toInt();

    QString errorstr;
    if (!emuThread->insertGBAAddon(type, errorstr))
    {
        QMessageBox::critical(this, "melonDS", errorstr);
        return;
    }

    updateCartInserted(true);
}

void MainWindow::onEjectGBACart()
{
    emuThread->ejectCart(true);
    updateCartInserted(true);
}

void MainWindow::onSaveState()
{
    int slot = ((QAction*)sender())->data().toInt();

    QString filename;
    if (slot > 0)
    {
        filename = QString::fromStdString(emuInstance->getSavestateName(slot));
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        emuThread->emuPause();
        filename = QFileDialog::getSaveFileName(this,
                                                         tr("Save state"),
                                                         globalCfg.GetQString("LastROMFolder"),
                                                         tr("melonDS savestates") + " (*.mln);;" + tr("Any file") + " (*.*)");
        emuThread->emuUnpause();
        if (filename.isEmpty())
            return;
    }

    if (emuThread->saveState(filename))
    {
        if (slot > 0) emuInstance->osdAddMessage(0, QT_TRANSLATE_NOOP("OSD", "State saved to slot %d"), slot);
        else          emuInstance->osdAddMessage(0, QT_TRANSLATE_NOOP("OSD", "State saved to file"));

        actLoadState[slot]->setEnabled(true);
    }
    else
    {
        emuInstance->osdAddMessage(0xFFA0A0, QT_TRANSLATE_NOOP("OSD", "State save failed"));
    }
}

void MainWindow::onLoadState()
{
    int slot = ((QAction*)sender())->data().toInt();

    QString filename;
    if (slot > 0)
    {
        filename = QString::fromStdString(emuInstance->getSavestateName(slot));
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        emuThread->emuPause();
        filename = QFileDialog::getOpenFileName(this,
                                                         tr("Load state"),
                                                         globalCfg.GetQString("LastROMFolder"),
                                                         tr("melonDS savestates") + " (*.ml*);;" + tr("Any file") + " (*.*)");
        emuThread->emuUnpause();
        if (filename.isEmpty())
            return;
    }

    if (!Platform::FileExists(filename.toStdString()))
    {
        if (slot > 0) emuInstance->osdAddMessage(0xFFA0A0, QT_TRANSLATE_NOOP("OSD", "State slot %d is empty"), slot);
        else          emuInstance->osdAddMessage(0xFFA0A0, QT_TRANSLATE_NOOP("OSD", "State file does not exist"));

        return;
    }

    if (emuThread->loadState(filename))
    {
        if (slot > 0) emuInstance->osdAddMessage(0, QT_TRANSLATE_NOOP("OSD", "State loaded from slot %d"), slot);
        else          emuInstance->osdAddMessage(0, QT_TRANSLATE_NOOP("OSD", "State loaded from file"));

        actUndoStateLoad->setEnabled(true);
    }
    else
    {
        emuInstance->osdAddMessage(0xFFA0A0, QT_TRANSLATE_NOOP("OSD", "State load failed"));
    }
}

void MainWindow::onUndoStateLoad()
{
    emuThread->undoStateLoad();

    emuInstance->osdAddMessage(0, QT_TRANSLATE_NOOP("OSD", "State load undone"));
}

void MainWindow::onImportSavefile()
{
    QString path = QFileDialog::getOpenFileName(this,
                                            tr("Select savefile"),
                                            globalCfg.GetQString("LastROMFolder"),
                                            tr("Savefiles") + " (*.sav *.srm *.bin *.dsv);;" + tr("Any file") + " (*.*)");

    if (path.isEmpty())
        return;

    if (!Platform::FileExists(path.toStdString()))
    {
        QMessageBox::critical(this, "melonDS", tr("Could not open the given savefile."));
        return;
    }

    if (emuThread->emuIsActive())
    {
        if (QMessageBox::warning(this,
                        "melonDS",
                        tr("The emulation will be reset and the current savefile overwritten."),
                        QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
        {
            return;
        }
    }

    if (!emuThread->importSavefile(path))
    {
        QMessageBox::critical(this, "melonDS", tr("Could not import the given savefile."));
        return;
    }
}

void MainWindow::retranslateMenus()
{
    if (!hasMenu) return;

    auto retranslate = [this](QAction* act)
    {
        const QVariant src = act->property("melonTrSource");
        if (src.isValid())
            act->setText(tr(src.toString().toUtf8().constData()));
    };
    for (QMenu* m : menuBar()->findChildren<QMenu*>())
    {
        retranslate(m->menuAction());
        if (m == recentMenu) continue;
        for (QAction* act : m->actions())
            retranslate(act);
    }

    // entries whose text is built at runtime

    // Keep the English word in the Language menu title so it can be found
    // even when the current UI language cannot be read.
    {
        const QString title = tr("Language");
        if (title.contains("Language", Qt::CaseInsensitive))
            languageMenu->setTitle(title);
        else
            languageMenu->setTitle(title + " / Language");
    }

    if (emuInstance)
    {
        actCurrentCart->setText(tr("DS slot: %1").arg(emuInstance->cartLabel()));
        actCurrentGBACart->setText(tr("GBA slot: %1").arg(emuInstance->gbaCartLabel()));
        for (QAction* act : actInsertGBAAddon)
            act->setText(emuInstance->gbaAddonName(act->data().toInt()));

        if (localCfg.GetString("Firmware.Username") == "Arisotura")
            actMPNewInstance->setText("Fart");
    }

    for (int i = 0; i < AspectRatiosNum; i++)
    {
        const QString label = tr(aspectRatios[i].label);
        actScreenAspectTop[i]->setText(tr("Top %1").arg(label));
        actScreenAspectBot[i]->setText(tr("Bottom %1").arg(label));
    }

    loadRecentFilesMenu(false);
}

void MainWindow::updateLanguageMenu()
{
    if (!hasMenu) return;

    const QString setting = uiLanguageSetting();
    actLanguageAuto->setChecked(setting == "auto");
    for (QAction* act : actLanguage)
        act->setChecked(act->data().toString() == setting);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateMenus();
        updateLanguageMenu();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::onQuit()
{
    close();
}


void MainWindow::onPause(bool checked)
{
    if (!emuThread->emuIsActive()) return;

    if (checked)
    {
        emuThread->emuPause();
        pausedManually = true;
    }
    else
    {
        emuThread->emuUnpause();
        pausedManually = false;
    }
}

bool MainWindow::confirmEmuAction(const QString& question)
{
    if (!globalCfg.GetBool("ConfirmResetStop")) return true;

    // a hotkey can queue another request while the box is open
    if (confirmPending) return false;
    confirmPending = true;

    emuThread->emuPause();
    bool ok = QMessageBox::question(this, "melonDS", question,
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No) == QMessageBox::Yes;
    emuThread->emuUnpause();

    confirmPending = false;
    return ok && emuThread->emuIsActive();
}

void MainWindow::requestReset()
{
    if (!emuThread->emuIsActive()) return;
    if (!confirmEmuAction(tr("Are you sure you want to reset the emulation?"))) return;

    emuThread->emuReset();
}

void MainWindow::onReset()
{
    if (!emuThread->emuIsActive()) return;

    emuThread->emuReset();
}

void MainWindow::onStop()
{
    if (!emuThread->emuIsActive()) return;
    if (!confirmEmuAction(tr("Are you sure you want to stop the emulation?"))) return;

    emuThread->emuStop(true);
}

void MainWindow::onFrameStep()
{
    if (!emuThread->emuIsActive()) return;

    emuThread->emuFrameStep();
}

void MainWindow::onOpenDateTime()
{
    DateTimeDialog* dlg = DateTimeDialog::openDlg(this);
    connect(dlg, &DateTimeDialog::finished, this, &MainWindow::onDateTimeDialogFinished);
}

void MainWindow::onDateTimeDialogFinished(int res)
{
    if (!res) return;
    if (!emuThread->emuIsActive()) return;

    emuInstance->setDateTime();
}

void MainWindow::onOpenPowerManagement()
{
    PowerManagementDialog* dlg = PowerManagementDialog::openDlg(this);
}

void MainWindow::onEnableCheats(bool checked)
{
    localCfg.SetBool("EnableCheats", checked);
    emuThread->enableCheats(checked);

    emuInstance->doOnAllWindows([=](MainWindow* win)
    {
        win->actEnableCheats->setChecked(checked);
    }, windowID);
}

void MainWindow::onSetupCheats()
{
    emuThread->emuPause();

    CheatsDialog* dlg = CheatsDialog::openDlg(this);
    connect(dlg, &CheatsDialog::finished, this, &MainWindow::onCheatsDialogFinished);
}

void MainWindow::onCheatsDialogFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onROMInfo()
{
    auto cart = emuInstance->nds->NDSCartSlot.GetCart();
    if (cart)
        ROMInfoDialog* dlg = ROMInfoDialog::openDlg(this);
}

void MainWindow::onRAMInfo()
{
    RAMInfoDialog* dlg = RAMInfoDialog::openDlg(this);
}

void MainWindow::onOpenTitleManager()
{
    TitleManagerDialog* dlg = TitleManagerDialog::openDlg(this);
}

void MainWindow::onMPNewInstance()
{
    createEmuInstance();
}

void MainWindow::onLANStartHost()
{
    if (!lanWarning(true)) return;
    LANStartHostDialog::openDlg(this);
}

void MainWindow::onLANStartClient()
{
    if (!lanWarning(false)) return;
    LANStartClientDialog::openDlg(this);
}

void MainWindow::onNPStartHost()
{
    //Netplay::StartHost();
    //NetplayStartHostDialog::openDlg(this);
}

void MainWindow::onNPStartClient()
{
    //Netplay::StartClient();
    //NetplayStartClientDialog::openDlg(this);
}

void MainWindow::onNPTest()
{
    // HAX
    //Netplay::StartGame();
}

void MainWindow::updateMPInterface(MPInterfaceType type)
{
    if (!hasMenu) return;

    // MP interface was changed, reflect it in the UI

    bool enable = (type == MPInterface_Local);
    actMPNewInstance->setEnabled(enable);
    actLANStartHost->setEnabled(enable);
    actLANStartClient->setEnabled(enable);
    /*actNPStartHost->setEnabled(enable);
    actNPStartClient->setEnabled(enable);
    actNPTest->setEnabled(enable);*/
}

bool MainWindow::lanWarning(bool host)
{
    if (numEmuInstances() < 2)
        return true;

    QString msg = host
        ? tr("Multiple emulator instances are currently open.\n"
             "If you host a LAN game now, all secondary instances will be closed.\n\n"
             "Do you wish to continue?")
        : tr("Multiple emulator instances are currently open.\n"
             "If you join a LAN game now, all secondary instances will be closed.\n\n"
             "Do you wish to continue?");

    auto res = QMessageBox::warning(this, "melonDS", msg, QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::No)
        return false;

    deleteAllEmuInstances(1);
    return true;
}

void MainWindow::onOpenEmuSettings()
{
    emuThread->emuPause();

    EmuSettingsDialog* dlg = EmuSettingsDialog::openDlg(this);
    connect(dlg, &EmuSettingsDialog::finished, this, &MainWindow::onEmuSettingsDialogFinished);
}

void MainWindow::onEmuSettingsDialogFinished(int res)
{
    if (globalCfg.GetInt("Emu.ConsoleType") == 1)
    {
        actInsertGBACart->setEnabled(false);
        for (auto act : actInsertGBAAddon)
            act->setEnabled(false);
        actEjectGBACart->setEnabled(false);
    }
    else
    {
        actInsertGBACart->setEnabled(true);
        for (auto act : actInsertGBAAddon)
            act->setEnabled(true);
        actEjectGBACart->setEnabled(emuInstance->gbaCartInserted());
    }

    if (EmuSettingsDialog::needsReset)
        onReset();

    actCurrentGBACart->setText(tr("GBA slot: %1").arg(emuInstance->gbaCartLabel()));

    if (!emuThread->emuIsActive())
        actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());

    emuThread->emuUnpause();
}

void MainWindow::onOpenInputConfig()
{
    emuThread->emuPause();

    InputConfigDialog* dlg = InputConfigDialog::openDlg(this);
    connect(dlg, &InputConfigDialog::finished, this, &MainWindow::onInputConfigFinished);
}

void MainWindow::onInputConfigFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onOpenVideoSettings()
{
    VideoSettingsDialog* dlg = VideoSettingsDialog::openDlg(this);
    connect(dlg, &VideoSettingsDialog::updateVideoSettings, this, &MainWindow::onUpdateVideoSettings);
}

void MainWindow::onOpenCameraSettings()
{
    emuThread->emuPause();

    camStarted[0] = camManager[0]->isStarted();
    camStarted[1] = camManager[1]->isStarted();
    if (camStarted[0]) camManager[0]->stop();
    if (camStarted[1]) camManager[1]->stop();

    CameraSettingsDialog* dlg = CameraSettingsDialog::openDlg(this);
    connect(dlg, &CameraSettingsDialog::finished, this, &MainWindow::onCameraSettingsFinished);
}

void MainWindow::onCameraSettingsFinished(int res)
{
    if (camStarted[0]) camManager[0]->start();
    if (camStarted[1]) camManager[1]->start();

    emuThread->emuUnpause();
}

void MainWindow::onOpenAudioSettings()
{
    AudioSettingsDialog* dlg = AudioSettingsDialog::openDlg(this);
    connect(emuThread, &EmuThread::syncVolumeLevel, dlg, &AudioSettingsDialog::onSyncVolumeLevel);
    connect(emuThread, &EmuThread::windowEmuStart, dlg, &AudioSettingsDialog::onConsoleReset);
    connect(dlg, &AudioSettingsDialog::updateAudioVolume, this, &MainWindow::onUpdateAudioVolume);
    connect(dlg, &AudioSettingsDialog::updateAudioSettings, this, &MainWindow::onUpdateAudioSettings);
    connect(dlg, &AudioSettingsDialog::finished, this, &MainWindow::onAudioSettingsFinished);
}

void MainWindow::onOpenFirmwareSettings()
{
    emuThread->emuPause();

    FirmwareSettingsDialog* dlg = FirmwareSettingsDialog::openDlg(this);
    connect(dlg, &FirmwareSettingsDialog::finished, this, &MainWindow::onFirmwareSettingsFinished);
}

void MainWindow::onFirmwareSettingsFinished(int res)
{
    if (FirmwareSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onOpenPathSettings()
{
    emuThread->emuPause();

    PathSettingsDialog* dlg = PathSettingsDialog::openDlg(this);
    connect(dlg, &PathSettingsDialog::finished, this, &MainWindow::onPathSettingsFinished);
}

void MainWindow::onPathSettingsFinished(int res)
{
    if (PathSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onUpdateAudioVolume(int vol, int dsisync)
{
    emuInstance->audioVolume = vol;
    emuInstance->audioDSiVolumeSync = dsisync;
}

void MainWindow::onUpdateAudioSettings()
{
    if (!emuThread->emuIsActive()) return;
    assert(emuInstance->nds != nullptr);

    int interp = globalCfg.GetInt("Audio.Interpolation");
    emuInstance->nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(interp));

    int bitdepth = globalCfg.GetInt("Audio.BitDepth");
    if (bitdepth == 0)
        emuInstance->nds->SPU.SetDegrade10Bit(emuInstance->nds->ConsoleType == 0);
    else
        emuInstance->nds->SPU.SetDegrade10Bit(bitdepth == 1);
}

void MainWindow::onAudioSettingsFinished(int res)
{
    emuInstance->audioUpdateSettings();
}

void MainWindow::onOpenMPSettings()
{
    emuThread->emuPause();

    MPSettingsDialog* dlg = MPSettingsDialog::openDlg(this);
    connect(dlg, &MPSettingsDialog::finished, this, &MainWindow::onMPSettingsFinished);
}

void MainWindow::onMPSettingsFinished(int res)
{
    emuInstance->mpAudioMode = globalCfg.GetInt("MP.AudioMode");
    emuInstance->updateAudioMuteByWindowFocus();
    MPInterface::Get().SetRecvTimeout(globalCfg.GetInt("MP.RecvTimeout"));

    emuThread->emuUnpause();
}

void MainWindow::onOpenWifiSettings()
{
    emuThread->emuPause();

    WifiSettingsDialog* dlg = WifiSettingsDialog::openDlg(this);
    connect(dlg, &WifiSettingsDialog::finished, this, &MainWindow::onWifiSettingsFinished);
}

void MainWindow::onWifiSettingsFinished(int res)
{
    if (WifiSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onOpenInterfaceSettings()
{
    emuThread->emuPause();
    InterfaceSettingsDialog* dlg = InterfaceSettingsDialog::openDlg(this);
    connect(dlg, &InterfaceSettingsDialog::finished, this, &MainWindow::onInterfaceSettingsFinished);
    connect(dlg, &InterfaceSettingsDialog::updateInterfaceSettings, this, &MainWindow::onUpdateInterfaceSettings);
}

void MainWindow::onUpdateInterfaceSettings()
{
    pauseOnLostFocus = globalCfg.GetBool("PauseLostFocus");
    emuInstance->targetFPS = globalCfg.GetDouble("TargetFPS");
    emuInstance->fastForwardFPS = globalCfg.GetDouble("FastForwardFPS");
    emuInstance->slowmoFPS = globalCfg.GetDouble("SlowmoFPS");
    // MuteFastForward is a legacy key name; it now enables a volume override
    emuInstance->speedVolumeFastForwardOn = globalCfg.GetBool("MuteFastForward");
    emuInstance->speedVolumeFastForward = globalCfg.GetInt("FastForwardVolume");
    emuInstance->speedVolumeSlowmoOn = globalCfg.GetBool("MuteSlowmo");
    emuInstance->speedVolumeSlowmo = globalCfg.GetInt("SlowmoVolume");
    panel->setMouseHide(globalCfg.GetBool("Mouse.Hide"),
                        globalCfg.GetInt("Mouse.HideSeconds")*1000);
}

void MainWindow::onInterfaceSettingsFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onChangeScreenSize()
{
    int factor = ((QAction*)sender())->data().toInt();
    QSize diff = size() - panel->size();
    resize(panel->screenGetMinSize(factor) + diff);
}

void MainWindow::onChangeScreenRotation(QAction* act)
{
    int rot = act->data().toInt();
    windowCfg.SetInt("ScreenRotation", rot);

    emit screenLayoutChange();
}

void MainWindow::updateScreenGapMenu()
{
    int screenGap = windowCfg.GetInt("ScreenGap");
    for (int i = 0; i < 6; i++)
    {
        if (actScreenGap[i]->data().toInt() == screenGap)
        {
            actScreenGap[i]->setChecked(true);
            return;
        }
    }

    // not one of the presets: the value was set through "Custom..."
    actScreenGapCustom->setChecked(true);
}

void MainWindow::onChangeScreenGap(QAction* act)
{
    int gap;
    if (act == actScreenGapCustom)
    {
        bool ok = false;
        gap = QInputDialog::getInt(this, tr("Screen gap"),
                                   tr("Gap between the screens (0-128 px):"),
                                   std::clamp(windowCfg.GetInt("ScreenGap"), 0, 128),
                                   0, 128, 1, &ok);
        if (!ok)
        {
            // cancelled: put the check mark back on the active setting
            updateScreenGapMenu();
            return;
        }
    }
    else
        gap = act->data().toInt();

    windowCfg.SetInt("ScreenGap", gap);
    updateScreenGapMenu();

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenLayout(QAction* act)
{
    int layout = act->data().toInt();
    windowCfg.SetInt("ScreenLayout", layout);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSwap(bool checked)
{
    windowCfg.SetBool("ScreenSwap", checked);

    // Swap between top and bottom screen when displaying one screen.
    int sizing = windowCfg.GetInt("ScreenSizing");
    if (sizing == screenSizing_TopOnly)
    {
        // Bottom Screen.
        sizing = screenSizing_BotOnly;
        actScreenSizing[screenSizing_TopOnly]->setChecked(false);
        actScreenSizing[sizing]->setChecked(true);
    }
    else if (sizing == screenSizing_BotOnly)
    {
        // Top Screen.
        sizing = screenSizing_TopOnly;
        actScreenSizing[screenSizing_BotOnly]->setChecked(false);
        actScreenSizing[sizing]->setChecked(true);
    }
    windowCfg.SetInt("ScreenSizing", sizing);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSizing(QAction* act)
{
    int sizing = act->data().toInt();
    windowCfg.SetInt("ScreenSizing", sizing);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenAspect(QAction* act)
{
    int aspect = act->data().toInt();
    QActionGroup* group = act->actionGroup();

    if (group == grpScreenAspectTop)
    {
        windowCfg.SetInt("ScreenAspectTop", aspect);
    }
    else
    {
        windowCfg.SetInt("ScreenAspectBot", aspect);
    }

    emit screenLayoutChange();
}

void MainWindow::onChangeIntegerScaling(bool checked)
{
    windowCfg.SetBool("IntegerScaling", checked);

    emit screenLayoutChange();
}

void MainWindow::onOpenNewWindow()
{
    emuInstance->createWindow();
}

void MainWindow::onChangeScreenFiltering(bool checked)
{
    windowCfg.SetBool("ScreenFilter", checked);

    //emit screenLayoutChange();
    panel->setFilter(checked);
}

void MainWindow::onChangeShowOSD(bool checked)
{
    showOSD = checked;
    panel->osdSetEnabled(showOSD);
    windowCfg.SetBool("ShowOSD", showOSD);
}

void MainWindow::onChangeLimitFramerate(bool checked)
{
    emuInstance->doLimitFPS = checked;
    globalCfg.SetBool("LimitFPS", emuInstance->doLimitFPS);
}

void MainWindow::onChangeAudioSync(bool checked)
{
    emuInstance->doAudioSync = checked;
    globalCfg.SetBool("AudioSync", emuInstance->doAudioSync);
}


void MainWindow::onTitleUpdate(QString title)
{
    if (!emuInstance) return;

    int numinst = numEmuInstances();
    int numwin = emuInstance->getNumWindows();
    if ((numinst > 1) && (numwin > 1))
    {
        // add player/window prefix
        QString prefix = QString("[p%1:w%2] ").arg(emuInstance->instanceID+1).arg(windowID+1);
        title = prefix + title;
    }
    else if (numinst > 1)
    {
        // add player prefix
        QString prefix = QString("[p%1] ").arg(emuInstance->instanceID+1);
        title = prefix + title;
    }
    else if (numwin > 1)
    {
        // add window prefix
        QString prefix = QString("[w%1] ").arg(windowID+1);
        title = prefix + title;
    }

    setWindowTitle(title);
}

void MainWindow::toggleFullscreen()
{
    if (!isFullScreen())
    {
        if (QApplication::activeModalWidget() || QApplication::activePopupWidget())
        {
            // Block entering fullscreen while a dialog/popup is active
            return;
        }
        wasMaximized = isMaximized();
        showFullScreen();
        if (hasMenu)
            menuBar()->setFixedHeight(0); // Don't use hide() as menubar actions stop working
    }
    else
    {
        if (wasMaximized)
            showMaximized();
        else
            showNormal();
        if (hasMenu)
        {
            int menuBarHeight = menuBar()->sizeHint().height();
            menuBar()->setFixedHeight(menuBarHeight);
        }
    }
}

void MainWindow::onFullscreenToggled()
{
    toggleFullscreen();
}

void MainWindow::onScreenLayoutToggled()
{
    int layout = windowCfg.GetInt("ScreenLayout");
    int rotation = windowCfg.GetInt("ScreenRotation");
    bool upright = (rotation == screenRot_0Deg || rotation == screenRot_180Deg);

    // Skip the next layout when it would look identical to the current one:
    // upright, Natural == Vertical; rotated 90/270, Horizontal == Natural.
    if ((layout == screenLayout_Natural && upright) ||
        (layout == screenLayout_Vertical && !upright))
        layout++;
    layout = (layout + 1) % screenLayout_MAX;

    windowCfg.SetInt("ScreenLayout", layout);
    actScreenLayout[layout]->setChecked(true);

    emit screenLayoutChange();
}

void MainWindow::onScreenEmphasisToggled()
{
    int currentSizing = windowCfg.GetInt("ScreenSizing");
    if (currentSizing == screenSizing_EmphTop)
    {
        currentSizing = screenSizing_EmphBot;
    }
    else if (currentSizing == screenSizing_EmphBot)
    {
        currentSizing = screenSizing_EmphTop;
    }
    else
    {
        // For any other sizing mode, switch to EmphTop as a sensible default
        currentSizing = screenSizing_EmphTop;
    }
    windowCfg.SetInt("ScreenSizing", currentSizing);
    actScreenSizing[currentSizing]->setChecked(true);

    emit screenLayoutChange();
}

void MainWindow::onEmuStart()
{
    if (!hasMenu) return;

    for (int i = 1; i < 9; i++)
    {
        actSaveState[i]->setEnabled(true);
        actLoadState[i]->setEnabled(emuInstance->savestateExists(i));
    }
    actSaveState[0]->setEnabled(true);
    actLoadState[0]->setEnabled(true);
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(true);
    actPause->setChecked(false);
    actReset->setEnabled(true);
    actStop->setEnabled(true);
    actFrameStep->setEnabled(true);

    //actDateTime->setEnabled(false);
    actPowerManagement->setEnabled(true);

    actTitleManager->setEnabled(false);
}

void MainWindow::onEmuStop()
{
    if (!hasMenu) return;

    for (int i = 0; i < 9; i++)
    {
        actSaveState[i]->setEnabled(false);
        actLoadState[i]->setEnabled(false);
    }
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(false);
    actReset->setEnabled(false);
    actStop->setEnabled(false);
    actFrameStep->setEnabled(false);

    //actDateTime->setEnabled(true);
    actPowerManagement->setEnabled(false);

    actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());
}

void MainWindow::onEmuPause(bool pause)
{
    if (!hasMenu) return;

    actPause->setChecked(pause);
}

void MainWindow::onEmuReset()
{
    if (!hasMenu) return;

    actUndoStateLoad->setEnabled(false);
}

void MainWindow::onQuickStateChange(bool loaded)
{
    if (!hasMenu) return;

    // keep the menu in sync with a quick save/load done from the hotkey
    if (loaded) actUndoStateLoad->setEnabled(true);
    else        actLoadState[1]->setEnabled(true);
}

void MainWindow::onUpdateVideoSettings(bool glchange)
{
    if (!emuInstance) return;

    // if we have a parent window: pass the message over to the parent
    // the topmost parent takes care of updating all the windows
    MainWindow* parentwin = (MainWindow*)parentWidget();
    if (parentwin)
        return parentwin->onUpdateVideoSettings(glchange);

    auto childwins = findChildren<MainWindow *>(nullptr);

    bool hadOGL = hasOGL;
    if (glchange)
    {
        emuThread->emuPause();
        if (hadOGL)
        {
            emuThread->deinitContext(windowID);
            for (auto child: childwins)
            {
                auto thread = child->getEmuInstance()->getEmuThread();
                thread->deinitContext(child->windowID);
            }
        }

        createScreenPanel();
        for (auto child: childwins)
        {
            child->createScreenPanel();
        }
    }

    emuThread->updateVideoSettings();
    for (auto child: childwins)
    {
        // child windows may belong to a different instance
        // in that case we need to signal their thread appropriately
        auto thread = child->getEmuInstance()->getEmuThread();
        if (child->getWindowID() == 0)
            thread->updateVideoSettings();
    }

    if (glchange)
    {
        if (hasOGL) 
        {
            emuThread->initContext(windowID);
            for (auto child: childwins)
            {
                auto thread = child->getEmuInstance()->getEmuThread();
                thread->initContext(child->windowID);
            }
        }
    }

    if (glchange)
    {
        emuThread->emuUnpause();
    }
}
