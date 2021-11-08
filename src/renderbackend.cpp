/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderbackend.h"

namespace KWin
{

RenderBackend::RenderBackend(QObject *parent)
    : QObject(parent)
{
}

SurfaceTexture *RenderBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *RenderBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *RenderBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

} // namespace KWin
