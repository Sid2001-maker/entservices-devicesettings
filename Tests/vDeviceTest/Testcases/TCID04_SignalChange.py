"""
/**
 * @file TCID04_SignalChange.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID04_SignalChange
 * @details Registers onSignalChanged and drives the HDMI vComponent through the
 *          signal states NO_SIGNAL -> UNSTABLE -> LOCKED on port 0. Verifies each
 *          stimulus is accepted and the plugin stays responsive; when the signal
 *          is LOCKED and the port is started, currentVideoMode returns a value.
 *
 * @note Asynchronous onSignalChanged notification capture requires the RAFT event
 *       listener; this curl-based case validates stimulus acceptance and the
 *       observable plugin state via currentVideoMode.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - vcomponent_configurations/commands/HdmiInput_Signal_*.yaml
 *
 * @expected_result
 *  - All signal stimuli accepted; plugin responsive; LOCKED yields a video mode.
 *
 * @pass_criteria
 *  - All transitions accepted and plugin responsive and run_test() returns True.
 *
 * @failure_criteria
 *  - Any stimulus rejected, plugin unresponsive, or run_test() returns False.
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


def run_test():
    start_time = time.perf_counter()

    reg = send_curl_command(AVInputApis.register_event("onSignalChanged", "ID_onSignalChanged"))
    log_warning(f"register onSignalChanged: {reg}")
    if not is_ok(reg):
        log_error("TCID04_SignalChange Failed ❌ (failed to register onSignalChanged)")
        return False

    for label, yaml_file in [
        ("NO_SIGNAL", "HdmiInput_Signal_NoSignal_Port0.yaml"),
        ("UNSTABLE", "HdmiInput_Signal_Unstable_Port0.yaml"),
        ("LOCKED", "HdmiInput_Signal_Stable_Port0.yaml"),
    ]:
        log_info(f"Simulating signal state {label} on port 0")
        if not _post_stimulus(yaml_file):
            log_error(f"TCID04_SignalChange Failed ❌ ({label} stimulus not accepted)")
            return False
        time.sleep(2)

    # With a LOCKED signal, start the port and confirm a video mode is reported.
    start_resp = send_curl_command(AVInputApis.start_input(PORT))
    log_warning(f"startInput response: {start_resp}")
    if not is_ok(start_resp):
        log_error("TCID04_SignalChange Failed ❌ (startInput not accepted for locked signal)")
        return False

    mode = None
    deadline = time.time() + 10
    while time.time() < deadline:
        mode_resp = send_curl_command(AVInputApis.current_video_mode)
        log_warning(f"currentVideoMode response: {mode_resp}")
        mode = parse_current_video_mode(mode_resp)
        if mode:
            break
        time.sleep(1)

    send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    if mode is None:
        log_error("TCID04_SignalChange Failed ❌ (no video mode reported under LOCKED signal)")
        return False
    log_success(f"✅ Video mode under LOCKED signal: {mode}")

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID04_SignalChange Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
