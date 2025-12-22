cmake_minimum_required(VERSION 3.16)

# Get version
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VER)

project(hpa VERSION ${VER} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network)
find_package(PkgConfig REQUIRED)
set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  hyprutils
  polkit-agent-1
  polkit-qt6-1)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(noctaliapolkitagent
    src/main.cpp
    src/core/Agent.cpp
    src/core/Agent.hpp
    src/core/PolkitListener.hpp
    src/core/PolkitListener.cpp
    src/QMLIntegration.cpp
    src/QMLIntegration.hpp
)

qt_add_qml_module(noctaliapolkitagent
    URI hpa
    VERSION 1.0
    QML_FILES
        qml/main.qml
    SOURCES
        src/QMLIntegration.cpp
        src/QMLIntegration.hpp
)

target_link_libraries(noctaliapolkitagent
    PRIVATE Qt6::Widgets Qt6::Quick Qt6::Gui Qt6::QuickControls2 PkgConfig::deps
)

include(GNUInstallDirs)

set_target_properties(noctaliapolkitagent PROPERTIES OUTPUT_NAME "noctalia-polkit")

set(LIBEXECDIR ${CMAKE_INSTALL_FULL_LIBEXECDIR})
configure_file(assets/noctalia-polkit.service.in noctalia-polkit.service @ONLY)
configure_file(assets/noctalia-polkit-dbus.service.in noctalia-polkit-dbus.service @ONLY)

install(TARGETS noctaliapolkitagent
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_BINARY_DIR}/noctalia-polkit.service
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/user")
install(FILES ${CMAKE_BINARY_DIR}/noctalia-polkit-dbus.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
        RENAME org.noctalia.polkitagent.service)
