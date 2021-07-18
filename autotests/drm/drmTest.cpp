/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "mock_drm.h"

#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_pointer.h"
#include "drm_gpu.h"
#include "egl_gbm_backend.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_pipeline.h"

namespace KWin
{

class DrmTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAmsDetection();
    void testPipeline();
    void testPipelineLegacy();
    void testOutputDetectionAMS();
    void testOutputDetectionLegacy();
    void testVrDetectionAMS();
    void testVrDetectionLegacy();
};

static void verifyCleanup(const QScopedPointer<MockGpu> &mockGpu) {
    QVERIFY(mockGpu->drmConnectors.isEmpty());
    QVERIFY(mockGpu->drmEncoders.isEmpty());
    QVERIFY(mockGpu->drmCrtcs.isEmpty());
    QVERIFY(mockGpu->drmPlanes.isEmpty());
    QVERIFY(mockGpu->drmPlaneRes.isEmpty());
    QVERIFY(mockGpu->fbs.isEmpty());
    QVERIFY(mockGpu->drmProps.isEmpty());
    QVERIFY(mockGpu->drmObjectProperties.isEmpty());
    // verifying the behavior of the mock layer in legacy mode may cause false negatives
    if (mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] == 1) {
        QVERIFY(mockGpu->drmPropertyBlobs.isEmpty());
    }
}

void DrmTest::testAmsDetection()
{
    QScopedPointer<MockGpu> mockGpu(new MockGpu(1, 0));

    QScopedPointer<DrmBackend> backend(new DrmBackend());
    QScopedPointer<DrmGpu> gpu(new DrmGpu(backend.get(), "legacy", 1, 0));
    QVERIFY(!gpu->atomicModeSetting());

    // gpu without planes should use legacy mode
    gpu.reset(new DrmGpu(backend.get(), "legacy", 1, 0));
    QVERIFY(!gpu->atomicModeSetting());

    // gpu with planes should use AMS
    auto plane = QSharedPointer<MockPlane>::create(mockGpu.get(), PlaneType::Primary, 0);
    mockGpu->planes << plane;
    gpu.reset(new DrmGpu(backend.get(), "AMS", 1, 0));
    QVERIFY(gpu->atomicModeSetting());

    gpu.reset();
    verifyCleanup(mockGpu);
}

void DrmTest::testPipeline()
{
    QScopedPointer<MockGpu> mockGpu(new MockGpu(1, 1));
    auto mockCrtc = mockGpu->crtcs[0];
    auto mockPlane = mockGpu->planes[0];

    const QSize modeSize(1920, 1080);
    auto mockConn = QSharedPointer<MockConnector>::create(mockGpu.get());
    mockConn->addMode(modeSize.width(), modeSize.height(), 60.0);
    mockGpu->connectors << mockConn;

    QScopedPointer<DrmBackend> backend(new DrmBackend());
    DrmGpu *gpu = new DrmGpu(backend.get(), "pipeline", 1, 0);

    DrmConnector *conn = new DrmConnector(gpu, mockConn->id);
    QVERIFY(conn->init());
    DrmCrtc *crtc = new DrmCrtc(gpu, mockCrtc->id, 0);
    QVERIFY(crtc->init());
    DrmPlane *plane = new DrmPlane(gpu, mockPlane->id);
    QVERIFY(plane->init());
    DrmPipeline *pipeline = new DrmPipeline(gpu, conn, crtc, plane);

    QVERIFY(pipeline->sourceSize() == modeSize);
    QVERIFY(pipeline->test({}));

    auto cursor = QSharedPointer<DrmDumbBuffer>::create(gpu, QSize(64, 64));
    QVERIFY(pipeline->setCursor(cursor));
    QVERIFY(mockCrtc->cursorBo != nullptr);
    QVERIFY(pipeline->moveCursor(QPoint(100, 100)));
    QVERIFY(mockCrtc->cursorRect == QRect(100, 100, 64, 64));

    // setActive(false) should be applied immediately
    QVERIFY(pipeline->setActive(false));
    QVERIFY(mockCrtc->getProp(QStringLiteral("ACTIVE")) == 0);
    // check for the amdgpu power-off workaround: cursor must be turned off before the output
    QVERIFY(mockCrtc->cursorBo == nullptr);

    // setActive(true) should not be applied immediately
    QVERIFY(pipeline->setActive(true));
    QVERIFY(mockCrtc->getProp(QStringLiteral("ACTIVE")) == 0);

    auto dumb = QSharedPointer<DrmDumbBuffer>::create(gpu, pipeline->sourceSize());
    QVERIFY(pipeline->present(dumb));
    // after the first presented frame ACTIVE should be true
    QVERIFY(mockCrtc->getProp(QStringLiteral("ACTIVE")) == 1);

    delete pipeline;
    delete conn;
    delete crtc;
    delete plane;
    delete gpu;
    cursor.reset();
    dumb.reset();
    verifyCleanup(mockGpu);
}

void DrmTest::testPipelineLegacy()
{
    QScopedPointer<MockGpu> mockGpu(new MockGpu(1, 1));
    mockGpu->deviceCaps.insert(MOCKDRM_DEVICE_CAP_ATOMIC, 0);
    auto mockCrtc = mockGpu->crtcs[0];
    auto mockPlane = mockGpu->planes[0];

    const QSize modeSize(1920, 1080);
    auto mockConn = QSharedPointer<MockConnector>::create(mockGpu.get());
    mockConn->addMode(modeSize.width(), modeSize.height(), 60.0);
    mockGpu->connectors << mockConn;

    QScopedPointer<DrmBackend> backend(new DrmBackend());
    DrmGpu *gpu = new DrmGpu(backend.get(), "legacyPipeline", 1, 0);

    DrmConnector *conn = new DrmConnector(gpu, mockConn->id);
    QVERIFY(conn->init());
    DrmCrtc *crtc = new DrmCrtc(gpu, mockCrtc->id, 0);
    QVERIFY(crtc->init());
    DrmPipeline *pipeline = new DrmPipeline(gpu, conn, crtc, nullptr);

    QVERIFY(pipeline->sourceSize() == modeSize);

    auto cursor = QSharedPointer<DrmDumbBuffer>::create(gpu, QSize(64, 64));
    QVERIFY(pipeline->setCursor(cursor));
    QVERIFY(mockCrtc->cursorBo != nullptr);
    QVERIFY(pipeline->moveCursor(QPoint(100, 100)));
    QVERIFY(mockCrtc->cursorRect == QRect(100, 100, 64, 64));

    QVERIFY(pipeline->setActive(false));
    QVERIFY(mockConn->getProp(QStringLiteral("DPMS")) == DRM_MODE_DPMS_OFF);
    // check for the amdgpu power-off workaround: cursor must be turned off before the output
    QVERIFY(mockCrtc->cursorBo == nullptr);

    QVERIFY(pipeline->setActive(true));
    QVERIFY(mockConn->getProp(QStringLiteral("DPMS")) == DRM_MODE_DPMS_ON);

    auto dumb = QSharedPointer<DrmDumbBuffer>::create(gpu, pipeline->sourceSize());
    QVERIFY(pipeline->present(dumb));
    QVERIFY(mockPlane->getProp(QStringLiteral("FB_ID")) == dumb->bufferId());

    delete pipeline;
    delete conn;
    delete crtc;
    delete gpu;
    cursor.reset();
    dumb.reset();
    verifyCleanup(mockGpu);
}

static void testOutputDetection(bool ams) {
    QScopedPointer<MockGpu> mockGpu(new MockGpu(1, 1, 1024));
    mockGpu->deviceCaps.insert(MOCKDRM_DEVICE_CAP_ATOMIC, ams);

    const QSize modeSize(1920, 1080);
    auto mockConn = QSharedPointer<MockConnector>::create(mockGpu.get());
    mockConn->addMode(modeSize.width(), modeSize.height(), 60.0);
    mockGpu->connectors << mockConn;

    QScopedPointer<DrmBackend> backend(new DrmBackend());
    DrmGpu *gpu = new DrmGpu(backend.get(), "outputs", 1, 0);
    QVERIFY(gpu->updateOutputs());

    Q_ASSERT(gpu->outputs().count() == 1);

    auto pipeline = qobject_cast<DrmOutput*>(gpu->outputs().first())->pipeline();

    auto c = pipeline->connector();
    Q_ASSERT(c->modes().count() == 1);
    // refresh rate unfortunately can't be tested atm because apparently libxcvts calculation is a little off
    QVERIFY(c->modes()[0].size == QSize(1920, 1080));

    QVERIFY(pipeline->crtc()->gammaRampSize() == 1024);

    delete gpu;
    verifyCleanup(mockGpu);
}

void DrmTest::testOutputDetectionAMS()
{
    testOutputDetection(true);
}

void DrmTest::testOutputDetectionLegacy()
{
    testOutputDetection(false);
}

static void testVrDetection(bool ams)
{
    QScopedPointer<MockGpu> mockGpu(new MockGpu(1, 1));
    mockGpu->deviceCaps.insert(MOCKDRM_DEVICE_CAP_ATOMIC, ams);

    auto conn = QSharedPointer<MockConnector>::create(mockGpu.get());
    conn->addMode(1920, 1080, 60.0);
    mockGpu->connectors << conn;

    conn->setProp(QStringLiteral("non-desktop"), 1);

    QScopedPointer<DrmBackend> backend(new DrmBackend());
    DrmGpu *gpu = new DrmGpu(backend.get(), "VR", 1, 0);
    QVERIFY(gpu->updateOutputs());
    QVERIFY(gpu->outputs().count() == 0);

    delete gpu;
    verifyCleanup(mockGpu);
}

void DrmTest::testVrDetectionAMS()
{
    testVrDetection(true);
}

void DrmTest::testVrDetectionLegacy()
{
    testVrDetection(false);
}

}

QTEST_GUILESS_MAIN(KWin::DrmTest)
#include "drmTest.moc"
