/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include "dDisplay.h"

#ifdef LOG_PRI
#undef LOG_PRI
#endif

#include <com/rdk/hal/hdmioutput/BnHDMIOutputControllerListener.h>
#include <com/rdk/hal/hdmioutput/BnHDMIOutputEventListener.h>
#include <com/rdk/hal/hdmioutput/IHDMIOutput.h>
#include <com/rdk/hal/hdmioutput/IHDMIOutputController.h>
#include <com/rdk/hal/hdmioutput/IHDMIOutputManager.h>
#include <com/rdk/hal/panel/IFactoryPanel.h>
#include <com/rdk/hal/panel/IPanelOutput.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>

// Implements the Display HAL Platform interface (see dDisplay.h) on top of the
// com.rdk.hal.hdmioutput and com.rdk.hal.panel AIDL HALs. API mapping follows
// "DS Display mapping.pdf":
//   dsDisplayInit/Term             -> IPanelOutput.getFactoryInterface() (+ HDMI output open/close)
//   dsGetDisplay                   -> IHDMIOutputManager.getHDMIOutputIds()/getHDMIOutput()
//   dsGetEDID/dsGetEDIDBytes       -> IHDMIOutputControllerListener.onEDID() (cached, parsed here)
//   dsGetDisplayAspectRatio        -> IHDMIOutput.getProperty(VIC)
//   dsSet/GetAVIContentType        -> IHDMIOutput(.Controller).get/setProperty(CONTENT_TYPE)
//   dsSet/GetAVIScanInformation    -> IHDMIOutput(.Controller).get/setProperty(SCAN_INFORMATION)
//   dsRegisterDisplayEventCallback -> IHDMIOutputEventListener.onStateChanged(STARTED/STOPPING),
//                                     IHDMIOutputControllerListener.onHDCPStatusChanged()
//   dsSetAllmEnabled               -> not defined in the AIDL HAL (rdk-halif-aidl issue #625)
class dDisplayAIDLImpl : public hal::dDisplay::IPlatform {
public:
    dDisplayAIDLImpl();
    ~dDisplayAIDLImpl() override;

    dDisplayAIDLImpl(const dDisplayAIDLImpl&) = delete;
    dDisplayAIDLImpl& operator=(const dDisplayAIDLImpl&) = delete;

    void InitialiseHAL() override;
    void DeInitialiseHAL() override;
    void setAllCallbacks(const CallbackBundle& bundle) override;
    void getPersistenceValue() override;

    uint32_t GetConnectedVideoDisplay(const int32_t videoPortHandle, bool& isConnected) override;
    uint32_t GetDisplaySurroundMode(const int32_t videoPortHandle, VideoPortSurroundMode& surroundMode) override;
    uint32_t GetDisplayEDID(const int32_t videoPortHandle, uint8_t edidBytes[], const uint16_t edidBytesLength) override;

    uint32_t GetDisplay(const int32_t type, const int32_t index, int32_t& handle) override;
    uint32_t GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio& aspectRatio) override;
    uint32_t GetDisplayEdid(const int32_t handle, DisplayEDID& edId, IDSVideoPortResolutionIterator*& supportedResolutionList) override;
    uint32_t GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength) override;
    uint32_t SetAllmEnabled(const int32_t handle, const bool enabled) override;
    uint32_t SetAVIContentType(const int32_t handle, const int32_t contentType) override;
    uint32_t SetAVIScanInformation(const int32_t handle, const int32_t scanInfo) override;

private:
    class ControllerListener;
    class EventListener;

    using Manager = com::rdk::hal::hdmioutput::IHDMIOutputManager;
    using Output = com::rdk::hal::hdmioutput::IHDMIOutput;
    using Controller = com::rdk::hal::hdmioutput::IHDMIOutputController;
    using PanelOutput = com::rdk::hal::panel::IPanelOutput;
    using FactoryPanel = com::rdk::hal::panel::IFactoryPanel;

    // Handle exposed to callers is the HDMI output Id value.
    struct Device {
        Output::Id id;
        android::sp<Output> output;
        android::sp<Controller> controller;
        android::sp<ControllerListener> controllerListener;
        android::sp<EventListener> eventListener;
        // Only stop the output on close if this instance was the one that started it.
        bool startedByUs = false;
    };

    static android::sp<Manager> GetManager();
    bool OpenDevice(const Output::Id& id, Device& device);
    static void CloseDevice(Device& device);
    // Returns a copy so Binder calls are made without holding _adminLock.
    bool FindDevice(const int32_t handle, Device& device) const;

    // Returns the cached EDID, waiting briefly for onEDID() when a sink is connected but the EDID has not arrived yet.
    bool GetEdid(const Device& device, std::vector<uint8_t>& edid);
    uint32_t CopyEdidBytes(const int32_t handle, uint8_t* edidBytes, const uint16_t edidBytesLength);

    void OnHotPlugDetectStateChanged(const int32_t handle, bool connected);
    void OnHDCPStatusChanged(const int32_t handle, com::rdk::hal::hdmioutput::HDCPStatus status, com::rdk::hal::hdmioutput::HDCPProtocolVersion version);
    void OnEDID(const int32_t handle, const std::vector<uint8_t>& edid);
    void OnStateChanged(const int32_t handle, com::rdk::hal::hdmioutput::State oldState, com::rdk::hal::hdmioutput::State newState);

    void NotifyHotPlug(const int32_t handle, bool connected);

    mutable std::mutex _adminLock;
    std::mutex _callbackLock;
    CallbackBundle _callbacks;
    android::sp<Manager> _manager;
    android::sp<PanelOutput> _panel;
    android::sp<FactoryPanel> _factoryPanel;
    std::vector<Device> _devices;
    std::atomic<bool> _opened;

    std::mutex _edidLock;
    std::condition_variable _edidCv;
    std::map<int32_t, std::vector<uint8_t>> _edidCache;
};
