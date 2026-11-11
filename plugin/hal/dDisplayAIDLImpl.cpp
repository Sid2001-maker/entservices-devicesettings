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
#include "dDisplayAIDL.h"
#include "DeviceSettingsTypes.h"
#include "DeviceSettingsHALConfig.h"
#include "DeviceSettingsHdmiStatus.h"
#include <dsDisplay.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <com/com.h>
#include <com/rdk/hal/PropertyValue.h>
#include <com/rdk/hal/hdmioutput/ContentType.h>
#include <com/rdk/hal/hdmioutput/HDCPProtocolVersion.h>
#include <com/rdk/hal/hdmioutput/HDCPStatus.h>
#include <com/rdk/hal/hdmioutput/Property.h>
#include <com/rdk/hal/hdmioutput/ScanInformation.h>
#include <com/rdk/hal/hdmioutput/State.h>
#include <com/rdk/hal/hdmioutput/VIC.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <optional>
#include <string>

namespace HdmiOutput = com::rdk::hal::hdmioutput;
using PropertyValue = com::rdk::hal::PropertyValue;

namespace {
constexpr size_t kEdidBlockSize = 128;
constexpr std::chrono::milliseconds kEdidWait { 1000 };

template <typename Interface>
android::sp<Interface> GetService()
{
    const android::sp<android::IServiceManager> serviceManager = android::defaultServiceManager();
    if (serviceManager == nullptr) {
        return nullptr;
    }
    const android::sp<android::IBinder> binder = serviceManager->checkService(
        android::String16(Interface::serviceName().c_str()));
    return binder == nullptr ? nullptr : android::interface_cast<Interface>(binder);
}

using Res = DisplayTVResolution;
using Ar = DisplayVideoAspectRatio;
using Fr = DisplayInVideoFrameRate;

// CTA-861 VICs (as signalled by the sink's Short Video Descriptors) that the Display interface can express.
// Names follow dVideoPortAIDLImpl so EDID resolutions line up with the platform resolution list.
struct VicEntry {
    HdmiOutput::VIC vic;
    const char* name;
    DisplayTVResolution resolution;
    DisplayVideoAspectRatio aspectRatio;
    DisplayInVideoFrameRate frameRate;
    bool interlaced;
};

const VicEntry kVicTable[] = {
    { HdmiOutput::VIC::VIC2_720_480_P_60_4_3,        "480p",    Res::DS_DISPLAY_RESOLUTION_480P,    Ar::DS_DISPLAY_ASPECT_RATIO_4X3,  Fr::DS_DISPLAY_FRAMERATE_60, false },
    { HdmiOutput::VIC::VIC3_720_480_P_60_16_9,       "480p",    Res::DS_DISPLAY_RESOLUTION_480P,    Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, false },
    { HdmiOutput::VIC::VIC6_720_1440_480_I_60_4_3,   "480i",    Res::DS_DISPLAY_RESOLUTION_480I,    Ar::DS_DISPLAY_ASPECT_RATIO_4X3,  Fr::DS_DISPLAY_FRAMERATE_60, true  },
    { HdmiOutput::VIC::VIC7_720_1440_480_I_60_16_9,  "480i",    Res::DS_DISPLAY_RESOLUTION_480I,    Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, true  },
    { HdmiOutput::VIC::VIC17_720_576_P_50_4_3,       "576p50",  Res::DS_DISPLAY_RESOLUTION_576P50,  Ar::DS_DISPLAY_ASPECT_RATIO_4X3,  Fr::DS_DISPLAY_FRAMERATE_50, false },
    { HdmiOutput::VIC::VIC18_720_576_P_50_16_9,      "576p50",  Res::DS_DISPLAY_RESOLUTION_576P50,  Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, false },
    { HdmiOutput::VIC::VIC21_720_1440_576_I_50_4_3,  "576i",    Res::DS_DISPLAY_RESOLUTION_576I,    Ar::DS_DISPLAY_ASPECT_RATIO_4X3,  Fr::DS_DISPLAY_FRAMERATE_50, true  },
    { HdmiOutput::VIC::VIC22_720_1440_576_I_50_16_9, "576i",    Res::DS_DISPLAY_RESOLUTION_576I,    Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, true  },
    { HdmiOutput::VIC::VIC4_1280_720_P_60_16_9,      "720p",    Res::DS_DISPLAY_RESOLUTION_720P,    Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, false },
    { HdmiOutput::VIC::VIC19_1280_720_P_50_16_9,     "720p50",  Res::DS_DISPLAY_RESOLUTION_720P50,  Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, false },
    { HdmiOutput::VIC::VIC5_1920_1080_I_60_16_9,     "1080i",   Res::DS_DISPLAY_RESOLUTION_1080I,   Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, true  },
    { HdmiOutput::VIC::VIC20_1920_1080_I_50_16_9,    "1080i50", Res::DS_DISPLAY_RESOLUTION_1080I50, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, true  },
    { HdmiOutput::VIC::VIC16_1920_1080_P_60_16_9,    "1080p",   Res::DS_DISPLAY_RESOLUTION_1080P,   Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, false },
    { HdmiOutput::VIC::VIC32_1920_1080_P_24_16_9,    "1080p24", Res::DS_DISPLAY_RESOLUTION_1080P24, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_24, false },
    { HdmiOutput::VIC::VIC34_1920_1080_P_30_16_9,    "1080p30", Res::DS_DISPLAY_RESOLUTION_1080P30, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_30, false },
    { HdmiOutput::VIC::VIC31_1920_1080_P_50_16_9,    "1080p50", Res::DS_DISPLAY_RESOLUTION_1080P50, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, false },
    { HdmiOutput::VIC::VIC93_3840_2160_P_24_16_9,    "2160p24", Res::DS_DISPLAY_RESOLUTION_2160P24, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_24, false },
    { HdmiOutput::VIC::VIC94_3840_2160_P_25_16_9,    "2160p25", Res::DS_DISPLAY_RESOLUTION_2160P25, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_25, false },
    { HdmiOutput::VIC::VIC95_3840_2160_P_30_16_9,    "2160p30", Res::DS_DISPLAY_RESOLUTION_2160P30, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_30, false },
    { HdmiOutput::VIC::VIC96_3840_2160_P_50_16_9,    "2160p50", Res::DS_DISPLAY_RESOLUTION_2160P50, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_50, false },
    { HdmiOutput::VIC::VIC97_3840_2160_P_60_16_9,    "2160p",   Res::DS_DISPLAY_RESOLUTION_2160P60, Ar::DS_DISPLAY_ASPECT_RATIO_16X9, Fr::DS_DISPLAY_FRAMERATE_60, false },
};

const VicEntry* FindVic(const int32_t vic)
{
    for (const auto& entry : kVicTable) {
        if (static_cast<int32_t>(entry.vic) == vic) {
            return &entry;
        }
    }
    return nullptr;
}

// The 60 Hz progressive modes are named "1080p"/"2160p" here, but a platform may configure them as "1080p60"/"2160p60".
bool ResolutionNamesMatch(const std::string& platformName, const std::string& edidName)
{
    return platformName == edidName || platformName == edidName + "60";
}

int32_t ExtractIntProperty(const std::optional<PropertyValue>& value, int32_t defaultValue)
{
    if (!value.has_value() || !value->value.has_value() || value->value->getTag() != PropertyValue::Value::intValue) {
        return defaultValue;
    }
    return value->value->get<PropertyValue::Value::intValue>();
}

bool GetIntProperty(const android::sp<HdmiOutput::IHDMIOutput>& output, const HdmiOutput::Property property, int32_t& result)
{
    std::optional<PropertyValue> value;
    if (!output->getProperty(property, &value).isOk() || !value.has_value()) {
        return false;
    }
    result = ExtractIntProperty(value, 0);
    return true;
}

bool SetIntProperty(const android::sp<HdmiOutput::IHDMIOutputController>& controller, const HdmiOutput::Property property, const int32_t newValue)
{
    PropertyValue value;
    value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(newValue);
    bool applied = false;
    return controller->setProperty(property, value, &applied).isOk() && applied;
}

bool ToAidlContentType(const int32_t contentType, HdmiOutput::ContentType& result)
{
    switch (contentType) {
    case DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_GRAPHICS:      result = HdmiOutput::ContentType::GRAPHICS;    return true;
    case DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_PHOTO:         result = HdmiOutput::ContentType::PHOTO;       return true;
    case DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_CINEMA:        result = HdmiOutput::ContentType::CINEMA;      return true;
    case DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_GAME:          result = HdmiOutput::ContentType::GAME;        return true;
    case DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_NOT_SIGNALLED: result = HdmiOutput::ContentType::UNSPECIFIED; return true;
    default:                                                          return false;
    }
}

bool ToAidlScanInformation(const int32_t scanInfo, HdmiOutput::ScanInformation& result)
{
    switch (scanInfo) {
    case DisplayAVIScanInformation::DS_DISPLAY_AVI_SCAN_NO_DATA:   result = HdmiOutput::ScanInformation::NO_DATA;   return true;
    case DisplayAVIScanInformation::DS_DISPLAY_AVI_SCAN_OVERSCAN:  result = HdmiOutput::ScanInformation::OVERSCAN;  return true;
    case DisplayAVIScanInformation::DS_DISPLAY_AVI_SCAN_UNDERSCAN: result = HdmiOutput::ScanInformation::UNDERSCAN; return true;
    default:                                                       return false;
    }
}

dsHdcpStatus_t ToDsHdcpStatus(const HdmiOutput::HDCPStatus status)
{
    switch (status) {
    case HdmiOutput::HDCPStatus::UNAUTHENTICATED:            return dsHDCP_STATUS_UNAUTHENTICATED;
    case HdmiOutput::HDCPStatus::AUTHENTICATION_IN_PROGRESS: return dsHDCP_STATUS_INPROGRESS;
    case HdmiOutput::HDCPStatus::AUTHENTICATION_FAILURE:     return dsHDCP_STATUS_AUTHENTICATIONFAILURE;
    case HdmiOutput::HDCPStatus::AUTHENTICATED:              return dsHDCP_STATUS_AUTHENTICATED;
    default:                                                 return dsHDCP_STATUS_UNPOWERED;
    }
}

struct ParsedEdid {
    DisplayEDID edid {};
    std::vector<DisplayVideoPortResolution> resolutions;
};

// Parses the base block and the CTA-861 extension blocks (HDMI VSDB + Short Video Descriptors) of a raw EDID.
bool ParseEdid(const std::vector<uint8_t>& raw, ParsedEdid& parsed)
{
    static const uint8_t kHeader[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    if (raw.size() < kEdidBlockSize || std::memcmp(raw.data(), kHeader, sizeof(kHeader)) != 0) {
        return false;
    }

    DisplayEDID& edid = parsed.edid;
    edid.productCode = static_cast<int32_t>(raw[10] | (raw[11] << 8));
    edid.serialNumber = static_cast<int32_t>(static_cast<uint32_t>(raw[12]) | (static_cast<uint32_t>(raw[13]) << 8)
        | (static_cast<uint32_t>(raw[14]) << 16) | (static_cast<uint32_t>(raw[15]) << 24));
    edid.manufactureWeek = raw[16];
    edid.manufactureYear = 1990 + raw[17];
    edid.hdmiDeviceType = false;
    // Repeater status is not carried in the EDID.
    edid.isRepeater = false;

    static const size_t kDescriptorOffsets[] = { 54, 72, 90, 108 };
    for (const size_t offset : kDescriptorOffsets) {
        if (raw[offset] == 0 && raw[offset + 1] == 0 && raw[offset + 2] == 0 && raw[offset + 3] == 0xFC) {
            std::string name;
            for (size_t i = offset + 5; i < offset + 18 && raw[i] != 0x0A; ++i) {
                name.push_back(static_cast<char>(raw[i]));
            }
            while (!name.empty() && name.back() == ' ') {
                name.pop_back();
            }
            edid.monitorName = name;
            break;
        }
    }

    const size_t extensions = raw[126];
    for (size_t ext = 1; ext <= extensions && (ext + 1) * kEdidBlockSize <= raw.size(); ++ext) {
        const uint8_t* block = raw.data() + ext * kEdidBlockSize;
        const size_t dataEnd = block[2];
        if (block[0] != 0x02 || dataEnd < 4 || dataEnd > kEdidBlockSize - 1) {
            continue;
        }
        for (size_t pos = 4; pos < dataEnd;) {
            const uint8_t tag = block[pos] >> 5;
            const size_t length = block[pos] & 0x1F;
            if (pos + 1 + length > dataEnd) {
                break;
            }
            const uint8_t* payload = block + pos + 1;
            if (tag == 0x02) {
                for (size_t i = 0; i < length; ++i) {
                    const int32_t code = payload[i] & 0x7F;
                    const int32_t vic = code <= 64 ? code : payload[i];
                    const VicEntry* entry = FindVic(vic);
                    if (entry == nullptr) {
                        continue;
                    }
                    bool duplicate = false;
                    for (const auto& existing : parsed.resolutions) {
                        if (existing.name == entry->name) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        DisplayVideoPortResolution resolution;
                        resolution.name = entry->name;
                        resolution.pixelResolution = entry->resolution;
                        resolution.aspectRatio = entry->aspectRatio;
                        resolution.stereoScopicMode = DisplayInVideoStereoScopicMode::DS_DISPLAY_SSMODE_2D;
                        resolution.frameRate = entry->frameRate;
                        resolution.interlaced = entry->interlaced;
                        parsed.resolutions.push_back(resolution);
                    }
                }
            } else if (tag == 0x03 && length >= 3) {
                // IEEE OUI is stored least significant byte first: 00-0C-03 (HDMI) and C4-5D-D8 (HDMI Forum).
                if (payload[0] == 0x03 && payload[1] == 0x0C && payload[2] == 0x00) {
                    edid.hdmiDeviceType = true;
                    if (length >= 5) {
                        edid.physicalAddressA = payload[3] >> 4;
                        edid.physicalAddressB = payload[3] & 0x0F;
                        edid.physicalAddressC = payload[4] >> 4;
                        edid.physicalAddressD = payload[4] & 0x0F;
                    }
                } else if (payload[0] == 0xD8 && payload[1] == 0x5D && payload[2] == 0xC4) {
                    edid.hdmiDeviceType = true;
                }
            }
            pos += 1 + length;
        }
    }

    // Narrow the sink's list down to resolutions the platform can output, as the legacy HAL path does.
    std::vector<VideoPortResolution> platformResolutions;
    DeviceSettingsHAL::PopulateVideoPortResolutionConfig(VideoPortType::DS_VIDEO_PORT_TYPE_HDMI, platformResolutions);
    if (platformResolutions.empty()) {
        DSLOG_WARN("No platform resolution configuration; EDID resolution list left unfiltered");
    } else {
        std::vector<DisplayVideoPortResolution> filtered;
        for (const auto& platformResolution : platformResolutions) {
            for (const auto& resolution : parsed.resolutions) {
                if (ResolutionNamesMatch(platformResolution.name, resolution.name)) {
                    filtered.push_back(resolution);
                }
            }
        }
        parsed.resolutions.swap(filtered);
    }
    edid.numOfSupportedResolution = static_cast<int32_t>(parsed.resolutions.size());
    return true;
}

IDSVideoPortResolutionIterator* BuildResolutionIterator(const std::vector<DisplayVideoPortResolution>& resolutions)
{
    if (resolutions.empty()) {
        return nullptr;
    }
    using ResolutionIterator = WPEFramework::RPC::IteratorType<IDSVideoPortResolutionIterator>;
    return WPEFramework::Core::Service<ResolutionIterator>::Create<IDSVideoPortResolutionIterator>(resolutions);
}
} // namespace

class dDisplayAIDLImpl::ControllerListener : public HdmiOutput::BnHDMIOutputControllerListener {
public:
    ControllerListener(dDisplayAIDLImpl& parent, const int32_t handle)
        : _parent(&parent)
        , _handle(handle)
    {
    }

    // Called before the owner goes away so late Binder callbacks are dropped.
    void Detach() { _parent.store(nullptr); }

    android::binder::Status onHotPlugDetectStateChanged(bool state) override
    {
        if (dDisplayAIDLImpl* parent = _parent.load()) {
            parent->OnHotPlugDetectStateChanged(_handle, state);
        }
        return android::binder::Status::ok();
    }

    android::binder::Status onFrameRateChanged() override { return android::binder::Status::ok(); }

    android::binder::Status onHDCPStatusChanged(HdmiOutput::HDCPStatus hdcpStatus, HdmiOutput::HDCPProtocolVersion hdcpProtocolVersion) override
    {
        if (dDisplayAIDLImpl* parent = _parent.load()) {
            parent->OnHDCPStatusChanged(_handle, hdcpStatus, hdcpProtocolVersion);
        }
        return android::binder::Status::ok();
    }

    android::binder::Status onEDID(const std::vector<uint8_t>& edid) override
    {
        if (dDisplayAIDLImpl* parent = _parent.load()) {
            parent->OnEDID(_handle, edid);
        }
        return android::binder::Status::ok();
    }

private:
    std::atomic<dDisplayAIDLImpl*> _parent;
    const int32_t _handle;
};

class dDisplayAIDLImpl::EventListener : public HdmiOutput::BnHDMIOutputEventListener {
public:
    EventListener(dDisplayAIDLImpl& parent, const int32_t handle)
        : _parent(&parent)
        , _handle(handle)
    {
    }

    void Detach() { _parent.store(nullptr); }

    android::binder::Status onStateChanged(HdmiOutput::State oldState, HdmiOutput::State newState) override
    {
        if (dDisplayAIDLImpl* parent = _parent.load()) {
            parent->OnStateChanged(_handle, oldState, newState);
        }
        return android::binder::Status::ok();
    }

private:
    std::atomic<dDisplayAIDLImpl*> _parent;
    const int32_t _handle;
};

dDisplayAIDLImpl::dDisplayAIDLImpl()
    : _opened(false)
{
    InitialiseHAL();
}

dDisplayAIDLImpl::~dDisplayAIDLImpl()
{
    DeInitialiseHAL();
}

android::sp<dDisplayAIDLImpl::Manager> dDisplayAIDLImpl::GetManager()
{
    return GetService<Manager>();
}

bool dDisplayAIDLImpl::OpenDevice(const Output::Id& id, Device& device)
{
    device.id = id;
    if (!_manager->getHDMIOutput(id, &device.output).isOk() || device.output == nullptr) {
        DSLOG_ERR("getHDMIOutput failed for id=%d", id.value);
        return false;
    }

    device.controllerListener = new ControllerListener(*this, id.value);
    device.eventListener = new EventListener(*this, id.value);

    // Registered before start() so the STARTED transition is observed.
    bool registered = false;
    if (!device.output->registerEventListener(device.eventListener, &registered).isOk() || !registered) {
        DSLOG_ERR("registerEventListener failed on HDMI output id=%d", id.value);
    }

    if (!device.output->open(device.controllerListener, &device.controller).isOk() || device.controller == nullptr) {
        DSLOG_ERR("open() failed on HDMI output id=%d", id.value);
        CloseDevice(device);
        return false;
    }

    HdmiOutput::State state = HdmiOutput::State::UNKNOWN;
    if (!device.output->getState(&state).isOk()) {
        DSLOG_ERR("getState failed on HDMI output id=%d", id.value);
        CloseDevice(device);
        return false;
    }
    // start() is only valid from READY; another client may already have started the output.
    if (state == HdmiOutput::State::READY) {
        if (!device.controller->start().isOk()) {
            DSLOG_ERR("HDMIOutputController::start() failed for id=%d", id.value);
            CloseDevice(device);
            return false;
        }
        device.startedByUs = true;
    }
    return true;
}

void dDisplayAIDLImpl::CloseDevice(Device& device)
{
    if (device.controllerListener != nullptr) {
        device.controllerListener->Detach();
    }
    if (device.eventListener != nullptr) {
        device.eventListener->Detach();
    }

    if (device.output != nullptr) {
        // stop() needs STARTED and close() needs READY; other states raise EX_ILLEGAL_STATE.
        HdmiOutput::State state = HdmiOutput::State::UNKNOWN;
        if (!device.output->getState(&state).isOk()) {
            state = HdmiOutput::State::UNKNOWN;
        }
        if (device.controller != nullptr) {
            if (state == HdmiOutput::State::STARTED && device.startedByUs) {
                device.controller->stop();
                state = HdmiOutput::State::READY;
            }
            if (state == HdmiOutput::State::READY) {
                bool closed = false;
                device.output->close(device.controller, &closed);
            }
        }
        if (device.eventListener != nullptr) {
            bool unregistered = false;
            device.output->unregisterEventListener(device.eventListener, &unregistered);
        }
    }
    device.controller.clear();
    device.controllerListener.clear();
    device.eventListener.clear();
    device.output.clear();
    device.startedByUs = false;
}

bool dDisplayAIDLImpl::FindDevice(const int32_t handle, Device& device) const
{
    if (!_opened) {
        return false;
    }
    std::lock_guard<std::mutex> lock(_adminLock);
    for (const auto& candidate : _devices) {
        if (candidate.id.value == handle) {
            device = candidate;
            return true;
        }
    }
    return false;
}

void dDisplayAIDLImpl::InitialiseHAL()
{
    std::lock_guard<std::mutex> lock(_adminLock);
    if (_opened) {
        return;
    }

    _manager = GetManager();
    if (_manager == nullptr) {
        DSLOG_ERR("HDMI Output AIDL service is not available");
        return;
    }
    android::ProcessState::self()->startThreadPool();

    // Mapping: dsDisplayInit -> IPanelOutput.getFactoryInterface(). The panel is not needed for the HDMI paths below.
    _panel = GetService<PanelOutput>();
    if (_panel == nullptr) {
        DSLOG_WARN("Panel Output AIDL service is not available");
    } else if (!_panel->getFactoryInterface(&_factoryPanel).isOk() || _factoryPanel == nullptr) {
        DSLOG_WARN("IPanelOutput.getFactoryInterface() returned no factory interface");
        _factoryPanel.clear();
    }

    std::vector<Output::Id> ids;
    if (!_manager->getHDMIOutputIds(&ids).isOk() || ids.empty()) {
        DSLOG_ERR("getHDMIOutputIds failed or returned no HDMI outputs");
        _factoryPanel.clear();
        _panel.clear();
        _manager.clear();
        return;
    }

    _devices.reserve(ids.size());
    for (const auto& id : ids) {
        Device device;
        if (OpenDevice(id, device)) {
            _devices.push_back(std::move(device));
            DSLOG_INFO("HDMI Output AIDL HAL initialized for Display (outputId=%d)", id.value);
        }
    }

    if (_devices.empty()) {
        _factoryPanel.clear();
        _panel.clear();
        _manager.clear();
        return;
    }
    DSLOG_INFO("%zu of %zu HDMI outputs opened", _devices.size(), ids.size());
    getPersistenceValue();
    _opened = true;
}

void dDisplayAIDLImpl::DeInitialiseHAL()
{
    {
        std::lock_guard<std::mutex> lock(_adminLock);
        _opened = false;
        for (auto& device : _devices) {
            CloseDevice(device);
        }
        _devices.clear();
        _factoryPanel.clear();
        _panel.clear();
        _manager.clear();
    }
    std::lock_guard<std::mutex> lock(_edidLock);
    _edidCache.clear();
}

void dDisplayAIDLImpl::setAllCallbacks(const CallbackBundle& bundle)
{
    std::lock_guard<std::mutex> lock(_callbackLock);
    _callbacks = bundle;
}

void dDisplayAIDLImpl::getPersistenceValue()
{
    DSLOG_INFO("Display has no persisted settings");
}

uint32_t dDisplayAIDLImpl::GetConnectedVideoDisplay(const int32_t videoPortHandle, bool& isConnected)
{
    Device device;
    if (!FindDevice(videoPortHandle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    if (!device.controller->getHotPlugDetectState(&isConnected).isOk()) {
        return WPEFramework::Core::ERROR_GENERAL;
    }
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::GetDisplaySurroundMode(const int32_t videoPortHandle, VideoPortSurroundMode& surroundMode)
{
    (void)videoPortHandle;
    // Not part of the mapping: IHDMIOutput does not expose sink audio capabilities.
    surroundMode = VideoPortSurroundMode::DS_VIDEO_PORT_SURROUNDMODE_NONE;
    return WPEFramework::Core::ERROR_UNAVAILABLE;
}

uint32_t dDisplayAIDLImpl::GetDisplayEDID(const int32_t videoPortHandle, uint8_t edidBytes[], const uint16_t edidBytesLength)
{
    return CopyEdidBytes(videoPortHandle, edidBytes, edidBytesLength);
}

uint32_t dDisplayAIDLImpl::GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength)
{
    return CopyEdidBytes(handle, edIdBytes, edidLength);
}

uint32_t dDisplayAIDLImpl::CopyEdidBytes(const int32_t handle, uint8_t* edidBytes, const uint16_t edidBytesLength)
{
    if (edidBytes == nullptr || edidBytesLength == 0) {
        DSLOG_ERR(" FAILED - Invalid parameters");
        return WPEFramework::Core::ERROR_GENERAL;
    }
    Device device;
    if (!FindDevice(handle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    std::vector<uint8_t> edid;
    if (!GetEdid(device, edid)) {
        DSLOG_ERR(" No EDID available for handle=%d", handle);
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    if (edid.size() > edidBytesLength) {
        DSLOG_ERR(" EDID length %zu exceeds buffer length %u", edid.size(), static_cast<unsigned>(edidBytesLength));
        return WPEFramework::Core::ERROR_GENERAL;
    }
    std::memcpy(edidBytes, edid.data(), edid.size());
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::GetDisplay(const int32_t type, const int32_t index, int32_t& handle)
{
    DSLOG_INFO(" type=%d, index=%d", type, index);
    if (type < 0 || index < 0) {
        return WPEFramework::Core::ERROR_BAD_REQUEST;
    }
    handle = -1;

    // Only HDMI/INTERNAL outputs are backed by the AIDL HAL.
    if (type != static_cast<int32_t>(DisplayPortType::DS_DISPLAY_PORT_TYPE_HDMI)
        && type != static_cast<int32_t>(DisplayPortType::DS_DISPLAY_PORT_TYPE_INTERNAL)) {
        DSLOG_WARN(" not supported for portType=%d", type);
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }

    std::lock_guard<std::mutex> lock(_adminLock);
    if (!_opened || static_cast<size_t>(index) >= _devices.size()) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    handle = _devices[index].id.value;
    DSLOG_INFO(" SUCCESS - handle=%d", handle);
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio& aspectRatio)
{
    Device device;
    if (!FindDevice(handle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    int32_t vic = 0;
    if (!GetIntProperty(device.output, HdmiOutput::Property::VIC, vic)) {
        DSLOG_ERR(" getProperty(VIC) failed for handle=%d", handle);
        return WPEFramework::Core::ERROR_GENERAL;
    }

    // VIC enumerator names end with the aspect ratio, e.g. VIC1_640_480_P_60_4_3.
    const std::string name = HdmiOutput::toString(static_cast<HdmiOutput::VIC>(vic));
    const std::string suffix = "_4_3";
    const bool is4x3 = name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    aspectRatio = is4x3 ? DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_4X3
                        : DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_16X9;
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::GetDisplayEdid(const int32_t handle, DisplayEDID& edId, IDSVideoPortResolutionIterator*& supportedResolutionList)
{
    supportedResolutionList = nullptr;
    Device device;
    if (!FindDevice(handle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    std::vector<uint8_t> raw;
    if (!GetEdid(device, raw)) {
        DSLOG_ERR(" No EDID available for handle=%d", handle);
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }
    ParsedEdid parsed;
    if (!ParseEdid(raw, parsed)) {
        DSLOG_ERR(" Invalid EDID (%zu bytes) for handle=%d", raw.size(), handle);
        return WPEFramework::Core::ERROR_GENERAL;
    }
    edId = parsed.edid;
    supportedResolutionList = BuildResolutionIterator(parsed.resolutions);
    DSLOG_INFO(" SUCCESS - monitor='%s', %d supported resolutions", edId.monitorName.c_str(), edId.numOfSupportedResolution);
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::SetAllmEnabled(const int32_t handle, const bool enabled)
{
    (void)handle;
    (void)enabled;
    DSLOG_WARN("ALLM control is not defined in the AIDL HAL (rdk-halif-aidl issue #625)");
    return WPEFramework::Core::ERROR_UNAVAILABLE;
}

uint32_t dDisplayAIDLImpl::SetAVIContentType(const int32_t handle, const int32_t contentType)
{
    DSLOG_INFO(" handle=%d, contentType=%d", handle, contentType);
    HdmiOutput::ContentType target;
    if (!ToAidlContentType(contentType, target)) {
        DSLOG_ERR(" invalid content type %d", contentType);
        return WPEFramework::Core::ERROR_BAD_REQUEST;
    }
    Device device;
    if (!FindDevice(handle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }

    int32_t current = 0;
    if (GetIntProperty(device.output, HdmiOutput::Property::CONTENT_TYPE, current) && current == static_cast<int32_t>(target)) {
        DSLOG_INFO(" HDMI AVI content type already set to %d", contentType);
        return WPEFramework::Core::ERROR_NONE;
    }
    if (!SetIntProperty(device.controller, HdmiOutput::Property::CONTENT_TYPE, static_cast<int32_t>(target))) {
        DSLOG_ERR(" setProperty(CONTENT_TYPE) failed");
        return WPEFramework::Core::ERROR_GENERAL;
    }
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t dDisplayAIDLImpl::SetAVIScanInformation(const int32_t handle, const int32_t scanInfo)
{
    DSLOG_INFO(" handle=%d, scanInfo=%d", handle, scanInfo);
    HdmiOutput::ScanInformation target;
    if (!ToAidlScanInformation(scanInfo, target)) {
        DSLOG_ERR(" invalid scan information %d", scanInfo);
        return WPEFramework::Core::ERROR_BAD_REQUEST;
    }
    Device device;
    if (!FindDevice(handle, device)) {
        return WPEFramework::Core::ERROR_UNAVAILABLE;
    }

    int32_t current = 0;
    if (GetIntProperty(device.output, HdmiOutput::Property::SCAN_INFORMATION, current) && current == static_cast<int32_t>(target)) {
        DSLOG_INFO(" HDMI AVI scan information already set to %d", scanInfo);
        return WPEFramework::Core::ERROR_NONE;
    }
    if (!SetIntProperty(device.controller, HdmiOutput::Property::SCAN_INFORMATION, static_cast<int32_t>(target))) {
        DSLOG_ERR(" setProperty(SCAN_INFORMATION) failed");
        return WPEFramework::Core::ERROR_GENERAL;
    }
    return WPEFramework::Core::ERROR_NONE;
}

bool dDisplayAIDLImpl::GetEdid(const Device& device, std::vector<uint8_t>& edid)
{
    const int32_t handle = device.id.value;
    std::unique_lock<std::mutex> lock(_edidLock);
    const auto cached = [this, handle]() {
        const auto it = _edidCache.find(handle);
        return it != _edidCache.end() && !it->second.empty();
    };

    if (!cached()) {
        // onEDID() follows the sink connection, so only wait if a sink is present.
        lock.unlock();
        bool connected = false;
        if (!device.controller->getHotPlugDetectState(&connected).isOk() || !connected) {
            return false;
        }
        lock.lock();
        _edidCv.wait_for(lock, kEdidWait, cached);
    }
    const auto it = _edidCache.find(handle);
    if (it == _edidCache.end() || it->second.empty()) {
        return false;
    }
    edid = it->second;
    return true;
}

void dDisplayAIDLImpl::NotifyHotPlug(const int32_t handle, bool connected)
{
    std::function<void(const uint8_t, const bool)> callback;
    {
        std::lock_guard<std::mutex> lock(_callbackLock);
        callback = _callbacks.OnDisplayHDMIHotPlug;
    }
    if (callback) {
        callback(static_cast<uint8_t>(handle & 0xFF), connected);
    }
}

void dDisplayAIDLImpl::OnHotPlugDetectStateChanged(const int32_t handle, bool connected)
{
    DSLOG_INFO("outputId=%d, connected=%s", handle, connected ? "true" : "false");
    if (!connected) {
        std::lock_guard<std::mutex> lock(_edidLock);
        _edidCache.erase(handle);
    }
}

void dDisplayAIDLImpl::OnEDID(const int32_t handle, const std::vector<uint8_t>& edid)
{
    DSLOG_INFO("outputId=%d, edid length=%zu", handle, edid.size());
    {
        std::lock_guard<std::mutex> lock(_edidLock);
        _edidCache[handle] = edid;
    }
    _edidCv.notify_all();
}

void dDisplayAIDLImpl::OnHDCPStatusChanged(const int32_t handle, HdmiOutput::HDCPStatus status, HdmiOutput::HDCPProtocolVersion version)
{
    DSLOG_INFO("outputId=%d, hdcpStatus=%d, version=%d", handle, static_cast<int>(status), static_cast<int>(version));
    const dsHdcpStatus_t dsStatus = ToDsHdcpStatus(status);
    // Same version rule as dVideoPortAIDLImpl so both listeners publish an identical value.
    const bool authenticated = (status == HdmiOutput::HDCPStatus::AUTHENTICATED);
    _dsSyncHdmiStatus(DS_HDMI_TAG_HDCPVERSION,
        (authenticated && version == HdmiOutput::HDCPProtocolVersion::VERSION_2_X) ? dsHDCP_VERSION_2X : dsHDCP_VERSION_1X);

    std::function<void(const uint8_t, const int32_t)> callback;
    {
        std::lock_guard<std::mutex> lock(_callbackLock);
        callback = _callbacks.OnDisplayHDCPStatus;
    }
    if (callback) {
        callback(static_cast<uint8_t>(handle & 0xFF), static_cast<int32_t>(dsStatus));
    }
}

void dDisplayAIDLImpl::OnStateChanged(const int32_t handle, HdmiOutput::State oldState, HdmiOutput::State newState)
{
    DSLOG_INFO("outputId=%d, oldState=%d, newState=%d", handle, static_cast<int>(oldState), static_cast<int>(newState));
    // The EDID cache is deliberately kept across STOPPING: onEDID() is not re-sent when the output is restarted.
    if (newState == HdmiOutput::State::STARTED) {
        _dsSyncHdmiStatus(DS_HDMI_TAG_HOTPLUP, dsDISPLAY_EVENT_CONNECTED);
        NotifyHotPlug(handle, true);
    } else if (newState == HdmiOutput::State::STOPPING) {
        _dsSyncHdmiStatus(DS_HDMI_TAG_HOTPLUP, dsDISPLAY_EVENT_DISCONNECTED);
        NotifyHotPlug(handle, false);
    }
}
