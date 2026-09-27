#pragma once
enum class MachineState
{
    Offline,
    Online,
    SSHUnreachable,
    AuthRequired,
    Connected,
    NotSupported,
    Error
};