// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonCreationTask.h"

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

std::unique_ptr<MinecraftInstance> HorizonCreationTask::createInstance()
{
    setStatus(tr("Создание экземпляра Horizon (Minecraft %1)").arg(m_minecraftVersion));

    auto inst = std::make_unique<MinecraftInstance>(
        m_globalSettings, std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")), m_stagingPath);
    SettingsObject::Lock lock(inst->settings());

    auto* components = inst->getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion("net.minecraft", m_minecraftVersion, true);

    auto* settings = inst->settings();
    Horizon::registerSettings(settings);
    settings->set(Horizon::SETTING_ENABLED, true);
    // The build is installed on the first launch, under this name.
    settings->set(Horizon::SETTING_BUILD, m_release.key());
    settings->set(Horizon::SETTING_TAG, m_release.tag);
    settings->set(Horizon::SETTING_CHANNEL, m_release.channel);
    settings->set(Horizon::SETTING_AUTO_UPDATE, m_autoUpdate);

    inst->setName(name());
    inst->setIconKey(m_instIcon);
    components->saveNow();
    return inst;
}
