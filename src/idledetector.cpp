/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "idledetector.h"
#include "wayland_server.h"

namespace KWin
{

IdleDetector::IdleDetector(std::chrono::milliseconds timeout, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(timeout);
    connect(m_timer, &QTimer::timeout, this, &IdleDetector::idle);
    m_timer->start();

    waylandServer()->addIdleDetector(this);
}

IdleDetector::~IdleDetector()
{
    if (waylandServer()) {
        waylandServer()->removeIdleDetector(this);
    }
}

void IdleDetector::inhibit(AbstractClient *inhibitor)
{
    if (!m_inhibitors.contains(inhibitor)) {
        m_inhibitors.append(inhibitor);
        if (m_inhibitors.count() == 1) {
            if (!m_timer->isActive()) {
                Q_EMIT resumed();
            }
            m_timer->stop();
        }
    }
}

void IdleDetector::uninhibit(AbstractClient *inhibitor)
{
    m_inhibitors.removeOne(inhibitor);
    if (m_inhibitors.isEmpty()) {
        m_timer->start();
    }
}

void IdleDetector::activity()
{
    if (m_inhibitors.isEmpty()) {
        if (!m_timer->isActive()) {
            Q_EMIT resumed();
        }
        m_timer->start();
    }
}

} // namespace KWin
