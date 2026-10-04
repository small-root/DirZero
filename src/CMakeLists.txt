cmake_minimum_required(VERSION 3.21)

project(DirZero LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)

set(DIRZERO_SOURCES
    main.cpp

    core/MachineInfo.cpp
    core/AccessRule.cpp
    core/PermissionPolicy.cpp

    security/CredentialStore.cpp

    fs/FSNode.cpp
    fs/SFTPManager.cpp
    fs/RemoteFSModel.cpp

    discovery/TailscaleDiscovery.cpp
    discovery/ProbeWorker.cpp

    workers/DiscoveryWorker.cpp
    workers/SFTPConnectWorker.cpp
    workers/SFTPListWorker.cpp
    workers/SFTPFileOpWorker.cpp
    workers/CrossMachineTransferWorker.cpp

    helpers/RemoteClipboard.cpp
    helpers/ResponsiveGridContainer.cpp

    ui/ThemeManager.cpp
    ui/MachineAuthBar.cpp
    ui/RemoteFileTreeView.cpp
    ui/MachinePanelWidget.cpp
    ui/Dir2ZeroWindow.cpp
)

set(DIRZERO_HEADERS
    core/MachineState.hpp
    core/MachineInfo.hpp
    core/AuthState.hpp
    core/AccessRule.hpp
    core/PermissionPolicy.hpp

    security/CredentialStore.hpp

    fs/RemoteEntry.hpp
    fs/FSNode.hpp
    fs/SFTPManager.hpp
    fs/RemoteFSModel.hpp

    discovery/TailscaleDiscovery.hpp
    discovery/ProbeWorker.hpp

    workers/WorkerSignals.hpp
    workers/DiscoveryWorker.hpp
    workers/SFTPConnectWorker.hpp
    workers/SFTPListWorker.hpp
    workers/SFTPFileOpWorker.hpp
    workers/CrossMachineTransferWorker.hpp

    helpers/RemoteClipboard.hpp
    helpers/ResponsiveGridContainer.hpp

    ui/ThemeManager.hpp
    ui/MachineAuthBar.hpp
    ui/RemoteFileTreeView.hpp
    ui/MachinePanelWidget.hpp
    ui/Dir2ZeroWindow.hpp
)

add_executable(DirZero
    ${DIRZERO_SOURCES}
    ${DIRZERO_HEADERS}
)

target_include_directories(DirZero PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/security
    ${CMAKE_CURRENT_SOURCE_DIR}/fs
    ${CMAKE_CURRENT_SOURCE_DIR}/discovery
    ${CMAKE_CURRENT_SOURCE_DIR}/workers
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(DirZero PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    PkgConfig::LIBSSH
)

if(MSVC)
    target_compile_options(DirZero PRIVATE /W4)
else()
    target_compile_options(DirZero PRIVATE
        -Wall
        -Wextra
    )
endif()