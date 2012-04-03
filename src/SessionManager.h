/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

// Qt
#include <QtGui/QKeySequence>
#include <QtCore/QAbstractListModel>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

// Konsole
#include "Profile.h"

class QSignalMapper;

namespace Konsole
{

class Session;

/**
 * Manages running terminal sessions.
 */
class KONSOLEPRIVATE_EXPORT SessionManager : public QObject
{
    Q_OBJECT

public:

    /**
     * Constructs a new session manager and loads information about the available
     * profiles.
     */
    SessionManager();

    /**
     * Destroys the SessionManager. All running sessions should be closed
     * (via closeAllSessions()) before the SessionManager is destroyed.
     */
    virtual ~SessionManager();

    /**
     * Returns the session manager instance.
     */
    static SessionManager* instance();

    /** Kill all running sessions. */
    void closeAllSessions();

    /**
     * Creates a new session using the settings specified by the specified
     * profile.
     *
     * The new session has no views associated with it.  A new TerminalDisplay view
     * must be created in order to display the output from the terminal session and
     * send keyboard or mouse input to it.
     *
     * @param profile A profile containing the settings for the new session.  If @p profile
     * is null the default profile (see defaultProfile()) will be used.
     */
    Session* createSession(Profile::Ptr profile = Profile::Ptr());

    /** Sets the profile associated with a session. */
    void setSessionProfile(Session* session, Profile::Ptr profile);

    /** Returns the profile associated with a session. */
    Profile::Ptr sessionProfile(Session* session) const;

    /**
     * Returns a list of active sessions.
     */
    const QList<Session*> sessions() const;

    // session management
    void saveSessions(KConfig* config);
    void restoreSessions(KConfig* config);
    int  getRestoreId(Session* session);
    Session* idToSession(int id);

signals:

    /**
     * Emitted when a session's settings are updated to match
     * its current profile.
     */
    void sessionUpdated(Session* session);

public slots:

protected slots:

    /**
     * Called to inform the manager that a session has finished executing.
     *
     * @param session The Session which has finished executing.
     */
    void sessionTerminated(QObject* session);

private slots:

    void sessionProfileCommandReceived(const QString& text);

    void profileChanged(Profile::Ptr profile);

private:

    // applies updates to a profile
    // to all sessions currently using that profile
    // if modifiedPropertiesOnly is true, only properties which
    // are set in the profile @p key are updated
    void applyProfile(Profile::Ptr profile , bool modifiedPropertiesOnly);

    // apples updates to the profile @p profile to the session @p session
    // if modifiedPropertiesOnly is true, only properties which
    // are set in @p profile are update ( ie. properties for which profile->isPropertySet(<property>)
    // returns true )
    void applyProfile(Session* session , const Profile::Ptr profile , bool modifiedPropertiesOnly);

    QList<Session*> _sessions; // list of running sessions

    QHash<Session*, Profile::Ptr> _sessionProfiles;
    QHash<Session*, Profile::Ptr> _sessionRuntimeProfiles;
    QHash<Session*, int> _restoreMapping;

    QSignalMapper* _sessionMapper;
};

/** Utility class to simplify code in SessionManager::applyProfile(). */
class ShouldApplyProperty2
{
public:
    ShouldApplyProperty2(const Profile::Ptr profile , bool modifiedOnly) :
        _profile(profile) , _modifiedPropertiesOnly(modifiedOnly) {}

    bool shouldApply(Profile::Property property) const {
        return !_modifiedPropertiesOnly || _profile->isPropertySet(property);
    }
private:
    const Profile::Ptr _profile;
    bool _modifiedPropertiesOnly;
};

/**
 * Item-view model which contains a flat list of sessions.
 * After constructing the model, call setSessions() to set the sessions displayed
 * in the list.  When a session ends (after emitting the finished() signal) it is
 * automatically removed from the list.
 *
 * The internal pointer for each item in the model (index.internalPointer()) is the
 * associated Session*
 */
class SessionListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit SessionListModel(QObject* parent = 0);

    /**
     * Sets the list of sessions displayed in the model.
     * To display all sessions that are currently running in the list,
     * call setSessions(SessionManager::instance()->sessions())
     */
    void setSessions(const QList<Session*>& sessions);

    // reimplemented from QAbstractItemModel
    virtual QModelIndex index(int row, int column, const QModelIndex& parent) const;
    virtual QVariant data(const QModelIndex& index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
                                int role) const;
    virtual int columnCount(const QModelIndex& parent) const;
    virtual int rowCount(const QModelIndex& parent) const;
    virtual QModelIndex parent(const QModelIndex& index) const;

protected:
    virtual void sessionRemoved(Session*) {}

private slots:
    void sessionFinished();

private:
    QList<Session*> _sessions;
};

}
#endif //SESSIONMANAGER_H
