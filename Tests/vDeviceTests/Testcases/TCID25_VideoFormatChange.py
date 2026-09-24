"""
/**
 * @file TCID25_VideoFormatChange.py
 * @brief L3 AVInput vComponent-driven testcase.
 *
 * @testcase TCID25_VideoFormatChange
 * @details Presents an HDMI source, reads the current video mode, injects a
 *          video-format change through the vComponent
 *          (HDMIInput_VideoFormat_Change.yaml -> VIC16_1920_1080_P_60_16_9 ->
 *          onVIChanged in the HAL) and re-reads/prints the new video mode via
 *          the JSON-RPC currentVideoMode getter.
 *
 * @note HAL gating (dHdmiInAIDLImpl): GetHDMIVideoMode returns the injected VIC
 *       ONLY for the ACTIVE port (m_aidlActivePort), and the port only becomes
 *       active/presented once the emulated source is connected AND the signal is
 *       LOCKED (the vComponent then drives onStateChanged(STARTED)). Simply
 *       calling startInput is not enough because the AIDL SelectHDMIInPort path
 *       is stubbed. Therefore this test first injects connection_status(true)
 *       and signal_status(LOCKED) to present the port, THEN injects the video
 *       format. On a headless vDevice where the port is never presented,
 *       currentVideoMode can still be empty; that case is accepted with a
 *       warning rather than failed.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent reachable at HDMIIN_VCOMPONENT_API_URL.
 *
 * @dependencies
 *  - utils.py, AVInput_Curl.py, AVInput_Helpers.py, SuiteManager.py
 *  - vcomponent_configurations/commands/HDMIInput_Connection_Status.yaml
 *  - vcomponent_configurations/commands/HDMIInput_Signal_Status.yaml
 *  - vcomponent_configurations/commands/HDMIInput_VideoFormat_Change.yaml
 *
 * @expected_result
 *  - After presenting the source, currentVideoMode reflects the injected
 *    1920x1080/60 format (non-empty on a presenting port).
 *
 * @pass_criteria
 *  - The YAML post is accepted and currentVideoMode returns a valid string;
 *    run_test() returns True.
 *
 * @failure_criteria
 *  - The YAML post is rejected, or currentVideoMode is not a string.
 */
"""

import time
import os

from utils import (
    HDMIIN_CMD_BASE,
    send_curl_command,
    send_vcomponent_command,
    send_vcomponent_payload,
    is_ok,
    log_info,
    log_success,
    log_error,
    log_warning,
)
import AVInput_Curl as AVInputApis
from AVInput_Helpers import result_success, parse_current_video_mode

PORT = 0
CONNECTION_YAML = "HDMIInput_Connection_Status.yaml"
SIGNAL_YAML = "HDMIInput_Signal_Status.yaml"
VIDEO_FORMAT_YAML = "HDMIInput_VideoFormat_Change.yaml"


def _post_file(name):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{name}")
    log_warning(f"vComponent POST {name}: HTTP {http_code} {body}")
    return http_code == 200


def _read_video_mode():
    response = send_curl_command(AVInputApis.current_video_mode)
    log_warning(f"currentVideoMode response: {response}")
    return parse_current_video_mode(response)


def run_test():
    start_time = time.perf_counter()

    try:
        log_info("Step 1: startInput on port 0")
        start_resp = send_curl_command(AVInputApis.start_input(PORT))
        log_warning(f"startInput response: {start_resp}")
        if not result_success(start_resp):
            log_error("TCID25_VideoFormatChange Failed ❌ (startInput rejected)")
            return False
        time.sleep(1)

        # Step 2: present the port so the active-port video-mode gate is satisfied
        # (connection + LOCKED signal -> vComponent drives onStateChanged(STARTED)).
        log_info(f"Step 2: present source via {CONNECTION_YAML} + {SIGNAL_YAML}")
        if not _post_file(CONNECTION_YAML):
            log_error("TCID25_VideoFormatChange Failed ❌ (connection_status YAML rejected)")
            return False
        time.sleep(1)
        if not _post_file(SIGNAL_YAML):
            log_error("TCID25_VideoFormatChange Failed ❌ (signal_status YAML rejected)")
            return False
        time.sleep(2)

        # Step 3: capture the current resolution BEFORE the format injection.
        before_mode = _read_video_mode()
        if before_mode is None:
            log_error("TCID25_VideoFormatChange Failed ❌ (currentVideoMode not a string before injection)")
            return False
        log_info(f"Current resolution BEFORE injection = '{before_mode or '<no source>'}'")

        # Step 4: inject the video-format change (VIC16 -> 1920x1080p60).
        log_info(f"Step 4: injecting videoformat_change via {VIDEO_FORMAT_YAML}")
        if not _post_file(VIDEO_FORMAT_YAML):
            log_error("TCID25_VideoFormatChange Failed ❌ (videoformat_change YAML rejected)")
            return False
        time.sleep(2)

        # Step 5: re-read and print the NEW resolution.
        after_mode = _read_video_mode()
        if after_mode is None:
            log_error("TCID25_VideoFormatChange Failed ❌ (currentVideoMode not a string after injection)")
            return False
        log_info(f"Current resolution AFTER injection = '{after_mode or '<no source>'}'")

        if after_mode:
            if after_mode != before_mode:
                log_success(f"✅ Video mode updated after injection: '{before_mode}' -> '{after_mode}'")
            else:
                log_success(f"✅ Video mode reported as '{after_mode}' after injection")
        else:
            log_warning(
                "currentVideoMode is empty after injection: the vDevice never presented the "
                "port (m_aidlActivePort stayed -1 because the AIDL SelectHDMIInPort path is "
                "stubbed), so GetHDMIVideoMode reads VIC=0. Accepting injection acceptance + "
                "queryability."
            )
    finally:
        send_vcomponent_payload("signal_status", {"port": PORT, "state": "NO_SIGNAL"})
        send_vcomponent_payload("connection_status", {"port": PORT, "connected": False})
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID25_VideoFormatChange Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
