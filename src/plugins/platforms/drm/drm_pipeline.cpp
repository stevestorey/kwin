/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "logging.h"
#include "drm_gpu.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_buffer.h"
#include "cursor.h"
#include "session.h"
#include "drm_output.h"
#include "drm_backend.h"

#if HAVE_GBM
#include <gbm.h>

#include "egl_gbm_backend.h"
#include "drm_buffer_gbm.h"
#endif

#include <drm_fourcc.h>

namespace KWin
{

DrmPipeline::DrmPipeline(DrmGpu *gpu, DrmConnector *conn, DrmCrtc *crtc, DrmPlane *primaryPlane)
    : m_output(nullptr)
    , m_gpu(gpu)
    , m_connector(conn)
    , m_crtc(crtc)
    , m_primaryPlane(primaryPlane)
{
    m_allObjects << m_connector << m_crtc;
    if (m_primaryPlane) {
        m_allObjects << m_primaryPlane;
    }
}

DrmPipeline::~DrmPipeline()
{
}

bool DrmPipeline::tryApply(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction)
{
    if (gpu->atomicModeSetting()) {
        return tryApplyAtomic(gpu, transaction);
    } else {
        return tryApplyLegacy(gpu, transaction);
    }
}

static bool addConnectorProperty(drmModeAtomicReq *req, DrmConnector *connector, DrmConnector::PropertyIndex property, uint64_t value)
{
    return true;
}

static bool addCrtcProperty(drmModeAtomicReq *req, DrmCrtc *crtc, DrmCrtc::PropertyIndex property, uint64_t value)
{
    return true;
}

static bool addPlaneProperty(drmModeAtomicReq *req, DrmPlane *plane, DrmPlane::PropertyIndex property, uint64_t value)
{
    return true;
}

bool DrmPipeline::tryApplyAtomic(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction)
{
    DrmScopedPointer<drmModeAtomicReq> req(drmModeAtomicAlloc());
    if (!req) {
        qCDebug(KWIN_DRM) << "Failed to allocate drmModeAtomicReq:" << strerror(errno);
        return false;
    }

    for (auto it = transaction.begin(); it != transaction.end(); ++it) {
        DrmPipeline *pipeline = it.key();
        DrmConnector *connector = pipeline->connector();
        DrmCrtc *crtc = pipeline->crtc();
        DrmPlane *primaryPlane = pipeline->primaryPlane();

        DrmConnectorState connectorState = connector->state();
        DrmCrtcState crtcState = crtc->state();
        DrmPlaneState primaryPlaneState = primaryPlane->state();

        // Apply connector state changes.
        const DrmPipelineChangeSet &pipelineChangeSet = it.value();
        if (pipelineChangeSet.connector) {
            const DrmConnectorChangeSet &connectorChangeSet = *pipelineChangeSet.connector;
            if (connectorChangeSet.mode) {
                connectorState.mode = connectorChangeSet.mode;
            }
        }

        // Apply crtc state changes.
        if (pipelineChangeSet.crtc) {
            const DrmCrtcChangeSet &crtcChangeSet = *pipelineChangeSet.crtc;
            crtcState.active = crtcChangeSet.active;
            if (crtcChangeSet.gammaRamp) {
                crtcState.gammaRamp = *crtcChangeSet.gammaRamp;
            }
            if (crtcChangeSet.vrr) {
                crtcState.vrr = *crtcChangeSet.vrr;
            }
        }

        // Apply primary plane state changes.
        if (pipelineChangeSet.primaryPlane) {
            const DrmPlaneChangeSet &planeChangeSet = *pipelineChangeSet.primaryPlane;
            primaryPlaneState.buffer = planeChangeSet.buffer;
            primaryPlaneState.crtcRect = planeChangeSet.crtcRect;
            primaryPlaneState.sourceRect = planeChangeSet.sourceRect;
        }

        // Add connector's properties to the atomic request.
        bool ok = true;

        if (crtcState.active) {
            ok &= addConnectorProperty(req.data(), connector, DrmConnector::PropertyIndex::CrtcId, crtc->id());

            // if (auto property = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
            //     ok &= addProperty(req, m_connector, property);
            // }

            // if (auto property = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
            //     ok &= addProperty(req, m_connector, property);

            //     // If the underscan property is available, these two are guaranteed to exist.
            //     ok &= addProperty(req, m_connector, m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder));
            //     ok &= addProperty(req, m_connector, m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder));
            // }

            // if (auto property = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
            //     ok &= addProperty(req, m_connector, property);
            // }

            // Add crtc's properties to the atomic request.
            ok &= addCrtcProperty(req.data(), crtc, DrmCrtc::PropertyIndex::ModeId, crtcState.mode->blobId());
            ok &= addCrtcProperty(req.data(), crtc, DrmCrtc::PropertyIndex::Active, 1);

            // if (auto property = m_crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled)) {
            //     ok &= addProperty(req, m_crtc, property);
            // }
            // if (auto property = m_crtc->getProp(DrmCrtc::PropertyIndex::Gamma_LUT)) {
            //     ok &= addProperty(req, m_crtc, property);
            // }

            // Add primary plane's properties to the atomic request.
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcId, crtc->id());
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::SrcX, primaryPlaneState.sourceRect.x() << 16);
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::SrcY, primaryPlaneState.sourceRect.y() << 16);
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::SrcW, primaryPlaneState.sourceRect.width() << 16);
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::SrcH, primaryPlaneState.sourceRect.height() << 16);
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcX, primaryPlaneState.crtcRect.x());
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcY, primaryPlaneState.crtcRect.y());
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcW, primaryPlaneState.crtcRect.width());
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcH, primaryPlaneState.crtcRect.height());
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::FbId, primaryPlaneState.buffer->bufferId());

            // if (auto rotationProperty = m_primaryPlane->getProp(DrmPlane::PropertyIndex::Rotation)) {
            //     ok &= addProperty(req, m_primaryPlane, rotationProperty);
            // }

        } else {
            ok &= addConnectorProperty(req.data(), connector, DrmConnector::PropertyIndex::CrtcId, 0);
            ok &= addCrtcProperty(req.data(), crtc, DrmCrtc::PropertyIndex::Active, 0);
            ok &= addPlaneProperty(req.data(), primaryPlane, DrmPlane::PropertyIndex::CrtcId, 0);
        }

        if (!ok) {
            qCWarning(KWIN_DRM) << "Wubba lubba dub dub";
            return false;
        }
    }

    // drmModeAtomicCommit()

    for (auto it = transaction.begin(); it != transaction.end(); ++it) {
        // TODO: Either commit or rollback the state.
    }

    return true;
}

bool DrmPipeline::tryApplyLegacy(DrmGpu *gpu, const QMap<DrmPipeline *, DrmPipelineChangeSet> &transaction)
{
    for (auto it = transaction.begin(); it != transaction.end(); ++it) {
        DrmPipeline *pipeline = it.key();
        DrmConnector *connector = pipeline->connector();
        DrmCrtc *crtc = pipeline->crtc();

        const DrmPipelineChangeSet &pipelineChangeSet = it.value();
        if (pipelineChangeSet.connector) {
            const DrmConnectorChangeSet &connectorChangeSet = *pipelineChangeSet.connector;

            if (connectorChangeSet.mode) {
                const DrmPlaneChangeSet &planeChangeSet = *pipelineChangeSet.primaryPlane;
                uint32_t connectorId = connector->id();
                if (drmModeSetCrtc(gpu->fd(),
                                   crtc->id(),
                                   planeChangeSet.buffer->bufferId(),
                                   planeChangeSet.crtcRect.x(),
                                   planeChangeSet.crtcRect.y(),
                                   &connectorId, 1, connectorChangeSet.mode->nativeMode()) != 0) {
                    return false;
                }
                // TODO: Set mode index
            }
        }

        if (pipelineChangeSet.crtc) {
            const DrmCrtcChangeSet &crtcChangeSet = *pipelineChangeSet.crtc;

            // m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->setPropertyLegacy(active ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF);

            if (crtcChangeSet.gammaRamp) {
                const GammaRamp &gammaRamp = *crtcChangeSet.gammaRamp;
                uint16_t *red = const_cast<uint16_t *>(gammaRamp.red());
                uint16_t *green = const_cast<uint16_t *>(gammaRamp.green());
                uint16_t *blue = const_cast<uint16_t *>(gammaRamp.blue());
                if (drmModeCrtcSetGamma(gpu->fd(), crtc->id(), gammaRamp.size(), red, green, blue) != 0) {
                    qCWarning(KWIN_DRM) << "Setting legacy drm gamma ramp failed:" << strerror(errno);
                    return false;
                }
            }
        }

        if (pipelineChangeSet.primaryPlane) {
            const DrmPlaneChangeSet &planeChangeSet = *pipelineChangeSet.primaryPlane;

            if (drmModePageFlip(gpu->fd(),
                                crtc->id(),
                                planeChangeSet.buffer ? planeChangeSet.buffer->bufferId() : 0,
                                DRM_MODE_PAGE_FLIP_EVENT,
                                pipeline->output()) != 0) {
                qCWarning(KWIN_DRM) << "Legacy page flip failed:" << strerror(errno);
                return false;
            }
            crtc->setNext(planeChangeSet.buffer);
        }

        if (pipelineChangeSet.cursorPlane) {
            const DrmPlaneChangeSet &planeChangeSet = *pipelineChangeSet.cursorPlane;

            if (drmModeSetCursor2(gpu->fd(),
                                  pipeline->crtc()->id(),
                                  planeChangeSet.buffer ? planeChangeSet.buffer->bufferId() : 0,
                                  planeChangeSet.sourceRect.width(),
                                  planeChangeSet.sourceRect.height(),
                                  0, 0) == -ENOTSUP) {
                // for NVIDIA case that does not support drmModeSetCursor2
                if (drmModeSetCursor(gpu->fd(),
                                     pipeline->crtc()->id(),
                                     planeChangeSet.buffer ? planeChangeSet.buffer->bufferId() : 0,
                                     planeChangeSet.sourceRect.width(),
                                     planeChangeSet.sourceRect.height()) != 0) {
                    qCWarning(KWIN_DRM) << "Failed to set the legacy drm cursor:" << strerror(errno);
                    return false;
                }
            } else {
                return false;
            }

            if (drmModeMoveCursor(gpu->fd(),
                                  pipeline->crtc()->id(),
                                  planeChangeSet.crtcRect.x(),
                                  planeChangeSet.crtcRect.y()) != 0) {
                qCWarning(KWIN_DRM) << "Failed to move the legacy drm cursor:" << strerror(errno);
                return false;
            }
        }
    }
    return true;
}

void DrmPipeline::setup()
{
    if (m_gpu->atomicModeSetting()) {
        if (m_connector->getProp(DrmConnector::PropertyIndex::CrtcId)->current() == m_crtc->id()) {
            m_connector->findCurrentMode(m_crtc->queryCurrentMode());
        }
        m_formats = m_primaryPlane->formats();
    } else {
        m_formats.insert(DRM_FORMAT_XRGB8888, {});
        m_formats.insert(DRM_FORMAT_ARGB8888, {});
    }
}

bool DrmPipeline::test()
{
    return checkTestBuffer() && commitPipelines(m_gpu->pipelines(), CommitMode::Test);
}

bool DrmPipeline::present(const QSharedPointer<DrmBuffer> &buffer)
{
    return true;
}

bool DrmPipeline::atomicCommit()
{
    return false;
}

bool DrmPipeline::commitPipelines(const QVector<DrmPipeline*> &pipelines, CommitMode mode)
{
    return false;
}

bool DrmPipeline::populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags)
{
    return true;
}

bool DrmPipeline::presentLegacy()
{
    return true;
}

bool DrmPipeline::modeset(int modeIndex)
{
    return true;
}

bool DrmPipeline::checkTestBuffer()
{
//     if (m_primaryBuffer && m_primaryBuffer->size() == sourceSize()) {
//         return true;
//     }
//     if (!isActive()) {
//         return true;
//     }
//     auto backend = m_gpu->eglBackend();
//     QSharedPointer<DrmBuffer> buffer;
//     // try to re-use buffers if possible.
//     const auto &checkBuffer = [this, backend, &buffer](const QSharedPointer<DrmBuffer> &buf){
//         const auto &mods = supportedModifiers(buf->format());
//         if (buf->format() == backend->drmFormat()
//             && (mods.isEmpty() || mods.contains(buf->modifier()))
//             && buf->size() == sourceSize()) {
//             buffer = buf;
//         }
//     };
//     if (m_primaryPlane && m_primaryPlane->next()) {
//         checkBuffer(m_primaryPlane->next());
//     } else if (m_primaryPlane && m_primaryPlane->current()) {
//         checkBuffer(m_primaryPlane->current());
//     } else if (m_crtc->next()) {
//         checkBuffer(m_crtc->next());
//     } else if (m_crtc->current()) {
//         checkBuffer(m_crtc->current());
//     }
//     // if we don't have a fitting buffer already, get or create one
//     if (buffer) {
// #if HAVE_GBM
//     } else if (backend && m_output) {
//         buffer = backend->renderTestFrame(m_output);
//     } else if (backend && m_gpu->gbmDevice()) {
//         gbm_bo *bo = gbm_bo_create(m_gpu->gbmDevice(), sourceSize().width(), sourceSize().height(), GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
//         if (!bo) {
//             return false;
//         }
//         buffer = QSharedPointer<DrmGbmBuffer>::create(m_gpu, bo, nullptr);
// #endif
//     } else {
//         buffer = QSharedPointer<DrmDumbBuffer>::create(m_gpu, sourceSize());
//     }
//     if (buffer && buffer->bufferId()) {
//         m_oldTestBuffer = m_primaryBuffer;
//         m_primaryBuffer = buffer;
//         return true;
//     }
    return false;
}

bool DrmPipeline::setCursor(const QSharedPointer<DrmDumbBuffer> &buffer, const QPoint &hotspot)
{
    return true;
}

bool DrmPipeline::moveCursor(QPoint pos)
{
    return true;
}

bool DrmPipeline::setActive(bool active)
{
    return false;
}

bool DrmPipeline::setGammaRamp(const GammaRamp &ramp)
{
    return true;
}

bool DrmPipeline::setTransformation(const DrmPlane::Transformations &transformation)
{
    return setPendingTransformation(transformation) && test();
}

bool DrmPipeline::setPendingTransformation(const DrmPlane::Transformations &transformation)
{
    if (this->transformation() == transformation) {
        return true;
    }
    if (!m_gpu->atomicModeSetting()) {
        return false;
    }
    if (!m_primaryPlane->setTransformation(transformation)) {
        return false;
    }
    return true;
}

bool DrmPipeline::setSyncMode(RenderLoopPrivate::SyncMode syncMode)
{
    auto vrrProp = m_crtc->getProp(DrmCrtc::PropertyIndex::VrrEnabled);
    if (!vrrProp || !m_connector->vrrCapable()) {
        return syncMode == RenderLoopPrivate::SyncMode::Fixed;
    }
    bool vrr = syncMode == RenderLoopPrivate::SyncMode::Adaptive;
    if (vrrProp->pending() == vrr) {
        return true;
    }
    if (m_gpu->atomicModeSetting()) {
        vrrProp->setPending(vrr);
        return test();
    }
    return false;
}

bool DrmPipeline::setOverscan(uint32_t overscan)
{
    if (overscan > 100 || (overscan != 0 && !m_connector->hasOverscan())) {
        return false;
    }
    m_connector->setOverscan(overscan, m_connector->currentMode()->size());
    return test();
}

bool DrmPipeline::setRgbRange(AbstractWaylandOutput::RgbRange rgbRange)
{
    if (const auto &prop = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
        prop->setEnum(rgbRange);
        return test();
    } else {
        return false;
    }
}

QSize DrmPipeline::sourceSize() const
{
    auto mode = m_connector->currentMode();
    if (transformation() & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return mode->size().transposed();
    }
    return mode->size();
}

DrmPlane::Transformations DrmPipeline::transformation() const
{
    return m_primaryPlane ? m_primaryPlane->transformation() : DrmPlane::Transformation::Rotate0;
}

bool DrmPipeline::isActive() const
{
    if (m_gpu->atomicModeSetting()) {
        return m_crtc->getProp(DrmCrtc::PropertyIndex::Active)->pending() != 0;
    } else {
        return m_connector->getProp(DrmConnector::PropertyIndex::Dpms)->current() == DRM_MODE_DPMS_ON;
    }
}

bool DrmPipeline::isCursorVisible() const
{
    return m_cursor.buffer && QRect(m_cursor.pos, m_cursor.buffer->size()).intersects(QRect(QPoint(0, 0), m_connector->currentMode()->size()));
}

QPoint DrmPipeline::cursorPos() const
{
    return m_cursor.pos;
}

DrmConnector *DrmPipeline::connector() const
{
    return m_connector;
}

DrmCrtc *DrmPipeline::crtc() const
{
    return m_crtc;
}

DrmPlane *DrmPipeline::primaryPlane() const
{
    return m_primaryPlane;
}

void DrmPipeline::pageFlipped()
{
    m_crtc->flipBuffer();
    if (m_primaryPlane) {
        m_primaryPlane->flipBuffer();
    }
}

void DrmPipeline::setOutput(DrmOutput *output)
{
    m_output = output;
}

DrmOutput *DrmPipeline::output() const
{
    return m_output;
}

bool DrmPipeline::isFormatSupported(uint32_t drmFormat) const
{
    return m_formats.contains(drmFormat);
}

QVector<uint64_t> DrmPipeline::supportedModifiers(uint32_t drmFormat) const
{
    return m_formats[drmFormat];
}

static void printProps(DrmObject *object)
{
    auto list = object->properties();
    for (const auto &prop : list) {
        if (prop) {
            uint64_t current = prop->name().startsWith("SRC_") ? prop->current() >> 16 : prop->current();
            if (prop->isImmutable() || !prop->needsCommit()) {
                qCWarning(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current;
            } else {
                uint64_t pending = prop->name().startsWith("SRC_") ? prop->pending() >> 16 : prop->pending();
                qCWarning(KWIN_DRM).nospace() << "\t" << prop->name() << ": " << current << "->" << pending;
            }
        }
    }
}

void DrmPipeline::printDebugInfo() const
{
    if (m_lastFlags == 0) {
        qCWarning(KWIN_DRM) << "Flags: none";
    } else {
        qCWarning(KWIN_DRM) << "Flags:";
        if (m_lastFlags & DRM_MODE_PAGE_FLIP_EVENT) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_EVENT";
        }
        if (m_lastFlags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_ATOMIC_ALLOW_MODESET";
        }
        if (m_lastFlags & DRM_MODE_PAGE_FLIP_ASYNC) {
            qCWarning(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_ASYNC";
        }
    }
    qCWarning(KWIN_DRM) << "Drm objects:";
    qCWarning(KWIN_DRM) << "connector" << m_connector->id();
    auto list = m_connector->properties();
    printProps(m_connector);
    qCWarning(KWIN_DRM) << "crtc" << m_crtc->id();
    printProps(m_crtc);
    if (m_primaryPlane) {
        qCWarning(KWIN_DRM) << "primary plane" << m_primaryPlane->id();
        printProps(m_primaryPlane);
    }
}

}
