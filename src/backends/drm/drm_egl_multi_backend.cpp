/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_egl_multi_backend.h"
#include <config-kwin.h>
#include "logging.h"
#if HAVE_GBM
#include "drm_egl_gbm_backend.h"
#endif
#if HAVE_EGL_STREAMS
#include "drm_egl_stream_backend.h"
#endif
#include "drm_backend.h"
#include "drm_gpu.h"

namespace KWin
{

DrmEglMultiBackend::DrmEglMultiBackend(DrmBackend *backend, DrmAbstractEglBackend *primaryEglBackend)
    : OpenGLBackend()
    , m_platform(backend)
{
    connect(m_platform, &DrmBackend::gpuAdded, this, &DrmEglMultiBackend::addGpu);
    connect(m_platform, &DrmBackend::gpuRemoved, this, &DrmEglMultiBackend::removeGpu);
    m_backends.append(primaryEglBackend);
    setIsDirectRendering(true);
}

DrmEglMultiBackend::~DrmEglMultiBackend()
{
    for (int i = 1; i < m_backends.count(); i++) {
        delete m_backends[i];
    }
    // delete primary backend last, or this will crash!
    delete m_backends[0];
}

void DrmEglMultiBackend::init()
{
    for (auto b : qAsConst(m_backends)) {
        b->init();
    }
    // we only care about the rendering GPU
    setSupportsBufferAge(m_backends[0]->supportsBufferAge());
    setSupportsPartialUpdate(m_backends[0]->supportsPartialUpdate());
    setSupportsSwapBuffersWithDamage(m_backends[0]->supportsSwapBuffersWithDamage());
    // these are client extensions and the same for all egl backends
    setExtensions(m_backends[0]->extensions());

    m_backends[0]->makeCurrent();
    m_initialized = true;
}

QRegion DrmEglMultiBackend::beginFrame(AbstractOutput *output)
{
    return findBackend(output)->beginFrame(output);
}

void DrmEglMultiBackend::endFrame(AbstractOutput *output, const QRegion &damage, const QRegion &damagedRegion)
{
    findBackend(output)->endFrame(output, damage, damagedRegion);
}

bool DrmEglMultiBackend::scanout(AbstractOutput *output, SurfaceItem *surfaceItem)
{
    return findBackend(output)->scanout(output, surfaceItem);
}

bool DrmEglMultiBackend::makeCurrent()
{
    return m_backends[0]->makeCurrent();
}

void DrmEglMultiBackend::doneCurrent()
{
    m_backends[0]->doneCurrent();
}

SurfaceTexture *DrmEglMultiBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backends[0]->createSurfaceTextureInternal(pixmap);
}

SurfaceTexture *DrmEglMultiBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backends[0]->createSurfaceTextureWayland(pixmap);
}

QSharedPointer<GLTexture> DrmEglMultiBackend::textureForOutput(AbstractOutput *requestedOutput) const
{
    // this assumes that all outputs are rendered on backend 0
    return m_backends[0]->textureForOutput(requestedOutput);
}

DrmAbstractEglBackend *DrmEglMultiBackend::findBackend(AbstractOutput *output) const
{
    for (int i = 1; i < m_backends.count(); i++) {
        if (m_backends[i]->hasOutput(output)) {
            return m_backends[i];
        }
    }
    return m_backends[0];
}

bool DrmEglMultiBackend::directScanoutAllowed(AbstractOutput *output) const
{
    return findBackend(output)->directScanoutAllowed(output);
}

void DrmEglMultiBackend::addGpu(DrmGpu *gpu)
{
    DrmAbstractEglBackend *backend;
    if (gpu->useEglStreams()) {
#if HAVE_EGL_STREAMS
        backend = new DrmEglStreamBackend(m_platform, gpu);
#endif
    } else {
#if HAVE_GBM
        backend = new DrmEglGbmBackend(m_platform, gpu);
#endif
    }
    if (backend) {
        if (m_initialized) {
            backend->init();
        }
        m_backends.append(backend);
    }
}

void DrmEglMultiBackend::removeGpu(DrmGpu *gpu)
{
    auto it = std::find_if(m_backends.begin(), m_backends.end(), [gpu](const auto &backend) {
        return backend->gpu() == gpu;
    });
    if (it != m_backends.end()) {
        delete *it;
        m_backends.erase(it);
    }
}

}
