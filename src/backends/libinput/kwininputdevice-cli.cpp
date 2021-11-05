/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol i Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "InputDevice_interface.h"
#include "config-kwin.h"
#include <KAboutData>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>

QSharedPointer<OrgKdeKWinInputDeviceInterface> fetchDevice(const QString &sysname)
{
    auto ret = QSharedPointer<OrgKdeKWinInputDeviceInterface>::create(QStringLiteral("org.kde.KWin"),
                                                                      QStringLiteral("/org/kde/KWin/InputDevice/") + sysname,
                                                                      QDBusConnection::sessionBus());
    if (ret->sysName() != sysname) {
        QTextStream err(stderr);
        err << i18n("error: Could not find a device named %1, use --list to find the available devices.", sysname) << Qt::endl;
        exit(1);
    }
    return ret;
}

const QByteArray s_supports = "supports";
static bool isPropertySupported(const QMetaProperty &prop, const QSharedPointer<OrgKdeKWinInputDeviceInterface> &device)
{
    if (!prop.isWritable()) {
        return false;
    }

    const int supportedEnd = s_supports.size();
    QByteArray supportedPropertyName = s_supports + prop.name();
    supportedPropertyName[supportedEnd] = QChar(supportedPropertyName[supportedEnd]).toUpper().toLatin1();
    QVariant supportedVariant = device->property(supportedPropertyName);
    return supportedVariant.isValid() && supportedVariant.toBool();
}

int main(int argc, char **argv)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    QCoreApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("org.kde.plasma.kwininputdevice-cli");

    KAboutData about(QStringLiteral("kwininputdevice-cli"),
                     i18n("KWin Input Device"),
                     QStringLiteral(KWIN_VERSION_STRING),
                     i18n("Configure your libinput devices on kwin_wayland sessions"),
                     KAboutLicense::GPL,
                     i18n("(C) 2021 Aleix Pol i Gonzalez"));
    about.addAuthor(QStringLiteral("Aleix Pol i Gonzalez"), QString(), QStringLiteral("aleixpol@kde.org"));
    about.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    QCommandLineOption optionListDevices(QStringLiteral("list"), i18n("List devices"));
    parser.addOption(optionListDevices);

    QCommandLineOption optionInspectDevice(QStringLiteral("inspect"), i18n("List device values"), i18n("device"));
    parser.addOption(optionInspectDevice);
    QCommandLineOption optionInspectFullDevice(QStringLiteral("inspect-full"), i18n("List device values, including read-only and unsupported"), i18n("device"));
    parser.addOption(optionInspectFullDevice);
    QCommandLineOption optionSet(QStringLiteral("set"), i18n("Set a value, pass them as name=value pairs"), i18n("device"));
    parser.addOption(optionSet);
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    if (parser.isSet(optionListDevices)) {
        auto m_deviceManager = new QDBusInterface(QStringLiteral("org.kde.KWin"),
                                                  QStringLiteral("/org/kde/KWin/InputDevice"),
                                                  QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                                  QDBusConnection::sessionBus(),
                                                  &app);
        const QVariant reply = m_deviceManager->property("devicesSysNames");
        QStringList devicesSysNames;
        if (reply.isValid()) {
            devicesSysNames = reply.toStringList();
            devicesSysNames.sort();
        } else {
            err << "error: Could not fetch a device list from KWin.";
            return 1;
        }

        for (const QString &sysname : devicesSysNames) {
            auto device = fetchDevice(sysname);
            out << sysname << ": " << device->name() << Qt::endl;
        }
    }

    if (parser.isSet(optionInspectDevice) || parser.isSet(optionInspectFullDevice)) {
        const bool full = parser.isSet(optionInspectFullDevice);
        const QString sysname = parser.value(full ? optionInspectFullDevice : optionInspectDevice);
        auto device = fetchDevice(sysname);

        const auto &mo = OrgKdeKWinInputDeviceInterface::staticMetaObject;

        out << i18n("Properties for '%1' (%2):", device->name(), sysname) << Qt::endl << Qt::endl;
        for (int i = 0, c = mo.propertyCount(); i < c; ++i) {
            const QMetaProperty prop = mo.property(mo.propertyOffset() + i);
            if (QByteArray(prop.name()).startsWith(s_supports) || prop.name() == 0) {
                continue;
            }

            bool isSupported = isPropertySupported(prop, device);
            if (isSupported || full) {
                QString output = !isSupported && !prop.isWritable() ? i18n("%1 (unsupported, read-only) = %2", prop.name(), prop.read(device.data()).toString())
                    : !isSupported                                  ? i18n("%1 (unsupported) = %2", prop.name(), prop.read(device.data()).toString())
                    : !prop.isWritable()                            ? i18n("%1 (read-only) = %2", prop.name(), prop.read(device.data()).toString())
                                                                    : i18n("%1 = %2", prop.name(), prop.read(device.data()).toString());
                out << output << Qt::endl;
            }
        }
    }

    if (parser.isSet(optionSet)) {
        auto device = fetchDevice(parser.value(optionSet));
        const auto &mo = OrgKdeKWinInputDeviceInterface::staticMetaObject;
        const auto args = parser.positionalArguments();
        if (args.isEmpty()) {
            err << i18n("No values to set") << Qt::endl;
            return 1;
        }

        for (const QString &name : args) {
            const auto pair = QStringView(name).split('=');
            if (pair.count() != 2 || pair.first().isEmpty() || pair.last().isEmpty()) {
                err << i18n("error: Wrong format for a name=value pair: %1", name) << Qt::endl;
                return 3;
            }
            int idx = mo.indexOfProperty(pair.first().toLatin1().constData());
            if (idx < 0) {
                err << i18n("error: Property '%1' not found from pair %2", pair[0].toString(), name) << Qt::endl;
                return 4;
            }

            const QMetaProperty prop = mo.property(idx);
            if (!isPropertySupported(prop, device)) {
                err << i18n("error: Property '%1' cannot be written into", pair[0].toString()) << Qt::endl;
                return 5;
            }

            QVariant value;
            switch (prop.type()) {
            case QVariant::Bool:
                if (pair[1] != QLatin1String("true") && pair[1] != QLatin1String("false")) {
                    err << i18nc("Not translating the true and false for portability",
                                 "error: Cannot convert '%1' is a boolean value, it should be either true or false rather than '%2'",
                                 pair[0].toString(),
                                 pair[1].toString())
                        << Qt::endl;
                    return 6;
                }
                value = pair[1] == QLatin1String("true");
                break;
            default:
                value = QVariant(pair.last().toString());
                if (!value.convert(prop.type())) {
                    err << i18n("error: Cannot convert '%1' into a value suitable for '%2'", pair[0].toString(), pair[1].toString()) << Qt::endl;
                    return 7;
                }
                break;
            }

            if (!prop.write(device.data(), value)) {
                err << i18n("error: Could not set '%1'", name) << Qt::endl;
                return 6;
            }
            out << prop.name() << '=' << prop.read(device.data()).toString() << Qt::endl;
        }
    }

    return 0;
}
