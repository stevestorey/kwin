/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "presentwindows.h"

namespace KWin
{

KWIN_EFFECT_FACTORY(PresentWindowsEffectFactory,
                    PresentWindowsEffect,
                    "metadata.json.stripped")

} // namespace KWin

#include "main.moc"
