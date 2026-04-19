/*
 * This file is part of Crystal Dock.
 * Copyright (C) 2022 Viet Dang (dangvd@gmail.com)
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

#include "desktop_file.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTextStream>

namespace crystaldock {

DesktopFile::DesktopFile(const QString& file) {
  QFile inputFile(file);
  if (inputFile.open(QIODevice::ReadOnly)) {
    appId_ = QFileInfo(file).completeBaseName().toLower();
    QTextStream input(&inputFile);
    bool parsingEntry = false;
    bool parsingAction = false;
    DesktopAction currentAction;
    while (!input.atEnd()) {
      QString line = input.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#')) continue;

      if (line == "[Desktop Entry]") {
        if (parsingAction && !currentAction.name.isEmpty()) {
          actions_.push_back(currentAction);
        }
        parsingEntry = true;
        parsingAction = false;
        continue;
      }
      if (line.startsWith("[Desktop Action ")) {
        if (parsingAction && !currentAction.name.isEmpty()) {
          actions_.push_back(currentAction);
        }
        parsingEntry = false;
        parsingAction = true;
        currentAction = DesktopAction();
        continue;
      }
      if (line.startsWith("[")) {
        if (parsingAction && !currentAction.name.isEmpty()) {
          actions_.push_back(currentAction);
        }
        parsingEntry = false;
        parsingAction = false;
        continue;
      }

      int index = line.indexOf('=');
      if (index < 0 || index >= line.length() - 1) continue;
      const QString key = line.left(index);
      const QString value = line.mid(index + 1);

      if (parsingEntry) {
        values_[key] = value;
      } else if (parsingAction) {
        if (key == "Name") currentAction.name = value;
        else if (key == "Exec") currentAction.exec = value;
        else if (key == "Icon") currentAction.icon = value;
      }
    }
    // Flush last action if any.
    if (parsingAction && !currentAction.name.isEmpty()) {
      actions_.push_back(currentAction);
    }
  }
}

bool DesktopFile::write(const QString &file) {
  QFile outputFile(file);
  if (outputFile.open(QIODevice::WriteOnly)) {
    QTextStream output(&outputFile);
    output << "[Desktop Entry]\n";
    for (const auto& key : values_.keys()) {
      output << key << "=" << values_[key] << "\n";
    }

    return true;
  }
  return false;
}

bool DesktopFile::showOnDesktop(const QString& desktop) const {
  if (!onlyShowIn().empty() && !onlyShowIn().contains(desktop)) {
    return false;
  }

  if (!notShowIn().empty() && notShowIn().contains(desktop)) {
    return false;
  }

  return true;
}

}  // namespace crystaldock
