/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "expoarea.h"
#include "expolayout.h"
#include "overviewconfig.h"

#include <KDeclarative/QmlObjectSharedEngine>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTimer>

namespace KWin
{

OverviewScreenView::OverviewScreenView(QQmlComponent *component, OverviewEffect *effect, EffectScreen *screen)
    : QuickSceneView(effect, screen)
{
    const QVariantMap initialProperties {
        { QStringLiteral("effect"), QVariant::fromValue(effect) },
        { QStringLiteral("targetScreen"), QVariant::fromValue(screen) },
    };

    setGeometry(screen->geometry());
    connect(screen, &EffectScreen::geometryChanged, this, [this, screen]() {
        setGeometry(screen->geometry());
    });

    m_rootItem = qobject_cast<QQuickItem *>(component->createWithInitialProperties(initialProperties));
    Q_ASSERT(m_rootItem);
    m_rootItem->setParentItem(contentItem());

    auto updateSize = [this]() { m_rootItem->setSize(contentItem()->size()); };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, m_rootItem, updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, m_rootItem, updateSize);
}

OverviewScreenView::~OverviewScreenView()
{
    delete m_rootItem;
    m_rootItem = nullptr;
}

void OverviewScreenView::stop()
{
    QMetaObject::invokeMethod(m_rootItem, "stop");
}

OverviewEffect::OverviewEffect()
    : m_shutdownTimer(new QTimer(this))
{
    qmlRegisterType<ExpoArea>("org.kde.kwin.private.overview", 1, 0, "ExpoArea");
    qmlRegisterType<ExpoLayout>("org.kde.kwin.private.overview", 1, 0, "ExpoLayout");
    qmlRegisterType<ExpoCell>("org.kde.kwin.private.overview", 1, 0, "ExpoCell");

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &OverviewEffect::realDeactivate);

    const QKeySequence defaultToggleShortcut = Qt::CTRL + Qt::META + Qt::Key_D;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &OverviewEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Overview"));
    m_toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut({defaultToggleShortcut}, m_toggleAction);

    connect(effects, &EffectsHandler::screenAboutToLock, this, &OverviewEffect::realDeactivate);

    initConfig<OverviewConfig>();
    reconfigure(ReconfigureAll);
}

OverviewEffect::~OverviewEffect()
{
}

QuickSceneView *OverviewEffect::createView(EffectScreen *screen)
{
    return new OverviewScreenView(m_qmlComponent, this, screen);
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setLayout(ExpoLayout::LayoutMode(OverviewConfig::layoutMode()));
    setAnimationDuration(animationTime(200));
    setBlurBackground(OverviewConfig::blurBackground());

    for (const ElectricBorder &border : qAsConst(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    for (const ElectricBorder &border : qAsConst(m_touchBorderActivate)) {
        effects->unregisterTouchBorder(border, m_toggleAction);
    }

    m_borderActivate.clear();
    m_touchBorderActivate.clear();

    const QList<int> activateBorders = OverviewConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    const QList<int> touchActivateBorders = OverviewConfig::touchBorderActivate();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerTouchBorder(ElectricBorder(border), m_toggleAction);
    }
}

int OverviewEffect::animationDuration() const
{
    return m_animationDuration;
}

void OverviewEffect::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

ExpoLayout::LayoutMode OverviewEffect::layout() const
{
    return m_layout;
}

void OverviewEffect::setLayout(ExpoLayout::LayoutMode layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

bool OverviewEffect::blurBackground() const
{
    return m_blurBackground;
}

void OverviewEffect::setBlurBackground(bool blur)
{
    if (m_blurBackground != blur) {
        m_blurBackground = blur;
        Q_EMIT blurBackgroundChanged();
    }
}

int OverviewEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool OverviewEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        toggle();
        return true;
    }
    return false;
}

void OverviewEffect::toggle()
{
    if (!isRunning()) {
        activate();
    } else {
        deactivate();
    }
}

void OverviewEffect::activate()
{
    if (!m_qmlEngine) {
        m_qmlEngine = new KDeclarative::QmlObjectSharedEngine(this);
    }

    if (!m_qmlComponent) {
        m_qmlComponent = new QQmlComponent(m_qmlEngine->engine(), this);
        m_qmlComponent->loadUrl(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/ScreenView.qml"))));
        if (m_qmlComponent->isError()) {
            qWarning() << "Failed to load the Overview effect:" << m_qmlComponent->errors();
            delete m_qmlComponent;
            m_qmlComponent = nullptr;
            return;
        }
    }

    setRunning(true);
}

void OverviewEffect::deactivate()
{
    const auto screenViews = views();
    for (QuickSceneView *view : screenViews) {
        static_cast<OverviewScreenView *>(view)->stop();
    }
    m_shutdownTimer->start(animationDuration());
}

void OverviewEffect::realDeactivate()
{
    setRunning(false);
}

void OverviewEffect::quickDeactivate()
{
    m_shutdownTimer->start(0);
}

void OverviewEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() + keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

} // namespace KWin
