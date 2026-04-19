/*
 * This file is part of Crystal Dock.
 * Copyright (C) 2024 Viet Dang (dangvd@gmail.com)
 *
 * Crystal Dock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Crystal Dock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Crystal Dock.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CRYSTALDOCK_ICON_UTILS_H_
#define CRYSTALDOCK_ICON_UTILS_H_

#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
#include <QScreen>

namespace crystaldock {

inline QPixmap loadIcon(const QString& iconName, int iconLoadSize) {
  // Use DPR-aware pixmap loading so icon themes provide full physical
  // resolution (e.g. 256x256 at scale 2 for a 128 logical-pixel request).
  const qreal dpr = QGuiApplication::primaryScreen()
      ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;

  // Normally desktop file stores icon as icon name in the icon theme.
  QPixmap icon = QIcon::fromTheme(iconName).pixmap(
      QSize(iconLoadSize, iconLoadSize), dpr);
  if (!icon.isNull()) {
    return icon;
  }
  // Snap stores icon name as path to the icon file e.g.
  // /snap/firefox/5134/default256.png
  return QPixmap(iconName);
}

}  // namespace crystaldock

#endif  // CRYSTALDOCK_ICON_UTILS_H_
