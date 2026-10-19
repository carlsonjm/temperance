/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QPointer>

namespace PulseAudioQt
{
class Sink;
class Source;
}

// The default output's volume, for Control Center's slider, and whether the
// default input is muted, for the row's reading of it.
class AudioStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int percent READ percent NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
    Q_PROPERTY(bool available READ available NOTIFY volumeChanged)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY microphoneChanged)
    Q_PROPERTY(bool microphoneAvailable READ microphoneAvailable NOTIFY microphoneChanged)

public:
    explicit AudioStatus(QObject *parent = nullptr);

    // Follows the sound server's default output from here on.
    void start();

    int percent() const;
    bool muted() const;
    bool available() const;
    bool microphoneMuted() const;
    bool microphoneAvailable() const;
    Q_INVOKABLE void setPercent(int percent);

Q_SIGNALS:
    void volumeChanged();
    void microphoneChanged();

private:
    void setSink(PulseAudioQt::Sink *sink);
    void setSource(PulseAudioQt::Source *source);

    QPointer<PulseAudioQt::Sink> m_sink;
    QPointer<PulseAudioQt::Source> m_source;
};
