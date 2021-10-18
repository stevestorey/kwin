/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QTimer>

namespace KWin
{

class AbstractClient;

class KWIN_EXPORT IdleDetector : public QObject
{
    Q_OBJECT

public:
    explicit IdleDetector(std::chrono::milliseconds timeout, QObject *parent = nullptr);
    ~IdleDetector() override;

    void inhibit(AbstractClient *inhibitor);
    void uninhibit(AbstractClient *inhibitor);

    void activity();

Q_SIGNALS:
    void idle();
    void resumed();

private:
    QTimer *m_timer;
    QList<AbstractClient *> m_inhibitors;
};

} // namespace KWin
