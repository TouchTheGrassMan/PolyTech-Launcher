// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonLaunch.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

HorizonLaunch::HorizonLaunch(LaunchTask* parent, HorizonLaunchInfoPtr info)
    : LaunchStep(parent), m_info(std::move(info)), m_process(QStringConverter::Utf8)
{
    connect(&m_process, &LoggedProcess::log, this, &HorizonLaunch::logLines);
    connect(&m_process, &LoggedProcess::stateChanged, this, &HorizonLaunch::on_state);
}

QStringList HorizonLaunch::arguments() const
{
    auto* instance = m_parent->instance();
    const QString game = QDir(instance->gameRoot()).absolutePath();
    QStringList args;
    // Pass only the options the build lists (`--launch-api`): any other is refused with exit code 2.
    auto add = [&](const QString& option, const QString& value = QString()) {
        if (!m_info->options.isEmpty() && !m_info->options.contains(option))
            return false;
        args << option;
        if (!value.isNull())
            args << value;
        return true;
    };
    add("--resources", m_info->resources);
    add("--game-dir", game);
    add("--home", game);

    auto* settings = instance->settings();
    if (!settings->get("LaunchMaximized").toBool()) {
        add("--width", QString::number(settings->get("MinecraftWinWidth").toInt()));
        add("--height", QString::number(settings->get("MinecraftWinHeight").toInt()));
    }
    if (m_targetToJoin) {
        if (!m_targetToJoin->world.isEmpty())
            add("--quickPlaySingleplayer", m_targetToJoin->world);
        else if (!m_targetToJoin->address.isEmpty())
            add("--quickPlayMultiplayer", QStringLiteral("%1:%2").arg(m_targetToJoin->address).arg(m_targetToJoin->port));
    }
    if (m_session)
        add("--session-stdin");
    return args;
}

QByteArray HorizonLaunch::sessionJson() const
{
    // {"name", "uuid", "access_token", "user_type": "msa", "xuid"}: only a Microsoft account is
    // "msa". Any other account still names the player; online-mode servers refuse it.
    QJsonObject session;
    session["name"] = m_session->player_name;
    session["uuid"] = m_session->uuid;
    session["access_token"] = m_session->access_token.isEmpty() ? QStringLiteral("0") : m_session->access_token;
    if (m_session->user_type == "msa")
        session["user_type"] = QStringLiteral("msa");
    return QJsonDocument(session).toJson(QJsonDocument::Compact);
}

void HorizonLaunch::executeTask()
{
    auto* instance = m_parent->instance();
    if (m_info->client.isEmpty()) {
        emitFailed(tr("Horizon не подготовлен."));
        return;
    }
    const QStringList args = arguments();
    emit logLine(tr("Horizon:\n  %1 %2\n").arg(m_info->client, args.join(' ')), MessageLevel::Launcher);

    m_process.setProcessEnvironment(instance->createLaunchEnvironment());
    m_process.setDetachable(true);
    m_process.start(m_info->client, args);
}

void HorizonLaunch::on_state(LoggedProcess::State state)
{
    auto* instance = m_parent->instance();
    switch (state) {
        case LoggedProcess::FailedToStart: {
            const QString reason = tr("Не удалось запустить Horizon!");
            emit logLine(reason, MessageLevel::Fatal);
            emitFailed(reason);
            return;
        }
        case LoggedProcess::Aborted:
        case LoggedProcess::Crashed: {
            m_parent->setPid(-1);
            instance->setMinecraftRunning(false);
            emitFailed(tr("Игра завершилась с ошибкой."));
            return;
        }
        case LoggedProcess::Finished: {
            if (instance->settings()->get("CloseAfterLaunch").toBool())
                APPLICATION->showMainWindow();
            m_parent->setPid(-1);
            instance->setMinecraftRunning(false);
            const int exitCode = m_process.exitCode();
            if (exitCode == 0)
                emitSucceeded();
            else if (exitCode == 2)  // a refused command line: stderr names the option
                emitFailed(tr("Horizon не принял параметры запуска (подробности — в консоли)."));
            else  // 1 or 101: the last stderr line names the crash report
                emitFailed(tr("Игра завершилась с ошибкой."));
            return;
        }
        case LoggedProcess::Running:
            emit logLine(tr("ID процесса Horizon: %1\n\n").arg(m_process.processId()), MessageLevel::Launcher);
            m_parent->setPid(m_process.processId());
            if (m_session)
                m_process.write(sessionJson());
            m_process.closeWriteChannel();
            m_mayProceed = true;
            emit readyForLaunch();
            break;
        default:
            break;
    }
}

void HorizonLaunch::proceed()
{
    if (!m_mayProceed)
        return;
    m_mayProceed = false;
    m_parent->instance()->setMinecraftRunning(true);
    if (m_parent->instance()->settings()->get("CloseAfterLaunch").toBool())
        APPLICATION->closeAllWindows();
}

bool HorizonLaunch::abort()
{
    m_mayProceed = false;
    const auto state = m_process.state();
    if (state == LoggedProcess::Running || state == LoggedProcess::Starting)
        m_process.kill();
    return true;
}
