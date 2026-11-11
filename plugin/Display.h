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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <core/Portability.h>
#include <core/Proxy.h>
#include <core/Trace.h>

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

#include <interfaces/IDeviceSettingsDisplay.h>

#include "dsUtl.h"
#include "dsError.h"
#include "dsDisplay.h"

#include "hal/dDisplay.h"
#include "hal/dDisplayImpl.h"
#include "DeviceSettingsTypes.h"

class Display {
    using IPlatform = hal::dDisplay::IPlatform;
    using DefaultImpl = dDisplayImpl;

    std::shared_ptr<IPlatform> _platform;

public:
    class INotification {

        public:
            virtual ~INotification() = default;
            virtual void OnDisplayRxSense(const DisplayEvent displayEvent) = 0;
            virtual void OnDisplayHDCPStatus(const int32_t hdcpStatus) = 0;
            virtual void OnDisplayHDMIHotPlug(const DisplayEvent displayEvent) = 0;
    };

public:
    
    // Display interface methods - exactly replicating dsDisplay.c functionality
    uint32_t GetDisplayEdid(const int32_t handle, DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList);
    uint32_t GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength);
    
    // General display initialization and management
    uint32_t DisplayInit();
    uint32_t DisplayTerm();
    uint32_t GetDisplay(const int32_t type, const int32_t index, int32_t &handle);
    uint32_t GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio &aspectRatio);
    uint32_t SetAllmEnabled(const int32_t handle, const bool enabled);
    uint32_t SetAVIContentType(const int32_t handle, const int32_t contentType);
    uint32_t SetAVIScanInformation(const int32_t handle, const int32_t scanInfo);

    // Display event handling methods - Called by DS HAL to forward events to parent
    void OnDisplayRxSense(const DisplayEvent displayEvent);
    void OnDisplayHDCPStatus(const int32_t hdcpStatus);
    void OnDisplayHDMIHotPlug(const DisplayEvent displayEvent);

    /** Runtime factory: picks the AIDL HDMI Output HAL when available, otherwise the legacy DS HAL. */
    static Display Create(INotification& parent);

    template <typename IMPL = DefaultImpl, typename... Args>
    static Display Create(INotification& parent, Args&&... args)
    {
        ENTRY_LOG;
        static_assert(std::is_base_of<IPlatform, IMPL>::value, "Impl must derive from hal::dDisplay::IPlatform");
        auto impl = std::shared_ptr<IMPL>(new IMPL(std::forward<Args>(args)...));
        ASSERT(impl != nullptr);
        EXIT_LOG;
        return Display(parent, std::move(impl));
    }
    
    private:
    Display(INotification& parent, std::shared_ptr<IPlatform> platform);

    inline IPlatform& platform() const
    {
        return *_platform;
    }

    void Platform_init();

public:
    /** Deferred HAL init — called from DeviceSettingsImp::Configure() */
    void InitialiseHAL() { platform().InitialiseHAL(); }

    INotification& _parent;
};