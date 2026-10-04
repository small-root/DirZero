# DirZero

A Qt 6 desktop file manager for direct, cloudless(as in without centralized storage) file transfer between machines over Tailscale using SSH/SFTP.

## Description
**DirZero** is a Qt 6 desktop file manager for direct file transfers between machines connected through a Tailscale network. It provides a graphical interface for discovering reachable machines, browsing remote filesystems, and transferring files over SSH/SFTP; allowing devices to exchange files directly over their private network without relying on cloud storage or a centralized file-transfer service.


## Install on Linux

From a checkout or source archive, run:

```sh
./install.sh
```

The installer displays the GPLv3 license and requires acceptance, installs the
build dependencies using `apt`, `dnf`, or `pacman`, builds DirZero, and installs
it for the current user. It adds a launcher to the desktop application menu and
a `dirzero` command under `~/.local/bin`. No root permission is needed for the
application itself; `sudo` is only used to install missing system packages.

## Install on Windows

Windows support is currently in development. A pre-built Windows installer (.exe) is planned and will be made available once the Windows build and packaging workflow has been fully tested.

If you need to run DirZero on Windows right now, you can build it directly from source. The project is designed to be cross-platform, and no Windows-specific source modifications should be required for a standard build.

## Build from source

Install CMake, a C++17 compiler, Qt 6 (`Core`, `Gui`, `Widgets`, `Network`),
`pkg-config`, and libssh development files, then run:

```sh
cmake -S . -B build
cmake --build build --parallel
./build/src/DirZero
```
#####  **Development & Liability Disclaimer**
Parts of this project are **AI-assisted / vibe-coded**, while other parts are manually written, reviewed, and integrated by the maintainers. The use of AI does not guarantee that the generated code is correct, secure, or suitable for production use. Clarfied for those AI-Phobic devs.

DirZero is provided **"as is" without warranty of any kind**. The maintainers are not responsible for any data loss, system damage, security issues, misconfiguration, or other consequences resulting from the use, modification, or deployment of this software. **Use it at your own risk and review the code before using it with important systems or data.**


DirZero is distributed under the GNU General Public License version 3. See
[LICENSE](LICENSE).
