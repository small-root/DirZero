#pragma once

#include <QString>

enum class AuthState {
    NotAttempted,
    Authenticating,
    Success,
    MissingCredentials,
    InvalidPassword,
    InvalidKey,
    HostKeyMismatch,
    ConnectionTimeout,
    PermissionDenied,
    UnknownError
};

inline QString authStateToString(AuthState state) {
    switch (state) {
        case AuthState::NotAttempted: return QStringLiteral("NOT_ATTEMPTED");
        case AuthState::Authenticating: return QStringLiteral("AUTHENTICATING");
        case AuthState::Success: return QStringLiteral("SUCCESS");
        case AuthState::MissingCredentials: return QStringLiteral("MISSING_CREDENTIALS");
        case AuthState::InvalidPassword: return QStringLiteral("INVALID_PASSWORD");
        case AuthState::InvalidKey: return QStringLiteral("INVALID_KEY");
        case AuthState::HostKeyMismatch: return QStringLiteral("HOST_KEY_MISMATCH");
        case AuthState::ConnectionTimeout: return QStringLiteral("CONNECTION_TIMEOUT");
        case AuthState::PermissionDenied: return QStringLiteral("PERMISSION_DENIED");
        case AuthState::UnknownError: return QStringLiteral("UNKNOWN_ERROR");
    }
    return QStringLiteral("UNKNOWN");
}

inline QString authStateDisplayMessage(AuthState state) {
    switch (state) {
        case AuthState::NotAttempted: return QStringLiteral("Authentication not attempted.");
        case AuthState::Authenticating: return QStringLiteral("Authenticating credentials...");
        case AuthState::Success: return QStringLiteral("Authentication successful.");
        case AuthState::MissingCredentials: return QStringLiteral("Authentication required: missing username, key, or password.");
        case AuthState::InvalidPassword: return QStringLiteral("Authentication failed: invalid password.");
        case AuthState::InvalidKey: return QStringLiteral("Authentication failed: invalid or unreadable SSH private key.");
        case AuthState::HostKeyMismatch: return QStringLiteral("Security warning: remote host key mismatch.");
        case AuthState::ConnectionTimeout: return QStringLiteral("Connection timed out while authenticating.");
        case AuthState::PermissionDenied: return QStringLiteral("Permission denied: insufficient credentials.");
        case AuthState::UnknownError: return QStringLiteral("Authentication error occurred.");
    }
    return QStringLiteral("Authentication error.");
}
