#include "notificationmanager.h"

#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QSettings>
#include <QDBusInterface>
#include <QDBusPendingReply>

class NotificationManagerPrivate
{
    Q_DECLARE_PUBLIC(NotificationManager)

public:
    NotificationManagerPrivate(NotificationManager *q)
        : q_ptr(q),
          interface(NULL),
          connected(false)
    { /*...*/ }

    NotificationManager *q_ptr;

    QDBusInterface *interface;

    bool connected;
};

NotificationManager::NotificationManager(Settings *settings, QObject *parent)
    : QObject(parent), d_ptr(new NotificationManagerPrivate(this)), settings(settings)
{
    Q_D(NotificationManager);
    QDBusConnection::sessionBus().registerObject("/org/freedesktop/Notifications", this, QDBusConnection::ExportAllSlots);

    d->interface = new QDBusInterface("org.freedesktop.DBus",
                                      "/org/freedesktop/DBus",
                                      "org.freedesktop.DBus");

    d->interface->call("AddMatch", "interface='org.freedesktop.Notifications',member='Notify',type='method_call',eavesdrop='true'");

    this->initialize();
}

NotificationManager::~NotificationManager()
{
    Q_D(NotificationManager);
    delete d;
}


void NotificationManager::initialize(bool notifyError)
{
    Q_D(NotificationManager);
    bool success = false;

    if(d->interface->isValid())
    {
        success = true;
    }

    if(!(d->connected = success))
    {
        QTimer::singleShot(2000, this, SLOT(initialize()));
        if(notifyError) emit this->error("Failed to connect to Notifications D-Bus service.");
    }
}

QDBusInterface* NotificationManager::interface() const
{
    Q_D(const NotificationManager);
    return d->interface;
}

QString NotificationManager::getCleanAppName(QString app_name)
{
    QString desktopFile = QString("/usr/share/applications/%1.desktop").arg(app_name);
    QFile testFile(desktopFile);
    if (testFile.exists()) {
        QSettings settings(desktopFile, QSettings::IniFormat);
        settings.beginGroup("Desktop Entry");
        QString cleanName = settings.value("Name").toString();
        settings.endGroup();
        if (!cleanName.isEmpty()) {
            return cleanName;
        }
    }
    return app_name;
}

QStringHash NotificationManager::getCategoryParams(QString category)
{
    if (!category.isEmpty()) {
        QString categoryConfigFile = QString("/usr/share/lipstick/notificationcategories/%1.conf").arg(category);
        QFile testFile(categoryConfigFile);
        if (testFile.exists()) {
            QStringHash categories;
            QSettings settings(categoryConfigFile, QSettings::IniFormat);
            const QStringList settingKeys = settings.allKeys();
            foreach (const QString &settingKey, settingKeys) {
                categories[settingKey] = settings.value(settingKey).toString();
            }
            return categories;
        }
    }
    return QStringHash();
}

void NotificationManager::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions, const QVariantHash &hints, int expire_timeout)
{
    Q_UNUSED(replaces_id);
    Q_UNUSED(app_icon);
    Q_UNUSED(actions);
    Q_UNUSED(expire_timeout);

    //Ignore notifcations from myself
    if (app_name == "pebbled") {
        return;
    }

    logger()->debug() << Q_FUNC_INFO  << "Got notification via dbus from" << this->getCleanAppName(app_name);
    logger()->debug() << hints;

    if (app_name == "messageserver5") {

        if (!settings->property("notificationsEmails").toBool()) {
            logger()->debug() << "Ignoring email notification because of setting!";
            return;
        }

        QString subject = hints.value("x-nemo-preview-summary", "").toString();
        QString data = hints.value("x-nemo-preview-body", "").toString();
        if (!data.isEmpty() && !subject.isEmpty()) {
            emit this->emailNotify(subject, data, "");
        }
    } else if (app_name == "commhistoryd") {
        if (summary == "" && body == "") {
            QString category = hints.value("category", "").toString();

            if (category == "x-nemo.call.missed") {
                if (!settings->property("notificationsMissedCall").toBool()) {
                    logger()->debug() << "Ignoring MissedCall notification because of setting!";
                    return;
                }
            } else {
                if (!settings->property("notificationsCommhistoryd").toBool()) {
                    logger()->debug() << "Ignoring commhistoryd notification because of setting!";
                    return;
                }
            }
            emit this->smsNotify(hints.value("x-nemo-preview-summary", "default").toString(),
                                 hints.value("x-nemo-preview-body", "default").toString()
                                );
        }
    } else if (app_name == "harbour-mitakuuluu2-server") {

        if (!settings->property("notificationsMitakuuluu").toBool()) {
            logger()->debug() << "Ignoring mitakuuluu notification because of setting!";
            return;
        }

        emit this->smsNotify(hints.value("x-nemo-preview-body", "default").toString(),
                             hints.value("x-nemo-preview-summary", "default").toString()
                            );

    } else if (app_name == "twitter-notifications-client") {

        if (!settings->property("notificationsTwitter").toBool()) {
            logger()->debug() << "Ignoring twitter notification because of setting!";
            return;
        }

        emit this->twitterNotify(hints.value("x-nemo-preview-body", body).toString(),
                             hints.value("x-nemo-preview-summary", summary).toString()
                            );

    } else if (app_name == "harbour-communi") {
        if (!settings->property("notificationsIRC").toBool()) {
            logger()->debug() << "Ignoring irc notification because of setting!";
            return;
        }
        emit this->smsNotify(hints.value("x-nemo-preview-summary", summary).toString(),
                             hints.value("x-nemo-preview-body",body).toString()
                             );

    } else {
        //Prioritize x-nemo-preview* over dbus direct summary and body
        QString subject = hints.value("x-nemo-preview-summary", "").toString();
        QString data = hints.value("x-nemo-preview-body", "").toString();
        QString category = hints.value("category", "").toString();
        QStringHash categoryParams = this->getCategoryParams(category);
        int prio = categoryParams.value("x-nemo-priority", "0").toInt();

        logger()->debug() << "MSG Prio:" << prio;

        if (!settings->property("notificationsAll").toBool() && prio <= 10) {
            logger()->debug() << "Ignoring notification because of setting! (all)";
            return;
        }

        if (!settings->property("notificationsOther").toBool() && prio < 90) {
            logger()->debug() << "Ignoring notification because of setting! (other)";
            return;
        }

        if (subject.isEmpty()) {
            subject = summary;
        }
        if (data.isEmpty()) {
            data = body;
        }

        //Prioritize data over subject
        if (data.isEmpty() && !subject.isEmpty()) {
            data = subject;
            subject = "";
        }

        //Never send empty data and subject
        if (data.isEmpty() && subject.isEmpty()) {
            logger()->warn() << Q_FUNC_INFO << "Empty subject and data in dbus app:" << app_name;
            return;
        }

        emit this->emailNotify(this->getCleanAppName(app_name), data, subject);
    }
}
