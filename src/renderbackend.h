/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWin
{

class SurfacePixmapInternal;
class SurfacePixmapX11;
class SurfacePixmapWayland;
class SurfaceTexture;

class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    explicit RenderBackend(QObject *parent = nullptr);

    virtual SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap);
    virtual SurfaceTexture *createSurfaceTextureX11(SurfacePixmapX11 *pixmap);
    virtual SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap);
};

} // namespace KWin
