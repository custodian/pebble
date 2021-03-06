/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -a dbusadaptor org.pebbled.xml
 *
 * qdbusxml2cpp is Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "dbusadaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class PebbledAdaptor
 */

PebbledAdaptor::PebbledAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

PebbledAdaptor::~PebbledAdaptor()
{
    // destructor
}

QString PebbledAdaptor::address() const
{
    // get the value of property address
    return qvariant_cast< QString >(parent()->property("address"));
}

bool PebbledAdaptor::connected() const
{
    // get the value of property connected
    return qvariant_cast< bool >(parent()->property("connected"));
}

QString PebbledAdaptor::name() const
{
    // get the value of property name
    return qvariant_cast< QString >(parent()->property("name"));
}

QVariantMap PebbledAdaptor::pebble() const
{
    // get the value of property pebble
    return qvariant_cast< QVariantMap >(parent()->property("pebble"));
}

void PebbledAdaptor::disconnect()
{
    // handle method call org.pebbled.disconnect
    QMetaObject::invokeMethod(parent(), "disconnect");
}

void PebbledAdaptor::ping(int val)
{
    // handle method call org.pebbled.ping
    QMetaObject::invokeMethod(parent(), "ping", Q_ARG(int, val));
}

void PebbledAdaptor::time()
{
    // handle method call org.pebbled.time
    QMetaObject::invokeMethod(parent(), "time");
}


void PebbledAdaptor::reconnect()
{
    // handle method call org.pebbled.reconnect
    QMetaObject::invokeMethod(parent(), "reconnect");
}

