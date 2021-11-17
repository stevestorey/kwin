/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

#include "expolayout.h"

namespace KDeclarative
{
class QmlObjectSharedEngine;
}

namespace KWin
{

class OverviewEffect;

class OverviewScreenView : public QuickSceneView
{
    Q_OBJECT

public:
    OverviewScreenView(QQmlComponent *component, OverviewEffect *effect, EffectScreen *screen);
    ~OverviewScreenView() override;

public Q_SLOTS:
    void stop();

private:
    QQuickItem *m_rootItem = nullptr;
};

class OverviewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(ExpoLayout::LayoutMode layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground NOTIFY blurBackgroundChanged)

public:
    OverviewEffect();
    ~OverviewEffect() override;

    ExpoLayout::LayoutMode layout() const;
    void setLayout(ExpoLayout::LayoutMode layout);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    bool blurBackground() const;
    void setBlurBackground(bool blur);

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void blurBackgroundChanged();

public Q_SLOTS:
    void activate();
    void deactivate();
    void quickDeactivate();
    void toggle();

protected:
    QuickSceneView *createView(EffectScreen *screen) override;

private:
    void realDeactivate();

    KDeclarative::QmlObjectSharedEngine *m_qmlEngine = nullptr;
    QQmlComponent *m_qmlComponent = nullptr;
    QTimer *m_shutdownTimer;
    QAction *m_toggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_touchBorderActivate;
    bool m_blurBackground = false;
    int m_animationDuration = 200;
    ExpoLayout::LayoutMode m_layout = ExpoLayout::LayoutNatural;
};

} // namespace KWin
