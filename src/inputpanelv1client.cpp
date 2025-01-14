/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1client.h"
#include "deleted.h"
#include "wayland_server.h"
#include "workspace.h"
#include "abstract_wayland_output.h"
#include "platform.h"
#include "inputmethod.h"
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/textinput_v2_interface.h>

using namespace KWaylandServer;

namespace KWin
{

InputPanelV1Client::InputPanelV1Client(InputPanelSurfaceV1Interface *panelSurface)
    : WaylandClient(panelSurface->surface())
    , m_panelSurface(panelSurface)
{
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &InputPanelV1Client::destroyClient);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &InputPanelV1Client::reposition);
    connect(surface(), &SurfaceInterface::mapped, this, &InputPanelV1Client::updateDepth);

    connect(panelSurface, &InputPanelSurfaceV1Interface::topLevel, this, &InputPanelV1Client::showTopLevel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::overlayPanel, this, &InputPanelV1Client::showOverlayPanel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::destroyed, this, &InputPanelV1Client::destroyClient);

    InputMethod::self()->setPanel(this);
}

void InputPanelV1Client::showOverlayPanel()
{
    setOutput(nullptr);
    m_mode = Overlay;
    reposition();
    setReadyForPainting();
}

void InputPanelV1Client::showTopLevel(OutputInterface *output, InputPanelSurfaceV1Interface::Position position)
{
    Q_UNUSED(position);
    m_mode = Toplevel;
    setOutput(output);
}

void InputPanelV1Client::allow()
{
    setReadyForPainting();
    reposition();
}

void KWin::InputPanelV1Client::reposition()
{
    if (!readyForPainting()) {
        return;
    }

    switch (m_mode) {
        case Toplevel: {
            if (m_output) {
                const QSize panelSize = surface()->size();
                if (!panelSize.isValid() || panelSize.isEmpty()) {
                    return;
                }

                QRect availableArea;
                if (waylandServer()->isScreenLocked()) {
                    availableArea = m_output->geometry();
                } else {
                    availableArea = workspace()->clientArea(MaximizeArea, this, m_output);
                }
                QRect geo(availableArea.topLeft(), panelSize);
                geo.translate((availableArea.width() - panelSize.width())/2, availableArea.height() - panelSize.height());
                moveResize(geo);
            }
        }   break;
        case Overlay: {
            auto textClient = waylandServer()->findClient(waylandServer()->seat()->focusedTextInputSurface());
            auto textInput = waylandServer()->seat()->textInputV2();
            if (textClient && textInput) {
                const auto cursorRectangle = textInput->cursorRectangle();
                moveResize({textClient->pos() + textClient->clientPos() + cursorRectangle.bottomLeft(), surface()->size()});
            }
        }   break;
    }
}

void InputPanelV1Client::destroyClient()
{
    markAsZombie();

    Deleted *deleted = Deleted::create(this);
    Q_EMIT windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    waylandServer()->removeClient(this);
    deleted->unrefWindow();

    delete this;
}

NET::WindowType InputPanelV1Client::windowType(bool, int) const
{
    return NET::Utility;
}

QRect InputPanelV1Client::inputGeometry() const
{
    return readyForPainting() ? surface()->input().boundingRect().translated(pos()) : QRect();
}

void InputPanelV1Client::setOutput(OutputInterface *outputIface)
{
    if (m_output) {
        disconnect(m_output, &AbstractWaylandOutput::geometryChanged, this, &InputPanelV1Client::reposition);
    }

    m_output = waylandServer()->findOutput(outputIface);

    if (m_output) {
        connect(m_output, &AbstractWaylandOutput::geometryChanged, this, &InputPanelV1Client::reposition);
    }
}

void InputPanelV1Client::moveResizeInternal(const QRect &rect, MoveResizeMode mode)
{
    Q_UNUSED(mode)
    updateGeometry(rect);
}

} // namespace KWin
