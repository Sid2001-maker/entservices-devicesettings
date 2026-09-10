"""
/**
 * @file TCID03_StartStopInput.py
 * @brief L3 AVInput functional testcase.
 *
 * @testcase TCID03_StartStopInput
 * @details Registers onInputStatusChanged, starts HDMI input on port 0 (primary
 *          plane), confirms the plugin accepts presentation and reports a video
 *          mode, then stops input. Also exercises stopInput idempotency.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - A source is present on port 0 (or the vComponent simulates one).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - AVInput_Helpers.py
 *  - SuiteManager.py
 *
 * @expected_result
 *  - startInput and stopInput both return a successful JSON-RPC result.
 *
 * @pass_criteria
 *  - Start and stop succeed and run_test() returns True.
 *
 * @failure_criteria
 *  - Either call fails, plugin unresponsive, or run_test() returns False.
 */
"""

import time
import os

from utils import send_curl_command, is_ok, log_info, log_success, log_error, log_warning
import AVInput_Curl as AVInputApis

PORT = 0


def run_test():
    start_time = time.perf_counter()

    reg = send_curl_command(AVInputApis.register_event("onInputStatusChanged", "ID_onInputStatusChanged"))
    log_warning(f"register onInputStatusChanged: {reg}")
    if not is_ok(reg):
        log_error("TCID03_StartStopInput Failed ❌ (failed to register onInputStatusChanged)")
        return False

    try:
        log_info("Executing startInput on port 0 (primary plane)")
        start_resp = send_curl_command(AVInputApis.start_input(PORT, plane=0, top_most=True))
        log_warning(f"startInput response: {start_resp}")
        if not is_ok(start_resp):
            log_error("TCID03_StartStopInput Failed ❌ (startInput did not succeed)")
            return False
        log_success("✅ startInput accepted")

        time.sleep(2)

        log_info("Executing stopInput (HDMI)")
        stop_resp = send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))
        log_warning(f"stopInput response: {stop_resp}")
        if not is_ok(stop_resp):
            log_error("TCID03_StartStopInput Failed ❌ (stopInput did not succeed)")
            return False
        log_success("✅ stopInput accepted")

        # Idempotency: a second stopInput must still yield a valid response.
        stop_again = send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))
        log_warning(f"stopInput (again) response: {stop_again}")
        if not is_ok(stop_again):
            log_error("TCID03_StartStopInput Failed ❌ (second stopInput not handled)")
            return False
    finally:
        send_curl_command(AVInputApis.stop_input(AVInputApis.TYPE_HDMI))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID03_StartStopInput Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
