/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "diminactive.h"

namespace KWin
{

KWIN_EFFECT_FACTORY(DimInactiveEffectFactory,
                    DimInactiveEffect,
                    "metadata.json.stripped")

} // namespace KWin

#include "main.moc"
