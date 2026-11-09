// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "InstanceCreationTask.h"
#include "horizon/Horizon.h"

/// A new Horizon instance: vanilla `minecraftVersion` (the launcher downloads its files and
/// Java as for any instance) that plays the Horizon build `release` instead of Java.
/// The build itself is downloaded on the first launch.
class HorizonCreationTask final : public InstanceCreationTask {
    Q_OBJECT
   public:
    HorizonCreationTask(QString minecraftVersion, Horizon::Release release, bool autoUpdate)
        : m_minecraftVersion(std::move(minecraftVersion)), m_release(std::move(release)), m_autoUpdate(autoUpdate)
    {}

    std::unique_ptr<MinecraftInstance> createInstance() override;

   private:
    QString m_minecraftVersion;
    Horizon::Release m_release;
    bool m_autoUpdate = true;
};
