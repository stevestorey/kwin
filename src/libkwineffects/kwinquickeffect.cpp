/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinquickeffect.h"

#include <QQuickItem>
#include <QWindow>

namespace KWin
{

class QuickSceneEffectPrivate
{
public:
    static QuickSceneEffectPrivate *get(QuickSceneEffect *effect) { return effect->d.data(); }

    QHash<EffectScreen *, QuickSceneView *> views;
    QPointer<QuickSceneView> mouseImplicitGrab;
    bool running = false;
    QScopedPointer<QWindow> dummyWindow;
    EffectScreen *paintedScreen = nullptr;
};

QuickSceneView::QuickSceneView(QuickSceneEffect *effect, EffectScreen *screen)
    : OffscreenQuickView(effect, QuickSceneEffectPrivate::get(effect)->dummyWindow.data())
    , m_effect(effect)
    , m_screen(screen)
{
}

QuickSceneEffect *QuickSceneView::effect() const
{
    return m_effect;
}

EffectScreen *QuickSceneView::screen() const
{
    return m_screen;
}

bool QuickSceneView::isDirty() const
{
    return m_dirty;
}

void QuickSceneView::markDirty()
{
    m_dirty = true;
}

void QuickSceneView::resetDirty()
{
    m_dirty = false;
}

void QuickSceneView::scheduleRepaint()
{
    markDirty();
    effects->addRepaint(geometry());
}

QuickSceneEffect::QuickSceneEffect(QObject *parent)
    : Effect(parent)
    , d(new QuickSceneEffectPrivate)
{
}

QuickSceneEffect::~QuickSceneEffect()
{
}

bool QuickSceneEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

bool QuickSceneEffect::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::CursorChange) {
        if (const QWindow *window = qobject_cast<QWindow *>(watched)) {
            effects->defineCursor(window->cursor().shape());
        }
    }
    return false;
}

bool QuickSceneEffect::isRunning() const
{
    return d->running;
}

void QuickSceneEffect::setRunning(bool running)
{
    if (d->running != running) {
        if (running) {
            startInternal();
        } else {
            stopInternal();
        }
    }
}

QHash<EffectScreen *, QuickSceneView *> QuickSceneEffect::views() const
{
    return d->views;
}

QuickSceneView *QuickSceneEffect::viewAt(const QPoint &pos) const
{
    for (QuickSceneView *screenView : qAsConst(d->views)) {
        if (screenView->geometry().contains(pos)) {
            return screenView;
        }
    }
    return nullptr;
}

void QuickSceneEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    Q_UNUSED(mask)
    Q_UNUSED(region)

    d->paintedScreen = data.screen();

    if (effects->waylandDisplay()) {
        OffscreenQuickView *screenView = d->views.value(data.screen());
        if (screenView) {
            effects->renderOffscreenQuickView(screenView);
        }
    } else {
        for (OffscreenQuickView *screenView : qAsConst(d->views)) {
            effects->renderOffscreenQuickView(screenView);
        }
    }
}

void QuickSceneEffect::postPaintScreen()
{
    // Screen views are repainted after kwin performs its compositing cycle. Another alternative
    // is to update the views after receiving a vblank.
    if (effects->waylandDisplay()) {
        QuickSceneView *screenView = d->views.value(d->paintedScreen);
        if (screenView && screenView->isDirty()) {
            QMetaObject::invokeMethod(screenView, &OffscreenQuickView::update, Qt::QueuedConnection);
            screenView->resetDirty();
        }
    } else {
        for (QuickSceneView *screenView : qAsConst(d->views)) {
            if (screenView->isDirty()) {
                QMetaObject::invokeMethod(screenView, &OffscreenQuickView::update, Qt::QueuedConnection);
                screenView->resetDirty();
            }
        }
    }
    effects->postPaintScreen();
}

bool QuickSceneEffect::isActive() const
{
    return !d->views.isEmpty() && !effects->isScreenLocked();
}

void QuickSceneEffect::handleScreenAdded(EffectScreen *screen)
{
    addScreen(screen);
}

void QuickSceneEffect::handleScreenRemoved(EffectScreen *screen)
{
    delete d->views.take(screen);
}

void QuickSceneEffect::addScreen(EffectScreen *screen)
{
    auto screenView = createView(screen);
    screenView->setAutomaticRepaint(false);

    connect(screenView, &OffscreenQuickView::repaintNeeded, this, [screenView]() {
        effects->addRepaint(screenView->geometry());
    });
    connect(screenView, &OffscreenQuickView::renderRequested, screenView, &QuickSceneView::scheduleRepaint);
    connect(screenView, &OffscreenQuickView::sceneChanged, screenView, &QuickSceneView::scheduleRepaint);

    screenView->scheduleRepaint();
    d->views.insert(screen, screenView);
}

void QuickSceneEffect::startInternal()
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    effects->setActiveFullScreenEffect(this);
    d->running = true;

    // Install an event filter to monitor cursor shape changes.
    qApp->installEventFilter(this);

    // This is an ugly hack to make hidpi rendering work as expected on wayland until we switch
    // to Qt 6.3 or newer. See https://codereview.qt-project.org/c/qt/qtdeclarative/+/361506
    if (effects->waylandDisplay()) {
        d->dummyWindow.reset(new QWindow());
        d->dummyWindow->setOpacity(0);
        d->dummyWindow->resize(1, 1);
        d->dummyWindow->setFlag(Qt::FramelessWindowHint);
        d->dummyWindow->setVisible(true);
        d->dummyWindow->requestActivate();
    }

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        addScreen(screen);
    }

    connect(effects, &EffectsHandler::screenAdded, this, &QuickSceneEffect::handleScreenAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &QuickSceneEffect::handleScreenRemoved);

    effects->grabKeyboard(this);
    effects->startMouseInterception(this, Qt::ArrowCursor);
}

void QuickSceneEffect::stopInternal()
{
    disconnect(effects, &EffectsHandler::screenAdded, this, &QuickSceneEffect::handleScreenAdded);
    disconnect(effects, &EffectsHandler::screenRemoved, this, &QuickSceneEffect::handleScreenRemoved);

    qDeleteAll(d->views);
    d->views.clear();
    d->dummyWindow.reset();
    d->running = false;
    qApp->removeEventFilter(this);
    effects->ungrabKeyboard();
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    effects->addRepaintFull();
}

void QuickSceneEffect::windowInputMouseEvent(QEvent *event)
{
    Qt::MouseButtons buttons;
    QPoint globalPosition;
    if (QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event)) {
        buttons = mouseEvent->buttons();
        globalPosition = mouseEvent->globalPos();
    } else if (QWheelEvent *wheelEvent = dynamic_cast<QWheelEvent *>(event)) {
        buttons = wheelEvent->buttons();
        globalPosition = wheelEvent->globalPosition().toPoint();
    } else {
        return;
    }

    if (buttons) {
        if (!d->mouseImplicitGrab) {
            d->mouseImplicitGrab = viewAt(globalPosition);
        }
    }

    QuickSceneView *target = d->mouseImplicitGrab;
    if (!target) {
        target = viewAt(globalPosition);
    }

    if (!buttons) {
        d->mouseImplicitGrab = nullptr;
    }

    if (target) {
        target->forwardMouseEvent(event);
    }
}

void QuickSceneEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    QuickSceneView *screenView = d->views.value(effects->activeScreen());
    if (screenView) {
        screenView->contentItem()->setFocus(true);
        screenView->forwardKeyEvent(keyEvent);
    }
}

bool QuickSceneEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    for (QuickSceneView *screenView : qAsConst(d->views)) {
        if (screenView->geometry().contains(pos.toPoint())) {
            return screenView->forwardTouchDown(id, pos, time);
        }
    }
    return false;
}

bool QuickSceneEffect::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    for (QuickSceneView *screenView : qAsConst(d->views)) {
        if (screenView->geometry().contains(pos.toPoint())) {
            return screenView->forwardTouchMotion(id, pos, time);
        }
    }
    return false;
}

bool QuickSceneEffect::touchUp(qint32 id, quint32 time)
{
    for (QuickSceneView *screenView : qAsConst(d->views)) {
        if (screenView->forwardTouchUp(id, time)) {
            return true;
        }
    }
    return false;
}

} // namespace KWin
