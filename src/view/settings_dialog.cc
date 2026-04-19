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

#include "settings_dialog.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <utils/math_utils.h>

namespace crystaldock {

SettingsDialog::SettingsDialog(QWidget* parent, MultiDockModel* model, int dockId)
    : QDialog(parent), model_(model), dockId_(dockId) {
  setWindowTitle("Settings");
  setWindowFlag(Qt::Tool);
  setAttribute(Qt::WA_TranslucentBackground, false);
  setAttribute(Qt::WA_NoSystemBackground, false);
  setMinimumSize(520, 480);

  auto* mainLayout = new QVBoxLayout(this);

  tabs_ = new QTabWidget(this);
  tabs_->addTab(createAppearanceTab(), "Appearance");
  tabs_->addTab(createBehaviorTab(), "Behavior");
  tabs_->addTab(createTaskManagerTab(), "Task Manager");
  tabs_->addTab(createComponentsTab(), "Components");
  mainLayout->addWidget(tabs_);

  buttonBox_ = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
      this);
  mainLayout->addWidget(buttonBox_);

  connect(buttonBox_, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttonBox_, &QDialogButtonBox::clicked, this, &SettingsDialog::buttonClicked);

  loadData();
}

void SettingsDialog::reload() {
  loadData();
}

void SettingsDialog::accept() {
  QDialog::accept();
  saveData();
}

void SettingsDialog::buttonClicked(QAbstractButton* button) {
  if (buttonBox_->buttonRole(button) == QDialogButtonBox::ApplyRole) {
    saveData();
  }
}

QWidget* SettingsDialog::createAppearanceTab() {
  auto* page = new QWidget();
  auto* layout = new QVBoxLayout(page);

  // --- Panel Style ---
  auto* styleGroup = new QGroupBox("Panel Style", page);
  auto* styleLayout = new QFormLayout(styleGroup);

  panelStyle_ = new QComboBox(styleGroup);
  panelStyle_->addItem("Glass 3D");
  panelStyle_->addItem("Glass 2D");
  panelStyle_->addItem("Flat 2D");
  panelStyle_->addItem("Metal 2D");
  styleLayout->addRow("Style:", panelStyle_);

  floatingPanel_ = new QCheckBox("Floating Panel", styleGroup);
  styleLayout->addRow(floatingPanel_);

  connect(floatingPanel_, &QCheckBox::toggled, [this](bool checked) {
    floatingMargin_->setEnabled(checked);
  });

  layout->addWidget(styleGroup);

  // --- Icon Size ---
  auto* sizeGroup = new QGroupBox("Icon Size", page);
  auto* sizeLayout = new QFormLayout(sizeGroup);

  minSize_ = new QSpinBox(sizeGroup);
  minSize_->setRange(16, 64);
  sizeLayout->addRow("Min Icon Size:", minSize_);

  enableZooming_ = new QCheckBox("Enable Zooming", sizeGroup);
  sizeLayout->addRow(enableZooming_);

  maxSize_ = new QSpinBox(sizeGroup);
  maxSize_->setRange(32, 192);
  sizeLayout->addRow("Max Icon Size:", maxSize_);

  spacingFactor_ = new QDoubleSpinBox(sizeGroup);
  spacingFactor_->setRange(0.1, 0.9);
  spacingFactor_->setSingleStep(0.1);
  sizeLayout->addRow("Spacing Factor:", spacingFactor_);

  floatingMargin_ = new QSpinBox(sizeGroup);
  floatingMargin_->setRange(2, 32);
  sizeLayout->addRow("Floating Margin:", floatingMargin_);

  layout->addWidget(sizeGroup);

  // --- Colors ---
  auto* colorGroup = new QGroupBox("Colors", page);
  auto* colorLayout = new QFormLayout(colorGroup);

  backgroundColor_ = new ColorButton(colorGroup);
  backgroundColor_->setFixedSize(80, 30);
  auto* bgRow = new QHBoxLayout();
  bgRow->addWidget(backgroundColor_);
  backgroundTransparency_ = new QSpinBox(colorGroup);
  backgroundTransparency_->setRange(0, 100);
  backgroundTransparency_->setSuffix("% transparent");
  bgRow->addWidget(backgroundTransparency_);
  colorLayout->addRow("Background:", bgRow);

  borderColor_ = new ColorButton(colorGroup);
  borderColor_->setFixedSize(80, 30);
  colorLayout->addRow("Border:", borderColor_);

  activeIndicatorColor_ = new ColorButton(colorGroup);
  activeIndicatorColor_->setFixedSize(80, 30);
  colorLayout->addRow("Active Indicator:", activeIndicatorColor_);

  inactiveIndicatorColor_ = new ColorButton(colorGroup);
  inactiveIndicatorColor_->setFixedSize(80, 30);
  colorLayout->addRow("Inactive Indicator:", inactiveIndicatorColor_);

  layout->addWidget(colorGroup);

  // --- Indicator Glow ---
  auto* glowGroup = new QGroupBox("Indicator Glow", page);
  auto* glowLayout = new QFormLayout(glowGroup);

  indicatorGlow_ = new QCheckBox("Enable Glow", glowGroup);
  glowLayout->addRow(indicatorGlow_);

  indicatorGlowActiveOnly_ = new QCheckBox("Active indicator only", glowGroup);
  glowLayout->addRow(indicatorGlowActiveOnly_);

  glowIntensity_ = new QSlider(Qt::Horizontal, glowGroup);
  glowIntensity_->setRange(10, 100);
  glowLayout->addRow("Intensity:", glowIntensity_);

  animatedIndicator_ = new QCheckBox("Animated indicator", glowGroup);
  glowLayout->addRow(animatedIndicator_);

  connect(indicatorGlow_, &QCheckBox::toggled, [this](bool checked) {
    glowIntensity_->setEnabled(checked);
    indicatorGlowActiveOnly_->setEnabled(checked);
    animatedIndicator_->setEnabled(checked);
  });

  layout->addWidget(glowGroup);

  // --- Tooltip ---
  auto* tooltipGroup = new QGroupBox("Tooltip", page);
  auto* tooltipLayout = new QFormLayout(tooltipGroup);

  showTooltip_ = new QCheckBox("Show Tooltip", tooltipGroup);
  tooltipLayout->addRow(showTooltip_);

  tooltipFontSize_ = new QSpinBox(tooltipGroup);
  tooltipFontSize_->setRange(8, 28);
  tooltipLayout->addRow("Font Size:", tooltipFontSize_);

  layout->addWidget(tooltipGroup);
  layout->addStretch();

  connect(enableZooming_, &QCheckBox::toggled, [this](bool checked) {
    maxSize_->setEnabled(checked);
    if (checked) {
      maxSize_->setValue(prevMaxIconSize_);
    } else {
      prevMaxIconSize_ = maxSize_->value();
      maxSize_->setValue(minSize_->value());
    }
  });

  return page;
}

QWidget* SettingsDialog::createBehaviorTab() {
  auto* page = new QWidget();
  auto* layout = new QVBoxLayout(page);

  // --- Position ---
  auto* posGroup = new QGroupBox("Position", page);
  auto* posLayout = new QFormLayout(posGroup);

  position_ = new QComboBox(posGroup);
  position_->addItem("Top");
  position_->addItem("Bottom");
  position_->addItem("Left");
  position_->addItem("Right");
  posLayout->addRow("Screen Edge:", position_);

  layout->addWidget(posGroup);

  // --- Visibility ---
  auto* visGroup = new QGroupBox("Visibility", page);
  auto* visLayout = new QFormLayout(visGroup);

  visibility_ = new QComboBox(visGroup);
  visibility_->addItem("Always Visible");
  visibility_->addItem("Auto Hide");
  visibility_->addItem("Always On Top");
  visibility_->addItem("Intelligent Auto Hide");
  visLayout->addRow("Mode:", visibility_);

  layout->addWidget(visGroup);

  // --- Animation ---
  auto* animGroup = new QGroupBox("Animation", page);
  auto* animLayout = new QFormLayout(animGroup);

  zoomingAnimationSpeed_ = new QSpinBox(animGroup);
  zoomingAnimationSpeed_->setRange(1, 30);
  animLayout->addRow("Zoom Animation Speed:", zoomingAnimationSpeed_);

  layout->addWidget(animGroup);

  auto* bounceGroup = new QGroupBox("Launch Bounce", page);
  auto* bounceLayout = new QFormLayout(bounceGroup);

  bouncingLauncherIcon_ = new QCheckBox("Bounce icon on launch", bounceGroup);
  bounceLayout->addRow(bouncingLauncherIcon_);

  bounceCount_ = new QSpinBox(bounceGroup);
  bounceCount_->setRange(1, 10);
  bounceLayout->addRow("Number of bounces:", bounceCount_);

  connect(bouncingLauncherIcon_, &QCheckBox::toggled,
          bounceCount_, &QSpinBox::setEnabled);

  layout->addWidget(bounceGroup);
  layout->addStretch();

  return page;
}

QWidget* SettingsDialog::createTaskManagerTab() {
  auto* page = new QWidget();
  auto* layout = new QVBoxLayout(page);

  auto* group = new QGroupBox("Task Manager", page);
  auto* formLayout = new QFormLayout(group);

  currentDesktopTasksOnly_ = new QCheckBox("Show current desktop tasks only", group);
  formLayout->addRow(currentDesktopTasksOnly_);

  currentScreenTasksOnly_ = new QCheckBox("Show current screen tasks only", group);
  formLayout->addRow(currentScreenTasksOnly_);

  groupTasksByApplication_ = new QCheckBox("Group tasks by application", group);
  formLayout->addRow(groupTasksByApplication_);

  layout->addWidget(group);
  layout->addStretch();

  return page;
}

QWidget* SettingsDialog::createComponentsTab() {
  auto* page = new QWidget();
  auto* layout = new QVBoxLayout(page);

  auto* group = new QGroupBox("Optional Features", page);
  auto* formLayout = new QVBoxLayout(group);

  showApplicationMenu_ = new QCheckBox("Application Menu", group);
  formLayout->addWidget(showApplicationMenu_);

  showPager_ = new QCheckBox("Pager (Desktop Switcher)", group);
  formLayout->addWidget(showPager_);

  showTaskManager_ = new QCheckBox("Task Manager", group);
  formLayout->addWidget(showTaskManager_);

  showTrash_ = new QCheckBox("Trash", group);
  formLayout->addWidget(showTrash_);

  showWifiManager_ = new QCheckBox("Wi-Fi Manager", group);
  formLayout->addWidget(showWifiManager_);

  showVolumeControl_ = new QCheckBox("Volume Control", group);
  formLayout->addWidget(showVolumeControl_);

  showBatteryIndicator_ = new QCheckBox("Battery Indicator", group);
  formLayout->addWidget(showBatteryIndicator_);

  showKeyboardLayout_ = new QCheckBox("Keyboard Layout", group);
  formLayout->addWidget(showKeyboardLayout_);

  showVersionChecker_ = new QCheckBox("Version Checker", group);
  formLayout->addWidget(showVersionChecker_);

  showClock_ = new QCheckBox("Clock", group);
  formLayout->addWidget(showClock_);

  layout->addWidget(group);
  layout->addStretch();

  return page;
}

void SettingsDialog::loadData() {
  // Appearance - Panel Style
  PanelStyle style = model_->panelStyle();
  bool floating = model_->isFloating();
  // Map style enum to combo index (0=Glass3D, 1=Glass2D, 2=Flat2D, 3=Metal2D)
  int styleIndex = 0;
  switch (style) {
    case PanelStyle::Glass3D_Floating:
    case PanelStyle::Glass3D_NonFloating: styleIndex = 0; break;
    case PanelStyle::Glass2D_Floating:
    case PanelStyle::Glass2D_NonFloating: styleIndex = 1; break;
    case PanelStyle::Flat2D_Floating:
    case PanelStyle::Flat2D_NonFloating: styleIndex = 2; break;
    case PanelStyle::Metal2D_Floating:
    case PanelStyle::Metal2D_NonFloating: styleIndex = 3; break;
  }
  panelStyle_->setCurrentIndex(styleIndex);
  floatingPanel_->setChecked(floating);

  // Appearance - Icon Size
  const bool enableZooming = model_->minIconSize() < model_->maxIconSize();
  enableZooming_->setChecked(enableZooming);
  minSize_->setValue(model_->minIconSize());
  maxSize_->setValue(model_->maxIconSize());
  maxSize_->setEnabled(enableZooming);
  prevMaxIconSize_ = model_->maxIconSize();
  spacingFactor_->setValue(model_->spacingFactor());
  floatingMargin_->setValue(model_->floatingMargin());
  floatingMargin_->setEnabled(model_->isFloating());

  QColor bgColor = model_->isGlass()
      ? model_->backgroundColor()
      : model_->isFlat2D()
          ? model_->backgroundColor2D()
          : model_->backgroundColorMetal2D();
  backgroundColor_->setColor(QColor(bgColor.rgb()));
  backgroundTransparency_->setValue(alphaFToTransparencyPercent(bgColor.alphaF()));
  borderColor_->setColor(model_->isGlass() ? model_->borderColor() : model_->borderColorMetal2D());
  borderColor_->setVisible(!model_->isFlat2D());

  activeIndicatorColor_->setColor(
      model_->isGlass() ? model_->activeIndicatorColor()
                        : model_->isFlat2D() ? model_->activeIndicatorColor2D()
                                             : model_->activeIndicatorColorMetal2D());
  inactiveIndicatorColor_->setColor(
      model_->isGlass() ? model_->inactiveIndicatorColor()
                        : model_->isFlat2D() ? model_->inactiveIndicatorColor2D()
                                             : model_->inactiveIndicatorColorMetal2D());

  indicatorGlow_->setChecked(model_->indicatorGlow());
  indicatorGlowActiveOnly_->setChecked(model_->indicatorGlowActiveOnly());
  indicatorGlowActiveOnly_->setEnabled(model_->indicatorGlow());
  glowIntensity_->setValue(model_->indicatorGlowIntensity());
  glowIntensity_->setEnabled(model_->indicatorGlow());
  animatedIndicator_->setChecked(model_->animatedIndicator());
  animatedIndicator_->setEnabled(model_->indicatorGlow());

  showTooltip_->setChecked(model_->showTooltip());
  tooltipFontSize_->setValue(model_->tooltipFontSize());

  // Behavior
  position_->setCurrentIndex(static_cast<int>(model_->panelPosition(dockId_)));
  visibility_->setCurrentIndex(static_cast<int>(model_->visibility(dockId_)));
  zoomingAnimationSpeed_->setValue(model_->zoomingAnimationSpeed());
  bouncingLauncherIcon_->setChecked(model_->bouncingLauncherIcon());
  bounceCount_->setValue(model_->bounceCount());
  bounceCount_->setEnabled(model_->bouncingLauncherIcon());

  // Task Manager
  currentDesktopTasksOnly_->setChecked(model_->currentDesktopTasksOnly());
  currentScreenTasksOnly_->setChecked(model_->currentScreenTasksOnly());
  groupTasksByApplication_->setChecked(model_->groupTasksByApplication());

  // Components
  showApplicationMenu_->setChecked(model_->showApplicationMenu(dockId_));
  showPager_->setChecked(model_->showPager(dockId_));
  showTaskManager_->setChecked(model_->showTaskManager(dockId_));
  showTrash_->setChecked(model_->showTrash(dockId_));
  showWifiManager_->setChecked(model_->showWifiManager(dockId_));
  showVolumeControl_->setChecked(model_->showVolumeControl(dockId_));
  showBatteryIndicator_->setChecked(model_->showBatteryIndicator(dockId_));
  showKeyboardLayout_->setChecked(model_->showKeyboardLayout(dockId_));
  showVersionChecker_->setChecked(model_->showVersionChecker(dockId_));
  showClock_->setChecked(model_->showClock(dockId_));
}

void SettingsDialog::saveData() {
  // Appearance - Panel Style
  const int si = panelStyle_->currentIndex();
  const bool fl = floatingPanel_->isChecked();
  PanelStyle newStyle;
  switch (si) {
    case 0: newStyle = fl ? PanelStyle::Glass3D_Floating : PanelStyle::Glass3D_NonFloating; break;
    case 1: newStyle = fl ? PanelStyle::Glass2D_Floating : PanelStyle::Glass2D_NonFloating; break;
    case 2: newStyle = fl ? PanelStyle::Flat2D_Floating : PanelStyle::Flat2D_NonFloating; break;
    default: newStyle = fl ? PanelStyle::Metal2D_Floating : PanelStyle::Metal2D_NonFloating; break;
  }
  model_->setPanelStyle(newStyle);

  // Appearance - Icon Size
  model_->setMinIconSize(minSize_->value());
  model_->setMaxIconSize(maxSize_->value());
  model_->setSpacingFactor(spacingFactor_->value());
  model_->setFloatingMargin(floatingMargin_->value());

  QColor bgColor(backgroundColor_->color());
  bgColor.setAlphaF(transparencyPercentToAlphaF(backgroundTransparency_->value()));
  if (model_->isGlass()) {
    model_->setBackgroundColor(bgColor);
  } else if (model_->isFlat2D()) {
    model_->setBackgroundColor2D(bgColor);
  } else {
    model_->setBackgroundColorMetal2D(bgColor);
  }
  if (model_->isGlass()) {
    model_->setBorderColor(borderColor_->color());
    model_->setActiveIndicatorColor(activeIndicatorColor_->color());
    model_->setInactiveIndicatorColor(inactiveIndicatorColor_->color());
  } else if (model_->isFlat2D()) {
    model_->setActiveIndicatorColor2D(activeIndicatorColor_->color());
    model_->setInactiveIndicatorColor2D(inactiveIndicatorColor_->color());
  } else {
    model_->setBorderColorMetal2D(borderColor_->color());
    model_->setActiveIndicatorColorMetal2D(activeIndicatorColor_->color());
    model_->setInactiveIndicatorColorMetal2D(inactiveIndicatorColor_->color());
  }

  model_->setIndicatorGlow(indicatorGlow_->isChecked());
  model_->setIndicatorGlowActiveOnly(indicatorGlowActiveOnly_->isChecked());
  model_->setIndicatorGlowIntensity(glowIntensity_->value());
  model_->setAnimatedIndicator(animatedIndicator_->isChecked());

  model_->setShowTooltip(showTooltip_->isChecked());
  model_->setTooltipFontSize(tooltipFontSize_->value());

  // Behavior
  auto newPosition = static_cast<PanelPosition>(position_->currentIndex());
  if (newPosition != model_->panelPosition(dockId_)) {
    model_->setPanelPosition(dockId_, newPosition);
    emit positionChanged(newPosition);
  }
  auto newVisibility = static_cast<PanelVisibility>(visibility_->currentIndex());
  if (newVisibility != model_->visibility(dockId_)) {
    emit visibilityChanged(newVisibility);
  }
  model_->setZoomingAnimationSpeed(zoomingAnimationSpeed_->value());
  model_->setBouncingLauncherIcon(bouncingLauncherIcon_->isChecked());
  model_->setBounceCount(bounceCount_->value());

  // Task Manager
  model_->setCurrentDesktopTasksOnly(currentDesktopTasksOnly_->isChecked());
  model_->setCurrentScreenTasksOnly(currentScreenTasksOnly_->isChecked());
  model_->setGroupTasksByApplication(groupTasksByApplication_->isChecked());

  // Components (dock-specific)
  model_->setShowApplicationMenu(dockId_, showApplicationMenu_->isChecked());
  model_->setShowPager(dockId_, showPager_->isChecked());
  model_->setShowTaskManager(dockId_, showTaskManager_->isChecked());
  model_->setShowTrash(dockId_, showTrash_->isChecked());
  model_->setShowWifiManager(dockId_, showWifiManager_->isChecked());
  model_->setShowVolumeControl(dockId_, showVolumeControl_->isChecked());
  model_->setShowBatteryIndicator(dockId_, showBatteryIndicator_->isChecked());
  model_->setShowKeyboardLayout(dockId_, showKeyboardLayout_->isChecked());
  model_->setShowVersionChecker(dockId_, showVersionChecker_->isChecked());
  model_->setShowClock(dockId_, showClock_->isChecked());

  model_->saveAppearanceConfig();
  model_->saveDockConfig(dockId_);

  emit settingsApplied();
}

}  // namespace crystaldock
