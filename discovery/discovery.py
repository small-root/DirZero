import json
import socket
import subprocess


def get_online_machines():
    """Get online machines from Tailscale."""
    try:
        result = subprocess.run(
            ["tailscale", "status", "--json"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return {}

    try:
        status = json.loads(result.stdout)
    except json.JSONDecodeError:
        return {}

    machines = {}

    peers = status.get("Peer") or {}

    for machine in peers.values():
        if not machine.get("Online", False):
            continue

        name = machine.get("HostName")
        ips = machine.get("TailscaleIPs", [])

        if not name or not ips:
            continue

        ip = next(
            (address for address in ips if "." in address),
            ips[0],
        )

        machines[name] = ip

    return machines


def is_port_22_reachable(ip, timeout=2):
    """Check whether SSH port 22 is reachable."""
    try:
        with socket.create_connection((ip, 22), timeout=timeout):
            return True
    except (socket.timeout, socket.error):
        return False


def discover_ssh_machines():
    """Return online Tailscale machines with reachable port 22."""
    machines = get_online_machines()

    reachable_machines = {}

    for name, ip in machines.items():
        if is_port_22_reachable(ip):
            reachable_machines[name] = ip

    return reachable_machines


if __name__ == "__main__":
    machines = discover_ssh_machines()
    print(machines)