/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerplugin.h"

#include "sshmanagermodel.h"
#include "sshmanagerpluginwidget.h"

#include "ProcessInfo.h"
#include "konsoledebug.h"
#include "session/SessionController.h"

#include <QDockWidget>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>

#include <KActionCollection>
#include <KCommandBar>
#include <KLocalizedString>
#include <KMessageBox>

#include "MainWindow.h"
#include "terminalDisplay/TerminalDisplay.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPluginPrivate {
    SSHManagerModel model;

    QMap<Konsole::MainWindow *, SSHManagerTreeWidget *> widgetForWindow;
    QMap<Konsole::MainWindow *, QDockWidget *> dockForWindow;
    QAction *showQuickAccess = nullptr;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , d(std::make_unique<SSHManagerPluginPrivate>())
{
    d->showQuickAccess = new QAction();

    setName(QStringLiteral("SshManager"));
}

SSHManagerPlugin::~SSHManagerPlugin()
{
}

void SSHManagerPlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    auto *sshDockWidget = new QDockWidget(mainWindow);
    auto *managerWidget = new SSHManagerTreeWidget();
    managerWidget->setModel(&d->model);
    sshDockWidget->setWidget(managerWidget);
    sshDockWidget->setWindowTitle(i18n("SSH Manager"));
    sshDockWidget->setObjectName(QStringLiteral("SSHManagerDock"));
    sshDockWidget->setVisible(false);

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, sshDockWidget);

    d->widgetForWindow[mainWindow] = managerWidget;
    d->dockForWindow[mainWindow] = sshDockWidget;

    connect(managerWidget, &SSHManagerTreeWidget::requestNewTab, this, [mainWindow] {
        mainWindow->newTab();
    });
}

QList<QAction *> SSHManagerPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    Q_UNUSED(mainWindow);

    QAction *toggleVisibilityAction = new QAction(i18n("Show SSH Manager"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    toggleVisibilityAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F2));
    connect(toggleVisibilityAction, &QAction::triggered, d->dockForWindow[mainWindow], &QDockWidget::setVisible);
    connect(d->dockForWindow[mainWindow], &QDockWidget::visibilityChanged, toggleVisibilityAction, &QAction::setChecked);

    return {toggleVisibilityAction};
}

#include <iostream>

void SSHManagerPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_ASSERT(controller);
    Q_ASSERT(mainWindow);

    auto terminalDisplay = controller->view();

    d->showQuickAccess->deleteLater();
    d->showQuickAccess = new QAction(i18n("Show Quick Access for SSH Actions"));

    d->showQuickAccess->setShortcut(QKeySequence(Qt::CTRL + Qt::ALT + Qt::Key_H));
    terminalDisplay->addAction(d->showQuickAccess);

    connect(d->showQuickAccess, &QAction::triggered, this, [this, terminalDisplay, controller] {
        KCommandBar bar(terminalDisplay->topLevelWidget());
        QList<QAction *> actions;
        for (int i = 0; i < d->model.rowCount(); i++) {
            QModelIndex folder = d->model.index(i, 0);
            for (int e = 0; e < d->model.rowCount(folder); e++) {
                QModelIndex idx = d->model.index(e, 0, folder);
                QAction *act = new QAction(idx.data().toString());
                connect(act, &QAction::triggered, this, [this, idx, controller] {
                    requestConnection(nullptr, &d->model, controller, idx);
                });
                actions.append(act);
            }
        }

        QVector<KCommandBar::ActionGroup> groups;
        groups.push_back(KCommandBar::ActionGroup{i18n("SSH Entries"), actions});
        bar.setActions(groups);
        bar.exec();
    });

    if (mainWindow) {
        d->widgetForWindow[mainWindow]->setCurrentController(controller);
    }
}

void SSHManagerPlugin::requestConnection(QSortFilterProxyModel *filterModel,
                                         QStandardItemModel *model,
                                         Konsole::SessionController *controller,
                                         const QModelIndex &idx)
{
    if (!controller) {
        return;
    }

    auto sourceIdx = filterModel ? filterModel->mapToSource(idx) : idx;
    if (sourceIdx.parent() == model->invisibleRootItem()->index()) {
        return;
    }

    Konsole::ProcessInfo *info = controller->session()->getProcessInfo();
    bool ok = false;
    QString processName = info->name(&ok);
    if (!ok) {
        KMessageBox::error(nullptr, i18n("Could not get the process name, assume that we can't request a connection"), i18n("Error issuing SSH Command"));
        return;
    }

    if (!QVector<QString>({QStringLiteral("fish"),
                           QStringLiteral("bash"),
                           QStringLiteral("dash"),
                           QStringLiteral("sh"),
                           QStringLiteral("csh"),
                           QStringLiteral("ksh"),
                           QStringLiteral("zsh")})
             .contains(processName)) {
        KMessageBox::error(nullptr, i18n("Can't issue SSH command outside the shell application (eg, bash, zsh, sh)"), i18n("Error issuing SSH Command"));
        return;
    }

    auto item = model->itemFromIndex(sourceIdx);
    auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();

    QString sshCommand = QStringLiteral("ssh ");
    if (!data.useSshConfig) {
        if (data.sshKey.length()) {
            sshCommand += QStringLiteral("-i %1 ").arg(data.sshKey);
        }

        if (data.port.length()) {
            sshCommand += QStringLiteral("-p %1 ").arg(data.port);
        }

        if (!data.username.isEmpty()) {
            sshCommand += data.username + QLatin1Char('@');
        }
    }

    if (!data.host.isEmpty()) {
        sshCommand += data.host;
    }

    controller->session()->sendTextToTerminal(sshCommand, QLatin1Char('\r'));
    if (controller->session()->views().count()) {
        controller->session()->views().at(0)->setFocus();
    }
}

#include "sshmanagerplugin.moc"
