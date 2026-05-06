#pragma once

#include <QColor>
#include <QString>

// Centralised look-and-feel. Both palettes ship a single accent (Breeze blue)
// but flip the surface colours so the "carbon" theme reads as woven dark
// fabric with off-white text/icons.
namespace Theme {

enum Name { Light, Carbon };

Name current();
QString currentKey();
void setCurrent(Name n);

// Reads the saved theme and applies stylesheet + QPalette to qApp.
void apply();

// Foreground colour the icon factory should use as pen/brush. Tracks the
// active theme so QPainter-drawn icons are readable on the current surface.
QColor foregroundColor();

QColor accentColor();

}
