// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QSet>

#include <functional>
#include <memory>
#include <optional>

#include "Horizon.h"
#include "launch/LaunchStep.h"

class QNetworkReply;

/// What the setup hands to HorizonLaunch.
struct HorizonLaunchInfo {
    QString client;     // horizon-client(.exe)
    QString buildDir;   // the build's folder
    QString resources;  // the prepared resources
    /// The options `horizon-client --launch-api` lists; empty when it could not be asked.
    QSet<QString> options;
};
using HorizonLaunchInfoPtr = std::shared_ptr<HorizonLaunchInfo>;

/// Before a Horizon instance starts: installs (or updates) its Horizon build, downloads
/// Mojang's version JSON and the server jar, and runs horizon-prepare when the prepared
/// resources are missing or out of date. The vanilla steps before it have downloaded the
/// client jar, the libraries, the assets and Java.
class HorizonSetup : public LaunchStep {
    Q_OBJECT
   public:
    HorizonSetup(LaunchTask* parent, HorizonLaunchInfoPtr info, bool online);
    ~HorizonSetup() override = default;

    void executeTask() override;
    void proceed() override;
    bool canAbort() const override { return true; }
    bool abort() override;

   private:
    using Ok = std::function<void(const QByteArray&)>;

    // the build
    void checkForUpdate();
    void tryCandidate(const QList<Horizon::Release>& releases, const QString& channel);
    void installFromTag();
    void useInstalledOrFail(const QString& why);
    void install(const Horizon::Release& release);
    void downloadArchive(const Horizon::Release& release, const QString& sha256);
    void unpack(const Horizon::Release& release, const QString& archive);
    void installed(const Horizon::Release& release);
    void removeUnusedBuild(const QString& key);

    // the game files and the resources
    void afterBuild();
    void ensureVersionJson();
    void ensureServerJar();
    void askLaunchApi();
    void checkResources();
    void prepare();
    void finish();

    // helpers
    void get(const QString& url, Ok ok, const QString& what);
    void downloadFile(const QString& url, const QString& path, QCryptographicHash::Algorithm algorithm, const QString& expected,
                      const QString& what, std::function<void()> ok);
    void fail(const QString& reason);
    void log(const QString& line, MessageLevel level = MessageLevel::Launcher);
    QString instanceMinecraftVersion() const;
    QString clientJar() const;
    QString javaHome() const;
    void setSetting(const char* id, const QVariant& value);

   private:
    HorizonLaunchInfoPtr m_info;
    bool m_online = true;
    bool m_started = false;
    bool m_aborted = false;

    QString m_key;  // the build in use
    Horizon::Pins m_pins;

    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QSaveFile> m_saveFile;
    std::unique_ptr<QCryptographicHash> m_hash;
    QPointer<QProcess> m_process;
    QByteArray m_stdout;
    QString m_lastError;
    std::unique_ptr<QFutureWatcher<std::optional<QStringList>>> m_unpackWatcher;
};
