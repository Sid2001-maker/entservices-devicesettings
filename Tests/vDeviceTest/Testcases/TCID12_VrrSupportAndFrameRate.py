"""
/**
 * @file TCID12_VrrSupportAndFrameRate.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID12_VrrSupportAndFrameRate
 * @details Selects EDID 2.0, enables VRR support in EDID on port 0 (gated on EDID
 *          2.0), verifies getVRRSupport, then drives the HDMI vComponent to report
 *          an active 120 Hz VRR stream and verifies getVRRFrameRate returns a
 *          plausible rate.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - AVInput_Helpers.py
 *  - vcomponent_configurations/commands/HdmiInput_VRR_Active_120_Port0.yaml
 *
 * @expected_result
 *  - VRR support round-trips true; getVRRFrameRate returns a positive rate.
 *
 * @pass_criteria
 *  - Support round-trip and frame-rate query succeed and run_test() returns True.
 *
 * @failure_criteria
 *  - Set/get mismatch, stimulus rejected, or run_test() returns False.
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
from AVInput_Helpers import result_success, parse_vrr_support, parse_vrr_frame_rate, parse_edid_version

PORT = 0


def _post_stimulus(yaml_file):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{yaml_file}")
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def run_test():
    start_time = time.perf_counter()

    original_version = parse_edid_version(send_curl_command(AVInputApis.get_edid_version(PORT)))

    try:
        ver_resp = send_curl_command(AVInputApis.set_edid_version(PORT, AVInputApis.EDID_VERSION_20))
        log_warning(f"setEdidVersion(HDMI2.0) response: {ver_resp}")
        if not result_success(ver_resp):
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (could not select EDID 2.0)")
            return False

        log_info("Enabling VRR support in EDID on port 0")
        set_resp = send_curl_command(AVInputApis.set_vrr_support(PORT, True))
        log_warning(f"setVRRSupport(true) response: {set_resp}")
        if not result_success(set_resp):
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (setVRRSupport not accepted)")
            return False

        get_resp = send_curl_command(AVInputApis.get_vrr_support(PORT))
        log_warning(f"getVRRSupport response: {get_resp}")
        if parse_vrr_support(get_resp) is not True:
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (VRR support not reported enabled)")
            return False
        log_success("✅ VRR support round-trip verified")

        start_resp = send_curl_command(AVInputApis.start_input(PORT))
        log_warning(f"startInput response: {start_resp}")
        if not is_ok(start_resp):
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (startInput not accepted)")
            return False

        log_info("Simulating active 120 Hz VRR stream on port 0")
        if not _post_stimulus("HdmiInput_VRR_Active_120_Port0.yaml"):
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (VRR stimulus not accepted)")
            return False

        rate = None
        deadline = time.time() + 10
        while time.time() < deadline:
            rate_resp = send_curl_command(AVInputApis.get_vrr_frame_rate(PORT))
            log_warning(f"getVRRFrameRate response: {rate_resp}")
            rate = parse_vrr_frame_rate(rate_resp)
            if isinstance(rate, float) and rate > 0:
                break
            time.sleep(1)

        if not (isinstance(rate, float) and rate > 0):
            log_error("TCID12_VrrSupportAndFrameRate Failed ❌ (no positive VRR frame rate reported)")
            return False
        log_success(f"✅ VRR frame rate reported: {rate} Hz")
    finally:
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))
        send_curl_command(AVInputApis.set_vrr_support(PORT, False))
        if original_version in (AVInputApis.EDID_VERSION_14, AVInputApis.EDID_VERSION_20):
            send_curl_command(AVInputApis.set_edid_version(PORT, original_version))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID12_VrrSupportAndFrameRate Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
