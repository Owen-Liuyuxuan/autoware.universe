/*
 * UOS-ROS packages - Robot Operating System code by the University of Osnabrück
 * Copyright (C) 2014 University of Osnabrück
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other materials provided with
 * the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * route_set_tool.cpp
 *
 * Author: Henning Deeken {hdeeken@uos.de}
 *
 *
 */

#include "operation_view_tool.hpp"

#include "rviz_common/display_context.hpp"
#include "rviz_common/view_manager.hpp"

#include <iostream>

namespace tier4_camera_view_rviz_plugin
{
OperationViewTool::OperationViewTool()
{
  shortcut_key_ = 'o';
}

OperationViewTool::~OperationViewTool()
{
}

void OperationViewTool::onInitialize()
{
  setName("Operation View");

  step_length_property_ = new rviz_common::properties::FloatProperty(
    "Step Length", 0.1, "The length by with the position is updated on each step.",
    getPropertyContainer(), SLOT(setOffset()), this);

  boost_property_ = new rviz_common::properties::FloatProperty(
    "Boost Property", 0.5, "Gives the boost factor which is applied if pressing shift.",
    getPropertyContainer(), SLOT(setBoost()), this);

  fly_property_ = new rviz_common::properties::BoolProperty(
    "Fly Mode", false, "In fly mode it is possible to move along the z axis as well.",
    getPropertyContainer(), SLOT(setFlyMode()), this);

  left_hand_property_ = new rviz_common::properties::BoolProperty(
    "Left Hand Mode", false, "In left hand mode one uses the arrows to move around",
    getPropertyContainer(), SLOT(setLeftHandMode()), this);

  // temporarily disabled
  // fallback_tool_property_ = new rviz_common::properties::EnumProperty(
  //   "Fallback Tool", QString("tier4_camera_view_rviz_plugin/OperationViewTool"),
  //   "Determines to which tool the control switches, if the tool is deactivated.",
  //   getPropertyContainer(), SLOT(setFallbackTool()), this);
  fallback_view_controller_property_ = new rviz_common::properties::EnumProperty(
    "Fallback ViewController", QString("tier4_camera_view_rviz_plugin/OperationView"),
    "Determines to which view controller the control switches, if the tool is deactivated.",
    getPropertyContainer(), SLOT(setFallbackViewController()), this);

  m_pos_offset = 0.1;
  m_boost = 0.5;
  m_fly_mode = false;
  m_left_hand_mode = false;

  // temporarily disabled
  // setFallbackToolProperty();
  setFallbackViewControllerProperty();
}

void OperationViewTool::setFallbackViewControllerProperty()
{
  fallback_view_controller_property_->clearOptions();
  m_view_controller_classes.clear();

  m_view_controller_classes = context_->getViewManager()->getDeclaredClassIdsFromFactory();

  for (int i = 0; i < m_view_controller_classes.size(); i++) {
    if (m_view_controller_classes[i] != QString("tier4_camera_view_rviz_plugin/OperationView")) {
      fallback_view_controller_property_->addOption(m_view_controller_classes[i], i);
      m_view_controller.push_back(context_->getViewManager()->getViewAt(i));
    }
  }

  fallback_view_controller_property_->show();
  setFallbackViewController();
}

// temporarily disabled
// void OperationViewTool::setFallbackToolProperty()
// {
//   fallback_tool_property_->clearOptions();
//   m_tools.clear();

//   m_tool_classes = context_->getToolManager()->getToolClasses();

//   for (int i = 0; i < m_tool_classes.size(); i++) {
//     if (m_tool_classes[i] != getClassId()) {
//       fallback_tool_property_->addOption(m_tool_classes[i], i);
//       m_tools.push_back(context_->getToolManager()->getTool(i));
//     }
//   }

//   fallback_tool_property_->show();
//   setFallbackTool();
// }

void OperationViewTool::activate()
{
  context_->getViewManager()->setCurrentViewControllerType(
    QString("tier4_camera_view_rviz_plugin/OperationView"));
  context_->getViewManager()->getCurrent()->reset();

  // temporarily disabled
  // setFallbackToolProperty();
  setFallbackViewControllerProperty();
}

void OperationViewTool::deactivate()
{
}

int OperationViewTool::processKeyEvent(QKeyEvent * event, rviz_common::RenderPanel * panel)
{
  return 0;
}

int OperationViewTool::processMouseEvent(rviz_common::ViewportMouseEvent & event)
{
  if (event.panel->getViewController()) {
    event.panel->getViewController()->handleMouseEvent(event);
    setCursor(event.panel->getViewController()->getCursor());
  }
  return 0;
}

}  // namespace tier4_camera_view_rviz_plugin

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(tier4_camera_view_rviz_plugin::OperationViewTool, rviz_common::Tool)