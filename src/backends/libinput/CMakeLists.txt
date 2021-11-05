qt5_generate_dbus_interface(device.h org.kde.kwin.InputDevice.xml OPTIONS -A)

add_custom_target(
    KWinInputDBusInterfaces
    ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.InputDevice.xml
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.InputDevice.xml
    DESTINATION
        ${KDE_INSTALL_DBUSINTERFACEDIR}
)

target_sources(kwin PRIVATE
    connection.cpp
    context.cpp
    device.cpp
    events.cpp
    libinput_logging.cpp
    libinputbackend.cpp
)
target_link_libraries(kwin Libinput::Libinput)

qt_add_dbus_interface(cli_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.InputDevice.xml InputDevice_interface)
add_executable(kwin-inputdevice kwininputdevice-cli.cpp ${cli_SRCS})
target_link_libraries(kwin-inputdevice Qt::DBus Qt::Core KF5::CoreAddons KF5::I18n)
install(TARGETS kwin-inputdevice ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
