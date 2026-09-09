"""
/**
 * @file TCID06_VideoStreamInfoUpdate.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID06_VideoStreamInfoUpdate
 * @details Registers videoStreamInfoUpdate, starts HDMI input on port 0, then
 *          drives the HDMI vComponent to change the source video format
 *          (1080p60 -> 2160p60) and verifies currentVideoMode tracks the change.
 *
 * @note Asynchronous videoStreamInfoUpdate capture requires the RAFT event
 *       listener; this curl-based case validates the observable state via
 *       currentVideoMode after each format stimulus.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - AVInput_Helpers.py
 *  - vcomponent_configurations/commands/HdmiInput_VideoFormat_*.yaml
 *
 * @expected_result
 *  - currentVideoMode differs between the two source formats.
 *
 * @pass_criteria
 *  - Both formats accepted and video mode changes and run_test() returns True.
 *
 * @failure_criteria
 *  - Stimulus rejected, mode does not update, or run_test() returns False.
 */
"""

import time
import os

from utils import (
    HDMIIN_CMD_BASE,
    send_curl_command,
    send_vcomponent_command,
    is_ok,
    log_info,
    log_success,
    log_error,
    log_warning,
)
import AVInput_Curl as AVInputApis
from AVInput_Helpers import parse_current_video_mode

PORT = 0


def _post_stimulus(yaml_file):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{yaml_file}")
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def _read_mode(timeout_seconds=8):
    deadline = time.time() + timeout_seconds
    mode = None
    while time.time() < deadline:
        resp = send_curl_command(AVInputApis.current_video_mode)
        log_warning(f"currentVideoMode response: {resp}")
        mode = parse_current_video_mode(resp)
        if mode:
            return mode
        time.sleep(1)
    return mode


def run_test():
    start_time = time.perf_counter()

    reg = send_curl_command(AVInputApis.register_event("videoStreamInfoUpdate", "ID_videoStreamInfoUpdate"))
    log_warning(f"register videoStreamInfoUpdate: {reg}")
    if not is_ok(reg):
        log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (failed to register videoStreamInfoUpdate)")
        return False

    try:
        start_resp = send_curl_command(AVInputApis.start_input(PORT))
        log_warning(f"startInput response: {start_resp}")
        if not is_ok(start_resp):
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (startInput not accepted)")
            return False

        log_info("Simulating source format 1080p60 on port 0")
        if not _post_stimulus("HdmiInput_VideoFormat_1080p60_Port0.yaml"):
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (1080p60 stimulus not accepted)")
            return False
        mode_1080 = _read_mode()
        if not mode_1080:
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (no video mode after 1080p60)")
            return False
        log_success(f"✅ Video mode after 1080p60: {mode_1080}")

        log_info("Simulating source format 2160p60 on port 0")
        if not _post_stimulus("HdmiInput_VideoFormat_2160p60_Port0.yaml"):
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (2160p60 stimulus not accepted)")
            return False
        mode_2160 = _read_mode()
        if not mode_2160:
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (no video mode after 2160p60)")
            return False
        log_success(f"✅ Video mode after 2160p60: {mode_2160}")

        if mode_1080 == mode_2160:
            log_error("TCID06_VideoStreamInfoUpdate Failed ❌ (video mode did not update on format change)")
            return False
    finally:
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID06_VideoStreamInfoUpdate Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
