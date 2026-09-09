"""
/**
 * @file TCID11_AviContentTypeUpdate.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID11_AviContentTypeUpdate
 * @details Registers aviContentTypeUpdate, starts HDMI input on port 0, and drives
 *          the HDMI vComponent to send an AVI InfoFrame carrying the GAME content
 *          type. Verifies the stimulus is accepted and the plugin remains
 *          responsive.
 *
 * @note Asynchronous aviContentTypeUpdate capture requires the RAFT event
 *       listener; this curl-based case validates stimulus acceptance and plugin
 *       health, since there is no synchronous getter for AVI content type.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - vcomponent_configurations/commands/HdmiInput_AVIInfoFrame_Game_Port0.yaml
 *
 * @expected_result
 *  - AVI InfoFrame stimulus accepted and plugin responsive afterwards.
 *
 * @pass_criteria
 *  - Stimulus accepted, plugin responsive, and run_test() returns True.
 *
 * @failure_criteria
 *  - Stimulus rejected, plugin unresponsive, or run_test() returns False.
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

PORT = 0


def _post_stimulus(yaml_file):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{yaml_file}")
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def run_test():
    start_time = time.perf_counter()

    reg = send_curl_command(AVInputApis.register_event("aviContentTypeUpdate", "ID_aviContentTypeUpdate"))
    log_warning(f"register aviContentTypeUpdate: {reg}")
    if not is_ok(reg):
        log_error("TCID11_AviContentTypeUpdate Failed ❌ (failed to register aviContentTypeUpdate)")
        return False

    try:
        start_resp = send_curl_command(AVInputApis.start_input(PORT))
        log_warning(f"startInput response: {start_resp}")
        if not is_ok(start_resp):
            log_error("TCID11_AviContentTypeUpdate Failed ❌ (startInput not accepted)")
            return False

        log_info("Simulating AVI InfoFrame with GAME content type on port 0")
        if not _post_stimulus("HdmiInput_AVIInfoFrame_Game_Port0.yaml"):
            log_error("TCID11_AviContentTypeUpdate Failed ❌ (AVI InfoFrame stimulus not accepted)")
            return False
        time.sleep(2)

        # Health check: the plugin must remain responsive after the InfoFrame.
        health = send_curl_command(AVInputApis.number_of_inputs)
        log_warning(f"health (numberOfInputs) response: {health}")
        if not is_ok(health):
            log_error("TCID11_AviContentTypeUpdate Failed ❌ (plugin unresponsive after AVI InfoFrame)")
            return False
        log_success("✅ AVI InfoFrame (GAME) accepted and plugin responsive")
    finally:
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID11_AviContentTypeUpdate Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
