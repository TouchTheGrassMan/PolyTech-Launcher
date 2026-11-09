// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <LoggedProcess.h>
#include <launch/LaunchStep.h>
#include <minecraft/auth/AuthSession.h>

#include "HorizonSetup.h"
#include "minecraft/launch/MinecraftTarget.h"

/// Starts horizon-client as the Launch API describes (docs/api/launch.md): one folder per
/// instance and the account as JSON on standard input, never on the command line.
class HorizonLaunch : public LaunchStep {
    Q_OBJECT
   public:
    HorizonLaunch(LaunchTask* parent, HorizonLaunchInfoPtr info);
    ~HorizonLaunch() override = default;

    void executeTask() override;
    bool abort() override;
    void proceed() override;
    bool canAbort() const override { return true; }

    void setWorkingDirectory(const QString& wd) { m_process.setWorkingDirectory(wd); }
    void setAuthSession(AuthSessionPtr session) { m_session = std::move(session); }
    void setTargetToJoin(MinecraftTarget::Ptr target) { m_targetToJoin = std::move(target); }

   private slots:
    void on_state(LoggedProcess::State state);

   private:
    QStringList arguments() const;
    QByteArray sessionJson() const;

   private:
    HorizonLaunchInfoPtr m_info;
    LoggedProcess m_process;
    AuthSessionPtr m_session;
    MinecraftTarget::Ptr m_targetToJoin;
    bool m_mayProceed = false;
};
