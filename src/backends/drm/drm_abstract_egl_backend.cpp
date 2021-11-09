/*
 * KWin - the KDE window manager
 * This file is part of the KDE project.
 *
 * SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "drm_abstract_egl_backend.h"
#include "drm_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"

using namespace KWin;

DrmAbstractEglBackend::DrmAbstractEglBackend(DrmBackend *drmBackend, DrmGpu *gpu) : m_backend(drmBackend), m_gpu(gpu)
{
    m_gpu->setEglBackend(this);
    // Egl is always direct rendering.
    setIsDirectRendering(true);
    connect(m_gpu, &DrmGpu::outputEnabled, this, &DrmAbstractEglBackend::addOutput);
    connect(m_gpu, &DrmGpu::outputDisabled, this, &DrmAbstractEglBackend::removeOutput);
}
