#include "voicecallmanager.h"

#include <QDebug>
#include <QTimer>
#include <QDBusInterface>
#include <QDBusPendingReply>

class VoiceCallManagerPrivate
{
    Q_DECLARE_PUBLIC(VoiceCallManager)

public:
    VoiceCallManagerPrivate(VoiceCallManager *q)
        : q_ptr(q),
          interface(NULL),
          activeVoiceCall(NULL),
          connected(false)
    { /*...*/ }

    VoiceCallManager *q_ptr;

    QDBusInterface *interface;

    QList<VoiceCallHandler*> voicecalls;
    QHash<QString,VoiceCallProviderData> providers;

    VoiceCallHandler* activeVoiceCall;

    bool connected;
};

VoiceCallManager::VoiceCallManager(Settings *settings, QObject *parent)
    : QObject(parent), d_ptr(new VoiceCallManagerPrivate(this)), settings(settings)
{
    this->initialize();
}

VoiceCallManager::~VoiceCallManager()
{
    Q_D(VoiceCallManager);
    delete d;
}

void VoiceCallManager::initialize(bool notifyError)
{
    Q_D(VoiceCallManager);
    bool success = false;

    delete d->interface;
    d->interface = new QDBusInterface("org.nemomobile.voicecall",
                                      "/",
                                      "org.nemomobile.voicecall.VoiceCallManager",
                                      QDBusConnection::sessionBus(),
                                      this);

    if(d->interface->isValid())
    {
        success = true;
        success &= (bool)QObject::connect(d->interface, SIGNAL(error(QString)), SIGNAL(error(QString)));
        success &= (bool)QObject::connect(d->interface, SIGNAL(voiceCallsChanged()), SLOT(onVoiceCallsChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(providersChanged()), SLOT(onProvidersChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(activeVoiceCallChanged()), SLOT(onActiveVoiceCallChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(audioModeChanged()), SIGNAL(audioModeChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(audioRoutedChanged()), SIGNAL(audioRoutedChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(microphoneMutedChanged()), SIGNAL(microphoneMutedChanged()));
        success &= (bool)QObject::connect(d->interface, SIGNAL(speakerMutedChanged()), SIGNAL(speakerMutedChanged()));

        onVoiceCallsChanged();
        onActiveVoiceCallChanged();
    }

    if(!(d->connected = success))
    {
        QTimer::singleShot(2000, this, SLOT(initialize()));
        if(notifyError) emit this->error("Failed to connect to VCM D-Bus service.");
    }
}

QDBusInterface* VoiceCallManager::interface() const
{
    Q_D(const VoiceCallManager);
    return d->interface;
}

VoiceCallHandlerList VoiceCallManager::voiceCalls() const
{
    Q_D(const VoiceCallManager);
    return d->voicecalls;
}

VoiceCallProviderHash VoiceCallManager::providers() const
{
    Q_D(const VoiceCallManager);
    return d->providers;
}

QString VoiceCallManager::defaultProviderId() const
{
    Q_D(const VoiceCallManager);
    if(d->providers.count() == 0) {
        logger()->debug() << Q_FUNC_INFO << "No provider added";
        return QString::null;
    }

    QStringList keys = d->providers.keys();
    qSort(keys);

    VoiceCallProviderData provider = d->providers.value(keys.value(0));
    return provider.id;
}

VoiceCallHandler* VoiceCallManager::activeVoiceCall() const
{
    Q_D(const VoiceCallManager);
    return d->activeVoiceCall;
}

QString VoiceCallManager::audioMode() const
{
    Q_D(const VoiceCallManager);
    return d->interface->property("audioMode").toString();
}

bool VoiceCallManager::isAudioRouted() const
{
    Q_D(const VoiceCallManager);
    return d->interface->property("isAudioRouted").toBool();
}

bool VoiceCallManager::isMicrophoneMuted() const
{
    Q_D(const VoiceCallManager);
    return d->interface->property("isMicrophoneMuted").toBool();
}

bool VoiceCallManager::isSpeakerMuted() const
{
    Q_D(const VoiceCallManager);
    return d->interface->property("isSpeakerMuted").toBool();
}

void VoiceCallManager::dial(const QString &provider, const QString &msisdn)
{
    Q_D(VoiceCallManager);
    QDBusPendingCall call = d->interface->asyncCall("dial", provider, msisdn);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(onPendingCallFinished(QDBusPendingCallWatcher*)));
}

void VoiceCallManager::silenceRingtone()
{
    Q_D(const VoiceCallManager);
    QDBusPendingCall call = d->interface->asyncCall("silenceRingtone");
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(onPendingSilenceFinished(QDBusPendingCallWatcher*)));
}

/*
  - Use of method calls instead of property setters to allow status checking.
 */
bool VoiceCallManager::setAudioMode(const QString &mode)
{
    Q_D(const VoiceCallManager);
    QDBusPendingReply<bool> reply = d->interface->call("setAudioMode", mode);
    return reply.isError() ? false : reply.value();
}

bool VoiceCallManager::setAudioRouted(bool on)
{
    Q_D(const VoiceCallManager);
    QDBusPendingReply<bool> reply = d->interface->call("setAudioRouted", on);
    return reply.isError() ? false : reply.value();
}

bool VoiceCallManager::setMuteMicrophone(bool on)
{
    Q_D(VoiceCallManager);
    QDBusPendingReply<bool> reply = d->interface->call("setMuteMicrophone", on);
    return reply.isError() ? false : reply.value();
}

bool VoiceCallManager::setMuteSpeaker(bool on)
{
    Q_D(VoiceCallManager);
    QDBusPendingReply<bool> reply = d->interface->call("setMuteSpeaker", on);
    return reply.isError() ? false : reply.value();
}

void VoiceCallManager::onVoiceCallsChanged()
{
    Q_D(VoiceCallManager);
    QStringList nIds = d->interface->property("voiceCalls").toStringList();
    QStringList oIds;

    QStringList added;
    QStringList removed;

    // Map current call handlers to handler ids for easy indexing.
    foreach(VoiceCallHandler *handler, d->voicecalls)
    {
        oIds.append(handler->handlerId());
    }

    // Index new handlers to be added.
    foreach(QString nId, nIds)
    {
        if(!oIds.contains(nId)) added.append(nId);
    }

    // Index old handlers to be removed.
    foreach(QString oId, oIds)
    {
        if(!nIds.contains(oId)) removed.append(oId);
    }

    // Remove handlers that need to be removed.
    foreach(QString removeId, removed)
    {
        for (int i = 0; i < d->voicecalls.count(); ++i) {
            VoiceCallHandler *handler = d->voicecalls.at(i);
            if(handler->handlerId() == removeId)
            {
                handler->disconnect(this);
                d->voicecalls.removeAt(i);
                handler->deleteLater();
                break;
            }
        }
    }

    // Add handlers that need to be added.
    foreach(QString addId, added)
    {
        VoiceCallHandler *handler = new VoiceCallHandler(addId, this);
        d->voicecalls.append(handler);
    }

    emit this->voiceCallsChanged();
}

void VoiceCallManager::onProvidersChanged()
{
    Q_D(VoiceCallManager);
    d->providers.clear();
    foreach(QString provider, d->interface->property("providers").toStringList())
    {
        QStringList parts = provider.split(':');
        d->providers.insert(parts.first(), VoiceCallProviderData(parts.first(),
                                                                 parts.last(),
                                                                 parts.first()));
    }

    emit this->providersChanged();
}

void VoiceCallManager::onActiveVoiceCallChanged()
{
    Q_D(VoiceCallManager);
    QString voiceCallId = d->interface->property("activeVoiceCall").toString();

    if(d->voicecalls.count() == 0 || voiceCallId.isNull() || voiceCallId.isEmpty())
    {
        d->activeVoiceCall = NULL;
    }
    else
    {
        bool found = false;
        d->activeVoiceCall = NULL;
        foreach(VoiceCallHandler* handler, d->voicecalls)
        {
            if(handler->handlerId() == voiceCallId)
            {
                d->activeVoiceCall = handler;
                found = true;
            }
            if(!found) d->activeVoiceCall = NULL;
        }
    }

    emit this->activeVoiceCallChanged();
}

void VoiceCallManager::onPendingCallFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<bool> reply = *watcher;

    if (reply.isError()) {
        emit this->error(reply.error().message());
    } else {
        logger()->debug() << QString("Received successful reply for member: ") + reply.reply().member();
    }

    watcher->deleteLater();
}

void VoiceCallManager::onPendingSilenceFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;

    if (reply.isError()) {
        emit this->error(reply.error().message());
    } else {
        logger()->debug() << QString("Received successful reply for member: ") + reply.reply().member();
    }

    watcher->deleteLater();
}
