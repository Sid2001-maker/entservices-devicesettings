"""
AIDL HDMI Input callback and format-mapping coverage test.

The virtual HDMI controller injects the same events a physical source would
produce, allowing the DeviceSettings AIDL listener paths to be exercised on a
headless vDevice.
"""

import time

import AVInput_Curl as AVInputApis
from utils import (
    is_ok,
    log_error,
    log_info,
    log_success,
    log_warning,
    send_curl_command,
    send_vcomponent_payload,
)

PORT = 0


def _post(command, **params):
    http_code, body = send_vcomponent_payload(command, {"port": PORT, **params})
    log_warning(f"vComponent {command}: HTTP {http_code}, body={body!r}")
    if not 200 <= http_code < 300:
        log_error(f"vComponent rejected {command}: HTTP {http_code}")
        return False
    time.sleep(0.2)
    return True


def _avi_frame(content_type=None):
    frame = [0x82, 0x02, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    if content_type is not None:
        frame[6] = 0x80
        frame[8] = content_type << 4
    return frame


def run_test():
    for event in (
        "onDevicesChanged",
        "onSignalChanged",
        "onInputStatusChanged",
        "aviContentTypeUpdate",
    ):
        response = send_curl_command(
            AVInputApis.register_event(event, f"ID_AIDL_{event}")
        )
        if not is_ok(response):
            log_warning(f"Event registration not available for {event}: {response}")

    start_response = send_curl_command(AVInputApis.start_input(PORT))
    if not is_ok(start_response):
        log_error(f"Unable to start HDMI input before AIDL stimulus: {start_response}")
        return False

    stimuli = [
        ("connection_status", {"connected": True}),
        ("signal_status", {"state": "NO_SIGNAL"}),
        ("signal_status", {"state": "UNSTABLE"}),
        ("signal_status", {"state": "NOT_SUPPORTED"}),
        ("signal_status", {"state": "LOCKED"}),
    ]

    # One VIC from every resolution/frame-rate branch, including 4:3 and interlaced.
    for vic in (
        "VIC1_640_480_P_60_4_3",
        "VIC17_720_576_P_50_4_3",
        "VIC4_1280_720_P_60_16_9",
        "VIC5_1920_1080_I_60_16_9",
        "VIC93_3840_2160_P_24_16_9",
        "VIC98_4096_2160_P_24_256_135",
        "VIC121_5120_2160_P_24_64_27",
        "VIC40_1920_1080_I_100_16_9",
        "VIC46_1920_1080_I_120_16_9",
        "VIC52_720_576_P_200_4_3",
        "VIC56_720_480_P_240_4_3",
        "VIC108_1280_720_P_48_16_9",
        "VIC61_1280_720_P_25_16_9",
        "VIC62_1280_720_P_30_16_9",
        "VIC205_7680_4320_P_48_64_27",
    ):
        stimuli.append(("videoformat_change", {"format": vic}))

    stimuli.extend([
        ("vrr_status", {
            "vrrActive": False, "M_CONST": False,
            "fastVActive": False, "frameRate": 0.0,
        }),
        ("vrr_status", {
            "vrrActive": True, "M_CONST": True,
            "fastVActive": False, "frameRate": 0.0,
        }),
        ("vrr_status", {
            "vrrActive": True, "M_CONST": True,
            "fastVActive": True, "frameRate": 120.0,
        }),
        ("aviinfo_frame", {"data": [0x82, 0x02]}),
        ("aviinfo_frame", {"data": [0x81, 0x02, 0x05] + [0x00] * 6}),
        ("aviinfo_frame", {"data": [0x82, 0x02, 0x04] + [0x00] * 6}),
        ("aviinfo_frame", {"data": [0x82, 0x02, 0x0D] + [0x00] * 6}),
        ("aviinfo_frame", {"data": _avi_frame()}),
    ])

    for content_type in range(4):
        stimuli.append(("aviinfo_frame", {"data": _avi_frame(content_type)}))

    stimuli.extend([
        ("audioinfo_frame", {
            "data": [0x84, 0x01, 0x0A, 0x70, 0x01, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
        }),
        ("spdinfo_frame", {
            "data": [0x83, 0x01, 0x19, 0x8E] + [0x00] * 24,
        }),
        ("drminfo_frame", {
            "data": [0x87, 0x01, 0x1A, 0x9A] + [0x00] * 26,
        }),
        ("vsifinfo_frame", {
            "data": [0x81, 0x01, 0x0D, 0xA7] + [0x00] * 13,
        }),
        ("hdcp_status", {
            "state": "AUTHENTICATED", "version": "VERSION_2_X",
        }),
    ])

    try:
        log_info(f"Injecting {len(stimuli)} AIDL HDMI events")
        for command, params in stimuli:
            if not _post(command, **params):
                return False

        send_curl_command(AVInputApis.current_video_mode)
        send_curl_command(AVInputApis.get_vrr_frame_rate(PORT))
        for label, command in (
            ("getSPD", AVInputApis.get_spd(PORT)),
            ("getRawSPD", AVInputApis.get_raw_spd(PORT)),
        ):
            response = send_curl_command(command)
            if not is_ok(response):
                log_error(f"{label} failed after SPD InfoFrame injection: {response}")
                return False

        stop_response = send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))
        if not is_ok(stop_response):
            log_error(f"Unable to stop HDMI input before state readback: {stop_response}")
            return False
        start_response = send_curl_command(AVInputApis.start_input(PORT))
        if not is_ok(start_response):
            log_error(f"Unable to restart HDMI input for state readback: {start_response}")
            return False
        if not is_ok(send_curl_command(AVInputApis.current_video_mode)):
            log_error("currentVideoMode failed after HDMI input restart")
            return False
    finally:
        _post("connection_status", connected=False)
        _post("signal_status", state="NO_SIGNAL")
        _post(
            "vrr_status",
            vrrActive=False,
            M_CONST=False,
            fastVActive=False,
            frameRate=0.0,
        )
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    log_success("TCID23_AidlEventCoverage Passed")
    return True
