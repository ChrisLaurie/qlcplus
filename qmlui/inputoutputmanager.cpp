/*
  Q Light Controller Plus
  inputoutputmanager.cpp

  Copyright (c) Massimo Callegari

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

#include <QDebug>

#include "inputoutputmanager.h"
#include "inputoutputobject.h"
#include "audiorenderer_qt.h"
#include "audiocapture_qt.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "doc.h"

InputOutputManager::InputOutputManager(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    Q_ASSERT(m_doc != NULL);
    m_ioMap = m_doc->inputOutputMap();
    m_selectedItem = NULL;

    qmlRegisterType<Universe>("com.qlcplus.classes", 1, 0, "Universe");
    qmlRegisterType<InputPatch>("com.qlcplus.classes", 1, 0, "InputPatch");
    qmlRegisterType<OutputPatch>("com.qlcplus.classes", 1, 0, "OutputPatch");
}

QQmlListProperty<Universe> InputOutputManager::universes()
{
    m_selectedItem = NULL;
    m_universeList.clear();
    m_universeList = m_ioMap->universes();
    return QQmlListProperty<Universe>(this, m_universeList);
}

QStringList InputOutputManager::universeNames() const
{
    return m_ioMap->universeNames();
}

void InputOutputManager::clearInputList()
{
    int count = m_inputSources.count();
    for (int i = 0; i < count; i++)
    {
        QObject *src = m_inputSources.takeLast();
        delete src;
    }
    m_inputSources.clear();
}

void InputOutputManager::clearOutputList()
{
    int count = m_outputSources.count();
    for (int i = 0; i < count; i++)
    {
        QObject *src = m_outputSources.takeLast();
        delete src;
    }
    m_outputSources.clear();
}

QVariant InputOutputManager::audioInputSources()
{
    QList<AudioDeviceInfo> devList = AudioRendererQt::getDevicesInfo();

    clearInputList();

    m_inputSources.append(new InputOutputObject(0, tr("Default device"), "__qlcplusdefault__"));

    foreach( AudioDeviceInfo info, devList)
    {
        if (info.capabilities & AUDIO_CAP_INPUT)
            m_inputSources.append(new InputOutputObject(0, info.deviceName, info.privateName));
    }

    return QVariant::fromValue(m_inputSources);
}

QVariant InputOutputManager::audioOutputSources()
{
    QList<AudioDeviceInfo> devList = AudioRendererQt::getDevicesInfo();

    clearOutputList();

    m_outputSources.append(new InputOutputObject(0, tr("Default device"), "__qlcplusdefault__"));

    foreach( AudioDeviceInfo info, devList)
    {
        if (info.capabilities & AUDIO_CAP_OUTPUT)
            m_outputSources.append(new InputOutputObject(0, info.deviceName, info.privateName));
    }

    return QVariant::fromValue(m_outputSources);
}

QVariant InputOutputManager::universeInputSources(int universe)
{
    clearInputList();
    QString currPlugin;
    int currLine;
    InputPatch *ip = m_ioMap->inputPatch(universe);
    if (ip != NULL)
    {
        currPlugin = ip->pluginName();
        currLine = ip->input();
    }

    foreach(QString pluginName,  m_ioMap->inputPluginNames())
    {
        QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
        int i = 0;
        foreach(QString pLine, m_ioMap->pluginInputs(pluginName))
        {
            if (pluginName == currPlugin && i == currLine)
            {
                i++;
                continue;
            }
            quint32 uni = m_ioMap->inputMapping(pluginName, i);
            if (uni == InputOutputMap::invalidUniverse() ||
               (uni == (quint32)universe || plugin->capabilities() & QLCIOPlugin::Infinite))
                m_inputSources.append(new InputOutputObject(universe, pLine, QString::number(i), pluginName));
            i++;
        }
    }

    return QVariant::fromValue(m_inputSources);
}

QVariant InputOutputManager::universeOutputSources(int universe)
{
    clearOutputList();
    QString currPlugin;
    int currLine;
    OutputPatch *op = m_ioMap->outputPatch(universe);
    if (op != NULL)
    {
        currPlugin = op->pluginName();
        currLine = op->output();
    }

    foreach(QString pluginName,  m_ioMap->outputPluginNames())
    {
        QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(pluginName);
        int i = 0;
        foreach(QString pLine, m_ioMap->pluginOutputs(pluginName))
        {
            if (pluginName == currPlugin && i == currLine)
            {
                i++;
                continue;
            }
            quint32 uni = m_ioMap->outputMapping(pluginName, i);
            if (uni == InputOutputMap::invalidUniverse() ||
               (uni == (quint32)universe || plugin->capabilities() & QLCIOPlugin::Infinite))
                m_outputSources.append(new InputOutputObject(universe, pLine, QString::number(i), pluginName));
            i++;
        }
    }

    return QVariant::fromValue(m_outputSources);
}

QVariant InputOutputManager::universeInputProfiles(int universe)
{
    int count = m_inputProfiles.count();
    for (int i = 0; i < count; i++)
    {
        QObject *src = m_inputProfiles.takeLast();
        delete src;
    }
    m_inputProfiles.clear();

    QStringList profileNames = m_doc->inputOutputMap()->profileNames();
    profileNames.sort();
    QString currentProfile = KInputNone;
    if (m_ioMap->inputPatch(universe) != NULL)
        currentProfile = m_ioMap->inputPatch(universe)->profileName();

    foreach(QString name, profileNames)
    {
        QLCInputProfile *ip = m_doc->inputOutputMap()->profile(name);
        if (ip != NULL)
        {
            QString type = ip->typeToString(ip->type());
            if (name != currentProfile)
                m_inputProfiles.append(new InputOutputObject(universe, name, name, type));
        }
    }

    return QVariant::fromValue(m_inputProfiles);
}

void InputOutputManager::addOutputPatch(int universe, QString plugin, QString line)
{
    m_doc->inputOutputMap()->setOutputPatch(universe, plugin, line.toUInt(), false);
}

void InputOutputManager::addInputPatch(int universe, QString plugin, QString line)
{
    m_doc->inputOutputMap()->setInputPatch(universe, plugin, line.toUInt());
}

void InputOutputManager::setInputProfile(int universe, QString profileName)
{
    m_doc->inputOutputMap()->setInputProfile(universe, profileName);
}

void InputOutputManager::setSelectedItem(QQuickItem *item, int index)
{
    if (m_selectedItem != NULL)
    {
        m_selectedItem->setProperty("isSelected", false);
    }
    m_selectedItem = item;
    m_selectedUniverseIndex = index;

    qDebug() << "[InputOutputManager] Selected universe:" << index;
}


