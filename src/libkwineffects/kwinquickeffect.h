/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinoffscreenquickview.h"
#include "kwineffects.h"

namespace KWin
{

class QuickSceneEffect;
class QuickSceneEffectPrivate;

/**
 * The QuickSceneView represents a QtQuick scene view on a particular screen.
 *
 * @see QuickSceneEffect, OffscreenQuickView
 */
class KWINEFFECTS_EXPORT QuickSceneView : public OffscreenQuickView
{
    Q_OBJECT

public:
    explicit QuickSceneView(QuickSceneEffect *effect, EffectScreen *screen);

    QuickSceneEffect *effect() const;
    EffectScreen *screen() const;

    bool isDirty() const;
    void markDirty();
    void resetDirty();

public Q_SLOTS:
    void scheduleRepaint();

private:
    QuickSceneEffect *m_effect;
    EffectScreen *m_screen;
    bool m_dirty = false;
};

/**
 * The QuickSceneEffect class provides a convenient way to write fullscreen
 * QtQuick-based effects.
 *
 * In order to activate the effect, call setRunning(true). The QuickSceneEffect will call
 * the createView() function for every available screen. To stop the effect, call
 * setRunning(false).
 *
 * QuickSceneView objects are managed internally.
 *
 * The QuickSceneEffect takes care of forwarding input events to QuickSceneView and
 * rendering. You can override relevant hooks from the Effect class to customize input
 * handling or rendering, although it's highly recommended that you avoid doing that.
 *
 * @see QuickSceneView
 */
class KWINEFFECTS_EXPORT QuickSceneEffect : public Effect
{
    Q_OBJECT

public:
    explicit QuickSceneEffect(QObject *parent = nullptr);
    ~QuickSceneEffect() override;

    bool isRunning() const;
    void setRunning(bool running);

    QHash<EffectScreen *, QuickSceneView *> views() const;
    QuickSceneView *viewAt(const QPoint &pos) const;

    bool eventFilter(QObject *watched, QEvent *event) override;

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    void windowInputMouseEvent(QEvent *event) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    static bool supported();

protected:
    /**
     * Reimplement this function to create a scene view for the specified @a screen.
     */
    virtual QuickSceneView *createView(EffectScreen *screen) = 0;

private:
    void handleScreenAdded(EffectScreen *screen);
    void handleScreenRemoved(EffectScreen *screen);

    void addScreen(EffectScreen *screen);
    void startInternal();
    void stopInternal();

    QScopedPointer<QuickSceneEffectPrivate> d;
    friend class QuickSceneEffectPrivate;
};

} // namespace KWin
