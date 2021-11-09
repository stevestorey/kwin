/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "drm_abstract_egl_backend.h"

namespace KWin
{

class DrmEglMultiBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    DrmEglMultiBackend(DrmBackend *backend, DrmAbstractEglBackend *primaryEglBackend);
    ~DrmEglMultiBackend();

    void init() override;

    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &damage, const QRegion &damagedRegion) override;
    bool scanout(AbstractOutput *output, SurfaceItem *surfaceItem) override;

    bool makeCurrent() override;
    void doneCurrent() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;
    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    bool directScanoutAllowed(AbstractOutput *output) const override;

public Q_SLOTS:
    void addGpu(DrmGpu *gpu);
    void removeGpu(DrmGpu *gpu);

private:
    DrmBackend *m_platform;
    QVector<DrmAbstractEglBackend *> m_backends;
    bool m_initialized = false;

    DrmAbstractEglBackend *findBackend(AbstractOutput *output) const;
};

}
