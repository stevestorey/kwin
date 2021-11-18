/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MODULE_H
#define MODULE_H

#include <KCModule>
#include <KPluginMetaData>
#include <KSharedConfig>

namespace Ui
{
class Module;
}

class KJob;
class KWinScriptsData;

class Module : public KCModule
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * @param parent Parent widget of the module
     * @param args Arguments for the module
     */
    explicit Module(QWidget *parent, const QVariantList &args = QVariantList());

    /**
     * Destructor.
     */
    ~Module() override;
    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void pendingDeletionsChanged();

protected Q_SLOTS:

    /**
     * Called when the import script button is clicked.
     */
    void importScript();

    void importScriptInstallFinished(KJob *job);

private:
    /**
     * UI
     */
    Ui::Module *ui;
    /**
     * Updates the contents of the list view.
     */
    void updateListViewContents();
    KSharedConfigPtr m_kwinConfig;
    KWinScriptsData *m_kwinScriptsData;
    QList<KPluginMetaData> m_pendingDeletions;
};

#endif // MODULE_H
