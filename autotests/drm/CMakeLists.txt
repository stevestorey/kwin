set(mockDRM_SRCS
    mock_drm.cpp
    ../../src/plugins/platforms/drm/drm_backend.cpp
    ../../src/plugins/platforms/drm/drm_object.cpp
    ../../src/plugins/platforms/drm/drm_object_connector.cpp
    ../../src/plugins/platforms/drm/drm_object_crtc.cpp
    ../../src/plugins/platforms/drm/drm_object_plane.cpp
    ../../src/plugins/platforms/drm/drm_output.cpp
    ../../src/plugins/platforms/drm/drm_buffer.cpp
    ../../src/plugins/platforms/drm/edid.cpp
    ../../src/plugins/platforms/drm/logging.cpp
    ../../src/plugins/platforms/drm/scene_qpainter_drm_backend.cpp
    ../../src/plugins/platforms/drm/drm_gpu.cpp
    ../../src/plugins/platforms/drm/egl_multi_backend.cpp
    ../../src/plugins/platforms/drm/abstract_egl_drm_backend.cpp
    ../../src/plugins/platforms/drm/dumb_swapchain.cpp
    ../../src/plugins/platforms/drm/shadowbuffer.cpp
    ../../src/plugins/platforms/drm/drm_pipeline.cpp
    ../../src/plugins/platforms/drm/drm_abstract_output.cpp
    ../../src/plugins/platforms/drm/drm_virtual_output.cpp
)
if (HAVE_GBM)
    set(mockDRM_SRCS ${mockDRM_SRCS}
        ../../src/plugins/platforms/drm/egl_gbm_backend.cpp
        ../../src/plugins/platforms/drm/drm_buffer_gbm.cpp
        ../../src/plugins/platforms/drm/gbm_surface.cpp
        ../../src/plugins/platforms/drm/gbm_dmabuf.cpp
    )
endif()

if (HAVE_EGL_STREAMS)
    set(mockDRM_SRCS ${mockDRM_SRCS}
        ../../src/plugins/platforms/drm/egl_stream_backend.cpp
    )
endif()

include_directories(${Libdrm_INCLUDE_DIRS})

find_package(libxcvt 0.1.0)
set_package_properties(libxcvt PROPERTIES TYPE REQUIRED PURPOSE "Required for generating mode info on Wayland")


add_library(LibDrmTest STATIC ${mockDRM_SRCS})
target_link_libraries(LibDrmTest Qt::Gui Qt::Widgets KF5::ConfigCore KF5::WindowSystem KF5::CoreAddons KF5::I18n Plasma::KWaylandServer XCB::XCB libxcvt::libxcvt kwin SceneQPainterBackend SceneOpenGLBackend VsyncSupport)
if (HAVE_GBM)
    target_link_libraries(LibDrmTest gbm::gbm)
endif()
target_include_directories(LibDrmTest PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/platformsupport/scenes/opengl ${CMAKE_SOURCE_DIR}/src/platformsupport/scenes/qpainter ${CMAKE_SOURCE_DIR}/src/plugins/platforms/drm/)

########################################################
# Tests
########################################################
add_executable(testDrm drmTest.cpp)
target_link_libraries(testDrm LibDrmTest Qt::Test)
add_test(NAME kwin-testDrm COMMAND testDrm)
ecm_mark_as_test(testDrm)
