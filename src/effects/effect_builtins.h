/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EFFECT_BUILTINS_H
#define KWIN_EFFECT_BUILTINS_H

#include <kwineffects.h>

#define KWIN_IMPORT_BUILTIN_EFFECTS                                                       \
    KWIN_IMPORT_BUILTIN_EFFECT(BlurEffectFactory)                                         \
    KWIN_IMPORT_BUILTIN_EFFECT(ColorPickerEffectFactory)                                  \
    KWIN_IMPORT_BUILTIN_EFFECT(ContrastEffectFactory)                                     \
    KWIN_IMPORT_BUILTIN_EFFECT(DesktopGridEffectFactory)                                  \
    KWIN_IMPORT_BUILTIN_EFFECT(DimInactiveEffectFactory)                                  \
    KWIN_IMPORT_BUILTIN_EFFECT(FallApartEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(GlideEffectFactory)                                        \
    KWIN_IMPORT_BUILTIN_EFFECT(HighlightWindowEffectFactory)                              \
    KWIN_IMPORT_BUILTIN_EFFECT(InvertEffectFactory)                                       \
    KWIN_IMPORT_BUILTIN_EFFECT(KscreenEffectFactory)                                      \
    KWIN_IMPORT_BUILTIN_EFFECT(LookingGlassEffectFactory)                                 \
    KWIN_IMPORT_BUILTIN_EFFECT(MagicLampEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(MagnifierEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(MouseClickEffectFactory)                                   \
    KWIN_IMPORT_BUILTIN_EFFECT(MouseMarkEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(OverviewEffectFactory)                                     \
    KWIN_IMPORT_BUILTIN_EFFECT(PresentWindowsEffectFactory)                               \
    KWIN_IMPORT_BUILTIN_EFFECT(ResizeEffectFactory)                                       \
    KWIN_IMPORT_BUILTIN_EFFECT(ScreenEdgeEffectFactory)                                   \
    KWIN_IMPORT_BUILTIN_EFFECT(ScreenShotEffectFactory)                                   \
    KWIN_IMPORT_BUILTIN_EFFECT(ScreenTransformEffectFactory)                              \
    KWIN_IMPORT_BUILTIN_EFFECT(SheetEffectFactory)                                        \
    KWIN_IMPORT_BUILTIN_EFFECT(ShowFpsEffectFactory)                                      \
    KWIN_IMPORT_BUILTIN_EFFECT(ShowPaintEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(SlideEffectFactory)                                        \
    KWIN_IMPORT_BUILTIN_EFFECT(SlideBackEffectFactory)                                    \
    KWIN_IMPORT_BUILTIN_EFFECT(SlidingPopupsEffectFactory)                                \
    KWIN_IMPORT_BUILTIN_EFFECT(SnapHelperEffectFactory)                                   \
    KWIN_IMPORT_BUILTIN_EFFECT(StartupFeedbackEffectFactory)                              \
    KWIN_IMPORT_BUILTIN_EFFECT(ThumbnailAsideEffectFactory)                               \
    KWIN_IMPORT_BUILTIN_EFFECT(TouchPointsEffectFactory)                                  \
    KWIN_IMPORT_BUILTIN_EFFECT(TrackMouseEffectFactory)                                   \
    KWIN_IMPORT_BUILTIN_EFFECT(WindowGeometryFactory)                                     \
    KWIN_IMPORT_BUILTIN_EFFECT(WobblyWindowsEffectFactory)                                \
    KWIN_IMPORT_BUILTIN_EFFECT(ZoomEffectFactory)

#endif
