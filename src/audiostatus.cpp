/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "audiostatus.h"

#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/Source>

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
        connect(server, &PulseAudioQt::Server::defaultSourceChanged, this, &AudioStatus::setSource);
        setSource(server->defaultSource());
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

void AudioStatus::setSource(PulseAudioQt::Source *source)
{
    if (m_source == source) {
        return;
    }
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    m_source = source;
    if (m_source) {
        connect(m_source, &PulseAudioQt::Source::mutedChanged, this, &AudioStatus::microphoneChanged);
    }
    Q_EMIT microphoneChanged();
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

bool AudioStatus::microphoneMuted() const
{
    return m_source && m_source->isMuted();
}

bool AudioStatus::microphoneAvailable() const
{
    return !m_source.isNull();
}

void AudioStatus::setPercent(int percent)
{
    if (!available()) {
        return;
    }
    const int bounded = std::clamp(percent, 0, 100);
    m_sink->setVolume(qRound64(PulseAudioQt::normalVolume() * bounded / 100.0));
}
