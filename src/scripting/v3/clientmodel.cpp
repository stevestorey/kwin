/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientmodel.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "platform.h"
#include "virtualdesktops.h"
#include "workspace.h"

namespace KWin::ScriptingModels::V3
{

ClientModel::ClientModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(workspace(), &Workspace::clientAdded, this, &ClientModel::handleClientAdded);
    connect(workspace(), &Workspace::clientRemoved, this, &ClientModel::handleClientRemoved);

    m_clients = workspace()->allClientList();
    for (AbstractClient *client : qAsConst(m_clients)) {
        setupClientConnections(client);
    }
}

void ClientModel::markRoleChanged(AbstractClient *client, int role)
{
    const QModelIndex row = index(m_clients.indexOf(client), 0);
    Q_EMIT dataChanged(row, row, {role});
}

void ClientModel::setupClientConnections(AbstractClient *client)
{
    connect(client, &AbstractClient::desktopChanged, this, [this, client]() {
        markRoleChanged(client, DesktopRole);
    });
    connect(client, &AbstractClient::screenChanged, this, [this, client]() {
        markRoleChanged(client, ScreenRole);
    });
    connect(client, &AbstractClient::activitiesChanged, this, [this, client]() {
        markRoleChanged(client, ActivityRole);
    });
}

void ClientModel::handleClientAdded(AbstractClient *client)
{
    beginInsertRows(QModelIndex(), m_clients.count(), m_clients.count());
    m_clients.append(client);
    endInsertRows();

    setupClientConnections(client);
}

void ClientModel::handleClientRemoved(AbstractClient *client)
{
    const int index = m_clients.indexOf(client);
    Q_ASSERT(index != -1);

    beginRemoveRows(QModelIndex(), index, index);
    m_clients.removeAt(index);
    endRemoveRows();
}

QHash<int, QByteArray> ClientModel::roleNames() const
{
    return {
        { Qt::DisplayRole, QByteArrayLiteral("display") },
        { ClientRole, QByteArrayLiteral("client") },
        { ScreenRole, QByteArrayLiteral("screen") },
        { DesktopRole, QByteArrayLiteral("desktop") },
        { ActivityRole, QByteArrayLiteral("activity") },
    };
}

QVariant ClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_clients.count()) {
        return QVariant();
    }

    AbstractClient *client = m_clients[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case ClientRole:
        return QVariant::fromValue(client);
    case ScreenRole:
        return client->screen();
    case DesktopRole:
        return client->desktop();
    case ActivityRole:
        return client->activities();
    default:
        return QVariant();
    }
}

int ClientModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_clients.count();
}

ClientFilterModel::ClientFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

ClientModel *ClientFilterModel::clientModel() const
{
    return m_clientModel;
}

void ClientFilterModel::setClientModel(ClientModel *clientModel)
{
    if (clientModel == m_clientModel) {
        return;
    }
    m_clientModel = clientModel;
    setSourceModel(m_clientModel);
    Q_EMIT clientModelChanged();
}

QString ClientFilterModel::activity() const
{
    return m_activity.value_or(QString());
}

void ClientFilterModel::setActivity(const QString &activity)
{
    if (m_activity != activity) {
        m_activity = activity;
        Q_EMIT activityChanged();
        invalidateFilter();
    }
}

void ClientFilterModel::resetActivity()
{
    if (m_activity.has_value()) {
        m_activity.reset();
        Q_EMIT activityChanged();
        invalidateFilter();
    }
}

VirtualDesktop *ClientFilterModel::desktop() const
{
    return m_desktop;
}

void ClientFilterModel::setDesktop(VirtualDesktop *desktop)
{
    if (m_desktop != desktop) {
        m_desktop = desktop;
        Q_EMIT desktopChanged();
        invalidateFilter();
    }
}

void ClientFilterModel::resetDesktop()
{
    setDesktop(nullptr);
}

QString ClientFilterModel::filter() const
{
    return m_filter;
}

void ClientFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    invalidateFilter();
}

QString ClientFilterModel::screenName() const
{
    return m_output ? m_output->name() : QString();
}

void ClientFilterModel::setScreenName(const QString &screen)
{
    AbstractOutput *output = kwinApp()->platform()->findOutput(screen);
    if (m_output != output) {
        m_output = output;
        Q_EMIT screenNameChanged();
        invalidateFilter();
    }
}

void ClientFilterModel::resetScreenName()
{
    if (m_output) {
        m_output = nullptr;
        Q_EMIT screenNameChanged();
        invalidateFilter();
    }
}

ClientFilterModel::WindowTypes ClientFilterModel::windowType() const
{
    return m_windowType.value_or(WindowTypes());
}

void ClientFilterModel::setWindowType(WindowTypes windowType)
{
    if (m_windowType != windowType) {
        m_windowType = windowType;
        Q_EMIT windowTypeChanged();
        invalidateFilter();
    }
}

void ClientFilterModel::resetWindowType()
{
    if (m_windowType.has_value()) {
        m_windowType.reset();
        Q_EMIT windowTypeChanged();
        invalidateFilter();
    }
}

bool ClientFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_clientModel) {
        return false;
    }
    const QModelIndex index = m_clientModel->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
        return false;
    }
    const QVariant data = index.data();
    if (!data.isValid()) {
        // an invalid QVariant is valid data
        return true;
    }

    AbstractClient *client = qvariant_cast<AbstractClient *>(data);
    if (!client) {
        return false;
    }

    if (m_activity.has_value()) {
        if (!client->isOnActivity(*m_activity)) {
            return false;
        }
    }

    if (m_desktop) {
        if (!client->isOnDesktop(m_desktop)) {
            return false;
        }
    }

    if (m_output) {
        if (!client->isOnOutput(m_output)) {
            return false;
        }
    }

    if (m_windowType.has_value()) {
        if (!(windowTypeMask(client) & *m_windowType)) {
            return false;
        }
    }

    if (!m_filter.isEmpty()) {
        if (client->caption().contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        const QString windowRole(QString::fromUtf8(client->windowRole()));
        if (windowRole.contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        const QString resourceName(QString::fromUtf8(client->resourceName()));
        if (resourceName.contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        const QString resourceClass(QString::fromUtf8(client->resourceClass()));
        if (resourceClass.contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        return false;
    }
    return true;
}

ClientFilterModel::WindowTypes ClientFilterModel::windowTypeMask(AbstractClient *client) const
{
    WindowTypes mask;
    if (client->isNormalWindow()) {
        mask |= WindowType::Normal;
    } else if (client->isDialog()) {
        mask |= WindowType::Dialog;
    } else if (client->isDock()) {
        mask |= WindowType::Dock;
    } else if (client->isDesktop()) {
        mask |= WindowType::Desktop;
    } else if (client->isNotification()) {
        mask |= WindowType::Notification;
    } else if (client->isCriticalNotification()) {
        mask |= WindowType::CriticalNotification;
    }
    return mask;
}

} // namespace KWin::ScriptingModels::V3
