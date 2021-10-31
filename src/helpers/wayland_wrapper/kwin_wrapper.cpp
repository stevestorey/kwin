/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * This tiny executable creates a socket, then starts kwin passing it the FD to the wayland socket
 * along with the name of the socket to use
 * On any non-zero kwin exit kwin gets restarted.
 *
 * After restart kwin is relaunched but now with the KWIN_RESTART_COUNT env set to an incrementing counter
 *
 * It's somewhat  akin to systemd socket activation, but we also need the lock file, finding the next free socket
 * and so on, hence our own binary.
 *
 * Usage kwin_wayland_wrapper [argForKwin] [argForKwin] ...
 */

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QTemporaryFile>
#include <QDBusConnection>

#include <UpdateLaunchEnvironmentJob>

#include <signal.h>

#include "wl-socket.h"
#include "xwaylandsocket.h"
#include "xauthority.h"
#include "wrapper_logging.h"

class KWinWrapper : public QObject
{
    Q_OBJECT
public:
    KWinWrapper(QObject *parent);
    ~KWinWrapper();
    void run();

private:
    wl_socket *m_socket;

    int m_crashCount = 0;
    QProcess *m_kwinProcess = nullptr;

    QScopedPointer<KWin::XwaylandSocket> m_xwlSocket;
    QTemporaryFile m_xauthorityFile;
};

KWinWrapper::KWinWrapper(QObject *parent)
    : QObject(parent)
    , m_kwinProcess(new QProcess(this))
{
    m_socket = wl_socket_create();
    if (!m_socket) {
        qFatal("Could not create wayland socket");
    }

    if (qApp->arguments().contains(QLatin1String("--xwayland"))) {
        m_xwlSocket.reset(new KWin::XwaylandSocket(KWin::XwaylandSocket::OperationMode::TransferFdsOnExec));
        if (!m_xwlSocket->isValid()) {
            qCWarning(KWIN_WRAPPER) << "Failed to create Xwayland connection sockets";
            m_xwlSocket.reset();
        }
        if (m_xwlSocket) {
            if (!qEnvironmentVariableIsSet("KWIN_WAYLAND_NO_XAUTHORITY")) {
                if (!generateXauthorityFile(m_xwlSocket->display(), &m_xauthorityFile)) {
                    qCWarning(KWIN_WRAPPER) << "Failed to create an Xauthority file";
                }
            }
        }
    }
}

KWinWrapper::~KWinWrapper()
{
    wl_socket_destroy(m_socket);
    if (m_kwinProcess) {
        m_kwinProcess->terminate();
        m_kwinProcess->waitForFinished();
        m_kwinProcess->kill();
        m_kwinProcess->waitForFinished();
    }
}

void KWinWrapper::run()
{
    m_kwinProcess->setProgram("kwin_wayland");

    QStringList args;

    args << "--wayland_fd" << QString::number(wl_socket_get_fd(m_socket));
    args << "--socket" << QString::fromUtf8(wl_socket_get_display_name(m_socket));

    if (m_xwlSocket) {
        args << "--xwayland-fd" << QString::number(m_xwlSocket->abstractFileDescriptor());
        args << "--xwayland-fd" << QString::number(m_xwlSocket->unixFileDescriptor());
        args << "--xwayland-display" << m_xwlSocket->name();
        if (m_xauthorityFile.open()) {
            args << "--xwayland-xauthority" << m_xauthorityFile.fileName();
        }

    }

    // attach our main process arguments
    // the first entry is dropped as it will be our program name
    args << qApp->arguments().mid(1);

    m_kwinProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_kwinProcess->setArguments(args);

    connect(m_kwinProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus)
        if (exitCode == 0) {
            qApp->quit();
            return;
        } else if (exitCode == 133) {
            m_crashCount = 0;
        }
        m_crashCount++;

        if (m_crashCount > 10) {
            qApp->quit();
            return;
        }
        qputenv("KWIN_RESTART_COUNT", QByteArray::number(m_crashCount));
        // restart
        m_kwinProcess->start();
    });

    m_kwinProcess->start();

    QProcessEnvironment env;
    env.insert("WAYLAND_DISPLAY", QString::fromUtf8(wl_socket_get_display_name(m_socket)));
    if (m_xwlSocket) {
        env.insert("DISPLAY", m_xwlSocket->name());
        if (m_xauthorityFile.open()) {
            env.insert("XAUTHORITY", m_xauthorityFile.fileName());
        }
    }

    auto envSyncJob = new UpdateLaunchEnvironmentJob(env);
    connect(envSyncJob, &UpdateLaunchEnvironmentJob::finished, this, []() {
        // The service name is merely there to indicate to the world that we're up and ready with all envs exported
        QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWinWrapper"));
    });
}

void sigtermHandler(int)
{
    qApp->quit();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setQuitLockEnabled(false); // don't exit when the first KJob finishes

    signal(SIGTERM, sigtermHandler);

    KWinWrapper wrapper(&app);
    wrapper.run();

    return app.exec();
}

#include "kwin_wrapper.moc"
