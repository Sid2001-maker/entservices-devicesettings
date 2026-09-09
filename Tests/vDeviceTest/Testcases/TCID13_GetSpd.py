"""
/**
 * @file TCID13_GetSpd.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID13_GetSpd
 * @details Starts HDMI input on port 0, drives the HDMI vComponent to send an SPD
 *          InfoFrame, then validates getSPD and getRawSPD return SPD data for the
 *          source.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - AVInput_Helpers.py
 *  - vcomponent_configurations/commands/HdmiInput_SPDInfoFrame_Port0.yaml
 *
 * @expected_result
 *  - getSPD and getRawSPD return non-empty SPD values.
 *
 * @pass_criteria
 *  - Both SPD queries return data and run_test() returns True.
 *
 * @failure_criteria
 *  - Empty/invalid SPD, call failure, or run_test() returns False.
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
from AVInput_Helpers import parse_spd

PORT = 0


def _post_stimulus(yaml_file):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{yaml_file}")
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def run_test():
    start_time = time.perf_counter()

    try:
        start_resp = send_curl_command(AVInputApis.start_input(PORT))
        log_warning(f"startInput response: {start_resp}")
        if not is_ok(start_resp):
            log_error("TCID13_GetSpd Failed ❌ (startInput not accepted)")
            return False

        log_info("Simulating SPD InfoFrame on port 0")
        if not _post_stimulus("HdmiInput_SPDInfoFrame_Port0.yaml"):
            log_error("TCID13_GetSpd Failed ❌ (SPD InfoFrame stimulus not accepted)")
            return False
        time.sleep(2)

        log_info("Executing getSPD on port 0")
        spd_resp = send_curl_command(AVInputApis.get_spd(PORT))
        log_warning(f"getSPD response: {spd_resp}")
        if not parse_spd(spd_resp):
            log_error("TCID13_GetSpd Failed ❌ (getSPD returned empty/invalid data)")
            return False
        log_success("✅ getSPD returned data")

        log_info("Executing getRawSPD on port 0")
        raw_resp = send_curl_command(AVInputApis.get_raw_spd(PORT))
        log_warning(f"getRawSPD response: {raw_resp}")
        if not parse_spd(raw_resp):
            log_error("TCID13_GetSpd Failed ❌ (getRawSPD returned empty/invalid data)")
            return False
        log_success("✅ getRawSPD returned data")
    finally:
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID13_GetSpd Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
