  /*
 * This file is part of Crystal Dock.
 * Copyright (C) 2025 Viet Dang (dangvd@gmail.com)
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

#include "dock_item.h"

#include "dock_panel.h"

namespace crystaldock {

void DockItem::showPopupMenu(QMenu* menu) {
  parent_->setShowingPopup(true);
  const int centerX = left_ + getWidth() / 2;
  const QSize menuSize = menu->sizeHint();
  const QRect screen = parent_->screenGeometry();
  int mx = centerX - menuSize.width() / 2;
  // Place the menu bottom edge at the top of the dock panel.
  int my = top_ - getHeight() - menuSize.height() - 16;
  QPoint pos = parent_->mapToGlobal(QPoint(mx, my));
  if (pos.x() < screen.left()) pos.setX(screen.left());
  if (pos.x() + menuSize.width() > screen.right())
    pos.setX(screen.right() - menuSize.width());
  if (pos.y() < screen.top()) pos.setY(screen.top());
  menu->popup(pos);
}

}  // namespace crystaldock
