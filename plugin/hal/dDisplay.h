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

#include "dsDisplay.h"
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"

#include "rfcapi.h"

#include <core/Portability.h>
#include <WPEFramework/interfaces/IDeviceSettingsDisplay.h>
#include "DeviceSettingsTypes.h"

#include <cstdint>

namespace hal {
namespace dDisplay {

    class IPlatform {

    public:
        virtual ~IPlatform() {} 
        virtual void InitialiseHAL() = 0;
        virtual void DeInitialiseHAL() = 0;
        virtual void setAllCallbacks(const CallbackBundle& bundle) = 0;
        virtual void getPersistenceValue() = 0;

        // Display Platform interface methods - all pure virtual
        virtual uint32_t GetConnectedVideoDisplay(const int32_t videoPortHandle, bool& isConnected) = 0;
        virtual uint32_t GetDisplaySurroundMode(const int32_t videoPortHandle, VideoPortSurroundMode& surroundMode) = 0;
        virtual uint32_t GetDisplayEDID(const int32_t videoPortHandle, uint8_t edidBytes[], const uint16_t edidBytesLength) = 0;

        // New Display Platform interface methods for the 5 required functions
        virtual uint32_t GetDisplay(const int32_t type, const int32_t index, int32_t &handle) = 0;
        virtual uint32_t GetDisplayAspectRatio(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio &aspectRatio) = 0; 
        virtual uint32_t GetDisplayEdid(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsDisplay::DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList) = 0;
        virtual uint32_t GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength) = 0;
        virtual uint32_t SetAllmEnabled(const int32_t handle, const bool enabled) = 0;
        virtual uint32_t SetAVIContentType(const int32_t handle, const int32_t contentType) = 0;
        virtual uint32_t SetAVIScanInformation(const int32_t handle, const int32_t scanInfo) = 0;

    };
} // namespace dDisplay
} // namespace hal