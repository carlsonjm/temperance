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
}

// The default output's volume, for Control Center's slider.
class AudioStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int percent READ percent NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
    Q_PROPERTY(bool available READ available NOTIFY volumeChanged)

public:
    explicit AudioStatus(QObject *parent = nullptr);

    // Follows the sound server's default output from here on.
    void start();

    int percent() const;
    bool muted() const;
    bool available() const;
    Q_INVOKABLE void setPercent(int percent);

Q_SIGNALS:
    void volumeChanged();

private:
    void setSink(PulseAudioQt::Sink *sink);

    QPointer<PulseAudioQt::Sink> m_sink;
};
