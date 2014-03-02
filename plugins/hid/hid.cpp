/*
  Q Light Controller
  hid.cpp

  Copyright (c) Heikki Junnila

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <QDebug>
#include <QDir>

#include "configurehid.h"
#include "hidfx5device.h"
#include "hidjsdevice.h"
#include "hidpoller.h"
#include "hidapi.h"
#include "hid.h"

/*****************************************************************************
 * HIDInputEvent
 *****************************************************************************/

static const QEvent::Type _HIDInputEventType = static_cast<QEvent::Type>
        (QEvent::registerEventType());

HIDInputEvent::HIDInputEvent(HIDDevice* device, quint32 input,
                             quint32 channel, uchar value,
                             bool alive) : QEvent(_HIDInputEventType)
{
    m_device = device;
    m_input = input;
    m_channel = channel;
    m_value = value;
    m_alive = alive;
}

HIDInputEvent::~HIDInputEvent()
{
}

/*****************************************************************************
 * HID Initialization
 *****************************************************************************/

void HID::init()
{
    m_poller = new HIDPoller(this);
    rescanDevices();
}

HID::~HID()
{
    while (m_devices.isEmpty() == false)
        delete m_devices.takeFirst();

    m_poller->stop();
    delete m_poller;
}

QString HID::name()
{
    return QString("HID");
}

int HID::capabilities() const
{
    return QLCIOPlugin::Input | QLCIOPlugin::Output;
}

/*****************************************************************************
 * Inputs
 *****************************************************************************/

void HID::openInput(quint32 input)
{
    HIDDevice* dev = device(input);
    if (dev != NULL)
        addPollDevice(dev);
    else
        qDebug() << name() << "has no input number:" << input;
}

void HID::closeInput(quint32 input)
{
    HIDDevice* dev = device(input);
    if (dev != NULL)
        removePollDevice(dev);
    else
        qDebug() << name() << "has no input number:" << input;
}

QStringList HID::inputs()
{
    QStringList list;

    QListIterator <HIDDevice*> it(m_devices);
    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->hasInput())
            list << dev->name();
    }

    return list;
}

void HID::customEvent(QEvent* event)
{
    if (event->type() == _HIDInputEventType)
    {
        HIDInputEvent* e = static_cast<HIDInputEvent*> (event);
        if (e != NULL && e->m_alive == true)
            emit valueChanged(e->m_input, e->m_channel, e->m_value);
        else
            removeDevice(e->m_device);

        event->accept();
    }
}

QString HID::pluginInfo()
{
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides input support for HID-based joysticks and the FX5 USB DMX adapter.");
    str += QString("</P>");

    return str;
}

QString HID::inputInfo(quint32 input)
{
    QString str;

    if (input != QLCIOPlugin::invalidLine())
    {
        /* A specific input line selected. Display its information if
           available. */
        HIDDevice* dev = device(input);
        if (dev != NULL)
            str += dev->infoText();
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

/*********************************************************************
 * Outputs
 *********************************************************************/
void HID::openOutput(quint32 output)
{
    Q_UNUSED(output)
}

void HID::closeOutput(quint32 output)
{
    Q_UNUSED(output)
}

QStringList HID::outputs()
{
    QStringList list;

    QListIterator <HIDDevice*> it(m_devices);
    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->hasOutput())
            list << dev->name();
    }

    return list;
}

QString HID::outputInfo(quint32 output)
{
    QString str;

    if (output != QLCIOPlugin::invalidLine())
    {
        /* A specific output line selected. Display its information if
           available. */
        HIDDevice* dev = device(output);
        if (dev != NULL)
            str += dev->infoText();
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

void HID::writeUniverse(quint32 universe, quint32 output, const QByteArray &data)
{
    Q_UNUSED(universe)
    Q_UNUSED(output)
    Q_UNUSED(data)
}

/*****************************************************************************
 * Configuration
 *****************************************************************************/

void HID::configure()
{
    ConfigureHID conf(NULL, this);
    conf.exec();
}

bool HID::canConfigure()
{
    return true;
}

/*****************************************************************************
 * Devices
 *****************************************************************************/

void HID::rescanDevices()
{
    struct hid_device_info *devs, *cur_dev;
    quint32 line = 0;

    devs = hid_enumerate(0x0, 0x0);

    cur_dev = devs;

    while (cur_dev)
    {
        qDebug() << "[HID Device found] path:" << QString(cur_dev->path) << ", name:" << cur_dev->product_string;
        if((cur_dev->vendor_id == FX5_DMX_INTERFACE_VENDOR_ID && cur_dev->product_id == FX5_DMX_INTERFACE_PRODUCT_ID) ||
           (cur_dev->vendor_id == FX5_DMX_INTERFACE_VENDOR_ID_2 && cur_dev->product_id == FX5_DMX_INTERFACE_PRODUCT_ID_2))
        {
            HIDDevice* dev = device(QString(cur_dev->path));
            if (dev == NULL)
            {
                dev = new HIDFX5Device(this, line++, QString(cur_dev->path));
                addDevice(dev);
            }
        }
        else if (QString(cur_dev->path).contains("js"))
        {
            HIDDevice* dev = device(QString(cur_dev->path));
            if (dev == NULL)
            {
                dev = new HIDJsDevice(this, line++, QString(cur_dev->path));
                addDevice(dev);
            }
        }
        cur_dev = cur_dev->next;
    }

    hid_free_enumeration(devs);
}

HIDDevice* HID::device(const QString& path)
{
    QListIterator <HIDDevice*> it(m_devices);

    while (it.hasNext() == true)
    {
        HIDDevice* dev = it.next();
        if (dev->path() == path)
            return dev;
    }

    return NULL;
}

HIDDevice* HID::device(quint32 index)
{
    if (index < quint32(m_devices.count()))
        return m_devices.at(index);
    else
        return NULL;
}

void HID::addDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);

    m_devices.append(device);
    emit deviceAdded(device);

    emit configurationChanged();
}

void HID::removeDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);

    removePollDevice(device);
    m_devices.removeAll(device);

    emit deviceRemoved(device);
    delete device;

    emit configurationChanged();
}

/*****************************************************************************
 * Device poller
 *****************************************************************************/

void HID::addPollDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);
    m_poller->addDevice(device);
}

void HID::removePollDevice(HIDDevice* device)
{
    Q_ASSERT(device != NULL);
    m_poller->removeDevice(device);
}

/*****************************************************************************
 * Plugin export
 ****************************************************************************/
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(hid, HID)
#endif
