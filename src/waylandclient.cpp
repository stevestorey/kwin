/*
    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandclient.h"
#include "rules.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/clientconnection.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/buffer_interface.h>

#include <QFileInfo>

#include <csignal>

#include <sys/types.h>
#include <unistd.h>

using namespace KWaylandServer;

namespace KWin
{

enum WaylandGeometryType {
    WaylandGeometryClient = 0x1,
    WaylandGeometryFrame = 0x2,
    WaylandGeometryBuffer = 0x4,
};
Q_DECLARE_FLAGS(WaylandGeometryTypes, WaylandGeometryType)

WaylandClient::WaylandClient(SurfaceInterface *surface)
{
    setSurface(surface);
    setupCompositing();

    connect(surface, &SurfaceInterface::shadowChanged,
            this, &WaylandClient::updateShadow);
    connect(this, &WaylandClient::frameGeometryChanged,
            this, &WaylandClient::updateClientOutputs);
    connect(this, &WaylandClient::desktopFileNameChanged,
            this, &WaylandClient::updateIcon);
    connect(screens(), &Screens::changed, this,
            &WaylandClient::updateClientOutputs);
    connect(surface->client(), &ClientConnection::aboutToBeDestroyed,
            this, &WaylandClient::destroyClient);

    updateResourceName();
    updateIcon();
}

QString WaylandClient::captionNormal() const
{
    return m_captionNormal;
}

QString WaylandClient::captionSuffix() const
{
    return m_captionSuffix;
}

QRect WaylandClient::transparentRect() const
{
    return QRect();
}

pid_t WaylandClient::pid() const
{
    return surface()->client()->processId();
}

bool WaylandClient::isLockScreen() const
{
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool WaylandClient::isLocalhost() const
{
    return true;
}

AbstractClient *WaylandClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

void WaylandClient::resizeWithChecks(const QSize &size)
{
    const QRect area = workspace()->clientArea(WorkArea, this);

    int width = size.width();
    int height = size.height();

    // don't allow growing larger than workarea
    if (width > area.width()) {
        width = area.width();
    }
    if (height > area.height()) {
        height = area.height();
    }
    resize(QSize(width, height));
}

void WaylandClient::killWindow()
{
    if (!surface()) {
        return;
    }
    auto c = surface()->client();
    if (c->processId() == getpid() || c->processId() == 0) {
        c->destroy();
        return;
    }
    ::kill(c->processId(), SIGTERM);
    // give it time to terminate and only if terminate fails, try destroy Wayland connection
    QTimer::singleShot(5000, c, &ClientConnection::destroy);
}

QByteArray WaylandClient::windowRole() const
{
    return QByteArray();
}

bool WaylandClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
{
    if (checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        if (other->desktopFileName() == desktopFileName()) {
            return true;
        }
    }
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

bool WaylandClient::belongsToDesktop() const
{
    const auto clients = waylandServer()->clients();

    return std::any_of(clients.constBegin(), clients.constEnd(),
        [this](const AbstractClient *client) {
            if (belongsToSameApplication(client, SameApplicationChecks())) {
                return client->isDesktop();
            }
            return false;
        }
    );
}

void WaylandClient::updateClientOutputs()
{
    QVector<OutputInterface *> clientOutputs;
    const auto outputs = waylandServer()->display()->outputs();
    for (const auto output : outputs) {
        if (output->isEnabled()) {
            const QRect outputGeometry(output->globalPosition(), output->pixelSize() / output->scale());
            if (frameGeometry().intersects(outputGeometry)) {
                clientOutputs << output;
            }
        }
    }
    surface()->setOutputs(clientOutputs);
}

void WaylandClient::updateIcon()
{
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

void WaylandClient::updateResourceName()
{
    const QFileInfo fileInfo(surface()->client()->executablePath());
    if (fileInfo.exists()) {
        const QByteArray executableFileName = fileInfo.fileName().toUtf8();
        setResourceClass(executableFileName, executableFileName);
    }
}

void WaylandClient::updateCaption()
{
    const QString oldSuffix = m_captionSuffix;
    const auto shortcut = shortcutCaptionSuffix();
    m_captionSuffix = shortcut;
    if ((!isSpecialWindow() || isToolbar()) && findClientWithSameCaption()) {
        int i = 2;
        do {
            m_captionSuffix = shortcut + QLatin1String(" <") + QString::number(i) + QLatin1Char('>');
            i++;
        } while (findClientWithSameCaption());
    }
    if (m_captionSuffix != oldSuffix) {
        emit captionChanged();
    }
}

void WaylandClient::setCaption(const QString &caption)
{
    const QString oldSuffix = m_captionSuffix;
    m_captionNormal = caption.simplified();
    updateCaption();
    if (m_captionSuffix == oldSuffix) {
        // Don't emit caption change twice it already got emitted by the changing suffix.
        emit captionChanged();
    }
}

void WaylandClient::doSetActive()
{
    if (isActive()) { // TODO: Xwayland clients must be unfocused somewhere else.
        StackingUpdatesBlocker blocker(workspace());
        workspace()->focusToNull();
    }
}

void WaylandClient::updateDepth()
{
    if (surface()->buffer()->hasAlphaChannel()) {
        setDepth(32);
    } else {
        setDepth(24);
    }
}

void WaylandClient::cleanGrouping()
{
    if (transientFor()) {
        transientFor()->removeTransient(this);
    }
    for (auto it = transients().constBegin(); it != transients().constEnd();) {
        if ((*it)->transientFor() == this) {
            removeTransient(*it);
            it = transients().constBegin(); // restart, just in case something more has changed with the list
        } else {
            ++it;
        }
    }
}

bool WaylandClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !isZombie() && !isHidden() && !isMinimized();
}

bool WaylandClient::isHiddenInternal() const
{
    return isHidden();
}

void WaylandClient::hideClient(bool hide)
{
    if (hide) {
        internalHide();
    } else {
        internalShow();
    }
}

bool WaylandClient::isHidden() const
{
    return m_isHidden;
}

void WaylandClient::internalShow()
{
    if (!isHidden()) {
        return;
    }
    m_isHidden = false;
    addRepaintFull();
    emit windowShown(this);
}

void WaylandClient::internalHide()
{
    if (isHidden()) {
        return;
    }
    if (isInteractiveMoveResize()) {
        leaveInteractiveMoveResize();
    }
    m_isHidden = true;
    addWorkspaceRepaint(visibleGeometry());
    workspace()->clientHidden(this);
    emit windowHidden(this);
}

QRect WaylandClient::frameRectToBufferRect(const QRect &rect) const
{
    return QRect(rect.topLeft(), surface()->size());
}

void WaylandClient::updateGeometry(const QRect &rect)
{
    const QRect oldClientGeometry = m_clientGeometry;
    const QRect oldFrameGeometry = m_frameGeometry;
    const QRect oldBufferGeometry = m_bufferGeometry;

    m_clientGeometry = frameRectToClientRect(rect);
    m_frameGeometry = rect;
    m_bufferGeometry = frameRectToBufferRect(rect);

    WaylandGeometryTypes changedGeometries;

    if (m_clientGeometry != oldClientGeometry) {
        changedGeometries |= WaylandGeometryClient;
    }
    if (m_frameGeometry != oldFrameGeometry) {
        changedGeometries |= WaylandGeometryFrame;
    }
    if (m_bufferGeometry != oldBufferGeometry) {
        changedGeometries |= WaylandGeometryBuffer;
    }

    if (!changedGeometries) {
        return;
    }

    updateWindowRules(RulesType::Position | RulesType::Size);

    if (changedGeometries & WaylandGeometryBuffer) {
        emit bufferGeometryChanged(this, oldBufferGeometry);
    }
    if (changedGeometries & WaylandGeometryClient) {
        emit clientGeometryChanged(this, oldClientGeometry);
    }
    if (changedGeometries & WaylandGeometryFrame) {
        emit frameGeometryChanged(this, oldFrameGeometry);
    }
    emit geometryShapeChanged(this, oldFrameGeometry);
}

} // namespace KWin
