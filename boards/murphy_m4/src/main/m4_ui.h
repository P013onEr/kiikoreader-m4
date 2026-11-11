#pragma once

#include <FreeInkDisplay.h>
#include <InputManager.h>
#include <FrontlightManager.h>
#include <BatteryMonitor.h>
#include <Rtc.h>

void murphyM4UiRun(freeink::FreeInkDisplay& display, InputManager& input,
                   FrontlightManager& frontlight, BatteryMonitor& battery,
                   Rtc& rtc, bool storageReady);
