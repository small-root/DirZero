import os.path
import posixpath
import time
import shutil
import subprocess


if shutil.which("apt"):
    package_manager = "apt"
    subprocess.run(["sudo", "apt", "install", "-y", "python3"])
    print("Machine is Debian based")
elif shutil.which("dnf"):
    package_manager = "dnf"
    subprocess.run(["sudo", "dnf", "install", "-y", "python3"])
    print("Machine is by RHEL")
elif shutil.which("pacman"):
    package_manager = "pacman"
    subprocess.run(["sudo", "pacman", "-S", "--no-confirm", "python3"])
    print("Machine is Arch based")
elif shutil.which("xbps-install"):
    package_manager = "xbps-install"
    subprocess.run(["sudo",])
    print("Machine is Void based")
elif shutil.which("emerge"):
    package_manager = "emerge"
    print("Machine is Gentoo")
else:
    print("Invalid state for package manager!")


print(f"PKG MANAGER: {package_manager}")


