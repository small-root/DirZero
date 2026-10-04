#pragma once

#include <QString>

enum class MachineState {
    Discovering,
    OnlineSshOk,
    OnlineSshUnavailable,
    AuthRequired,
    Connecting,
    Connected,
    Error,
    Disconnected,
    Offline
};

inline QString machineStateToString(MachineState state) {
    switch (state) {
        case MachineState::Discovering: return QStringLiteral("DISCOVERING");
        case MachineState::OnlineSshOk: return QStringLiteral("ONLINE_SSH_OK");
        case MachineState::OnlineSshUnavailable: return QStringLiteral("ONLINE_SSH_UNAVAILABLE");
        case MachineState::AuthRequired: return QStringLiteral("AUTH_REQUIRED");
        case MachineState::Connecting: return QStringLiteral("CONNECTING");
        case MachineState::Connected: return QStringLiteral("CONNECTED");
        case MachineState::Error: return QStringLiteral("ERROR");
        case MachineState::Disconnected: return QStringLiteral("DISCONNECTED");
        case MachineState::Offline: return QStringLiteral("OFFLINE");
    }
    return QStringLiteral("UNKNOWN");
}

inline MachineState machineStateFromString(const QString& str) {
    if (str == QStringLiteral("DISCOVERING")) return MachineState::Discovering;
    if (str == QStringLiteral("ONLINE_SSH_OK")) return MachineState::OnlineSshOk;
    if (str == QStringLiteral("ONLINE_SSH_UNAVAILABLE")) return MachineState::OnlineSshUnavailable;
    if (str == QStringLiteral("AUTH_REQUIRED")) return MachineState::AuthRequired;
    if (str == QStringLiteral("CONNECTING")) return MachineState::Connecting;
    if (str == QStringLiteral("CONNECTED")) return MachineState::Connected;
    if (str == QStringLiteral("ERROR")) return MachineState::Error;
    if (str == QStringLiteral("DISCONNECTED")) return MachineState::Disconnected;
    return MachineState::Offline;
}

inline QString machineStateDisplayName(MachineState state) {
    switch (state) {
        case MachineState::Discovering: return QStringLiteral("● DISCOVERING...");
        case MachineState::OnlineSshOk: return QStringLiteral("● ONLINE — SSH READY");
        case MachineState::OnlineSshUnavailable: return QStringLiteral("⚠ ONLINE — SSH UNAVAILABLE");
        case MachineState::AuthRequired: return QStringLiteral("🔑 AUTHENTICATION REQUIRED");
        case MachineState::Connecting: return QStringLiteral("● CONNECTING...");
        case MachineState::Connected: return QStringLiteral("● CONNECTED");
        case MachineState::Error: return QStringLiteral("✕ CONNECTION FAILED");
        case MachineState::Disconnected: return QStringLiteral("○ DISCONNECTED");
        case MachineState::Offline: return QStringLiteral("○ OFFLINE");
    }
    return QStringLiteral("UNKNOWN");
}

inline QString machineStateColor(MachineState state) {
    switch (state) {
        case MachineState::Discovering: return QStringLiteral("#38bdf8");
        case MachineState::OnlineSshOk: return QStringLiteral("#10b981");
        case MachineState::OnlineSshUnavailable: return QStringLiteral("#f59e0b");
        case MachineState::AuthRequired: return QStringLiteral("#f59e0b");
        case MachineState::Connecting: return QStringLiteral("#38bdf8");
        case MachineState::Connected: return QStringLiteral("#10b981");
        case MachineState::Error: return QStringLiteral("#ef4444");
        case MachineState::Disconnected: return QStringLiteral("#94a3b8");
        case MachineState::Offline: return QStringLiteral("#64748b");
    }
    return QStringLiteral("#94a3b8");
}