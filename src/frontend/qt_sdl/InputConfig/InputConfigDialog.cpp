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

#include <QGroupBox>
#include <QLabel>
#include <QKeyEvent>
#include <QDebug>

#include <algorithm>
#include <iterator>
#include <vector>

#include <SDL2/SDL.h>

#include "types.h"
#include "Platform.h"

#include "InputConfigDialog.h"
#include "ui_InputConfigDialog.h"
#include "MapButton.h"


using namespace melonDS;
InputConfigDialog* InputConfigDialog::currentDlg = nullptr;

const int dskeyorder[12] = {0, 1, 10, 11, 5, 4, 6, 7, 9, 8, 2, 3};
const char* dskeylabels[12] = {"A", "B", "X", "Y", "Left", "Right", "Up", "Down", "L", "R", "Select", "Start"};

InputConfigDialog::InputConfigDialog(QWidget* parent) : QDialog(parent), ui(new Ui::InputConfigDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    Config::Table& instcfg = emuInstance->getLocalConfig();
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");

    for (int i = 0; i < keypad_num; i++)
    {
        const char* btn = EmuInstance::buttonNames[dskeyorder[i]];
        keypadKeyMap[i] = keycfg.GetInt(btn);
    }

    int i = 0;
    for (int hotkey : hk_addons)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        addonsKeyMap[i] = keycfg.GetInt(btn);
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        hkGeneralKeyMap[i] = keycfg.GetInt(btn);
        i++;
    }

    // the flat Joystick table is the default joystick mapping
    readJoyMapSet(joycfg, defaultJoyMaps);

    // per-model joystick mappings, keyed by SDL joystick GUID string
    std::vector<std::string> cfgkeys = instcfg.GetTableKeys();
    if (std::find(cfgkeys.begin(), cfgkeys.end(), "JoystickModels") != cfgkeys.end())
    {
        Config::Table modelcfg = instcfg.GetTable("JoystickModels");
        for (const std::string& guid : modelcfg.GetTableKeys())
        {
            Config::Table entrycfg = modelcfg.GetTable(guid);
            JoyMapSet& set = modelJoyMaps[guid];
            readJoyMapSet(entrycfg, set);
            set.name = entrycfg.GetString("Name");
        }
    }

    joystickID = instcfg.GetInt("JoystickID");

    int njoy = SDL_NumJoysticks();
    // same fallback as EmuInstance::openJoystick(), so the GUID matches the open device
    int joyIndex = (joystickID >= 0 && joystickID < njoy) ? joystickID : 0;

    currentJoyGUID = (njoy > 0) ? EmuInstance::getJoystickGUIDForIndex(joyIndex) : "";
    editingModel = !currentJoyGUID.empty() && (modelJoyMaps.count(currentJoyGUID) != 0);

    // the mapping buttons read their value when created, so fill the
    // working arrays with the mapping of the selected joystick first
    loadJoyMaps(currentJoyMapSet());

    populatePage(ui->tabAddons, hk_addons_labels, addonsKeyMap, addonsJoyMap);
    populatePage(ui->tabHotkeysGeneral, hk_general_labels, hkGeneralKeyMap, hkGeneralJoyMap);

    // filling the list must not trigger the joystick switch handler
    ui->cbxJoystick->blockSignals(true);
    if (njoy > 0)
    {
        for (int i = 0; i < njoy; i++)
        {
            const char* name = SDL_JoystickNameForIndex(i);
            ui->cbxJoystick->addItem(QString(name));
        }
        ui->cbxJoystick->setCurrentIndex(joyIndex);
    }
    else
    {
        ui->cbxJoystick->addItem(tr("(no joysticks available)"));
        ui->cbxJoystick->setEnabled(false);
    }
    ui->cbxJoystick->blockSignals(false);

    ui->cbModelMapping->blockSignals(true);
    ui->cbModelMapping->setChecked(editingModel);
    ui->cbModelMapping->setEnabled(!currentJoyGUID.empty());
    ui->cbModelMapping->blockSignals(false);

    ui->cbAutoMap->setChecked(joycfg.GetBool("AutoMap"));

    setupKeypadPage();
    setJoyMappingEnabled(ui->cbAutoMap->isChecked());

    int inst = emuInstance->getInstanceID();
    if (inst > 0)
        ui->lblInstanceNum->setText(tr("Configuring mappings for instance %1").arg(inst+1));
    else
        ui->lblInstanceNum->hide();
}

InputConfigDialog::~InputConfigDialog()
{
    delete ui;
}

void InputConfigDialog::setupKeypadPage()
{
    for (int i = 0; i < keypad_num; i++)
    {
        QPushButton* pushButtonKey = this->findChild<QPushButton*>(QStringLiteral("btnKey") + dskeylabels[i]);
        QPushButton* pushButtonJoy = this->findChild<QPushButton*>(QStringLiteral("btnJoy") + dskeylabels[i]);

        KeyMapButton* keyMapButtonKey = new KeyMapButton(&keypadKeyMap[i], false);
        JoyMapButton* keyMapButtonJoy = new JoyMapButton(&keypadJoyMap[i], false);

        pushButtonKey->parentWidget()->layout()->replaceWidget(pushButtonKey, keyMapButtonKey);
        pushButtonJoy->parentWidget()->layout()->replaceWidget(pushButtonJoy, keyMapButtonJoy);

        delete pushButtonKey;
        delete pushButtonJoy;

        if (ui->cbxJoystick->isEnabled() && (!ui->cbAutoMap->isChecked() || editingModel))
        {
            ui->stackMapping->setCurrentIndex(1);
        }
    }
}

void InputConfigDialog::populatePage(QWidget* page,
    const std::initializer_list<const char*>& labels,
    int* keymap, int* joymap)
{
    // kind of a hack
    bool ishotkey = (page != ui->tabInput);

    QHBoxLayout* main_layout = new QHBoxLayout();

    QGroupBox* group;
    QGridLayout* group_layout;

    group = new QGroupBox(tr("Keyboard mappings:"));
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    int i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(tr(labelStr)+":");
        KeyMapButton* btn = new KeyMapButton(&keymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    group = new QGroupBox(tr("Joystick mappings:"));
    main_layout->addWidget(group);
    group_layout = new QGridLayout();
    group_layout->setSpacing(1);
    i = 0;
    for (const char* labelStr : labels)
    {
        QLabel* label = new QLabel(tr(labelStr)+":");
        JoyMapButton* btn = new JoyMapButton(&joymap[i], ishotkey);

        group_layout->addWidget(label, i, 0);
        group_layout->addWidget(btn, i, 1);
        i++;
    }
    group_layout->setRowStretch(labels.size(), 1);
    group->setLayout(group_layout);
    group->setMinimumWidth(275);

    page->setLayout(main_layout);
}

void InputConfigDialog::on_InputConfigDialog_accepted()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();
    Config::Table keycfg = instcfg.GetTable("Keyboard");
    Config::Table joycfg = instcfg.GetTable("Joystick");

    for (int i = 0; i < keypad_num; i++)
    {
        const char* btn = EmuInstance::buttonNames[dskeyorder[i]];
        keycfg.SetInt(btn, keypadKeyMap[i]);
    }

    int i = 0;
    for (int hotkey : hk_addons)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        keycfg.SetInt(btn, addonsKeyMap[i]);
        i++;
    }

    i = 0;
    for (int hotkey : hk_general)
    {
        const char* btn = EmuInstance::hotkeyNames[hotkey];
        keycfg.SetInt(btn, hkGeneralKeyMap[i]);
        i++;
    }

    storeJoyMaps(currentJoyMapSet());
    writeJoyMapSet(joycfg, defaultJoyMaps);

    instcfg.SetInt("JoystickID", joystickID);
    joycfg.SetBool("AutoMap", ui->cbAutoMap->isChecked());

    // rewrite the per-model table so entries removed in the dialog disappear
    instcfg.Remove("JoystickModels");
    if (!modelJoyMaps.empty())
    {
        Config::Table modelcfg = instcfg.GetTable("JoystickModels");
        for (auto& entry : modelJoyMaps)
        {
            Config::Table entrycfg = modelcfg.GetTable(entry.first);
            entrycfg.SetString("Name", entry.second.name);
            writeJoyMapSet(entrycfg, entry.second);
        }
    }

    Config::Save();

    emuInstance->inputLoadConfig();

    closeDlg();
}

void InputConfigDialog::on_InputConfigDialog_rejected()
{
    Config::Table& instcfg = emuInstance->getLocalConfig();
    emuInstance->setJoystick(instcfg.GetInt("JoystickID"));

    closeDlg();
}

void InputConfigDialog::on_btnKeyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(0);
}

void InputConfigDialog::on_btnJoyMapSwitch_clicked()
{
    ui->stackMapping->setCurrentIndex(1);
}

void InputConfigDialog::on_cbxJoystick_currentIndexChanged(int id)
{
    // prevent a spurious change
    if (ui->cbxJoystick->count() < 2) return;
    if (id < 0) return;

    // stop any button that is still waiting for input, then keep the edits
    // made for the previous joystick in memory
    for (JoyMapButton* btn : findChildren<JoyMapButton*>())
    {
        if (btn->isChecked())
            btn->refreshText();
    }
    storeJoyMaps(currentJoyMapSet());

    joystickID = id;
    // opening the newly selected joystick also makes the mapping buttons listen to it
    emuInstance->setJoystick(id);

    currentJoyGUID = EmuInstance::getJoystickGUIDForIndex(id);
    editingModel = !currentJoyGUID.empty() && (modelJoyMaps.count(currentJoyGUID) != 0);

    ui->cbModelMapping->blockSignals(true);
    ui->cbModelMapping->setChecked(editingModel);
    ui->cbModelMapping->setEnabled(!currentJoyGUID.empty());
    ui->cbModelMapping->blockSignals(false);

    selectJoystickMapping();
    setJoyMappingEnabled(ui->cbAutoMap->isChecked());
}

void InputConfigDialog::on_cbAutoMap_toggled(bool checked)
{
    setJoyMappingEnabled(checked);
}

void InputConfigDialog::on_cbModelMapping_toggled(bool checked)
{
    if (currentJoyGUID.empty()) return;

    for (JoyMapButton* btn : findChildren<JoyMapButton*>())
    {
        if (btn->isChecked())
            btn->refreshText();
    }

    if (checked)
    {
        // the edits shown so far belong to the default mapping
        storeJoyMaps(defaultJoyMaps);

        // start from what this controller currently uses: the default mapping,
        // or SDL's automatic mapping when that is enabled and available
        int joyIndex = ui->cbxJoystick->currentIndex();
        JoyMapSet set = defaultJoyMaps;
        if (ui->cbAutoMap->isChecked())
        {
            // buttons SDL cannot express as a raw button or hat keep the default binding
            int btnmap[12];
            for (int i = 0; i < keypad_num; i++)
                btnmap[dskeyorder[i]] = set.keypad[i];

            auto mutex = emuInstance->getJoyMutex();
            SDL_LockMutex(mutex.get());
            bool hasauto = EmuInstance::getGameControllerJoyMapping(joyIndex, btnmap);
            SDL_UnlockMutex(mutex.get());

            if (hasauto)
            {
                for (int i = 0; i < keypad_num; i++)
                    set.keypad[i] = btnmap[dskeyorder[i]];
            }
        }

        const char* name = SDL_JoystickNameForIndex(joyIndex);
        set.name = name ? name : "";

        modelJoyMaps[currentJoyGUID] = set;
        editingModel = true;
    }
    else
    {
        // the per-model entry is dropped; it is removed from the config on OK
        modelJoyMaps.erase(currentJoyGUID);
        editingModel = false;
    }

    selectJoystickMapping();
    setJoyMappingEnabled(ui->cbAutoMap->isChecked());
}

void InputConfigDialog::setJoyMappingEnabled(bool enable)
{
    // a per-model mapping takes precedence over automatic mapping
    if (enable && !editingModel && SDL_IsGameController(emuInstance->getJoystickID()))
    {
        ui->stackMapping->setCurrentIndex(0);
        ui->btnJoyMapSwitch->setEnabled(false);
    }
    else
    {
        ui->btnJoyMapSwitch->setEnabled(true);
    }
}

void InputConfigDialog::readJoyMapSet(Config::Table& table, JoyMapSet& set)
{
    for (int i = 0; i < keypad_num; i++)
        set.keypad[i] = table.GetInt(EmuInstance::buttonNames[dskeyorder[i]]);

    int i = 0;
    for (int hotkey : hk_addons)
        set.addons[i++] = table.GetInt(EmuInstance::hotkeyNames[hotkey]);

    i = 0;
    for (int hotkey : hk_general)
        set.hkGeneral[i++] = table.GetInt(EmuInstance::hotkeyNames[hotkey]);
}

void InputConfigDialog::writeJoyMapSet(Config::Table& table, const JoyMapSet& set)
{
    for (int i = 0; i < keypad_num; i++)
        table.SetInt(EmuInstance::buttonNames[dskeyorder[i]], set.keypad[i]);

    int i = 0;
    for (int hotkey : hk_addons)
        table.SetInt(EmuInstance::hotkeyNames[hotkey], set.addons[i++]);

    i = 0;
    for (int hotkey : hk_general)
        table.SetInt(EmuInstance::hotkeyNames[hotkey], set.hkGeneral[i++]);
}

void InputConfigDialog::storeJoyMaps(JoyMapSet& set)
{
    std::copy(std::begin(keypadJoyMap), std::end(keypadJoyMap), set.keypad);
    std::copy(std::begin(addonsJoyMap), std::end(addonsJoyMap), set.addons);
    std::copy(std::begin(hkGeneralJoyMap), std::end(hkGeneralJoyMap), set.hkGeneral);
}

void InputConfigDialog::loadJoyMaps(const JoyMapSet& set)
{
    std::copy(std::begin(set.keypad), std::end(set.keypad), keypadJoyMap);
    std::copy(std::begin(set.addons), std::end(set.addons), addonsJoyMap);
    std::copy(std::begin(set.hkGeneral), std::end(set.hkGeneral), hkGeneralJoyMap);
}

InputConfigDialog::JoyMapSet& InputConfigDialog::currentJoyMapSet()
{
    if (editingModel)
    {
        auto it = modelJoyMaps.find(currentJoyGUID);
        if (it != modelJoyMaps.end())
            return it->second;
    }

    return defaultJoyMaps;
}

void InputConfigDialog::selectJoystickMapping()
{
    loadJoyMaps(currentJoyMapSet());

    for (JoyMapButton* btn : findChildren<JoyMapButton*>())
        btn->refreshText();
}

SDL_Joystick* InputConfigDialog::getJoystick()
{
    return emuInstance->getJoystick();
}

std::shared_ptr<SDL_mutex> InputConfigDialog::getJoyMutex()
{
    return emuInstance->getJoyMutex();
}
