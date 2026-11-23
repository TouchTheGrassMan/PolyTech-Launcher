// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QStringList>
#include <cstdint>

/**
 * PolyTech: how this computer compares with the pack's needs, for the graphics preset chooser.
 *
 * The processor and the video cards the launcher finds (HardwareInfo) are looked up in benchmark
 * tables (resources/hardware, made by tools/hardware_tables.py): a CPU Mark for the processor and
 * a G3D Mark for the video card. The minimum is the Microsoft Surface Pro 6 (Core i5-8250U, UHD
 * Graphics 620) with 8500 MB of memory free when the game starts; anything weaker is told it will
 * likely struggle. The suggested preset follows from the weaker of the two parts.
 */
namespace HardwareRating {

/// The Surface Pro 6: Intel UHD Graphics 620 and Intel Core i5-8250U.
inline constexpr int MIN_GPU_MARK = 1038;
inline constexpr int MIN_CPU_MARK = 5779;
/// Memory that must be free when the game starts, MiB.
inline constexpr uint64_t MIN_FREE_RAM_MIB = 8500;

/// Where a preset starts: the video card's G3D Mark and the processor's CPU Mark.
/// "creeper": GTX 1050 Ti, GTX 1650, Radeon 780M and better with a 4-core desktop or a recent laptop processor.
inline constexpr int CREEPER_GPU_MARK = 5000;
inline constexpr int CREEPER_CPU_MARK = 7000;
/// "reactor" (shaders, Distant Horizons): RTX 2060, RTX 3060 (and its laptop version), RX 6600 and better
/// with a 6-core processor.
inline constexpr int REACTOR_GPU_MARK = 13000;
inline constexpr int REACTOR_CPU_MARK = 11000;

struct Part {
    QString name;   // as the system calls it, without (R), (TM) and the like; empty if not found
    int mark = -1;  // the benchmark score; -1 if the part is not in the table
    bool known() const { return mark >= 0; }
};

struct Assessment {
    Part cpu;
    Part gpu;  // the strongest of the video cards found
    uint64_t totalRamMiB = 0;
    uint64_t availableRamMiB = 0;

    bool cpuBelowMinimum = false;
    bool gpuBelowMinimum = false;
    bool ramBelowMinimum = false;
    bool meetsMinimum() const { return !cpuBelowMinimum && !gpuBelowMinimum && !ramBelowMinimum; }

    /// potato / creeper / reactor
    QString recommended;

    /// One line for the launch log.
    QString describe() const;
};

/// This computer, as HardwareInfo sees it. Asks Vulkan and OpenGL for the video cards, so it takes
/// a moment and is called from the GUI thread.
Assessment assessThisComputer();

/// The same from given data (`gpuLines` as HardwareInfo::gpuInfo() writes them).
Assessment assess(const QString& cpuName, const QStringList& gpuLines, uint64_t totalRamMiB, uint64_t availableRamMiB);

}  // namespace HardwareRating
