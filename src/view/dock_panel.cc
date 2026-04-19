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

#include "dock_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

#include <QBitmap>
#include <QColor>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QWheelEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QIcon>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QScreen>
#include <QSize>
#include <QStringList>
#include <QVariant>

#include <KWindowEffects>
#include <LayerShellQt/Window>

#include "add_panel_dialog.h"
#include "application_menu.h"
#include "battery_indicator.h"
#include "clock.h"
#include "desktop_selector.h"
#include "keyboard_layout.h"
#include "multi_dock_view.h"
#include "program.h"
#include "separator.h"
#include "trash.h"
#include "version_checker.h"
#include "volume_control.h"
#include "wifi_manager.h"
#include <display/window_system.h>
#include <utils/draw_utils.h>
#include <utils/icon_utils.h>

namespace ranges = std::ranges;

namespace crystaldock {

/*static*/ constexpr char DockPanel::kVersion[] = "2.17 alpha";

DockPanel::DockPanel(MultiDockView* parent, MultiDockModel* model, int dockId)
    : QWidget(),
      parent_(parent),
      model_(model),
      dockId_(dockId),
      aboutDialog_(QMessageBox::Information, "About Crystal Dock",
                   QString("<h3>Crystal Dock ") + kVersion + "</h3>"
                   + "<p>Copyright (C) 2025 Viet Dang (dangvd@gmail.com)"
                   + "<p><a href=\"https://github.com/dangvd/crystal-dock\">https://github.com/dangvd/crystal-dock</a>"
                   + "<p>License: GPLv3"
                   + "<hr>"
                   + "<h3>Crystal Dock (Nakildias Fork)</h3>"
                   + "<p><a href=\"https://github.com/Nakildias/crystal-dock\">https://github.com/Nakildias/crystal-dock</a>"
                   + "<p>License: GPLv3",
                   QMessageBox::Ok, this, Qt::Tool),
      addPanelDialog_(this, model, dockId),
      appearanceSettingsDialog_(this, model),
      editKeyboardLayoutsDialog_(this, model),
      editLaunchersDialog_(this, model, dockId),
      applicationMenuSettingsDialog_(this, model),
      wallpaperSettingsDialog_(this, model),
      taskManagerSettingsDialog_(this, model),
      settingsDialog_(this, model, dockId),
      isMinimized_(true),
      isHidden_(false),
      isEntering_(false),
      isLeaving_(false),
      isAnimationActive_(false),
      isEnterAnimating_(false),
      isResizeAnimating_(false),
      enterProgress_(1.0),
      isSliding_(false),
      isSlidOut_(false),
      slideDirection_(false),
      slideOffset_(0),
      slideTarget_(0),
      slideStart_(0),
      slideStepCount_(0),
      slideTotalSteps_(12),
      isShowingPopup_(false),
      animationTimer_(std::make_unique<QTimer>(this)),
      slideTimer_(std::make_unique<QTimer>(this)) {
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlag(Qt::FramelessWindowHint);
  setMouseTracking(true);
  setAcceptDrops(true);

  createMenu();
  loadDockConfig();
  loadAppearanceConfig();
  initUi();

  connect(&settingsDialog_, &SettingsDialog::settingsApplied, this, &DockPanel::reload);
  connect(&settingsDialog_, &SettingsDialog::positionChanged, this, &DockPanel::updatePosition);
  connect(&settingsDialog_, &SettingsDialog::visibilityChanged, this, &DockPanel::updateVisibility);
  connect(animationTimer_.get(), SIGNAL(timeout()), this,
      SLOT(updateAnimation()));
  connect(slideTimer_.get(), &QTimer::timeout, this, &DockPanel::updateSlideAnimation);
  connect(WindowSystem::self(), SIGNAL(numberOfDesktopsChanged(int)),
      this, SLOT(updatePager()));
  connect(WindowSystem::self(), SIGNAL(currentDesktopChanged(std::string_view)),
          this, SLOT(onCurrentDesktopChanged()));
  connect(WindowSystem::self(), SIGNAL(windowStateChanged(const WindowInfo*)),
          this, SLOT(onWindowStateChanged(const WindowInfo*)));
  connect(WindowSystem::self(), SIGNAL(windowTitleChanged(const WindowInfo*)),
          this, SLOT(onWindowTitleChanged(const WindowInfo*)));
  connect(WindowSystem::self(), SIGNAL(activeWindowChanged(void*)),
          this, SLOT(onActiveWindowChanged()));
  connect(WindowSystem::self(), SIGNAL(windowAdded(const WindowInfo*)),
          this, SLOT(onWindowAdded(const WindowInfo*)));
  connect(WindowSystem::self(), SIGNAL(windowRemoved(void*)),
          this, SLOT(onWindowRemoved(void*)));
  connect(WindowSystem::self(), SIGNAL(windowLeftCurrentDesktop(void*)),
          this, SLOT(onWindowLeftCurrentDesktop(void*)));
  connect(WindowSystem::self(), SIGNAL(windowLeftCurrentActivity(void*)),
          this, SLOT(onWindowLeftCurrentActivity(void*)));
  connect(WindowSystem::self(), SIGNAL(windowEnteredOutput(const WindowInfo*, const wl_output*)),
          this, SLOT(onWindowEnteredOutput(const WindowInfo*, const wl_output*)));
  connect(WindowSystem::self(), SIGNAL(windowLeftOutput(const WindowInfo*, const wl_output*)),
          this, SLOT(onWindowLeftOutput(const WindowInfo*, const wl_output*)));
  connect(WindowSystem::self(), SIGNAL(windowGeometryChanged(const WindowInfo*)),
          this, SLOT(onWindowGeometryChanged(const WindowInfo*)));
  connect(WindowSystem::self(), SIGNAL(currentActivityChanged(std::string_view)),
          this, SLOT(onCurrentActivityChanged()));
  connect(model_, SIGNAL(appearanceOutdated()), this, SLOT(update()));
  connect(model_, SIGNAL(appearanceChanged()), this, SLOT(reload()));
  connect(model_, SIGNAL(dockLaunchersChanged(int)),
          this, SLOT(onDockLaunchersChanged(int)));
}

void DockPanel::reload() {
  // Stop any running animation to prevent painting stale/invalid items.
  animationTimer_->stop();
  isAnimationActive_ = false;
  isEnterAnimating_ = false;
  isResizeAnimating_ = false;
  isLeaving_ = false;
  slideTimer_->stop();
  isSliding_ = false;

  loadDockConfig();
  loadAppearanceConfig();
  items_.clear();
  initUi();
  setMask();
}

void DockPanel::refresh() {
  for (int i = 0; i < itemCount(); ++i) {
    if (items_[i]->shouldBeRemoved() && !items_[i]->fadingOut_) {
      items_[i]->fadingOut_ = true;
      resizeTaskManager();
      return;
    }
  }
}

void DockPanel::delayedRefresh() {
  QTimer::singleShot(100 /* msecs */, this, SLOT(refresh()));
}

void DockPanel::onCurrentDesktopChanged() {
  reloadTasks();
  intellihideHideUnhide();
}

void DockPanel::onCurrentActivityChanged() {
  reloadTasks();
  intellihideHideUnhide();
}

void DockPanel::setStrut() {
  switch(visibility_) {
    case PanelVisibility::AlwaysVisible:
      setStrut(isHorizontal() ? minHeight_ : minWidth_);
      break;
    case PanelVisibility::AutoHide:
    case PanelVisibility::IntelligentAutoHide:
      setStrut(WindowSystem::hasAutoHideManager() ? 0 : 1);
      break;
    default:
      setStrut(0);
      break;
  }
}

void DockPanel::togglePager() {
  showPager_ = !showPager_;
  reload();
  saveDockConfig();
}

void DockPanel::setScreen(int screen) {
  screen_ = screen;
  for (int i = 0; i < static_cast<int>(screenActions_.size()); ++i) {
    screenActions_[i]->setChecked(i == screen);
  }
  screenGeometry_ = WindowSystem::screens()[screen]->geometry();
  screenOutput_ = WindowSystem::getWlOutputForScreen(screen);
  WindowSystem::setScreen(this, screen);
}

void DockPanel::changeScreen(int screen) {
  if (screen_ == screen) {
    return;
  }
  model_->cloneDock(dockId_, position_, screen);
  deleteLater();
  model_->removeDock(dockId_);
}

void DockPanel::updateAnimation() {
  if (isEnterAnimating_) {
    // Ramp up the enter zoom progress and re-layout with current mouse pos.
    ++currentAnimationStep_;
    constexpr int kEnterSteps = 8;  // Fewer steps for a snappier enter.
    double t = static_cast<double>(currentAnimationStep_) / kEnterSteps;
    if (t > 1.0) t = 1.0;
    // easeOutCubic — fast start, gentle settle.
    double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    enterProgress_ = eased;
    if (currentAnimationStep_ >= kEnterSteps) {
      animationTimer_->stop();
      isAnimationActive_ = false;
      isEnterAnimating_ = false;
      enterProgress_ = 1.0;
    }
    updateLayout(mouseX_, mouseY_);
    return;
  }

  // Resize animation (task added/removed while minimized).
  if (isResizeAnimating_) {
    ++currentAnimationStep_;
    constexpr int kPhaseSteps = 10;
    constexpr int kTotalSteps = kPhaseSteps * 2;
    const bool isExpanding = endBackgroundWidth_ > startBackgroundWidth_
                          || endBackgroundHeight_ > startBackgroundHeight_;

    if (isExpanding) {
      // Phase 1: pill expands + items slide, new icon stays hidden.
      // Phase 2: new icon fades in.
      if (currentAnimationStep_ <= kPhaseSteps) {
        double t = static_cast<double>(currentAnimationStep_) / kPhaseSteps;
        double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
        backgroundWidth_ = startBackgroundWidth_
            + static_cast<int>((endBackgroundWidth_ - startBackgroundWidth_) * eased);
        backgroundHeight_ = startBackgroundHeight_
            + static_cast<int>((endBackgroundHeight_ - startBackgroundHeight_) * eased);
        for (auto& item : items_) {
          if (item->opacity_ < 1.0f && !item->fadingOut_) item->opacity_ = 0.0f;
          item->left_ = item->startLeft_ +
              static_cast<int>((item->endLeft_ - item->startLeft_) * eased);
          item->top_ = item->startTop_ +
              static_cast<int>((item->endTop_ - item->startTop_) * eased);
        }
      } else {
        backgroundWidth_ = endBackgroundWidth_;
        backgroundHeight_ = endBackgroundHeight_;
        double t = static_cast<double>(currentAnimationStep_ - kPhaseSteps) / kPhaseSteps;
        if (t > 1.0) t = 1.0;
        double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
        for (auto& item : items_) {
          item->left_ = item->endLeft_;
          item->top_ = item->endTop_;
          if (!item->fadingOut_ && item->opacity_ < 1.0f) {
            item->opacity_ = static_cast<float>(eased);
          }
        }
      }
    } else {
      // Phase 1: icon fades out, pill stays.
      // Phase 2: pill shrinks + items slide.
      if (currentAnimationStep_ <= kPhaseSteps) {
        backgroundWidth_ = startBackgroundWidth_;
        backgroundHeight_ = startBackgroundHeight_;
        double t = static_cast<double>(currentAnimationStep_) / kPhaseSteps;
        double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
        for (auto& item : items_) {
          if (item->fadingOut_) {
            item->opacity_ = std::max(0.0f, 1.0f - static_cast<float>(eased));
          }
        }
      } else {
        for (auto& item : items_) {
          if (item->fadingOut_) item->opacity_ = 0.0f;
        }
        double t = static_cast<double>(currentAnimationStep_ - kPhaseSteps) / kPhaseSteps;
        if (t > 1.0) t = 1.0;
        double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
        backgroundWidth_ = startBackgroundWidth_
            + static_cast<int>((endBackgroundWidth_ - startBackgroundWidth_) * eased);
        backgroundHeight_ = startBackgroundHeight_
            + static_cast<int>((endBackgroundHeight_ - startBackgroundHeight_) * eased);
        for (auto& item : items_) {
          if (!item->fadingOut_) {
            item->left_ = item->startLeft_ +
                static_cast<int>((item->endLeft_ - item->startLeft_) * eased);
            item->top_ = item->startTop_ +
                static_cast<int>((item->endTop_ - item->startTop_) * eased);
          }
        }
      }
    }

    if (currentAnimationStep_ >= kTotalSteps) {
      animationTimer_->stop();
      isAnimationActive_ = false;
      isResizeAnimating_ = false;
      // Remove items that finished fading out.
      items_.erase(
          std::remove_if(items_.begin(), items_.end(),
              [](const auto& item) { return item->fadingOut_; }),
          items_.end());
      // Ensure all remaining items are fully opaque.
      for (auto& item : items_) { item->opacity_ = 1.0f; }
      // Recalculate layout vars to restore correct maxWidth_/maxHeight_
      // (they may have been temporarily enlarged for the animation).
      initLayoutVars();
      updateLayout();
    }
    repaint();
    return;
  }

  // Leaving animation: step per-item positions and background.
  for (const auto& item : items_) {
    item->nextAnimationStep();
  }
  ++currentAnimationStep_;
  double t = static_cast<double>(currentAnimationStep_) / numAnimationSteps_;
  double eased = (t < 0.5) ? 2 * t * t : -1 + (4 - 2 * t) * t;
  backgroundWidth_ = startBackgroundWidth_
      + (endBackgroundWidth_ - startBackgroundWidth_) * eased;
  backgroundHeight_ = startBackgroundHeight_
      + (endBackgroundHeight_ - startBackgroundHeight_) * eased;
  if (currentAnimationStep_ == numAnimationSteps_) {
    animationTimer_->stop();
    isAnimationActive_ = false;
    if (isLeaving_) {
      isLeaving_ = false;
      updateLayout();
      if (isHidden_ && !hasFocus()) { startSlide(true); }
      // If the mouse came back while we were leaving, re-enter now.
      if (underMouse() && isMinimized_) {
        isEntering_ = true;
      }
    }
  }
  repaint();
}

void DockPanel::startSlide(bool out) {
  slideDirection_ = out;
  slideTarget_ = out ? minHeight_ : 0;
  slideStepCount_ = 0;
  slideTotalSteps_ = 12;
  slideStart_ = slideOffset_;
  isSliding_ = true;
  slideTimer_->start(16);  // ~60fps
}

void DockPanel::updateSlideAnimation() {
  ++slideStepCount_;
  double t = static_cast<double>(slideStepCount_) / slideTotalSteps_;
  if (t > 1.0) t = 1.0;
  // easeOutCubic for snappy slide.
  double eased = 1.0 - std::pow(1.0 - t, 3);

  slideOffset_ = slideStart_ + static_cast<int>((slideTarget_ - slideStart_) * eased);

  if (slideStepCount_ >= slideTotalSteps_) {
    slideOffset_ = slideTarget_;
    slideTimer_->stop();
    isSliding_ = false;
    isSlidOut_ = slideDirection_;
  }
  repaint();
}

void DockPanel::showOnlineDocumentation() {
  Program::launch(
      "xdg-open https://github.com/dangvd/crystal-dock/wiki/Documentation");
}

void DockPanel::about() {
  aboutDialog_.exec();
}

void DockPanel::showAppearanceSettingsDialog() {
  appearanceSettingsDialog_.reload();
  appearanceSettingsDialog_.show();
}

void DockPanel::showSettingsDialog() {
  settingsDialog_.reload();
  settingsDialog_.show();
}

void DockPanel::showEditKeyboardLayoutsDialog() {
  editKeyboardLayoutsDialog_.refreshData();
  editKeyboardLayoutsDialog_.show();
}

void DockPanel::showEditLaunchersDialog() {
  editLaunchersDialog_.reload();
  editLaunchersDialog_.show();
}

void DockPanel::showApplicationMenuSettingsDialog() {
  applicationMenuSettingsDialog_.reload();
  applicationMenuSettingsDialog_.show();
}

void DockPanel::showWallpaperSettingsDialog(int desktop) {
  wallpaperSettingsDialog_.setFor(desktop, screen_);
  wallpaperSettingsDialog_.show();
}

void DockPanel::showTaskManagerSettingsDialog() {
  taskManagerSettingsDialog_.reload();
  taskManagerSettingsDialog_.show();
}

void DockPanel::addDock() {
  addPanelDialog_.setMode(AddPanelDialog::Mode::Add);
  addPanelDialog_.show();
}

void DockPanel::cloneDock() {
  addPanelDialog_.setMode(AddPanelDialog::Mode::Clone);
  addPanelDialog_.show();
}

void DockPanel::removeDock() {
  if (model_->dockCount() == 1) {
    QMessageBox message(QMessageBox::Information, "Remove Panel",
                        "The last panel cannot be removed.",
                        QMessageBox::Ok, this, Qt::Tool);
    message.exec();
    return;
  }

  QMessageBox question(QMessageBox::Question, "Remove Panel",
                       "Do you really want to remove this panel?",
                       QMessageBox::Yes | QMessageBox::No, this, Qt::Tool);
  if (question.exec() == QMessageBox::Yes) {
    deleteLater();
    model_->removeDock(dockId_);
  }
}

void DockPanel::onWindowAdded(const WindowInfo* info) {
  intellihideHideUnhide();
  if (autoHide() && !isHidden_) { setAutoHide(); }

  if (!showTaskManager()) {
    return;
  }

  if (isValidTask(info)) {
    if (addTask(info, /*fadeIn=*/true)) {
      resizeTaskManager();
    } else {
      update();
    }
  }
}

void DockPanel::onWindowRemoved(void* window) {
  intellihideHideUnhide(window);

  if (!showTaskManager()) {
    return;
  }

  removeTask(window);

  if (isEmpty()) {
    intellihideHideUnhide();
  }
}

void DockPanel::onWindowLeftCurrentDesktop(void* window) {
  if (showTaskManager() && model_->currentDesktopTasksOnly()) {
    removeTask(window);
  }
}

void DockPanel::onWindowLeftCurrentActivity(void* window) {
  if (showTaskManager()) {
    removeTask(window);
  }
}

void DockPanel::onWindowGeometryChanged(const WindowInfo* task) {
  intellihideHideUnhide();

  if (!showTaskManager()) {
    return;
  }

  if (!model_->currentScreenTasksOnly()) {
    return;
  }

  QRect windowGeometry(task->x, task->y, task->width, task->height);
  if (hasTask(task->window)) {
    if (!windowGeometry.intersects(screenGeometry_)) {
      removeTask(task->window);
    }
  } else {
    if (windowGeometry.intersects(screenGeometry_) && isValidTask(task)) {
      if (addTask(task)) {
        resizeTaskManager();
      }
    }
  }
}


void DockPanel::onWindowStateChanged(const WindowInfo *task) {
  intellihideHideUnhide();

  if (!showTaskManager()) {
    return;
  }

  for (auto& item : items_) {
    if (item->hasTask(task->window)) {
      item->setDemandsAttention(task->demandsAttention);
      return;
    }
  }
}

void DockPanel::onWindowTitleChanged(const WindowInfo *task) {
  if (model_->groupTasksByApplication()) {
    return;
  }

  for (auto& item : items_) {
    if (item->hasTask(task->window)) {
      item->setLabel(QString::fromStdString(task->title));
      update();
      return;
    }
  }
}

void DockPanel::onActiveWindowChanged() {
  update();
}

void DockPanel::onWindowEnteredOutput(const WindowInfo* task, const wl_output* output) {
  intellihideHideUnhide();

  if (!showTaskManager()) {
    return;
  }

  if (!model_->currentScreenTasksOnly()) {
    return;
  }

  if (screenOutput_ != output) {
    return;
  }

  if (addTask(task, /*fadeIn=*/true)) {
    resizeTaskManager();
  }
}

void DockPanel::onWindowLeftOutput(const WindowInfo* task, const wl_output* output) {
  intellihideHideUnhide();

  if (!showTaskManager()) {
    return;
  }

  if (!model_->currentScreenTasksOnly()) {
    return;
  }

  if (screenOutput_ != output) {
    return;
  }

  removeTask(task->window);
}

int DockPanel::taskIndicatorPos() {
  const auto margin = isGlass2D() || (is3D() && !isBottom())
      ? kIndicatorMarginGlass2D
      : isFlat2D()
          ? kIndicatorSizeFlat2D
          : kIndicatorSizeMetal2D / 2;
  if (isHorizontal()) {
    int y = 0;
    if (is3D() && isBottom()) {
      y = maxHeight_ - k3DPanelThickness - 2;
    } else {  // 2D
      if (isTop()) {
        y = itemSpacing_ / 3;
      } else {  // bottom
        y = maxHeight_ - itemSpacing_ / 3 - margin;
      }
    }

    if (isFloating()) {
      if (isTop()) {
        y += floatingMargin_;
      } else {
        y -= floatingMargin_;
      }
    }

    return y;
  } else {  // Vertical.
    int x = 0;
    if (isLeft()) {
      x = itemSpacing_ / 3;
    } else {  // right
      x = maxWidth_ - itemSpacing_ / 3 - margin;
    }

    if (isFloating()) {
      if (isLeft()) {
        x += floatingMargin_;
      } else {
        x -= floatingMargin_;
      }
    }

    return x;
  }
}

int DockPanel::itemCount(const QString& appId) {
  const auto first = ranges::find_if(
      items_,
      [&appId](auto& item) { return appId == item->getAppId(); });
  if (first == items_.end()) {
    return 0;
  }
  const auto last = ranges::find_if(
      first, items_.end(),
      [&appId](auto& item) { return appId != item->getAppId(); });
  return last - first;
}

void DockPanel::updatePinnedStatus(const QString& appId, bool pinned) {
  const auto first = ranges::find_if(
      items_,
      [&appId](auto& item) { return appId == item->getAppId(); });
  if (first == items_.end()) {
    return;
  }
  const auto last = ranges::find_if(
      first, items_.end(),
      [&appId](auto& item) { return appId != item->getAppId(); });
  ranges::for_each(first, last, [pinned](auto& item) { item->updatePinnedStatus(pinned); });

  // When unpinning, move the item to the end of the task section
  // (after all other pinned/task items, before system tray items).
  if (!pinned && first != items_.end()) {
    // Find the last Program item (task/launcher) in the list.
    int lastProgramIndex = -1;
    for (int i = 0; i < itemCount(); ++i) {
      if (!items_[i]->getAppId().isEmpty()) {
        lastProgramIndex = i;
      }
    }
    int currentIndex = std::distance(items_.begin(), first);
    if (lastProgramIndex > currentIndex) {
      auto item = std::move(items_[currentIndex]);
      items_.erase(items_.begin() + currentIndex);
      items_.insert(items_.begin() + lastProgramIndex, std::move(item));
      updateLayout();
    }
  }
}

void DockPanel::setShowingPopup(bool showingPopup) {
  isShowingPopup_ = showingPopup;
  if (!isShowingPopup_) {
    // We have to do these complicated workarounds because QCursor::pos() does not
    // exactly return the current mouse position but it depends on related mouse events.
    auto mousePosition = mapFromGlobal(QCursor::pos());
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int x2 = 0;
    int y2 = 0;
    int w2 = 0;
    int h2 = 0;
    int itemCount = static_cast<int>(items_.size());
    switch (position_) {
    case PanelPosition::Top:
      x = itemSpacing_;
      w = maxWidth_ - 2 * x;
      y = itemSpacing_ + (isFloating() ? floatingMargin_ : 0);
      h = minSize_;
      if (activeItem_ >= 0 && activeItem_ < itemCount) {
        x2 = items_[activeItem_]->left_;
        w2 = items_[activeItem_]->getMaxWidth();
        y2 = y;
        h2 = maxSize_;
      }
      break;
    case PanelPosition::Bottom:
      x = itemSpacing_ + (is3D() && isBottom() ? margin3D_ : 0);
      w = maxWidth_ - 2 * x;
      y = maxHeight_ - itemSpacing_ - (isFloating() ? floatingMargin_ : 0)
          - (is3D() && isBottom() ? k3DPanelThickness : 0) - minSize_;
      h = minSize_;
      if (activeItem_ >= 0 && activeItem_ < itemCount) {
        x2 = items_[activeItem_]->left_;
        w2 = items_[activeItem_]->getMaxWidth();
        y2 = y + minSize_ - maxSize_;
        h2 = maxSize_;
      }
      break;
    case PanelPosition::Left:
      y = itemSpacing_;
      h = maxHeight_ - 2 * y;
      x = itemSpacing_ + (isFloating() ? floatingMargin_ : 0);
      w = minSize_;
      if (activeItem_ >= 0 && activeItem_ < itemCount) {
        y2 = items_[activeItem_]->top_;
        h2 = items_[activeItem_]->getMaxHeight();
        x2 = y;
        w2 = maxSize_;
      }
      break;
    case PanelPosition::Right:
      y = itemSpacing_;
      h = maxHeight_ - 2 * y;
      x = maxWidth_ - itemSpacing_ - (isFloating() ? floatingMargin_ : 0) - minSize_;
      w = minSize_;
      if (activeItem_ >= 0 && activeItem_ < itemCount) {
        y2 = items_[activeItem_]->top_;
        h2 = items_[activeItem_]->getMaxHeight();
        x2 = x + minSize_ - maxSize_;
        w2 = maxSize_;
      }
      break;
    }

    QRect rect(x, y, w, h);
    QRect rect2(x2, y2, w2, h2);
    if (!rect.contains(mousePosition) && !rect2.contains(mousePosition)) {
      leaveEvent(nullptr);
    }
  }
}

void DockPanel::paintEvent(QPaintEvent* e) {
  if (itemCount() == 0 || !layoutDone_) return;

  // Set the blur region to match just the background pill, not the whole widget.
  updateBlurRegion();

  QPainter painter(this);

  // Clip to the dock's visible region to prevent stray items from rendering
  // outside the pill area (e.g. items at default position before layout).
  if (isMinimized_) {
    static constexpr int kClipPad = 40;
    if (isHorizontal()) {
      const int pillX = (maxWidth_ - backgroundWidth_) / 2;
      const int pillY = isTop() ? 0 : maxHeight_ - minHeight_;
      painter.setClipRect(pillX - kClipPad, pillY - kClipPad,
                          backgroundWidth_ + 2 * kClipPad,
                          minHeight_ + 2 * kClipPad);
    } else {
      const int pillX = isLeft() ? 0 : maxWidth_ - minWidth_;
      const int pillY = (maxHeight_ - backgroundHeight_) / 2;
      painter.setClipRect(pillX - kClipPad, pillY - kClipPad,
                          minWidth_ + 2 * kClipPad,
                          backgroundHeight_ + 2 * kClipPad);
    }
  }

  // Apply slide offset to push content off the visible edge.
  if (slideOffset_ > 0) {
    if (isBottom()) {
      painter.translate(0, slideOffset_);
    } else if (isTop()) {
      painter.translate(0, -slideOffset_);
    } else if (isLeft()) {
      painter.translate(-slideOffset_, 0);
    } else {
      painter.translate(slideOffset_, 0);
    }
  }

  if (is3D()) {
    drawGlass3D(painter);
  } else {
    draw2D(painter);
  }

  drawTooltip(painter);
}

void DockPanel::updateBlurRegion() {
  QWindow* win = windowHandle();
  if (!win) return;

  QRect pillRect;
  int r = 0;
  if (isHorizontal()) {
    int y = isTop()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxHeight_ - backgroundHeight_ - floatingMargin_
                       : maxHeight_ - backgroundHeight_;
    if (is3D() && isBottom()) { y -= k3DPanelThickness; }
    pillRect = QRect((maxWidth_ - backgroundWidth_) / 2, y,
                     backgroundWidth_, backgroundHeight_);
    r = isGlass2D() ? backgroundHeight_ / 16
      : isFlat2D()  ? backgroundHeight_ / 4
      : 0;
  } else {
    int x = isLeft()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxWidth_ - backgroundWidth_ - floatingMargin_
                       : maxWidth_ - backgroundWidth_;
    pillRect = QRect(x, (maxHeight_ - backgroundHeight_) / 2,
                     backgroundWidth_, backgroundHeight_);
    r = isGlass2D() ? backgroundWidth_ / 16
      : isFlat2D()  ? backgroundWidth_ / 4
      : 0;
  }

  // Apply slide offset.
  if (slideOffset_ > 0) {
    if (isBottom()) pillRect.translate(0, slideOffset_);
    else if (isTop()) pillRect.translate(0, -slideOffset_);
    else if (isLeft()) pillRect.translate(-slideOffset_, 0);
    else pillRect.translate(slideOffset_, 0);
  }

  QRegion blurRegion;
  if (r > 0 && pillRect.width() > 0 && pillRect.height() > 0) {
    // Create a rounded-corner region using a bitmap mask.
    QBitmap mask(pillRect.size());
    mask.fill(Qt::color0);
    QPainter p(&mask);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::color1);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawRoundedRect(0, 0, pillRect.width(), pillRect.height(), r, r);
    p.end();
    blurRegion = QRegion(mask);
    blurRegion.translate(pillRect.topLeft());
  } else {
    blurRegion = QRegion(pillRect);
  }

  KWindowEffects::enableBlurBehind(win, true, blurRegion);
}

void DockPanel::drawGlass3D(QPainter& painter) {
  if (isHorizontal()) {
    int y = isTop()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxHeight_ - backgroundHeight_ - floatingMargin_
                       : maxHeight_ - backgroundHeight_;
    if (isBottom()) {  // 3D styles only apply to bottom dock.
      y -= k3DPanelThickness;
      draw3dDockPanel(
          (maxWidth_ - backgroundWidth_) / 2, y, backgroundWidth_ - 1, backgroundHeight_ - 1,
           backgroundHeight_ / 16, borderColor_, backgroundColor_, &painter);
    } else {
      fillRoundedRect(
          (maxWidth_ - backgroundWidth_) / 2, y, backgroundWidth_ - 1, backgroundHeight_ - 1,
           backgroundHeight_ / 16, /*showBorder=*/true, borderColor_, backgroundColor_, &painter);
    }
  } else {  // Vertical
    const int x =  isLeft()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxWidth_ - backgroundWidth_ - floatingMargin_
                       : maxWidth_ - backgroundWidth_;
    fillRoundedRect(x, (maxHeight_ - backgroundHeight_) / 2, backgroundWidth_ - 1, backgroundHeight_ - 1,
                    backgroundWidth_ / 16, /*showBorder=*/true, borderColor_, backgroundColor_, &painter);
  }

  if (isBottom()) {
    const qreal dpr = devicePixelRatioF();
    QImage mainImage(width() * dpr, height() * dpr, QImage::Format_ARGB32);
    mainImage.setDevicePixelRatio(dpr);
    mainImage.fill(0);
    QPainter mainPainter(&mainImage);
    // Draw the items from the end to avoid zoomed items getting clipped by
    // non-zoomed items.
    for (int i = itemCount() - 1; i >= 0; --i) {
      if (items_[i]->opacity_ <= 0.0f) continue;
      if (items_[i]->opacity_ < 1.0f) { mainPainter.setOpacity(items_[i]->opacity_); }
      items_[i]->draw(&mainPainter);
      if (items_[i]->opacity_ < 1.0f) { mainPainter.setOpacity(1.0f); }
    }
    painter.drawImage(0, 0, mainImage);

    int y = height() - itemSpacing_ - k3DPanelThickness;
    if (isFloating()) { y -= floatingMargin_; }
    const int copyY = qRound((y - itemSpacing_ + 2) * dpr);
    const int copyH = qRound((itemSpacing_ - 2) * dpr);
    QImage toMirrorImage = mainImage.copy(0, copyY, mainImage.width(), copyH);
    toMirrorImage.setDevicePixelRatio(dpr);
    QImage mirrorImage = toMirrorImage.flipped(Qt::Vertical);
    painter.setOpacity(0.3);
    painter.drawImage(0, y, mirrorImage);
    painter.setOpacity(1.0);
  } else {
    // Draw the items from the end to avoid zoomed items getting clipped by
    // non-zoomed items.
    for (int i = itemCount() - 1; i >= 0; --i) {
      if (items_[i]->opacity_ <= 0.0f) continue;
      if (items_[i]->opacity_ < 1.0f) { painter.setOpacity(items_[i]->opacity_); }
      items_[i]->draw(&painter);
      if (items_[i]->opacity_ < 1.0f) { painter.setOpacity(1.0f); }
    }
  }
}

void DockPanel::draw2D(QPainter& painter) {
  const QColor bgColor = isGlass2D()
      ? model_->backgroundColor()
      : isFlat2D()
          ? model_->backgroundColor2D()
          : model_->backgroundColorMetal2D();
  const auto showBorder = isGlass2D() || isMetal2D();
  const QColor borderColor = isGlass2D() ? model_->borderColor() : model_->borderColorMetal2D();
  if (isHorizontal()) {
    const int y = isTop()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxHeight_ - backgroundHeight_ - floatingMargin_
                       : maxHeight_ - backgroundHeight_;
    const int r = isGlass2D()
        ? backgroundHeight_ / 16
        : isFlat2D()
            ? backgroundHeight_ / 4
            : 0;
    fillRoundedRect(
        (maxWidth_ - backgroundWidth_) / 2, y, backgroundWidth_ - 1, backgroundHeight_ - 1,
         r, showBorder, borderColor, bgColor, &painter);
  } else {  // Vertical
    const int x =  isLeft()
        ? isFloating() ? floatingMargin_ : 0
        : isFloating() ? maxWidth_ - backgroundWidth_ - floatingMargin_
                       : maxWidth_ - backgroundWidth_;
    const int r = isGlass2D()
        ? backgroundWidth_ / 16
        : isFlat2D()
            ? backgroundWidth_ / 4
            : 0;
    fillRoundedRect(
          x, (maxHeight_ - backgroundHeight_) / 2, backgroundWidth_ - 1, backgroundHeight_ - 1,
          r, showBorder, borderColor, bgColor, &painter);
  }

  // Draw the items from the end to avoid zoomed items getting clipped by
  // non-zoomed items.
  for (int i = itemCount() - 1; i >= 0; --i) {
    if (items_[i]->opacity_ <= 0.0f) continue;
    if (items_[i]->opacity_ < 1.0f) { painter.setOpacity(items_[i]->opacity_); }
    items_[i]->draw(&painter);
    if (items_[i]->opacity_ < 1.0f) { painter.setOpacity(1.0f); }
  }
}

void DockPanel::drawTooltip(QPainter& painter) {
  if (model_->showTooltip() && !isAnimationActive_ && activeItem_ >= 0 &&
      activeItem_ < static_cast<int>(items_.size()) &&
      !items_[activeItem_]->isBouncing()) {
    if (isHorizontal()) {
      const auto& item = items_[activeItem_];
      QFont font;
      font.setPointSize(model_->tooltipFontSize());
      font.setWeight(QFont::Medium);
      QFontMetrics metrics(font);

      const int hPad = metrics.height() * 0.8;
      const int vPad = metrics.height() * 0.3;
      const auto textRect = metrics.boundingRect(item->getLabel());
      const int pillW = textRect.width() + 2 * hPad;
      const int pillH = metrics.height() + 2 * vPad;
      const int pillR = pillH / 2;

      const int arrowH = 6;
      const int arrowW = 12;

      int centerX = item->left_ + item->getWidth() / 2;

      int px = centerX - pillW / 2;
      px = std::clamp(px, 0, maxWidth_ - pillW);

      int py;
      if (isTop()) {
        py = maxHeight_ - pillH - arrowH - 2;
      } else {
        // Place tooltip above the hovered icon — follows the zoom level.
        py = item->top_ - arrowH - pillH - 8;
        if (py < 0) py = 0;
      }

      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setPen(Qt::NoPen);
      QColor tooltipBg(30, 30, 30, 200);
      painter.setBrush(tooltipBg);

      // Rounded pill.
      painter.drawRoundedRect(px, py, pillW, pillH, pillR, pillR);

      // Small arrow pointing down toward the icon center.
      int arrowCx = centerX;
      int arrowTop = py + pillH;
      QPolygon arrow;
      arrow << QPoint(arrowCx - arrowW / 2, arrowTop)
            << QPoint(arrowCx + arrowW / 2, arrowTop)
            << QPoint(arrowCx, arrowTop + arrowH);
      painter.drawPolygon(arrow);

      // White text centered in pill.
      painter.setFont(font);
      painter.setPen(Qt::white);
      painter.drawText(px, py, pillW, pillH, Qt::AlignCenter, item->getLabel());
      painter.setRenderHint(QPainter::Antialiasing, false);
    } else {  // Vertical
      // Do not draw tooltip for Vertical positions for now because the total
      // area of the dock would take too much desktop space.
    }
  }
}

void DockPanel::mouseMoveEvent(QMouseEvent* e) {
  const auto x = e->position().x();
  const auto y = e->position().y();

  // Drag-to-reorder: detect drag start.
  if (dragItemIndex_ >= 0 && !isDragging_ && (e->buttons() & Qt::LeftButton)) {
    const int dx = x - dragMouseX_;
    const int dy = y - dragMouseY_;
    if (dx * dx + dy * dy > 25) {  // 5px threshold
      isDragging_ = true;
    }
  }

  // Drag-to-reorder: handle ongoing drag.
  if (isDragging_) {
    dragMouseX_ = x;
    dragMouseY_ = y;

    // Find which pinned slot the cursor is over based on minCenter_ positions.
    int targetIndex = dragItemIndex_;
    for (int i = 0; i < itemCount(); ++i) {
      if (i == dragItemIndex_ || !items_[i]->isPinned()) continue;
      const int center = items_[i]->minCenter_;
      const int mouse = isHorizontal() ? x : y;
      if (dragItemIndex_ < i && mouse > center) {
        targetIndex = i;
      } else if (dragItemIndex_ > i && mouse < center) {
        targetIndex = std::min(targetIndex, i);
      }
    }

    // Swap items if the target changed.
    if (targetIndex != dragItemIndex_) {
      const int step = (targetIndex > dragItemIndex_) ? 1 : -1;
      while (dragItemIndex_ != targetIndex) {
        const int next = dragItemIndex_ + step;
        std::swap(items_[dragItemIndex_], items_[next]);
        dragItemIndex_ = next;
      }
      activeItem_ = dragItemIndex_;
    }

    // Recalculate minCenter_ after swaps so parabolic zoom uses correct positions.
    {
      int pos = isHorizontal()
          ? (isBottom() && is3D() ? itemSpacing_ + (maxWidth_ - minWidth_) / 2 + margin3D_
                                  : itemSpacing_ + (maxWidth_ - minWidth_) / 2)
          : itemSpacing_ + (maxHeight_ - minHeight_) / 2;
      for (int i = 0; i < itemCount(); ++i) {
        if (items_[i]->fadingOut_) continue;
        if (isHorizontal()) {
          items_[i]->minCenter_ = pos + items_[i]->getMinWidth() / 2;
          pos += items_[i]->getMinWidth() + itemSpacing_;
        } else {
          items_[i]->minCenter_ = pos + items_[i]->getMinHeight() / 2;
          pos += items_[i]->getMinHeight() + itemSpacing_;
        }
      }
    }

    // Save current visual positions (after swap, before layout recalc).
    std::vector<int> prevLeft(itemCount()), prevTop(itemCount());
    for (int i = 0; i < itemCount(); ++i) {
      prevLeft[i] = items_[i]->left_;
      prevTop[i] = items_[i]->top_;
    }

    // Compute target positions with parabolic zoom.
    updateLayout(x, y);

    // Smooth interpolation for non-dragged items; dragged item follows cursor.
    constexpr float kLerp = 0.3f;
    for (int i = 0; i < itemCount(); ++i) {
      if (i == dragItemIndex_) {
        if (isHorizontal()) {
          items_[i]->left_ = x - items_[i]->getWidth() / 2;
        } else {
          items_[i]->top_ = y - items_[i]->getHeight() / 2;
        }
      } else {
        items_[i]->left_ = prevLeft[i] +
            static_cast<int>((items_[i]->left_ - prevLeft[i]) * kLerp);
        items_[i]->top_ = prevTop[i] +
            static_cast<int>((items_[i]->top_ - prevTop[i]) * kLerp);
      }
    }

    // Keep updating to converge the lerp even if the mouse pauses.
    QTimer::singleShot(16, this, [this] {
      if (isDragging_) {
        // Re-run layout and lerp with current mouse position.
        std::vector<int> pL(itemCount()), pT(itemCount());
        for (int i = 0; i < itemCount(); ++i) {
          pL[i] = items_[i]->left_;
          pT[i] = items_[i]->top_;
        }
        updateLayout(dragMouseX_, dragMouseY_);
        constexpr float kLerp2 = 0.3f;
        for (int i = 0; i < itemCount(); ++i) {
          if (i == dragItemIndex_) {
            if (isHorizontal()) {
              items_[i]->left_ = dragMouseX_ - items_[i]->getWidth() / 2;
            } else {
              items_[i]->top_ = dragMouseY_ - items_[i]->getHeight() / 2;
            }
          } else {
            items_[i]->left_ = pL[i] +
                static_cast<int>((items_[i]->left_ - pL[i]) * kLerp2);
            items_[i]->top_ = pT[i] +
                static_cast<int>((items_[i]->top_ - pT[i]) * kLerp2);
          }
        }
        repaint();
      }
    });
    return;
  }

  if (isEntering_) {
    if (!checkMouseEnter(x, y)) {
      return;
    }
  }

  if (isAnimationActive_ && isLeaving_) {
    return;
  }

  updateLayout(x, y);
}

bool DockPanel::checkMouseEnter(int x, int y) {
  int x0, y0;
  if (position_ == PanelPosition::Bottom) {
    if (!WindowSystem::hasAutoHideManager() && (visibility_ == PanelVisibility::AutoHide
        || visibility_ == PanelVisibility::IntelligentAutoHide)) {
      y0 = maxHeight_ - 4;
    } else {
      y0 = maxHeight_ - minHeight_;
      if (isFloating()) { y0 += floatingMargin_; }
    }
    if (y < y0) {
      return false;
    }
  } else if (position_ == PanelPosition::Top) {
    if (!WindowSystem::hasAutoHideManager() && (visibility_ == PanelVisibility::AutoHide
        || visibility_ == PanelVisibility::IntelligentAutoHide)) {
      y0 = 4;
    } else {
      y0 = minHeight_;
      if (isFloating()) { y0 -= floatingMargin_; }
    }
    if (y > y0) {
      return false;
    }
  } else if (position_ == PanelPosition::Left) {
    if (!WindowSystem::hasAutoHideManager() && (visibility_ == PanelVisibility::AutoHide
        || visibility_ == PanelVisibility::IntelligentAutoHide)) {
      x0 = 4;
    } else {
      x0 = minWidth_;
      if (isFloating()) { x0 -= floatingMargin_; }
    }
    if (x > x0) {
      return false;
    }
  } else {  // Right
    if (!WindowSystem::hasAutoHideManager() && (visibility_ == PanelVisibility::AutoHide
        || visibility_ == PanelVisibility::IntelligentAutoHide)) {
      x0 = maxWidth_ - 4;
    } else {
      x0 = maxWidth_ - minWidth_;
      if (isFloating()) { x0 += floatingMargin_; }
    }
    if (x < x0) {
      return false;
    }
  }

  if (isHorizontal() &&
      (x < (maxWidth_ - minWidth_) / 2 || x > (maxWidth_ + minWidth_) / 2)) {
    return false;
  }
  if (!isHorizontal() &&
      (y < (maxHeight_ - minHeight_) / 2 || y > (maxHeight_ + minHeight_) / 2)) {
    return false;
  }

  return true;
}

bool DockPanel::intellihideShouldHide(void* excluding_window) {
  if (visibility_ != PanelVisibility::IntelligentAutoHide) {
    return false;
  }

  if (!isMinimized_) {
    return false;
  }

  if (isEmpty()) {
    return true;
  }

  // For tiling compositors, we only show the dock if there's no window.
  if (DesktopEnv::getDesktopEnv()->isTiling()) {
    for (const auto* task : WindowSystem::windows()) {
      if (shouldConsiderTaskForIntellihide(task)
          && (!excluding_window || task->window != excluding_window)) {
        return true;
      }
    }
    return false;
  }

  // For stacking compositors, we hide the dock if there's a maximized/fullscreen window.
  // If the compositor emits window geometry event, we also hide the dock if there's
  // a window that overlaps the dock.
  QRect dockGeometry = getMinimizedDockGeometry();
  for (const auto* task : WindowSystem::windows()) {
    if (shouldConsiderTaskForIntellihide(task)
        && (!excluding_window || task->window != excluding_window)) {
      if ((task->maximized || task->fullscreen)
          && task->outputs.contains(screenOutput_)) {
        return true;
      }

      QRect windowGeometry(task->x, task->y, task->width, task->height);
      if (windowGeometry.isValid() && !task->minimized && windowGeometry.intersects(dockGeometry)) {
        return true;
      }
    }
  }

  return false;
}

void DockPanel::intellihideHideUnhide(void* excluding_window) {
  if (visibility_ != PanelVisibility::IntelligentAutoHide) {
    return;
  }

  if (intellihideShouldHide(excluding_window)) {
    if (!isHidden_ && isMinimized_) {
      isHidden_ = true;
      startSlide(true);
    }
  } else {
    if (isHidden_) {
      isHidden_ = false;
      startSlide(false);
    }
  }
}

bool DockPanel::isEmpty() {
  for (const auto& item : items_) {
    if (item->getAppId() != kSeparatorId && item->getAppId() != kLauncherSeparatorId) {
      return false;
    }
  }
  return true;
}

void DockPanel::mousePressEvent(QMouseEvent* e) {
  if (isAnimationActive_) {
    return;
  }

  if (e->button() == Qt::LeftButton && activeItem_ >= 0 &&
      activeItem_ < static_cast<int>(items_.size())) {
    if (items_[activeItem_]->isPinned()) {
      // Potential drag — record start but don't fire click yet.
      dragItemIndex_ = activeItem_;
      dragMouseX_ = e->position().x();
      dragMouseY_ = e->position().y();
      return;
    }
  }

  if (activeItem_ >= 0 && activeItem_ < static_cast<int>(items_.size())) {
    items_[activeItem_]->maybeResetActiveWindow(e);
    items_[activeItem_]->mousePressEvent(e);
  } else if (e->button() == Qt::RightButton) {
    showDockMenu(e->pos().x(), e->pos().y());
  }
}

void DockPanel::mouseReleaseEvent(QMouseEvent* e) {
  if (e->button() == Qt::LeftButton) {
    if (isDragging_) {
      isDragging_ = false;
      // Save the new launcher order from all pinned items.
      QStringList newOrder;
      for (int i = 0; i < itemCount(); ++i) {
        if (items_[i]->isPinned()) {
          newOrder.append(items_[i]->getAppId());
        }
      }
      model_->setLaunchers(dockId_, newOrder);
      dragItemIndex_ = -1;
      updateLayout(e->position().x(), e->position().y());
      return;
    }
    // No drag happened — fire the deferred click.
    if (dragItemIndex_ >= 0 && dragItemIndex_ < static_cast<int>(items_.size())) {
      items_[dragItemIndex_]->maybeResetActiveWindow(e);
      items_[dragItemIndex_]->mousePressEvent(e);
    }
    dragItemIndex_ = -1;
  }
}

void DockPanel::wheelEvent(QWheelEvent* e) {
  if (isAnimationActive_) {
    return;
  }

  if (activeItem_ >= 0 && activeItem_ < static_cast<int>(items_.size())) {
    items_[activeItem_]->wheelEvent(e);
  }
}

void DockPanel::enterEvent (QEnterEvent* e) {
  if (isSlidOut_ || isSliding_) {
    // Slide back in.
    isHidden_ = false;
    isSlidOut_ = false;
    startSlide(false);
    // Reset any stale layer-shell margins.
    WindowSystem::setMargins(this, QMargins(0, 0, 0, 0));
  }

  // If we re-enter during a leave animation, reverse it into an enter.
  if (isAnimationActive_ && isLeaving_) {
    animationTimer_->stop();
    isAnimationActive_ = false;
    isLeaving_ = false;

    // Estimate how much zoom remains from the background width progress.
    // Leave animation goes from zoomed (start) to minimized (end).
    double leaveFraction = 1.0;
    if (startBackgroundWidth_ != endBackgroundWidth_) {
      leaveFraction = static_cast<double>(backgroundWidth_ - startBackgroundWidth_)
                    / (endBackgroundWidth_ - startBackgroundWidth_);
    } else if (startBackgroundHeight_ != endBackgroundHeight_) {
      leaveFraction = static_cast<double>(backgroundHeight_ - startBackgroundHeight_)
                    / (endBackgroundHeight_ - startBackgroundHeight_);
    }
    // leaveFraction 0 = still fully zoomed, 1 = fully minimized.
    enterProgress_ = std::clamp(1.0 - leaveFraction, 0.0, 1.0);

    // Start enter animation from the current partial progress.
    isEnterAnimating_ = true;
    isAnimationActive_ = true;
    // Inverse of easeOutCubic to find the step matching current enterProgress_.
    // easeOutCubic: eased = 1 - (1-t)^3  =>  t = 1 - cbrt(1 - eased)
    constexpr int kEnterSteps = 8;
    double t = 1.0 - std::cbrt(std::max(0.0, 1.0 - enterProgress_));
    currentAnimationStep_ = static_cast<int>(t * kEnterSteps);
    animationTimer_->start(32 - model_->zoomingAnimationSpeed());
    return;
  }

  if (isMinimized_) {
    isEntering_ = true;
  }
}

void DockPanel::leaveEvent(QEvent* e) {
  if (isMinimized_ || isShowingPopup_) {
    return;
  }

  // Cancel any in-progress enter animation.
  if (isEnterAnimating_) {
    animationTimer_->stop();
    isAnimationActive_ = false;
    isEnterAnimating_ = false;
  }
  enterProgress_ = 1.0;

  isLeaving_ = true;
  updateLayout();
  activeItem_ = -1;
}

void DockPanel::dragEnterEvent(QDragEnterEvent* e) {
  if (e->mimeData()->hasUrls()) {
    e->acceptProposedAction();
    
    for (const auto& item : items_) {
      Trash* trash = dynamic_cast<Trash*>(item.get());
      if (trash) {
        trash->setAcceptDrops(true);
      }
    }
  }
}

void DockPanel::dragMoveEvent(QDragMoveEvent* e) {
  if (e->mimeData()->hasUrls()) {
    e->acceptProposedAction();
  }
}

void DockPanel::dropEvent(QDropEvent* e) {
  for (const auto& item : items_) {
    Trash* trash = dynamic_cast<Trash*>(item.get());
    if (trash) {
      trash->setAcceptDrops(false);
      trash->dropEvent(e);
      return;
    }
  }
}

void DockPanel::initUi() {
  initApplicationMenu();
  initPager();
  initLaunchers();
  initTasks();
  initTrash();
  initWifiManager();
  initVolumeControl();
  initBatteryIndicator();
  initKeyboardLayout();
  initVersionChecker();
  initClock();
  initLayoutVars();
  updateLayout();
  layoutDone_ = true;
  setStrut();
  if (WindowSystem::hasAutoHideManager() && intellihideShouldHide()) {
    setAutoHide();
  }
}

void DockPanel::addPanelSettings(QMenu* menu) {
  for (const auto& action : menu_.actions()) {
    menu->addAction(action);
  }
}

void DockPanel::showDockMenu(int x, int y) {
  setShowingPopup(true);
  menu_.exec(mapToGlobal(QPoint(x, y)));
  setShowingPopup(false);
}

void DockPanel::createMenu() {
  menu_.addAction(QIcon::fromTheme("list-add"), QString("&Add Panel"),
      this, SLOT(addDock()));
  menu_.addAction(QIcon::fromTheme("edit-copy"), QString("&Clone Panel"),
      this, SLOT(cloneDock()));
  menu_.addAction(QIcon::fromTheme("edit-delete"), QString("&Remove Panel"),
      this, SLOT(removeDock()));

  const int numScreens = WindowSystem::screens().size();
  if (numScreens > 1) {
    QMenu* screen = menu_.addMenu(QString("Scr&een"));
    for (int i = 0; i < numScreens; ++i) {
      QAction* action = screen->addAction(
          "Screen " + QString::number(i + 1), this,
          [this, i]() {
            changeScreen(i);
          });
      action->setCheckable(true);
      screenActions_.push_back(action);
    }
  }

  menu_.addSeparator();

  menu_.addAction(QIcon::fromTheme("configure"), QString("&Settings"), this,
      [this] {
        minimize();
        QTimer::singleShot(DockPanel::kExecutionDelayMs, [this]{
          showSettingsDialog();
        });
      });

  QMenu* helpMenu = menu_.addMenu(QIcon::fromTheme("help-contents"), "&Help");
  helpMenu->addAction(QIcon::fromTheme("help-contents"),
                  QString("Online &Documentation"),
                  this, SLOT(showOnlineDocumentation()));
  helpMenu->addAction(QIcon::fromTheme("help-about"), QString("A&bout Crystal Dock"), this,
      [this] {
        minimize();
        QTimer::singleShot(DockPanel::kExecutionDelayMs, [this]{
          about();
        });
      });

  menu_.addSeparator();
  menu_.addAction(QIcon::fromTheme("application-exit"), "E&xit", parent_, SLOT(exit()));
}

void DockPanel::setPosition(PanelPosition position) {
  position_ = position;
  orientation_ = (position_ == PanelPosition::Top ||
      position_ == PanelPosition::Bottom)
      ? Qt::Horizontal : Qt::Vertical;
}

void DockPanel::setVisibility(PanelVisibility visibility) {
  visibility_ = visibility;
}

void DockPanel::setPanelStyle(PanelStyle panelStyle) {
  panelStyle_ = panelStyle;
}

void DockPanel::loadDockConfig() {
  setPosition(model_->panelPosition(dockId_));
  setScreen(model_->screen(dockId_));
  setVisibility(model_->visibility(dockId_));

  showApplicationMenu_ = model_->showApplicationMenu(dockId_);
  showPager_ = model_->showPager(dockId_) && WindowSystem::hasVirtualDesktopManager();
  showTrash_ = model_->showTrash(dockId_);
  showWifiManager_ = model_->showWifiManager(dockId_);
  showVolumeControl_ = model_->showVolumeControl(dockId_);
  showBatteryIndicator_ = model_->showBatteryIndicator(dockId_);
  showKeyboardLayout_ = model_->showKeyboardLayout(dockId_);
  showVersionChecker_ = model_->showVersionChecker(dockId_);
  showClock_ = model_->showClock(dockId_);
}

void DockPanel::saveDockConfig() {
  model_->setPanelPosition(dockId_, position_);
  model_->setScreen(dockId_, screen_);
  model_->setVisibility(dockId_, visibility_);
  model_->setShowApplicationMenu(dockId_, showApplicationMenu_);
  model_->setShowPager(dockId_, showPager_);
  model_->setShowTaskManager(dockId_, model_->showTaskManager(dockId_));
  model_->setShowTrash(dockId_, showTrash_);
  model_->setShowWifiManager(dockId_, showWifiManager_);
  model_->setShowVolumeControl(dockId_, showVolumeControl_);
  model_->setShowBatteryIndicator(dockId_, showBatteryIndicator_);
  model_->setShowKeyboardLayout(dockId_, showKeyboardLayout_);
  model_->setShowVersionChecker(dockId_, showVersionChecker_);
  model_->setShowClock(dockId_, showClock_);
  model_->saveDockConfig(dockId_);
}

void DockPanel::loadAppearanceConfig() {
  minSize_ = model_->minIconSize();
  maxSize_ = model_->maxIconSize();
  spacingFactor_ = model_->spacingFactor();
  backgroundColor_ = model_->backgroundColor();
  borderColor_ = model_->borderColor();
  tooltipFontSize_ = model_->tooltipFontSize();
  setPanelStyle(model_->panelStyle());
}

void DockPanel::initApplicationMenu() {
  if (showApplicationMenu_) {
    items_.push_back(std::make_unique<ApplicationMenu>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initLaunchers() {
  for (const auto& launcherConfig : model_->launcherConfigs(dockId_)) {
    if (launcherConfig.appId == kSeparatorId || launcherConfig.appId == kLauncherSeparatorId) {
      items_.push_back(std::make_unique<Separator>(
          this, model_, orientation_, minSize_, maxSize_,
          launcherConfig.appId == kLauncherSeparatorId));
    } else {
      QPixmap icon = loadIcon(launcherConfig.icon, kIconLoadSize);
      items_.push_back(std::make_unique<Program>(
          this, model_, launcherConfig.appId, launcherConfig.name, orientation_,
          icon, minSize_, maxSize_, launcherConfig.command,
          model_->isAppMenuEntry(launcherConfig.appId.toStdString()), /*pinned=*/true));
    }
  }
}

void DockPanel::initPager() {
  if (showPager_) {
    for (const auto& desktop : WindowSystem::desktops()) {
      items_.push_back(std::make_unique<DesktopSelector>(
          this, model_, orientation_, minSize_, maxSize_, desktop, screen_));
    }
  }
}

void DockPanel::initTasks() {
  if (!showTaskManager()) {
    return;
  }

  for (const auto* task : WindowSystem::windows()) {
    if (isValidTask(task)) {
      addTask(task);
    }
  }
}

void DockPanel::reloadTasks() {
  if (!showTaskManager()) {
    return;
  }

  const int itemsToKeep = applicationMenuItemCount() + pagerItemCount();
  items_.resize(itemsToKeep);
  initLaunchers();
  initTasks();
  initTrash();
  initWifiManager();
  initVolumeControl();
  initBatteryIndicator();
  initKeyboardLayout();
  initVersionChecker();
  initClock();
  resizeTaskManager();
  if (WindowSystem::hasAutoHideManager() && intellihideShouldHide()) {
    setAutoHide();
  }
}

bool DockPanel::addTask(const WindowInfo* task, bool fadeIn) {
  // Checks is the task already exists.
  if (hasTask(task->window)) {
    return false;
  }

  // Tries adding the task to existing programs.
  for (auto& item : items_) {
    if (item->addTask(task)) {
      return false;
    }
  }

  // Adds a new program.
  auto app = model_->findApplication(task->appId);
  if (!app && !task->appId.empty()) {
    std::cerr << "Could not find application with id: " << task->appId
              << ". The window icon will have limited functionalities." << std::endl;
  }
  const QString label = app ? app->name : QString::fromStdString(task->title);
  const QString appId = app ? app->appId : QString::fromStdString(task->appId);
  QPixmap appIcon = app ? loadIcon(app->icon, kIconLoadSize) : QPixmap();
  QString taskIconName = QString::fromStdString(task->icon);
  QPixmap taskIcon = appIcon.isNull() && !taskIconName.isEmpty()
      ? loadIcon(taskIconName, kIconLoadSize) : QPixmap();
  if (app && appIcon.isNull()) {
    std::cerr << "Could not find icon with name: " << app->icon.toStdString()
              << " in the current icon theme and its fallbacks."
              << " The window icon will have limited functionalities." << std::endl;
  }

  int i = 0;
  for (; i < itemCount() && items_[i]->beforeTask(label); ++i);
  if (!model_->groupTasksByApplication()) {
    for (; i < itemCount() && items_[i]->getAppLabel() == label; ++i);
  }
  if (!appIcon.isNull()) {
    const auto pinned = !model_->groupTasksByApplication() &&
                        model_->launchers(dockId_).contains(app->appId);
    items_.insert(items_.begin() + i, std::make_unique<Program>(
        this, model_, appId, label, orientation_, appIcon, minSize_,
        maxSize_, app->command, /*isAppMenuEntry=*/true, pinned));
  } else if (!taskIcon.isNull()) {
    items_.insert(items_.begin() + i, std::make_unique<Program>(
        this, model_, appId, label, orientation_, taskIcon, minSize_, maxSize_));
  } else {
    items_.insert(items_.begin() + i, std::make_unique<Program>(
        this, model_, appId, label, orientation_, QPixmap(), minSize_, maxSize_));
  }
  if (fadeIn) {
    items_[i]->opacity_ = 0.0f;
  }
  // Set a reasonable initial position so the item is never drawn at (0,0).
  // resizeTaskManager() will compute the correct position.
  if (i > 0) {
    items_[i]->left_ = items_[i - 1]->left_;
    items_[i]->top_ = items_[i - 1]->top_;
  } else if (itemCount() > 1) {
    items_[i]->left_ = items_[i + 1]->left_;
    items_[i]->top_ = items_[i + 1]->top_;
  }
  items_[i]->addTask(task);

  return true;
}

void DockPanel::removeTask(void* window) {
  for (int i = 0; i < itemCount(); ++i) {
    if (items_[i]->removeTask(window)) {
      if (items_[i]->shouldBeRemoved()) {
        items_[i]->fadingOut_ = true;
        resizeTaskManager();
      }
      return;
    }
  }
}

void DockPanel::updateTask(const WindowInfo* task) {
  for (auto& item : items_) {
    if (item->updateTask(task)) {
      return;
    }
  }
}

bool DockPanel::isValidTask(const WindowInfo* task) {
  if (task == nullptr) {
    return false;
  }

  if (task->skipTaskbar) {
    return false;
  }

  if (WindowSystem::hasVirtualDesktopManager() && model_->currentDesktopTasksOnly()
      && !task->onAllDesktops && task->desktop != WindowSystem::currentDesktop()) {
    return false;
  }

  if (model_->currentScreenTasksOnly()) {
    if (!task->outputs.empty() && !task->outputs.contains(screenOutput_)) {
      return false;
    }

    QRect taskGeometry(task->x, task->y, task->width, task->height);
    if (taskGeometry.isValid() && !screenGeometry_.intersects(taskGeometry)) {
      return false;
    }
  }

  if (WindowSystem::hasActivityManager() && !WindowSystem::currentActivity().empty()
      && !task->activity.empty() && task->activity != WindowSystem::currentActivity()) {
    return false;
  }

  return true;
}

bool DockPanel::shouldConsiderTaskForIntellihide(const WindowInfo* task) {
  if (task == nullptr) {
    return false;
  }

  if (WindowSystem::hasVirtualDesktopManager()
      && !task->onAllDesktops && task->desktop != WindowSystem::currentDesktop()) {
    return false;
  }

  QRect taskGeometry(task->x, task->y, task->width, task->height);
  if (taskGeometry.isValid() && !screenGeometry_.intersects(taskGeometry)) {
    return false;
  }

  if (WindowSystem::hasActivityManager() && !WindowSystem::currentActivity().empty()
      && !task->activity.empty() && task->activity != WindowSystem::currentActivity()) {
    return false;
  }

  return true;
}

bool DockPanel::hasTask(void* window) {
  for (auto& item : items_) {
    if (item->hasTask(window)) {
      return true;
    }
  }
  return false;
}

void DockPanel::initTrash() {
  if (showTrash_) {
    items_.push_back(std::make_unique<Trash>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initWifiManager() {
  if (showWifiManager_) {
    items_.push_back(std::make_unique<WifiManager>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initVolumeControl() {
  if (showVolumeControl_) {
    items_.push_back(std::make_unique<VolumeControl>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initBatteryIndicator() {
  if (showBatteryIndicator_) {
    items_.push_back(std::make_unique<BatteryIndicator>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initKeyboardLayout() {
  if (showKeyboardLayout_) {
    items_.push_back(std::make_unique<KeyboardLayout>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initVersionChecker() {
  if (showVersionChecker_) {
    items_.push_back(std::make_unique<VersionChecker>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initClock() {
  if (showClock_) {
    items_.push_back(std::make_unique<Clock>(
        this, model_, orientation_, minSize_, maxSize_));
  }
}

void DockPanel::initLayoutVars() {
  const auto spacingMultiplier = isMetal2D() ? kSpacingMultiplierMetal2D : kSpacingMultiplier;
  itemSpacing_ = std::round(minSize_* spacingMultiplier * spacingFactor_);
  margin3D_ = static_cast<int>(minSize_ * 0.6);
  floatingMargin_ = model_->floatingMargin();
  parabolicMaxX_ = std::round(3.5 * (minSize_ + itemSpacing_));
  numAnimationSteps_ = 14;

  QFont font;
  font.setPointSize(model_->tooltipFontSize());
  font.setBold(true);
  QFontMetrics metrics(font);
  // Account for pill padding + arrow so the widget is tall enough.
  int textH = metrics.height();
  int vPad = textH * 0.3;
  tooltipSize_ = textH + 2 * vPad + 6 /*arrowH*/ + 8 /*gap*/;

  const int distance = minSize_ + itemSpacing_;
  // The difference between minWidth_ and maxWidth_
  // (horizontal mode) or between minHeight_ and
  // maxHeight_ (vertical mode).
  // Compute dynamically based on how many icons the zoom radius covers.
  int delta = 0;
  if (itemCount() >= 1) {
    delta = parabolic(0) - minSize_;  // center icon
    int affected = 1;
    for (int d = 1; d * distance <= parabolicMaxX_ && affected + 2 <= itemCount(); ++d) {
      delta += 2 * (parabolic(d * distance) - minSize_);
      affected += 2;
    }
    // Handle even item counts (one extra neighbor on one side).
    if (affected < itemCount()) {
      int d = (affected + 1) / 2;
      if (d * distance <= parabolicMaxX_) {
        delta += parabolic(d * distance) - minSize_;
      }
    }
  }

  if (orientation_ == Qt::Horizontal) {
    minWidth_ = itemSpacing_;
    if (isBottom() && is3D()) { minWidth_ += 2 * margin3D_; }
    for (const auto& item : items_) {
      if (!item->fadingOut_) {
        minWidth_ += (item->getMinWidth() + itemSpacing_);
      }
    }
    minBackgroundWidth_ = minWidth_;
    minHeight_ = minSize_ + 2 * itemSpacing_;
    minBackgroundHeight_ = minHeight_;
    maxWidth_ = minWidth_ + delta;
    maxHeight_ = 2 * itemSpacing_ + maxSize_ + tooltipSize_;
    if (isFloating()) {
      maxHeight_ += 2 * floatingMargin_;
      minHeight_ += 2 * floatingMargin_;
    }
    if (is3D() && isBottom()) {
      maxHeight_ += k3DPanelThickness;
      minHeight_ += k3DPanelThickness;
    }
  } else {  // Vertical
    minHeight_ = itemSpacing_;
    for (const auto& item : items_) {
      if (!item->fadingOut_) {
        minHeight_ += (item->getMinHeight() + itemSpacing_);
      }
    }
    minBackgroundHeight_ = minHeight_;
    minWidth_ = minSize_ + 2 * itemSpacing_;
    minBackgroundWidth_ = minWidth_;
    maxHeight_ = minHeight_ + delta;
    maxWidth_ = 2 * itemSpacing_ + maxSize_ + tooltipSize_;
    if (isFloating()) {
      maxWidth_ += 2 * floatingMargin_;
      minWidth_ += 2 * floatingMargin_;
    }
  }

  resize(maxWidth_, maxHeight_);
}

QRect DockPanel::getMinimizedDockGeometry() {
  QRect dockGeometry;
  dockGeometry.setX(
      isHorizontal()
      ? screenGeometry_.x() + (screenGeometry_.width() - minWidth_) / 2
      : isLeft()
          ? screenGeometry_.x()
          : screenGeometry_.x() + screenGeometry_.width() - minWidth_);
  dockGeometry.setY(
      isHorizontal()
      ? isTop()
        ? screenGeometry_.y()
        : screenGeometry_.y() + screenGeometry_.height() - minHeight_
      : screenGeometry_.y() + (screenGeometry_.height() - minHeight_) / 2);
  dockGeometry.setWidth(minWidth_);
  dockGeometry.setHeight(minHeight_);
  return dockGeometry;
}

void DockPanel::updateLayout() {
  if (isLeaving_) {
    for (const auto& item : items_) {
      item->setAnimationStartAsCurrent();
      if (isHorizontal()) {
        startBackgroundWidth_ = backgroundWidth_;
        startBackgroundHeight_ = minSize_ + 2 * itemSpacing_;
      } else {  // Vertical
        startBackgroundHeight_ = backgroundHeight_;
        startBackgroundWidth_ = minSize_ + 2 * itemSpacing_;
      }
    }
  }

  int prevNonFading = -1;  // index of last non-fading item for positioning
  for (int i = 0; i < itemCount(); ++i) {
    if (items_[i]->fadingOut_) continue;
    items_[i]->size_ = minSize_;
    if (isHorizontal()) {
      items_[i]->left_ =
          (prevNonFading < 0) ? isBottom() && is3D() ? itemSpacing_ + (maxWidth_ - minWidth_) / 2 + margin3D_
                                          : itemSpacing_ + (maxWidth_ - minWidth_) / 2
                   : items_[prevNonFading]->left_ + items_[prevNonFading]->getMinWidth() + itemSpacing_;
      items_[i]->top_ = isTop() ? itemSpacing_
                                : itemSpacing_ + maxHeight_ - minHeight_;
      if (isFloating()) { items_[i]->top_ += floatingMargin_; }
      items_[i]->minCenter_ = items_[i]->left_ + items_[i]->getMinWidth() / 2;
    } else {  // Vertical
      items_[i]->left_ = isLeft() ? itemSpacing_
                                  : itemSpacing_ + maxWidth_ - minWidth_;
      if (isFloating()) { items_[i]->left_ += floatingMargin_; }
      items_[i]->top_ = (prevNonFading < 0) ? itemSpacing_ + (maxHeight_ - minHeight_) / 2
          : items_[prevNonFading]->top_ + items_[prevNonFading]->getMinHeight() + itemSpacing_;
      items_[i]->minCenter_ = items_[i]->top_ + items_[i]->getMinHeight() / 2;
    }
    prevNonFading = i;
  }

  backgroundWidth_ = minBackgroundWidth_;
  backgroundHeight_ = minBackgroundHeight_;

  if (isLeaving_) {
    for (const auto& item : items_) {
      item->endSize_ = item->size_;
      item->endLeft_ = item->left_;
      item->endTop_ = item->top_;
      item->startAnimation(numAnimationSteps_);
    }

    endBackgroundWidth_ = minBackgroundWidth_;
    backgroundWidth_ = startBackgroundWidth_;
    endBackgroundHeight_ = minBackgroundHeight_;
    backgroundHeight_ = startBackgroundHeight_;

    currentAnimationStep_ = 0;
    isAnimationActive_ = true;
    animationTimer_->start(32 - model_->zoomingAnimationSpeed());
  } else {
    WindowSystem::setLayer(this,
                           visibility_ == PanelVisibility::AlwaysVisible
                               ? LayerShellQt::Window::LayerBottom
                               : LayerShellQt::Window::LayerTop);
    isMinimized_ = true;
    if (autoHide()) { isHidden_ = true; }
    if (intellihide()) { isHidden_ = intellihideShouldHide(); }
    update();
    QTimer::singleShot(500, [this]{ setMask(); });
  }
}

void DockPanel::updateLayout(int x, int y) {
  if (isEntering_) {
    // Cancel any resize animation in progress.
    if (isResizeAnimating_) {
      isResizeAnimating_ = false;
      backgroundWidth_ = endBackgroundWidth_;
      backgroundHeight_ = endBackgroundHeight_;
      // Finalize any pending fades.
      items_.erase(
          std::remove_if(items_.begin(), items_.end(),
              [](const auto& item) { return item->fadingOut_; }),
          items_.end());
      for (auto& item : items_) { item->opacity_ = 1.0f; }
    }
    // Initialize enter animation before the layout loop so enterProgress_
    // starts at 0 on the very first frame (no full-zoom flash).
    enterProgress_ = 0.0;
    currentAnimationStep_ = 0;
    isAnimationActive_ = true;
    isEnterAnimating_ = true;
    isEntering_ = false;
    animationTimer_->start(32 - model_->zoomingAnimationSpeed());
  }

  int first_update_index = -1;
  int last_update_index = 0;
  if (isHorizontal()) {
    items_[0]->left_ = isBottom() && is3D() ? itemSpacing_ + margin3D_
                                            : itemSpacing_;
  } else {  // Vertical
    items_[0]->top_ = itemSpacing_;
  }

  // Clamp the zoom coordinate to the range of icon centers so the zoom
  // effect never applies in empty space beyond the first/last icon.
  int zoomPos;
  if (isHorizontal()) {
    int firstCenter = items_[0]->minCenter_;
    int lastCenter = items_[itemCount() - 1]->minCenter_;
    zoomPos = std::clamp(x, firstCenter, lastCenter);
  } else {
    int firstCenter = items_[0]->minCenter_;
    int lastCenter = items_[itemCount() - 1]->minCenter_;
    zoomPos = std::clamp(y, firstCenter, lastCenter);
  }

  for (int i = 0; i < itemCount(); ++i) {
    int delta;
    if (isHorizontal()) {
      delta = std::abs(items_[i]->minCenter_ - zoomPos);
    } else {  // Vertical
      delta = std::abs(items_[i]->minCenter_ - zoomPos);
    }
    if (delta < parabolicMaxX_) {
      if (first_update_index == -1) {
        first_update_index = i;
      }
      last_update_index = i;
    }
    // Scale the parabolic zoom by enterProgress_ so it ramps up smoothly.
    items_[i]->size_ = minSize_ +
        static_cast<int>((parabolic(delta) - minSize_) * enterProgress_);
    if (isHorizontal()) {
      items_[i]->top_ = isTop() ? itemSpacing_
                                : itemSpacing_ + tooltipSize_ + maxSize_ - items_[i]->size_;
      if (isFloating()) { items_[i]->top_ += floatingMargin_; }
    } else {  // Vertical
      items_[i]->left_ = isLeft() ? itemSpacing_
                                  : itemSpacing_ + tooltipSize_ + maxSize_ - items_[i]->size_;
      if (isFloating()) { items_[i]->left_ += floatingMargin_; }
    }
    if (i > 0) {
      if (isHorizontal()) {
        items_[i]->left_ = items_[i - 1]->left_ + items_[i - 1]->getWidth()
            + itemSpacing_;
      } else {  // Vertical
        items_[i]->top_ = items_[i - 1]->top_ + items_[i - 1]->getHeight()
            + itemSpacing_;
      }
    }
  }

  // Center all items within the panel so zooming at edges doesn't
  // push icons into empty space on the opposite side.
  if (isHorizontal()) {
    const auto& last = items_[itemCount() - 1];
    int totalWidth = last->left_ + last->getWidth() + itemSpacing_;
    if (isBottom() && is3D()) { totalWidth += margin3D_; }
    int offset = (maxWidth_ - totalWidth) / 2;
    for (int i = 0; i < itemCount(); ++i) {
      items_[i]->left_ += offset;
    }
  } else {
    const auto& last = items_[itemCount() - 1];
    int totalHeight = last->top_ + last->getHeight() + itemSpacing_;
    int offset = (maxHeight_ - totalHeight) / 2;
    for (int i = 0; i < itemCount(); ++i) {
      items_[i]->top_ += offset;
    }
  }

  // Compute background pill size from actual item content, not maxWidth_.
  if (isHorizontal()) {
    const auto& last = items_[itemCount() - 1];
    int contentWidth = last->left_ + last->getWidth() + itemSpacing_ - items_[0]->left_ + itemSpacing_;
    if (isBottom() && is3D()) { contentWidth += 2 * margin3D_; }
    int targetWidth = std::max(contentWidth, minBackgroundWidth_);
    backgroundWidth_ = minBackgroundWidth_ +
        static_cast<int>((targetWidth - minBackgroundWidth_) * enterProgress_);
    backgroundHeight_ = minSize_ + 2 * itemSpacing_;
  } else {
    const auto& last = items_[itemCount() - 1];
    int contentHeight = last->top_ + last->getHeight() + itemSpacing_ - items_[0]->top_ + itemSpacing_;
    int targetHeight = std::max(contentHeight, minBackgroundHeight_);
    backgroundHeight_ = minBackgroundHeight_ +
        static_cast<int>((targetHeight - minBackgroundHeight_) * enterProgress_);
    backgroundWidth_ = minSize_ + 2 * itemSpacing_;
  }

  // During drag, pin the dragged item to the cursor so it never flickers
  // back to its layout-computed position (setMask/update may repaint).
  if (isDragging_ && dragItemIndex_ >= 0 && dragItemIndex_ < itemCount()) {
    if (isHorizontal()) {
      items_[dragItemIndex_]->left_ = dragMouseX_ - items_[dragItemIndex_]->getWidth() / 2;
    } else {
      items_[dragItemIndex_]->top_ = dragMouseY_ - items_[dragItemIndex_]->getHeight() / 2;
    }
  }

  mouseX_ = x;
  mouseY_ = y;

  //resize(maxWidth_, maxHeight_);
  WindowSystem::setLayer(this, LayerShellQt::Window::LayerTop);
  isMinimized_ = false;
  if (autoHide() || intellihide()) { isHidden_ = false; }
  setMask();
  updateActiveItem(x, y);
  update();
}

void DockPanel::resizeTaskManager() {
  // Save old dimensions for smooth resize animation.
  const int oldBgWidth = backgroundWidth_;
  const int oldBgHeight = backgroundHeight_;
  const int oldMaxWidth = maxWidth_;
  const int oldMaxHeight = maxHeight_;
  const bool wasMinimized = isMinimized_;

  // Re-calculate panel's size.
  initLayoutVars();

  if (isMinimized_) {
    // Smoothly animate the background pill if its size changed.
    if (wasMinimized && (oldBgWidth != minBackgroundWidth_ ||
                         oldBgHeight != minBackgroundHeight_)) {
      // Stop any conflicting animation.
      if (isAnimationActive_) {
        animationTimer_->stop();
        isAnimationActive_ = false;
        isLeaving_ = false;
        isEnterAnimating_ = false;
        isResizeAnimating_ = false;
      }

      // Use the larger of old/new widget size so the shrinking pill
      // isn't clipped during animation.
      int animMaxW = std::max(oldMaxWidth, maxWidth_);
      int animMaxH = std::max(oldMaxHeight, maxHeight_);
      if (animMaxW != maxWidth_ || animMaxH != maxHeight_) {
        maxWidth_ = animMaxW;
        maxHeight_ = animMaxH;
        resize(maxWidth_, maxHeight_);
      }

      // Save current positions before layout recalculates them.
      for (auto& item : items_) {
        item->setAnimationStartAsCurrent();
      }

      updateLayout();  // Places items at final positions within the wider widget.

      // Save final positions for interpolation.
      for (auto& item : items_) {
        item->setAnimationEndAsCurrent();
        if (item->opacity_ < 0.01f || item->fadingOut_) {
          // New or fading items: pin to end position (no slide from 0,0).
          item->startLeft_ = item->endLeft_;
          item->startTop_ = item->endTop_;
        }
        // Restore to start position; animation will interpolate.
        item->left_ = item->startLeft_;
        item->top_ = item->startTop_;
      }

      startBackgroundWidth_ = oldBgWidth;
      endBackgroundWidth_ = minBackgroundWidth_;
      startBackgroundHeight_ = oldBgHeight;
      endBackgroundHeight_ = minBackgroundHeight_;
      backgroundWidth_ = oldBgWidth;
      backgroundHeight_ = oldBgHeight;
      currentAnimationStep_ = 0;
      isResizeAnimating_ = true;
      isAnimationActive_ = true;

      // Set mask to cover the old (larger) background area.
      if (isHorizontal()) {
        int maskW = std::max(oldBgWidth, static_cast<int>(minBackgroundWidth_)) + 2;
        int maskX = (maxWidth_ - maskW) / 2;
        int maskY = isTop() ? 0 : maxHeight_ - minHeight_;
        QWidget::setMask(QRegion(maskX, maskY, maskW, minHeight_));
      } else {
        int maskH = std::max(oldBgHeight, static_cast<int>(minBackgroundHeight_)) + 2;
        int maskY = (maxHeight_ - maskH) / 2;
        int maskX = isLeft() ? 0 : maxWidth_ - minWidth_;
        QWidget::setMask(QRegion(maskX, maskY, minWidth_, maskH));
      }

      animationTimer_->start(16);
    } else {
      // No size change — finalize fades immediately.
      items_.erase(
          std::remove_if(items_.begin(), items_.end(),
              [](const auto& item) { return item->fadingOut_; }),
          items_.end());
      for (auto& item : items_) { item->opacity_ = 1.0f; }
      updateLayout();
    }
    return;
  } else {
    // Not minimized — finalize fades immediately.
    items_.erase(
        std::remove_if(items_.begin(), items_.end(),
            [](const auto& item) { return item->fadingOut_; }),
        items_.end());
    for (auto& item : items_) { item->opacity_ = 1.0f; }
    // Need to call QWidget::resize(), not DockPanel::resize(), in order not to
    // mess up the zooming.
    //QWidget::resize(maxWidth_, maxHeight_);
    if (isHorizontal()) {
      backgroundWidth_ = maxWidth_;
    } else {
      backgroundHeight_ = maxHeight_;
    }
  }

  const int itemsToKeep = (showApplicationMenu_ ? 1 : 0) +
      (showPager_ ? WindowSystem::numberOfDesktops() : 0);
  int left = 0;
  int top = 0;
  for (int i = 0; i < itemCount(); ++i) {
    if (isHorizontal()) {
      left = (i == 0) ? isBottom() && is3D() ? itemSpacing_ + (maxWidth_ - minWidth_) / 2 + margin3D_
                                             : itemSpacing_ + (maxWidth_ - minWidth_) / 2
                      : left + items_[i - 1]->getMinWidth() + itemSpacing_;
      if (i >= itemsToKeep) {
        items_[i]->minCenter_ = left + items_[i]->getMinWidth() / 2;
      }
    } else {  // Vertical
      top = (i == 0) ? itemSpacing_ + (maxHeight_ - minHeight_) / 2
                     : top + items_[i - 1]->getMinHeight() + itemSpacing_;
      if (i >= itemsToKeep) {
        items_[i]->minCenter_ = top + items_[i]->getMinHeight() / 2;
      }
    }
  }

  int last_update_index = 0;
  for (int i = itemsToKeep; i < itemCount(); ++i) {
    int delta;
    if (isHorizontal()) {
      delta = std::abs(items_[i]->minCenter_ - mouseX_);
    } else {  // Vertical
      delta = std::abs(items_[i]->minCenter_ - mouseY_);
    }
    if (delta < parabolicMaxX_) {
      last_update_index = i;
    }
    items_[i]->size_ = parabolic(delta);
    if (isHorizontal()) {
      items_[i]->top_ = isTop() ? itemSpacing_
                                : itemSpacing_ + tooltipSize_ + maxSize_ - items_[i]->getHeight();
      if (isFloating()) { items_[i]->top_ += floatingMargin_; }
    } else {  // Vertical
      items_[i]->left_ = isLeft() ? itemSpacing_
                                  : itemSpacing_ + tooltipSize_ + maxSize_ - items_[i]->getWidth();
      if (isFloating()) { items_[i]->left_ += floatingMargin_; }
    }
    if (i > 0) {
      if (isHorizontal()) {
        items_[i]->left_ = items_[i - 1]->left_ + items_[i - 1]->getWidth()
            + itemSpacing_;
      } else {  // Vertical
        items_[i]->top_ = items_[i - 1]->top_ + items_[i - 1]->getHeight()
            + itemSpacing_;
      }
    }
  }

  for (int i = itemCount() - 1;
       i >= std::max(itemsToKeep, last_update_index + 1); --i) {
    if (isHorizontal()) {
      items_[i]->left_ = (i == itemCount() - 1)
          ? isBottom() && is3D() ? maxWidth_ - itemSpacing_ - items_[i]->getMinWidth() - margin3D_
                                 : maxWidth_ - itemSpacing_ - items_[i]->getMinWidth()
          : items_[i + 1]->left_ - items_[i]->getMinWidth() - itemSpacing_;
    } else {  // Vertical
      items_[i]->top_ = (i == itemCount() - 1)
          ? maxHeight_ - itemSpacing_ - items_[i]->getMinHeight()
          : items_[i + 1]->top_ - items_[i]->getMinHeight() - itemSpacing_;
    }
  }

  setMask();
}

void DockPanel::setStrut(int width) {
  LayerShellQt::Window::Anchors anchor = LayerShellQt::Window::AnchorBottom;
  switch (position_) {
    case PanelPosition::Top:
      anchor = LayerShellQt::Window::AnchorTop;
      break;
    case PanelPosition::Bottom:
      anchor = LayerShellQt::Window::AnchorBottom;
      break;
    case PanelPosition::Left:
      anchor = LayerShellQt::Window::AnchorLeft;
      break;
    case PanelPosition::Right:
      anchor = LayerShellQt::Window::AnchorRight;
      break;
  }

  WindowSystem::setAnchorAndStrut(this, anchor, width);
}

void DockPanel::setMask() {
  static constexpr int kBounceHeight = 32;
  if (isMinimized_) {
    if (isHorizontal()) {
      const int x = (maxWidth_ - minWidth_) / 2;
      const int baseY = isTop() ? 0 : maxHeight_ - minHeight_;
      // Expand mask to allow bounce animation to be visible.
      const int y = isTop() ? baseY : std::max(0, baseY - kBounceHeight);
      const int h = minHeight_ + kBounceHeight;
      QWidget::setMask(QRegion(x, y, minWidth_, h));
    } else {  // Vertical.
      const int y = (maxHeight_ - minHeight_) / 2;
      const int baseX = isLeft() ? 0 : maxWidth_ - minWidth_;
      const int x = isLeft() ? baseX : std::max(0, baseX - kBounceHeight);
      const int w = minWidth_ + kBounceHeight;
      QWidget::setMask(QRegion(x, y, w, minHeight_));
    }
  } else {
    QWidget::setMask(QRegion(0, 0, maxWidth_, maxHeight_));
  }
  repaint();
}

void DockPanel::updatePosition(PanelPosition position) {
  setPosition(position);
  reload();
  if (isHidden_) {
    // we have to deactivate, wait then re-activate Auto Hide
    // otherwise the Auto Hide screen edge's border length would not be updated
    // correctly.
    setAutoHide(false);
    update();
    QTimer::singleShot(1000, [this]{ setAutoHide(); });
  }
  saveDockConfig();
}

void DockPanel::updateVisibility(PanelVisibility visibility) {
  setVisibility(visibility);
  setStrut();
  setAutoHide(autoHide() || intellihideShouldHide());
  saveDockConfig();
}

void DockPanel::setAutoHide(bool on) {
  if (isHidden_ != on) {
    isHidden_ = on;
  }

  if (!WindowSystem::hasAutoHideManager()) {
    repaint();
    setMask();
    return;
  }

  Qt::Edge edge = Qt::BottomEdge;
  switch (position_) {
    case PanelPosition::Top:
      edge = Qt::TopEdge;
      break;
    case PanelPosition::Bottom:
      edge = Qt::BottomEdge;
      break;
    case PanelPosition::Left:
      edge = Qt::LeftEdge;
      break;
    case PanelPosition::Right:
      edge = Qt::RightEdge;
      break;
  }
  WindowSystem::setAutoHide(this, edge, on);
}

void DockPanel::updateActiveItem(int x, int y) {
  int i = 0;
  while (i < itemCount() &&
      ((orientation_ == Qt::Horizontal && items_[i]->left_ < x) ||
      (orientation_ == Qt::Vertical && items_[i]->top_ < y))) {
    ++i;
  }
  activeItem_ = i - 1;
}

int DockPanel::parabolic(int x) {
  // Cosine bell curve: smooth .oOo. pyramid centered on the cursor.
  if (x > parabolicMaxX_) {
    return minSize_;
  }
  const double t = static_cast<double>(x) / parabolicMaxX_;
  // (1 + cos(π·t)) / 2 gives a smooth 1→0 falloff.
  const double scale = (1.0 + std::cos(M_PI * t)) / 2.0;
  return minSize_ + static_cast<int>((maxSize_ - minSize_) * scale);
}

}  // namespace crystaldock
