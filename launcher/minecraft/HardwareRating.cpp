// SPDX-License-Identifier: GPL-3.0-only
#include "HardwareRating.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

#include "HardwareInfo.h"

namespace HardwareRating {

namespace {

struct Row {
    int mark = 0;
    int samples = 0;
};
using Table = QHash<QString, Row>;

/// A table of resources/hardware: key, score, [single thread score,] samples - tab separated.
Table loadTable(const QString& path)
{
    Table table;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Hardware rating table is missing:" << path;
        return table;
    }
    while (!file.atEnd()) {
        const QList<QByteArray> cols = file.readLine().trimmed().split('\t');
        if (cols.size() < 3)
            continue;
        Row row;
        row.mark = cols[1].toInt();
        row.samples = cols.last().toInt();
        table.insert(QString::fromLatin1(cols[0]), row);
    }
    return table;
}

const Table& gpuTable()
{
    static const Table table = loadTable(QStringLiteral(":/hardware/gpus.tsv"));
    return table;
}

const Table& cpuTable()
{
    static const Table table = loadTable(QStringLiteral(":/hardware/cpus.tsv"));
    return table;
}

QString collapse(QString s)
{
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    return s.replace(spaces, QStringLiteral(" ")).trimmed();
}

/// "Intel(R) Core(TM)" -> "Intel Core"
QString withoutMarks(QString s)
{
    static const QRegularExpression marks(QStringLiteral("\\((r|tm|c)\\)|[\\x{00AE}\\x{2122}\\x{00A9}]"),
                                          QRegularExpression::CaseInsensitiveOption);
    return collapse(s.replace(marks, QString()));
}

/// The lookup key of a name: lower case letters and digits only, as in the tables.
QString keyOf(const QString& name)
{
    QString key;
    for (const QChar c : name.toLower()) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            key += c;
    }
    return key;
}

/// "Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz" -> "Intel Core i5-8250U",
/// "AMD Ryzen 5 5600X 6-Core Processor" -> "AMD Ryzen 5 5600X", "12th Gen Intel(R) Core(TM) i5-12400F" -> "Intel Core i5-12400F".
QString cleanCpuName(const QString& raw)
{
    static const QList<QRegularExpression> noise = {
        QRegularExpression(QStringLiteral("@.*$")),
        QRegularExpression(QStringLiteral("\\b(with|w/)\\s+radeon.*$"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("\\b\\d+(st|nd|rd|th)\\s+gen(eration)?\\b"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("\\b\\w+-core\\b|\\b(cpu|processor)\\b"), QRegularExpression::CaseInsensitiveOption),
    };
    QString name = withoutMarks(raw);
    for (const auto& re : noise)
        name.replace(re, QString());
    return collapse(name);
}

/// "NVIDIA GeForce RTX 3060/PCIe/SSE2" -> "NVIDIA GeForce RTX 3060",
/// "Mesa Intel(R) UHD Graphics 620 (KBL GT2)" -> "Intel UHD Graphics 620".
QString cleanGpuName(const QString& raw)
{
    static const QRegularExpression brackets(QStringLiteral("\\([^)]*\\)"));
    static const QRegularExpression mesa(QStringLiteral("^mesa\\s+"), QRegularExpression::CaseInsensitiveOption);
    QString name = withoutMarks(raw);
    name = name.section('/', 0, 0);
    name.replace(brackets, QString());
    name.replace(mesa, QString());
    return collapse(name);
}

/// A tail that still names the same chip: "8gb", "oem", the core count of an Apple chip...
bool sameChipTail(const QString& tail)
{
    static const QRegularExpression ok(QStringLiteral("^(\\d+gb|\\d+core.*|oem|gpu|graphics)$"));
    return ok.match(tail).hasMatch();
}

/**
 * The best row of `table` for any of the `keys` (in the order given):
 * the key itself; else the longest key of the table it begins with ("intelirisxegraphics" -> "intelirisxe",
 * "geforcegtx10606gb" -> "geforcegtx1060", but never "radeonrx5700" -> "radeonrx570"); else the most tested variant of it ("geforcertx3050" ->
 * "geforcertx30508gb").
 */
bool lookup(const Table& table, const QStringList& keys, Row* out)
{
    static const QRegularExpression memoryTail(QStringLiteral("^\\d+gb"));
    for (const QString& key : keys) {
        if (auto it = table.constFind(key); it != table.cend()) {
            *out = *it;
            return true;
        }
    }
    for (const QString& key : keys) {
        qsizetype bestLength = 0;
        for (auto it = table.cbegin(); it != table.cend(); ++it) {
            const QString& k = it.key();
            if (k.size() < 8 || k.size() <= bestLength || !key.startsWith(k))
                continue;
            // "radeonrx5700" is not a "radeonrx570", but "geforcegtx10606gb" is a "geforcegtx1060"
            if (key.size() > k.size() && key.at(k.size()).isDigit() && !memoryTail.match(key.mid(k.size())).hasMatch())
                continue;
            bestLength = k.size();
            *out = *it;
        }
        if (bestLength > 0)
            return true;
    }
    for (const QString& key : keys) {
        if (key.size() < 8)
            continue;
        bool found = false;
        for (auto it = table.cbegin(); it != table.cend(); ++it) {
            const QString& k = it.key();
            if (!k.startsWith(key) || !sameChipTail(k.mid(key.size())))
                continue;
            if (!found || it->samples > out->samples)
                *out = *it;
            found = true;
        }
        if (found)
            return true;
    }
    return false;
}

QStringList gpuKeys(const QString& name)
{
    const QString key = keyOf(name);
    QStringList keys{ key };
    // the tables name NVIDIA and AMD cards without the maker, Intel ones with it
    static const QRegularExpression maker(QStringLiteral("^(nvidia|amd|ati|advancedmicrodevices(inc)?)"));
    QString bare = key;
    bare.replace(maker, QString());
    if (bare != key)
        keys << bare;
    if (!bare.startsWith(QLatin1String("intel")) && (bare.startsWith(QLatin1String("uhd")) || bare.startsWith(QLatin1String("iris")) ||
                                                     bare.startsWith(QLatin1String("hdgraphics")) || bare.startsWith(QLatin1String("arc"))))
        keys << QStringLiteral("intel") + bare;
    return keys;
}

/// Graphics that do not say which chip they are: Ryzen APUs ("AMD Radeon(TM) Graphics").
bool isUnnamedRadeon(const QString& name)
{
    static const QRegularExpression unnamed(QStringLiteral("^(amd)?radeon(vega)?graphics$"));
    return unnamed.match(keyOf(name)).hasMatch();
}

/// The table lists such graphics by processor: "Ryzen 5 5600G with Radeon Graphics".
bool lookupApu(const QString& cpuRaw, Row* out)
{
    static const QRegularExpression amd(QStringLiteral("^amd"));
    QString full = keyOf(withoutMarks(cpuRaw).section('@', 0, 0));
    full.replace(amd, QString());
    QString model = keyOf(cleanCpuName(cpuRaw));
    model.replace(amd, QString());
    const QStringList keys{ full, model + QStringLiteral("withradeongraphics") };
    for (const QString& key : keys) {
        if (auto it = gpuTable().constFind(key); it != gpuTable().cend()) {
            *out = *it;
            return true;
        }
    }
    return false;
}

bool isSoftwareRenderer(const QString& name)
{
    static const QRegularExpression software(
        QStringLiteral("llvmpipe|softpipe|swiftshader|basic render|gdi generic|virtualbox|vmware|parallels"),
        QRegularExpression::CaseInsensitiveOption);
    return software.match(name).hasMatch();
}

/// The video card names in HardwareInfo::gpuInfo().
QStringList gpuNames(const QStringList& lines)
{
    static const QRegularExpression vulkan(QStringLiteral("^Found Vulkan device: (.*) \\(API version"));
    static const QString openGl = QStringLiteral("OpenGL renderer: ");
    QStringList names;
    for (const QString& line : lines) {
        QString raw;
        if (const auto m = vulkan.match(line); m.hasMatch())
            raw = m.captured(1);
        else if (line.startsWith(openGl))
            raw = line.mid(openGl.size());
        const QString name = cleanGpuName(raw);
        if (!name.isEmpty() && !isSoftwareRenderer(name) && !names.contains(name, Qt::CaseInsensitive))
            names << name;
    }
    return names;
}

int tierOf(const Part& part, int creeper, int reactor, int unknownTier)
{
    if (!part.known())
        return unknownTier;
    return part.mark >= reactor ? 2 : part.mark >= creeper ? 1 : 0;
}

}  // namespace

Assessment assess(const QString& cpuName, const QStringList& gpuLines, uint64_t totalRamMiB, uint64_t availableRamMiB)
{
    Assessment a;
    a.totalRamMiB = totalRamMiB;
    a.availableRamMiB = availableRamMiB;

    if (!cpuName.isEmpty() && cpuName != QLatin1String("unknown")) {
        a.cpu.name = cleanCpuName(cpuName);
        const QString key = keyOf(a.cpu.name);
        QStringList keys{ key };
        // tables say "Intel Core i5-8250U", "AMD Ryzen 5 5600X"
        if (!key.startsWith(QLatin1String("intel")) && key.startsWith(QLatin1String("core")))
            keys << QStringLiteral("intel") + key;
        if (!key.startsWith(QLatin1String("amd")) && key.startsWith(QLatin1String("ryzen")))
            keys << QStringLiteral("amd") + key;
        Row row;
        if (!key.isEmpty() && lookup(cpuTable(), keys, &row))
            a.cpu.mark = row.mark;
    }

    // a laptop has two: the game runs on the stronger one
    for (const QString& name : gpuNames(gpuLines)) {
        Row row;
        const bool found = isUnnamedRadeon(name) ? lookupApu(cpuName, &row) : lookup(gpuTable(), gpuKeys(name), &row);
        if (found && row.mark > a.gpu.mark) {
            a.gpu.name = name;
            a.gpu.mark = row.mark;
        } else if (a.gpu.name.isEmpty()) {
            a.gpu.name = name;  // not in the table: still say what it is
        }
    }

    a.cpuBelowMinimum = a.cpu.known() && a.cpu.mark < MIN_CPU_MARK;
    a.gpuBelowMinimum = a.gpu.known() && a.gpu.mark < MIN_GPU_MARK;
    a.ramBelowMinimum = availableRamMiB > 0 && availableRamMiB < MIN_FREE_RAM_MIB;

    // An unknown processor does not hold the choice back; an unknown video card does not get shaders.
    int tier = std::min(tierOf(a.gpu, CREEPER_GPU_MARK, REACTOR_GPU_MARK, 1), tierOf(a.cpu, CREEPER_CPU_MARK, REACTOR_CPU_MARK, 2));
    if (!a.meetsMinimum())
        tier = 0;
    static const char* const PRESETS[] = { "potato", "creeper", "reactor" };
    a.recommended = QString::fromLatin1(PRESETS[tier]);
    return a;
}

Assessment assessThisComputer()
{
    return assess(HardwareInfo::cpuInfo(), HardwareInfo::gpuInfo(), HardwareInfo::totalRamMiB(), HardwareInfo::availableRamMiB());
}

QString Assessment::describe() const
{
    auto part = [](const Part& p, const char* score) {
        if (p.name.isEmpty())
            return QStringLiteral("not found");
        return p.known() ? QStringLiteral("%1 (%2 %3)").arg(p.name, QLatin1String(score)).arg(p.mark)
                         : QStringLiteral("%1 (not in the table)").arg(p.name);
    };
    QStringList below;
    if (cpuBelowMinimum)
        below << QStringLiteral("CPU");
    if (gpuBelowMinimum)
        below << QStringLiteral("GPU");
    if (ramBelowMinimum)
        below << QStringLiteral("free RAM");
    return QStringLiteral("CPU %1, GPU %2, RAM %3 MiB (%4 MiB free); minimum %5; suggested preset: %6")
        .arg(part(cpu, "CPU Mark"), part(gpu, "G3D Mark"))
        .arg(totalRamMiB)
        .arg(availableRamMiB)
        .arg(below.isEmpty() ? QStringLiteral("met") : QStringLiteral("not met (%1)").arg(below.join(QStringLiteral(", "))),
             recommended);
}

}  // namespace HardwareRating
