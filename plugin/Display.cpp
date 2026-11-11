/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <functional>
#include <unistd.h>
#include <memory>

#include <core/IAction.h>
#include <core/Time.h>
#include <core/WorkerPool.h>

#include "secure_wrapper.h"
#include "Display.h"
#ifdef ENABLE_DISPLAY_AIDL
#include "HalFactoryUtility.h"
#include "hal/dDisplayAIDL.h"
#include <com/rdk/hal/hdmioutput/IHDMIOutputManager.h>
#endif

Display Display::Create(INotification& parent)
{
    ENTRY_LOG;
#ifdef ENABLE_DISPLAY_AIDL
    if (HalFactoryUtility::isAidlServiceAvailable(android::String16(com::rdk::hal::hdmioutput::IHDMIOutputManager::serviceName().c_str()))) {
        DSLOG_INFO("Using HDMI Output AIDL HAL");
        return Display(parent, std::make_shared<dDisplayAIDLImpl>());
    }
#endif
    DSLOG_INFO("Using legacy Display DS HAL");
    return Display(parent, std::make_shared<DefaultImpl>());
}

Display::Display(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    Platform_init();
}

void Display::Platform_init()
{
    DSLOG_INFO("Display Init - Setting up event callbacks");
    
    // Set up callback bundle for Display events - using global CallbackBundle pattern
    CallbackBundle bundle;
    
    bundle.OnDisplayRxSense = [this](const uint8_t /*port*/, const bool rxSenseOn) {
        this->OnDisplayRxSense(rxSenseOn ? DisplayEvent::DS_DISPLAY_RXSENSE_ON
                                         : DisplayEvent::DS_DISPLAY_RXSENSE_OFF);
    };
    bundle.OnDisplayHDCPStatus = [this](const uint8_t /*port*/, const int32_t hdcpStatus) {
        this->OnDisplayHDCPStatus(hdcpStatus);
    };
    bundle.OnDisplayHDMIHotPlug = [this](const uint8_t /*port*/, const bool connected) {
        this->OnDisplayHDMIHotPlug(connected ? DisplayEvent::DS_DISPLAY_EVENT_CONNECTED
                                             : DisplayEvent::DS_DISPLAY_EVENT_DISCONNECTED);
    };

    if (_platform) {
        // Use interface method directly - no casting needed
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
    
}

void Display::OnDisplayRxSense(const DisplayEvent displayEvent)
{
    DSLOG_INFO("Display OnDisplayRxSense event: displayEvent=%d", static_cast<int>(displayEvent));
    _parent.OnDisplayRxSense(displayEvent);
}

void Display::OnDisplayHDCPStatus(const int32_t hdcpStatus)
{
    DSLOG_INFO("Display OnDisplayHDCPStatus event");
    _parent.OnDisplayHDCPStatus(hdcpStatus);
}

void Display::OnDisplayHDMIHotPlug(const DisplayEvent displayEvent)
{
    DSLOG_INFO("Display OnDisplayHDMIHotPlug event: displayEvent=%d", static_cast<int>(displayEvent));
    _parent.OnDisplayHDMIHotPlug(displayEvent);
}

uint32_t Display::GetDisplayEdid(const int32_t handle, DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList)
{
    supportedResolutionList = nullptr;
    uint32_t result = this->platform().GetDisplayEdid(handle, edId, supportedResolutionList);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d", handle);
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength)
{
    uint32_t result = this->platform().GetDisplayEdidBytes(handle, edIdBytes, edidLength);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d, edidLength=%d", handle, edidLength);
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::DisplayInit()
{
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    // Initialize through platform interface - HAL is already initialized in constructor
    result = WPEFramework::Core::ERROR_NONE;
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded");
    } else {
        DSLOG_ERR("failed: error=%u", result);
    }
    return result;
}

uint32_t Display::DisplayTerm()
{
    uint32_t result = WPEFramework::Core::ERROR_NONE;
    // Termination handled by platform destructors
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded");
    } else {
        DSLOG_ERR("failed: error=%u", result);
    }
    return result;
}

uint32_t Display::GetDisplay(const int32_t type, const int32_t index, int32_t &handle)
{

    uint32_t result = this->platform().GetDisplay(type, index, handle);

    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("SUCCESS: type=%d, index=%d, handle=%d", type, index, handle);
    } else {
        DSLOG_ERR("FAILED: type=%d, index=%d, error=%u", type, index, result);
    }
    return result;
}

uint32_t Display::GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio &aspectRatio)
{
    uint32_t result = this->platform().GetDisplayAspectRatio(handle, aspectRatio);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d, aspectRatio=%d", handle, static_cast<int>(aspectRatio));
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAllmEnabled(const int32_t handle, const bool enabled)
{
    uint32_t result = this->platform().SetAllmEnabled(handle, enabled);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAVIContentType(const int32_t handle, const int32_t contentType)
{
    uint32_t result = this->platform().SetAVIContentType(handle, contentType);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d, contentType=%d", handle, contentType);
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAVIScanInformation(const int32_t handle, const int32_t scanInfo)
{
    uint32_t result = this->platform().SetAVIScanInformation(handle, scanInfo);
    if (result == WPEFramework::Core::ERROR_NONE) {
        DSLOG_INFO("succeeded: handle=%d, scanInfo=%d", handle, scanInfo);
    } else {
        DSLOG_ERR("failed: handle=%d, error=%u", handle, result);
    }
    return result;
}