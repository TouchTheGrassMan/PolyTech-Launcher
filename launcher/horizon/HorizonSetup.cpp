// SPDX-License-Identifier: GPL-3.0-only
#include "HorizonSetup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QtConcurrent>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "MMCZip.h"
#include "java/JavaVersion.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/SettingsObject.h"

HorizonSetup::HorizonSetup(LaunchTask* parent, HorizonLaunchInfoPtr info, bool online)
    : LaunchStep(parent), m_info(std::move(info)), m_online(online)
{}

void HorizonSetup::executeTask()
{
    // Show the progress dialog; LaunchController then calls proceed().
    emit progressReportingRequest();
}

void HorizonSetup::proceed()
{
    if (m_started)
        return;
    m_started = true;

    auto* settings = m_parent->instance()->settings();
    m_key = settings->get(Horizon::SETTING_BUILD).toString();
    const bool autoUpdate = settings->get(Horizon::SETTING_AUTO_UPDATE).toBool();

    if (Horizon::platformAsset().isEmpty()) {
        fail(tr("Horizon не выпускается для этой системы."));
        return;
    }
    if (m_online && autoUpdate) {
        checkForUpdate();
    } else if (Horizon::isBuildInstalled(m_key)) {
        afterBuild();
    } else if (m_online) {
        installFromTag();
    } else {
        fail(tr("Horizon ещё не скачан, а запуск идёт без сети. Подключитесь к интернету и запустите экземпляр снова."));
    }
}

bool HorizonSetup::abort()
{
    m_aborted = true;
    if (m_reply)
        m_reply->abort();
    if (m_process)
        m_process->kill();
    if (m_saveFile)
        m_saveFile->cancelWriting();
    if (isRunning())
        emitFailed(tr("Запуск отменён."));
    return true;
}

// ---- the build ------------------------------------------------------------------------------

void HorizonSetup::checkForUpdate()
{
    setStatus(tr("Проверка обновлений Horizon..."));
    const QString channel = m_parent->instance()->settings()->get(Horizon::SETTING_CHANNEL).toString();
    get(
        Horizon::releasesUrl(), [this, channel](const QByteArray& data) { tryCandidate(Horizon::parseReleases(data), channel); },
        tr("список сборок Horizon"));
}

void HorizonSetup::tryCandidate(const QList<Horizon::Release>& releases, const QString& channel)
{
    const auto candidate = Horizon::newest(releases, channel);
    if (!candidate.isValid()) {
        useInstalledOrFail(tr("На GitHub нет сборок Horizon для этой системы."));
        return;
    }
    if (candidate.key() == m_key && Horizon::isBuildInstalled(m_key)) {
        log(tr("Horizon %1 — последняя версия.").arg(candidate.displayName()));
        afterBuild();
        return;
    }
    // A newer build: only when it is made for this instance's Minecraft version...
    get(
        Horizon::researchLockUrl(candidate.tag),
        [this, candidate, releases, channel](const QByteArray& lock) {
            const auto pins = Horizon::parsePins(lock);
            const QString minecraft = instanceMinecraftVersion();
            if (pins.minecraft != minecraft) {
                useInstalledOrFail(tr("Новая сборка Horizon %1 сделана для Minecraft %2, а этот экземпляр — Minecraft %3.")
                                       .arg(candidate.displayName(), pins.minecraft.isEmpty() ? tr("(неизвестно)") : pins.minecraft,
                                            minecraft));
                return;
            }
            // ...and starts the way the launcher starts it (the Launch API).
            auto request = QNetworkRequest(QUrl(Horizon::launchApiProbeUrl(candidate.tag)));
            request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
            request.setTransferTimeout(30000);
            auto* reply = APPLICATION->network()->head(request);
            m_reply = reply;
            connect(reply, &QNetworkReply::finished, this, [this, reply, candidate, releases, channel] {
                reply->deleteLater();
                if (m_aborted)
                    return;
                if (reply->error() == QNetworkReply::NoError) {
                    log(tr("Доступна новая сборка Horizon: %1.").arg(candidate.displayName()));
                    install(candidate);
                } else if (reply->error() == QNetworkReply::ContentNotFoundError && channel != "nightly") {
                    // The newest release can't be started from here yet: the Nightly can.
                    log(tr("Сборку Horizon %1 пока нельзя запускать из лаунчера, используется Nightly.").arg(candidate.displayName()));
                    tryCandidate(releases, "nightly");
                } else {
                    useInstalledOrFail(tr("Не удалось проверить сборку Horizon %1: %2").arg(candidate.displayName(), reply->errorString()));
                }
            });
        },
        tr("описание сборки Horizon"));
}

void HorizonSetup::useInstalledOrFail(const QString& why)
{
    if (Horizon::isBuildInstalled(m_key)) {
        log(why + ' ' + tr("Запускается установленная сборка."), MessageLevel::Warning);
        afterBuild();
    } else {
        fail(why);
    }
}

void HorizonSetup::installFromTag()
{
    const QString tag = m_parent->instance()->settings()->get(Horizon::SETTING_TAG).toString();
    if (tag.isEmpty()) {
        fail(tr("В экземпляре не указана сборка Horizon. Включите автообновление в настройках экземпляра или создайте его заново."));
        return;
    }
    setStatus(tr("Поиск сборки Horizon %1...").arg(tag));
    get(
        Horizon::releaseByTagUrl(tag),
        [this, tag](const QByteArray& data) {
            const auto release = Horizon::parseRelease(QJsonDocument::fromJson(data).object());
            if (!release.isValid()) {
                fail(tr("В сборке Horizon %1 нет архива для этой системы.").arg(tag));
                return;
            }
            install(release);
        },
        tr("сборку Horizon %1").arg(tag));
}

void HorizonSetup::install(const Horizon::Release& release)
{
    if (Horizon::isBuildInstalled(release.key())) {
        installed(release);
        return;
    }
    if (release.checksumAssets.isEmpty()) {
        fail(tr("В сборке Horizon %1 нет списка контрольных сумм (SHA256SUMS): её нельзя проверить, поэтому она не установлена.")
                 .arg(release.displayName()));
        return;
    }
    // SHA256SUMS itself first, then any other list it was split into (SHA256SUMS.v2, ...).
    auto sums = release.checksumAssets;
    std::stable_sort(sums.begin(), sums.end(), [](const auto& a, const auto& b) { return a.first == "SHA256SUMS" && b.first != "SHA256SUMS"; });
    setStatus(tr("Скачивание Horizon %1...").arg(release.displayName()));
    get(
        sums.first().second,
        [this, release](const QByteArray& data) {
            const QString sha256 = Horizon::checksumFor(data, Horizon::platformAsset());
            if (sha256.isEmpty()) {
                fail(tr("Для архива %1 нет контрольной суммы: сборка не установлена.").arg(Horizon::platformAsset()));
                return;
            }
            downloadArchive(release, sha256);
        },
        tr("контрольные суммы Horizon"));
}

void HorizonSetup::downloadArchive(const Horizon::Release& release, const QString& sha256)
{
    const QString partial = FS::PathCombine(Horizon::rootDir(), "builds", '.' + release.key() + ".partial");
    FS::deletePath(partial);
    const QString archive = FS::PathCombine(partial, Horizon::platformAsset());
    log(tr("Скачивание %1").arg(release.assetUrl));
    downloadFile(release.assetUrl, archive, QCryptographicHash::Sha256, sha256, tr("Horizon %1").arg(release.displayName()),
                 [this, release, archive] { unpack(release, archive); });
}

void HorizonSetup::unpack(const Horizon::Release& release, const QString& archive)
{
    setStatus(tr("Распаковка Horizon %1...").arg(release.displayName()));
    setProgress(0, 0);
    const QString unpacked = FS::PathCombine(QFileInfo(archive).absolutePath(), "unpacked");
    m_unpackWatcher = std::make_unique<QFutureWatcher<std::optional<QStringList>>>();
    connect(m_unpackWatcher.get(), &QFutureWatcherBase::finished, this, [this, release, archive, unpacked] {
        if (m_aborted)
            return;
        const auto result = m_unpackWatcher->result();
        if (!result) {
            fail(tr("Не удалось распаковать %1.").arg(archive));
            return;
        }
        // The archives hold one top folder (horizon-windows-x64/...): the build is its contents.
        QString top = unpacked;
        const auto entries = QDir(unpacked).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        if (entries.size() == 1 && entries.first().isDir())
            top = entries.first().absoluteFilePath();
        if (!QFileInfo::exists(FS::PathCombine(top, Horizon::clientProgramName()))) {
            fail(tr("В архиве Horizon нет %1.").arg(Horizon::clientProgramName()));
            return;
        }
#ifndef Q_OS_WIN
        for (const auto& program : { QStringLiteral("horizon-client"), QStringLiteral("horizon-prepare"), QStringLiteral("horizon-ctl"),
                                     QStringLiteral("horizon-server") }) {
            QFile file(FS::PathCombine(top, program));
            if (file.exists())
                file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup |
                                    QFileDevice::ExeOther);
        }
#endif
        const QString target = Horizon::buildDir(release.key());
        FS::deletePath(target);
        FS::ensureFolderPathExists(QFileInfo(target).absolutePath());
        if (!QDir().rename(top, target) && !FS::move(top, target)) {
            fail(tr("Не удалось перенести Horizon в %1.").arg(target));
            return;
        }
        QFile marker(FS::PathCombine(target, Horizon::INSTALLED_MARKER));
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write(release.tag.toUtf8());
            marker.close();
        }
        FS::deletePath(QFileInfo(archive).absolutePath());
        log(tr("Horizon %1 установлен в %2").arg(release.displayName(), target));
        installed(release);
    });
    m_unpackWatcher->setFuture(QtConcurrent::run([archive, unpacked] { return MMCZip::extractDir(archive, unpacked); }));
}

void HorizonSetup::installed(const Horizon::Release& release)
{
    const QString previous = m_key;
    m_key = release.key();
    setSetting(Horizon::SETTING_BUILD, m_key);
    setSetting(Horizon::SETTING_TAG, release.tag);
    if (!previous.isEmpty() && previous != m_key)
        removeUnusedBuild(previous);
    afterBuild();
}

void HorizonSetup::removeUnusedBuild(const QString& key)
{
    // Builds are a few hundred megabytes and a new one comes out every few days: an old one
    // goes once no instance plays it.
    auto* instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); i++) {
        auto* instance = instances->at(i);
        if (Horizon::isHorizon(instance) && instance->settings()->get(Horizon::SETTING_BUILD).toString() == key)
            return;
    }
    if (FS::deletePath(Horizon::buildDir(key)))
        log(tr("Удалена старая сборка Horizon %1.").arg(key));
}

// ---- the game files and the resources --------------------------------------------------------

void HorizonSetup::afterBuild()
{
    const QString build = Horizon::buildDir(m_key);
    m_pins = Horizon::readPins(build);
    if (!m_pins.isValid()) {
        fail(tr("В сборке Horizon %1 нет research-lock.json.").arg(m_key));
        return;
    }
    const QString minecraft = instanceMinecraftVersion();
    if (minecraft != m_pins.minecraft) {
        fail(tr("Сборка Horizon сделана для Minecraft %1, а в экземпляре выбрана версия %2. Верните Minecraft %1 на странице «Версия» "
                "экземпляра.")
                 .arg(m_pins.minecraft, minecraft));
        return;
    }
    m_info->buildDir = build;
    m_info->client = FS::PathCombine(build, Horizon::clientProgramName());
    m_info->resources = Horizon::preparedDir(m_pins.minecraft);
    ensureVersionJson();
}

void HorizonSetup::ensureVersionJson()
{
    // Mojang's own version JSON, which no launcher but the official one keeps as is.
    const QString path = FS::PathCombine(Horizon::minecraftDir(m_pins.minecraft), m_pins.minecraft + ".json");
    if (Horizon::fileHash(path, QCryptographicHash::Sha1) == m_pins.manifestSha1) {
        ensureServerJar();
        return;
    }
    if (!m_online || m_pins.manifestSha1.isEmpty()) {
        fail(tr("Нет файла версии Minecraft %1 для Horizon, а скачать его сейчас нельзя.").arg(m_pins.minecraft));
        return;
    }
    setStatus(tr("Скачивание файла версии Minecraft %1...").arg(m_pins.minecraft));
    downloadFile(Horizon::versionJsonUrl(m_pins), path, QCryptographicHash::Sha1, m_pins.manifestSha1,
                 tr("файл версии Minecraft %1").arg(m_pins.minecraft), [this] { ensureServerJar(); });
}

void HorizonSetup::ensureServerJar()
{
    // Preparing runs vanilla's data generator, which is in the server jar.
    const QString dir = Horizon::minecraftDir(m_pins.minecraft);
    const QString jar = FS::PathCombine(dir, "server.jar");
    if (Horizon::fileHash(jar, QCryptographicHash::Sha1) == m_pins.serverSha1) {
        askLaunchApi();
        return;
    }
    QFile file(FS::PathCombine(dir, m_pins.minecraft + ".json"));
    QString url;
    if (file.open(QIODevice::ReadOnly))
        url = QJsonDocument::fromJson(file.readAll()).object().value("downloads").toObject().value("server").toObject().value("url").toString();
    if (url.isEmpty() || !m_online) {
        fail(tr("Нет серверного jar Minecraft %1 для подготовки Horizon, а скачать его сейчас нельзя.").arg(m_pins.minecraft));
        return;
    }
    setStatus(tr("Скачивание сервера Minecraft %1...").arg(m_pins.minecraft));
    downloadFile(url, jar, QCryptographicHash::Sha1, m_pins.serverSha1, tr("сервер Minecraft %1").arg(m_pins.minecraft),
                 [this] { askLaunchApi(); });
}

void HorizonSetup::askLaunchApi()
{
    // Which options this build takes (docs/api/launch.md): any other is refused with exit code 2.
    auto* process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(m_info->buildDir);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (m_aborted)
            return;
        m_info->options.clear();
        const auto reply = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
        if (status != QProcess::NormalExit || code != 0 || !reply.contains("launch_api")) {
            fail(tr("Сборка Horizon %1 не поддерживает запуск из других лаунчеров (Launch API). Включите автообновление в "
                    "настройках экземпляра или создайте экземпляр заново на вкладке Horizon.")
                     .arg(m_key));
            return;
        }
        for (const auto& option : reply.value("options").toArray())
            m_info->options.insert(option.toObject().value("name").toString());
        log(tr("Horizon %1, Launch API %2").arg(reply.value("horizon").toString(), reply.value("launch_api").toString()));
        checkResources();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_aborted)
            return;
        process->deleteLater();
        fail(tr("Не удалось запустить %1: %2").arg(m_info->client, process->errorString()));
    });
    process->start(m_info->client, { "--launch-api" });
    QTimer::singleShot(30000, process, [process] { process->kill(); });
}

void HorizonSetup::checkResources()
{
    // Fast and reads no game files: exit 0 means the resources are complete and current for this build.
    setStatus(tr("Проверка ресурсов Horizon..."));
    const QString program = FS::PathCombine(m_info->buildDir, Horizon::prepareProgramName());
    auto* process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(m_info->buildDir);
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (m_aborted)
            return;
        if (status == QProcess::NormalExit && code == 0) {
            finish();
            return;
        }
        const QString gaps = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        if (!gaps.isEmpty())
            log(gaps);
        prepare();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, program](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_aborted)
            return;
        process->deleteLater();
        fail(tr("Не удалось запустить %1: %2").arg(program, process->errorString()));
    });
    process->start(program, { "--root", m_info->buildDir, "--check", "--output", m_info->resources });
}

void HorizonSetup::prepare()
{
    const QString java = javaHome();
    const QString clientJarPath = clientJar();
    if (!QFileInfo::exists(clientJarPath)) {
        fail(tr("Не найден jar клиента Minecraft %1 (%2).").arg(m_pins.minecraft, clientJarPath));
        return;
    }
    const auto javaVersion = JavaVersion(m_parent->instance()->settings()->get("JavaVersion").toString());
    if (m_pins.javaMajor > 0 && javaVersion.major() != m_pins.javaMajor) {
        log(tr("Для подготовки Horizon нужна Java %1, а у экземпляра Java %2. Если подготовка не пройдёт, выберите Java %1 в настройках "
               "экземпляра.")
                .arg(m_pins.javaMajor)
                .arg(javaVersion.major()),
            MessageLevel::Warning);
    }

    const QString minecraft = Horizon::minecraftDir(m_pins.minecraft);
    QStringList args{ "--root",         m_info->buildDir,
                      "--prepare",      "--output",
                      m_info->resources, "--complete",
                      "--progress",     "json",
                      "--server-jar",   FS::PathCombine(minecraft, "server.jar"),
                      "--version-json", FS::PathCombine(minecraft, m_pins.minecraft + ".json"),
                      "--client-jar",   clientJarPath,
                      "--libraries-dir", QDir::current().absoluteFilePath("libraries"),
                      "--assets-dir",   QDir::current().absoluteFilePath("assets") };
    if (!java.isEmpty())
        args << "--java-home" << java;

    setStatus(tr("Подготовка ресурсов Horizon из Minecraft %1 (первый раз занимает несколько минут)...").arg(m_pins.minecraft));
    setProgress(0, 1000);
    log(tr("Подготовка ресурсов Horizon:\n  %1 %2").arg(Horizon::prepareProgramName(), args.join(' ')));
    FS::ensureFolderPathExists(m_info->resources);

    auto* process = new QProcess(this);
    m_process = process;
    m_stdout.clear();
    m_lastError.clear();
    process->setWorkingDirectory(m_info->buildDir);
    if (!java.isEmpty()) {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("JAVA_HOME", java);
        process->setProcessEnvironment(env);
    }
    // --progress json: every stdout line is one JSON object; stderr is text for people.
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        m_stdout += process->readAllStandardOutput();
        qsizetype newline;
        while ((newline = m_stdout.indexOf('\n')) >= 0) {
            const QByteArray line = m_stdout.left(newline).trimmed();
            m_stdout.remove(0, newline + 1);
            if (line.isEmpty())
                continue;
            const auto object = QJsonDocument::fromJson(line).object();
            if (object.contains("fraction")) {
                setProgress(static_cast<qint64>(object.value("fraction").toDouble() * 1000.0), 1000);
            } else if (object.contains("message")) {
                const QString message = object.value("message").toString();
                setDetails(message);
                log(message, MessageLevel::Info);
            } else if (object.contains("error")) {
                m_lastError = object.value("error").toString();
                log(m_lastError, MessageLevel::Error);
            } else if (object.isEmpty()) {
                log(QString::fromUtf8(line), MessageLevel::StdOut);
            }
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        const QString text = QString::fromUtf8(process->readAllStandardError()).trimmed();
        if (!text.isEmpty())
            emit logLines(text.split('\n'), MessageLevel::StdErr);
    });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (m_aborted)
            return;
        if (status == QProcess::NormalExit && code == 0) {
            log(tr("Ресурсы Horizon готовы."));
            finish();
            return;
        }
        fail(m_lastError.isEmpty() ? tr("Подготовка ресурсов Horizon не удалась (код %1). Подробности — в консоли экземпляра.").arg(code)
                                   : tr("Подготовка ресурсов Horizon не удалась: %1").arg(m_lastError));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_aborted)
            return;
        process->deleteLater();
        fail(tr("Не удалось запустить %1: %2").arg(Horizon::prepareProgramName(), process->errorString()));
    });
    process->start(FS::PathCombine(m_info->buildDir, Horizon::prepareProgramName()), args);
}

void HorizonSetup::finish()
{
    if (!isRunning())
        return;
    setProgress(1, 1);
    emitSucceeded();
}

// ---- helpers ------------------------------------------------------------------------------

static QNetworkRequest makeRequest(const QString& url)
{
    QNetworkRequest request{ QUrl(url) };
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    if (url.startsWith("https://api.github.com/"))
        request.setRawHeader("Accept", "application/vnd.github+json");
    return request;
}

void HorizonSetup::get(const QString& url, Ok ok, const QString& what)
{
    auto request = makeRequest(url);
    request.setTransferTimeout(30000);
    auto* reply = APPLICATION->network()->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, ok, what] {
        reply->deleteLater();
        if (m_aborted)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            const QString why = tr("Не удалось получить %1: %2").arg(what, reply->errorString());
            // Without the network an installed build still plays.
            if (Horizon::isBuildInstalled(m_key) && m_info->client.isEmpty())
                useInstalledOrFail(why);
            else
                fail(why);
            return;
        }
        ok(reply->readAll());
    });
}

void HorizonSetup::downloadFile(const QString& url,
                                const QString& path,
                                QCryptographicHash::Algorithm algorithm,
                                const QString& expected,
                                const QString& what,
                                std::function<void()> ok)
{
    FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
    m_saveFile = std::make_unique<QSaveFile>(path);
    if (!m_saveFile->open(QIODevice::WriteOnly)) {
        fail(tr("Не удалось записать %1: %2").arg(path, m_saveFile->errorString()));
        return;
    }
    m_hash = std::make_unique<QCryptographicHash>(algorithm);
    auto* reply = APPLICATION->network()->get(makeRequest(url));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        const QByteArray data = reply->readAll();
        m_hash->addData(data);
        m_saveFile->write(data);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) { setProgress(received, total); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, expected, what, ok, path] {
        reply->deleteLater();
        if (m_aborted)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            m_saveFile->cancelWriting();
            m_saveFile.reset();
            fail(tr("Не удалось скачать %1: %2").arg(what, reply->errorString()));
            return;
        }
        const QByteArray rest = reply->readAll();
        m_hash->addData(rest);
        m_saveFile->write(rest);
        const QString actual = QString::fromLatin1(m_hash->result().toHex());
        if (actual.compare(expected, Qt::CaseInsensitive) != 0) {
            m_saveFile->cancelWriting();
            m_saveFile.reset();
            fail(tr("Скачанный файл (%1) повреждён: контрольная сумма не совпала.").arg(what));
            return;
        }
        if (!m_saveFile->commit()) {
            const QString error = m_saveFile->errorString();
            m_saveFile.reset();
            fail(tr("Не удалось записать %1: %2").arg(path, error));
            return;
        }
        m_saveFile.reset();
        ok();
    });
}

void HorizonSetup::fail(const QString& reason)
{
    if (!isRunning())
        return;
    emit logLine(reason, MessageLevel::Fatal);
    emitFailed(reason);
}

void HorizonSetup::log(const QString& line, MessageLevel level)
{
    emit logLine(line + '\n', level);
}

QString HorizonSetup::instanceMinecraftVersion() const
{
    return m_parent->instance()->getPackProfile()->getComponentVersion("net.minecraft");
}

QString HorizonSetup::clientJar() const
{
    // The vanilla steps have downloaded it as the library com.mojang:minecraft:<version>:client.
    static const QRegularExpression s_client(R"(com[/\\]mojang[/\\]minecraft[/\\][^/\\]+[/\\]minecraft-[^/\\]+-client\.jar$)");
    for (const auto& jar : m_parent->instance()->getClassPath()) {
        if (s_client.match(jar).hasMatch())
            return QFileInfo(jar).absoluteFilePath();
    }
    const QString version = m_pins.minecraft;
    return QDir::current().absoluteFilePath(QStringLiteral("libraries/com/mojang/minecraft/%1/minecraft-%1-client.jar").arg(version));
}

QString HorizonSetup::javaHome() const
{
    const QString java = FS::ResolveExecutable(m_parent->instance()->settings()->get("JavaPath").toString());
    if (java.isEmpty())
        return {};
    QFileInfo info(java);
    const QString real = info.canonicalFilePath();
    QDir bin = QFileInfo(real.isEmpty() ? info.absoluteFilePath() : real).absoluteDir();
    if (!bin.cdUp())
        return {};
    return bin.absolutePath();
}

void HorizonSetup::setSetting(const char* id, const QVariant& value)
{
    m_parent->instance()->settings()->set(id, value);
}
