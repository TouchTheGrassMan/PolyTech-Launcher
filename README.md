<p align="center">
  <img alt="Obsidium" src="/program_info/obsidium.source.svg" width="160">
</p>

<h1 align="center">Obsidium</h1>

<p align="center">
  Лаунчер Minecraft для серверов и сборок QU4Dteam: готовая сборка PolyTech, скины, пресеты графики,
  портативная и стационарная установка, а также Horizon — Minecraft: Java Edition, переписанная на Rust.
</p>

Obsidium — <b>форк</b> PineconeMC (ранее ElyPrismLauncher), который, в свою очередь, является форком Prism Launcher.
Obsidium не является ни Prism Launcher, ни PineconeMC и не одобрен их авторами, Ely.by, Mojang или Microsoft.
Раньше проект назывался PolyTech Launcher.

## Установка

Все загрузки — на странице [Releases](https://github.com/TouchTheGrassMan/PolyTech-Launcher/releases/latest):

- `Obsidium-Windows-MSVC-Setup-<версия>.exe` — установщик: при установке можно выбрать портативную или стационарную версию;
- `Obsidium-Windows-MSVC-Portable-<версия>.zip` — портативная версия архивом.

## Сборка

Сборка — как у Prism Launcher ([инструкция](https://prismlauncher.org/wiki/development/build-instructions)), имя программы — `Obsidium`:

```
cmake --preset windows_msvc -DLauncher_APP_BINARY_NAME="Obsidium"
cmake --build --preset windows_msvc --config Debug
```

## The following comes from the original Prism Launcher README

### Forking/Redistributing/Custom builds policy

You are free to fork, redistribute and provide custom builds as long as you follow the terms of the [license](LICENSE) (this is a legal responsibility), and if you made code changes rather than just packaging a custom build, please do the following as a basic courtesy:

- Make it clear that your fork is not Prism Launcher and is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).
- Go through [CMakeLists.txt](CMakeLists.txt) and change Prism Launcher's API keys to your own or set them to empty strings (`""`) to disable them (this way the program will still compile but the functionality requiring those keys will be disabled).

If you have any questions or want any clarification on the above conditions please make an issue and ask us.

If you are just building Prism Launcher for your distribution, please make sure to set the `Launcher_BUILD_PLATFORM` to a slug representing your distribution. Examples are `archlinux`, `fedora` and `nixpkgs`.

Note that if you build this software without removing the provided API keys in [CMakeLists.txt](CMakeLists.txt) you are accepting the following terms and conditions:

- [Microsoft Identity Platform Terms of Use](https://docs.microsoft.com/en-us/legal/microsoft-identity-platform/terms-of-use)
- [CurseForge 3rd Party API Terms and Conditions](https://support.curseforge.com/en/support/solutions/articles/9000207405-curse-forge-3rd-party-api-terms-and-conditions)

If you do not agree with these terms and conditions, then remove the associated API keys from the [CMakeLists.txt](CMakeLists.txt) file by setting them to an empty string (`""`).

### License [![https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE](https://img.shields.io/github/license/PrismLauncher/PrismLauncher?label=License&logo=gnu&color=C4282D)](LICENSE)

All launcher code is available under the GPL-3.0-only license.

The logo and related assets are under the CC BY-SA 4.0 license.
