/*
 * KWin - the KDE window manager
 * This file is part of the KDE project.
 *
 * SPDX-FileCopyrightText: 2020 Xaver Hugl <xaver.hugl@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "abstract_egl_backend.h"

namespace KWin
{

class DrmBackend;
class DrmGpu;
class DrmOutput;
class DrmBuffer;
class DrmAbstractOutput;

class DrmAbstractEglBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    virtual bool hasOutput(AbstractOutput *output) const = 0;
    virtual bool addOutput(DrmAbstractOutput *output) = 0;
    virtual void removeOutput(DrmAbstractOutput *output) = 0;
    virtual bool swapBuffers(DrmAbstractOutput *output, const QRegion &dirty) {
        Q_UNUSED(output)
        Q_UNUSED(dirty)
        return false;
    }
    virtual bool exportFramebuffer(DrmAbstractOutput *output, void *data, const QSize &size, uint32_t stride) {
        Q_UNUSED(output)
        Q_UNUSED(data)
        Q_UNUSED(size)
        Q_UNUSED(stride)
        return false;
    }
    virtual bool exportFramebufferAsDmabuf(DrmAbstractOutput *output, int *fds, int *strides, int *offsets, uint32_t *num_fds, uint32_t *format, uint64_t *modifier) {
        Q_UNUSED(output)
        Q_UNUSED(fds)
        Q_UNUSED(strides)
        Q_UNUSED(offsets)
        Q_UNUSED(num_fds)
        Q_UNUSED(format)
        Q_UNUSED(modifier)
        return false;
    }
    virtual QRegion beginFrameForSecondaryGpu(DrmAbstractOutput *output) {
        Q_UNUSED(output)
        return QRegion();
    }

    DrmGpu *gpu() const {
        return m_gpu;
    }

    virtual QSharedPointer<DrmBuffer> renderTestFrame(DrmAbstractOutput *output) = 0;

    virtual uint32_t drmFormat() const = 0;

    static DrmAbstractEglBackend *renderingBackend() {
        return static_cast<DrmAbstractEglBackend *>(primaryBackend());
    }

protected:
    DrmAbstractEglBackend(DrmBackend *drmBackend, DrmGpu *gpu);

    DrmBackend *m_backend;
    DrmGpu *m_gpu;

};

}
