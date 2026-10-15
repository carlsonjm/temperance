/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "audiostatus.h"

#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>

#include <algorithm>

AudioStatus::AudioStatus(QObject *parent)
    : QObject(parent)
{
}

void AudioStatus::start()
{
    auto *server = PulseAudioQt::Context::instance()->server();
    if (server) {
        connect(server, &PulseAudioQt::Server::defaultSinkChanged, this, &AudioStatus::setSink);
        setSink(server->defaultSink());
    }
}

void AudioStatus::setSink(PulseAudioQt::Sink *sink)
{
    if (m_sink == sink) {
        return;
    }
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }
    m_sink = sink;
    if (m_sink) {
        connect(m_sink, &PulseAudioQt::Sink::volumeChanged, this, &AudioStatus::volumeChanged);
        connect(m_sink, &PulseAudioQt::Sink::mutedChanged, this, &AudioStatus::volumeChanged);
    }
    Q_EMIT volumeChanged();
}

int AudioStatus::percent() const
{
    if (!m_sink) {
        return 0;
    }
    return qRound(100.0 * m_sink->volume() / PulseAudioQt::normalVolume());
}

bool AudioStatus::muted() const
{
    return m_sink && m_sink->isMuted();
}

bool AudioStatus::available() const
{
    return m_sink && m_sink->isVolumeWritable();
}

void AudioStatus::setPercent(int percent)
{
    if (!available()) {
        return;
    }
    const int bounded = std::clamp(percent, 0, 100);
    m_sink->setVolume(qRound64(PulseAudioQt::normalVolume() * bounded / 100.0));
}
