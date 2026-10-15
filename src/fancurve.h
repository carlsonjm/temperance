/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>
#include <QStringList>

// The fan curve a Performance preset may save: eight temp:pwm points, the
// temperatures rising and the speeds never falling, within what the helper
// takes. From hotFanTemperature up a point must keep the fans turning at
// least minimumHotFanPwm (a fifth of full speed), so no curve leaves the
// fans off while the machine is hot. PerformancePage.qml holds a dragged
// point to the same floor.
constexpr int hotFanTemperature = 70;
constexpr int minimumHotFanPwm = 51;

inline bool fanCurveAllowed(const QString &curve)
{
    const QStringList points = curve.split(QLatin1Char(','));
    if (points.size() != 8) {
        return false;
    }
    int lastTemp = -1;
    int lastPwm = 0;
    for (const QString &point : points) {
        const QStringList parts = point.split(QLatin1Char(':'));
        bool tempOk = false;
        bool pwmOk = false;
        const int temp = parts.value(0).toInt(&tempOk);
        const int pwm = parts.value(1).toInt(&pwmOk);
        if (parts.size() != 2 || !tempOk || !pwmOk || temp <= lastTemp || temp > 120 || pwm < lastPwm || pwm > 255) {
            return false;
        }
        if (temp >= hotFanTemperature && pwm < minimumHotFanPwm) {
            return false;
        }
        lastTemp = temp;
        lastPwm = pwm;
    }
    return true;
}
