/*
 * This file is part of Crystal Dock.
 * Copyright (C) 2023 Viet Dang (dangvd@gmail.com)
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

#ifndef CRYSTALDOCK_SETTINGS_DIALOG_H_
#define CRYSTALDOCK_SETTINGS_DIALOG_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>

#include "color_button.h"
#include <model/multi_dock_model.h>

namespace crystaldock {

class DockPanel;

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  SettingsDialog(QWidget* parent, MultiDockModel* model, int dockId);
  ~SettingsDialog() = default;

  void reload();

 signals:
  void settingsApplied();
  void positionChanged(PanelPosition position);
  void visibilityChanged(PanelVisibility visibility);

 public slots:
  void accept() override;
  void buttonClicked(QAbstractButton* button);

 private:
  QWidget* createAppearanceTab();
  QWidget* createBehaviorTab();
  QWidget* createTaskManagerTab();
  QWidget* createComponentsTab();

  void loadData();
  void saveData();

  MultiDockModel* model_;
  int dockId_;

  QTabWidget* tabs_;
  QDialogButtonBox* buttonBox_;

  // Appearance tab
  QComboBox* panelStyle_;
  QCheckBox* floatingPanel_;
  QSpinBox* minSize_;
  QSpinBox* maxSize_;
  QCheckBox* enableZooming_;
  QDoubleSpinBox* spacingFactor_;
  QSpinBox* backgroundTransparency_;
  ColorButton* backgroundColor_;
  ColorButton* borderColor_;
  ColorButton* activeIndicatorColor_;
  ColorButton* inactiveIndicatorColor_;
  QCheckBox* indicatorGlow_;
  QCheckBox* indicatorGlowActiveOnly_;
  QSlider* glowIntensity_;
  QCheckBox* animatedIndicator_;
  QSpinBox* floatingMargin_;
  QCheckBox* showTooltip_;
  QSpinBox* tooltipFontSize_;

  // Behavior tab
  QComboBox* position_;
  QComboBox* visibility_;
  QSpinBox* autoHideTriggerZone_;
  QCheckBox* bouncingLauncherIcon_;
  QSpinBox* bounceCount_;
  QSpinBox* zoomingAnimationSpeed_;

  // Task Manager tab
  QCheckBox* currentDesktopTasksOnly_;
  QCheckBox* currentScreenTasksOnly_;
  QCheckBox* groupTasksByApplication_;

  // Components tab
  QCheckBox* showApplicationMenu_;
  QCheckBox* showPager_;
  QCheckBox* showTaskManager_;
  QCheckBox* showTrash_;
  QCheckBox* showWifiManager_;
  QCheckBox* showVolumeControl_;
  QCheckBox* showBatteryIndicator_;
  QCheckBox* showKeyboardLayout_;
  QCheckBox* showVersionChecker_;
  QCheckBox* showClock_;

  int prevMaxIconSize_ = 0;
};

}  // namespace crystaldock

#endif  // CRYSTALDOCK_SETTINGS_DIALOG_H_
