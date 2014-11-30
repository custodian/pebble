#include <QtDBus>
#include "musicmanager.h"

MusicManager::MusicManager(WatchConnector *watch, QObject *parent)
    : QObject(parent), watch(watch)
{
    watch->setEndpointHandler(WatchConnector::watchMUSIC_CONTROL, [this](const QByteArray& data) {
        musicControl(WatchConnector::MusicControl(data.at(0)));
        return true;
    });
}

void MusicManager::onMprisMetadataChanged(QVariantMap metadata)
{
    QString track = metadata.value("xesam:title").toString();
    QString album = metadata.value("xesam:album").toString();
    QString artist = metadata.value("xesam:artist").toString();
    logger()->debug() << __FUNCTION__ << track << album << artist;
    watch->sendMusicNowPlaying(track, album, artist);
}

void MusicManager::musicControl(WatchConnector::MusicControl operation)
{
    logger()->debug() << "Operation:" << operation;

    QString mpris = parent()->property("mpris").toString();
    if (mpris.isEmpty()) {
        logger()->debug() << "No mpris interface active";
        return;
    }

    QString method;

    switch(operation) {
    case WatchConnector::musicPLAY_PAUSE:
        method = "PlayPause";
        break;
    case WatchConnector::musicPAUSE:
        method = "Pause";
        break;
    case WatchConnector::musicPLAY:
        method = "Play";
        break;
    case WatchConnector::musicNEXT:
        method = "Next";
        break;
    case WatchConnector::musicPREVIOUS:
        method = "Previous";
        break;
    case WatchConnector::musicVOLUME_UP:
    case WatchConnector::musicVOLUME_DOWN: {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusReply<QDBusVariant> VolumeReply = bus.call(
                    QDBusMessage::createMethodCall(mpris, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get")
                    << "org.mpris.MediaPlayer2.Player" << "Volume");
        if (VolumeReply.isValid()) {
            double volume = VolumeReply.value().variant().toDouble();
            if (operation == WatchConnector::musicVOLUME_UP) {
                volume += 0.1;
            }
            else {
                volume -= 0.1;
            }
            logger()->debug() << "Setting volume" << volume;
            QDBusError err = QDBusConnection::sessionBus().call(
                        QDBusMessage::createMethodCall(mpris, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Set")
                        << "org.mpris.MediaPlayer2.Player" << "Volume" << QVariant::fromValue(QDBusVariant(volume)));
            if (err.isValid()) {
                logger()->error() << err.message();
            }
        } else {
            logger()->error() << VolumeReply.error().message();
        }
        }
        return;
    case WatchConnector::musicGET_NOW_PLAYING:
        onMprisMetadataChanged(parent()->property("mprisMetadata").toMap());
        return;

    case WatchConnector::musicSEND_NOW_PLAYING:
        logger()->warn() << "Operation" << operation << "not supported";
        return;
    }

    if (method.isEmpty()) {
        logger()->error() << "Requested unsupported operation" << operation;
        return;
    }

    logger()->debug() << operation << "->" << method;

    QDBusError err = QDBusConnection::sessionBus().call(
                QDBusMessage::createMethodCall(mpris, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", method));
    if (err.isValid()) {
        logger()->error() << err.message();
    }
}