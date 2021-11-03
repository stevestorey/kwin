set(FBDEV_SOURCES
    fb_backend.cpp
    fb_qpainter_backend.cpp
    logging.cpp
)

add_library(KWinWaylandFbdevBackend MODULE ${FBDEV_SOURCES})
set_target_properties(KWinWaylandFbdevBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandFbdevBackend kwin)

install(
    TARGETS
        KWinWaylandFbdevBackend
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kwin.waylandbackends/
)
