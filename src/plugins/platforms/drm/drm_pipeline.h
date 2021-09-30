/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPoint>
#include <QSize>
#include <QVector>
#include <QSharedPointer>

#include <optional>
#include <xf86drmMode.h>

#include "drm_object_plane.h"
#include "renderloop_p.h"
#include "abstract_wayland_output.h"

namespace KWin
{

class DrmGpu;
class DrmConnector;
class DrmConnectorMode;
class DrmCrtc;
class DrmBuffer;
class DrmDumbBuffer;
class GammaRamp;

/**
 * The DrmConnectorState type represents double-buffered state of a drm connector. TODO(vlad): move to DrmConnector
 */
struct DrmConnectorState
{
    DrmConnectorMode *mode = nullptr;
};

/**
 * The DrmCrtcState type represents double-buffered state of a drm crtc. TODO(vlad): move to DrmCrtc
 */
struct DrmCrtcState
{
    bool active = true;
    GammaRamp gammaRamp;
    bool vrr = false;
};

/**
 * The DrmPlaneState type represents double-buffered state of a drm plane. TODO(vlad): move to DrmPlane.
 */
struct DrmPlaneState
{
    QSharedPointer<DrmBuffer> buffer;
    QRect crtcRect;
    QRect sourceRect;
};

/**
 * The DrmConnectorChangeSet type is used to describe changes applied to the connector.
 */
struct DrmConnectorChangeSet
{
    DrmConnectorMode *mode = nullptr;
};

/**
 * The DrmCrtcChangeSet type is used to describe changes applied to the crtc.
 */
struct DrmCrtcChangeSet
{
    bool active = true;
    std::optional<GammaRamp> gammaRamp;
    std::optional<bool> vrr;
};

/**
 * The DrmPlaneChangeSet type is used to describe changes applied to a plane, e.g. cursor.
 */
struct DrmPlaneChangeSet
{
    QSharedPointer<DrmBuffer> buffer;
    QRect crtcRect;
    QRect sourceRect;
};

/**
 * The DrmPipelineChangeSet type is used to describe changes applied to a pipeline.
 */
struct DrmPipelineChangeSet
{
    std::optional<DrmConnectorChangeSet> connector;
    std::optional<DrmCrtcChangeSet> crtc;
    std::optional<DrmPlaneChangeSet> primaryPlane;
    std::optional<DrmPlaneChangeSet> cursorPlane;
};

class DrmPipeline
{
public:
    DrmPipeline(DrmGpu *gpu, DrmConnector *conn, DrmCrtc *crtc, DrmPlane *primaryPlane);
    ~DrmPipeline();

// TODO(vlad): Start
    /**
     * Sets the necessary initial drm properties for the pipeline to work
     */
    void setup();

    /**
     * tests the pending commit first and commits it if the test passes
     * if the test fails, there is a guarantee for no lasting changes
     */
    bool present(const QSharedPointer<DrmBuffer> &buffer);

    bool modeset(int modeIndex);
    bool setCursor(const QSharedPointer<DrmDumbBuffer> &buffer, const QPoint &hotspot = QPoint());
    bool setActive(bool enable);
    bool setGammaRamp(const GammaRamp &ramp);
    bool setTransformation(const DrmPlane::Transformations &transform);
    bool moveCursor(QPoint pos);
    bool setSyncMode(RenderLoopPrivate::SyncMode syncMode);
    bool setOverscan(uint32_t overscan);
    bool setRgbRange(AbstractWaylandOutput::RgbRange rgbRange);
// TODO(vlad): End. Remove in the middle.

    DrmPlane::Transformations transformation() const;
    bool isCursorVisible() const;
    QPoint cursorPos() const;

    DrmConnector *connector() const;
    DrmCrtc *crtc() const;
    DrmPlane *primaryPlane() const;

    void pageFlipped();
    void printDebugInfo() const;
    QSize sourceSize() const;

    bool isFormatSupported(uint32_t drmFormat) const;
    QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const;

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    enum class CommitMode {
        Test,
        Commit,
        CommitWithPageflipEvent
    };
    Q_ENUM(CommitMode);
    static bool commitPipelines(const QVector<DrmPipeline*> &pipelines, CommitMode mode);

    static bool tryApply(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction); // TODO: flags

    static bool tryApplyAtomic(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction);
    static bool tryApplyLegacy(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction);

private:
    bool populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags);
    bool test();
    bool atomicCommit();
    bool presentLegacy();
    bool checkTestBuffer();
    bool isActive() const;

    bool setPendingTransformation(const DrmPlane::Transformations &transformation);

    DrmOutput *m_output = nullptr;
    DrmGpu *m_gpu = nullptr;
    DrmConnector *m_connector = nullptr;
    DrmCrtc *m_crtc = nullptr;

    DrmPlane *m_primaryPlane = nullptr;
    QSharedPointer<DrmBuffer> m_primaryBuffer;
    QSharedPointer<DrmBuffer> m_oldTestBuffer;

    bool m_legacyNeedsModeset = true;
    struct {
        QPoint pos;
        QPoint hotspot;
        QSharedPointer<DrmDumbBuffer> buffer;
        bool dirtyBo = true;
        bool dirtyPos = true;
    } m_cursor;

    QVector<DrmObject*> m_allObjects;
    QMap<uint32_t, QVector<uint64_t>> m_formats;

    int m_lastFlags = 0;
};

}
