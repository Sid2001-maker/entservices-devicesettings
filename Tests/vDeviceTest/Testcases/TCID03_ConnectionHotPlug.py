"""
/**
 * @file TCID03_ConnectionHotPlug.py
 * @brief L3 AVInput vDevice combination testcase.
 *
 * @testcase TCID03_ConnectionHotPlug
 * @details Drives the HDMI vComponent to simulate a source cable connect then
 *          disconnect on port 0, registers onDevicesChanged, and verifies the
 *          plugin reflects the connection transition via getInputDevices.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - HDMI Input vComponent control plane is reachable (postKVP).
 *
 * @dependencies
 *  - utils.py
 *  - AVInput_Curl.py
 *  - AVInput_Helpers.py
 *  - vcomponent_configurations/commands/HdmiInput_Connect_Port0.yaml
 *  - vcomponent_configurations/commands/HdmiInput_Disconnect_Port0.yaml
 *
 * @expected_result
 *  - getInputDevices reports connected=true after connect and false after disconnect.
 *
 * @pass_criteria
 *  - Both transitions observed for port 0 and run_test() returns True.
 *
 * @failure_criteria
 *  - vComponent stimulus rejected, transition not observed, or run_test() False.
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
from AVInput_Helpers import parse_input_devices, get_device_connected

PORT = 0


def _post_stimulus(yaml_file):
    http_code, body = send_vcomponent_command(f"{HDMIIN_CMD_BASE}/{yaml_file}")
    log_warning(f"vComponent POST {yaml_file}: HTTP {http_code}  {body}")
    return http_code == 200


def _wait_for_connected(expected, timeout_seconds=15):
    deadline = time.time() + timeout_seconds
    last = None
    while time.time() < deadline:
        resp = send_curl_command(AVInputApis.get_input_devices(AVInputApis.TYPE_HDMI))
        log_warning(f"getInputDevices response: {resp}")
        devices = parse_input_devices(resp)
        last = get_device_connected(devices, PORT)
        if last is expected:
            return last
        time.sleep(1)
    return last


def run_test():
    start_time = time.perf_counter()

    reg = send_curl_command(AVInputApis.register_event("onDevicesChanged", "ID_onDevicesChanged"))
    log_warning(f"register onDevicesChanged: {reg}")
    if not is_ok(reg):
        log_error("TCID03_ConnectionHotPlug Failed ❌ (failed to register onDevicesChanged)")
        return False

    log_info("Simulating HDMI source connect on port 0")
    if not _post_stimulus("HdmiInput_Connect_Port0.yaml"):
        log_error("TCID03_ConnectionHotPlug Failed ❌ (connect stimulus not accepted)")
        return False
    if _wait_for_connected(True) is not True:
        log_error("TCID03_ConnectionHotPlug Failed ❌ (port 0 not reported connected)")
        return False
    log_success("✅ Port 0 reported connected")

    log_info("Simulating HDMI source disconnect on port 0")
    if not _post_stimulus("HdmiInput_Disconnect_Port0.yaml"):
        log_error("TCID03_ConnectionHotPlug Failed ❌ (disconnect stimulus not accepted)")
        return False
    if _wait_for_connected(False) is not False:
        log_error("TCID03_ConnectionHotPlug Failed ❌ (port 0 not reported disconnected)")
        return False
    log_success("✅ Port 0 reported disconnected")

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID03_ConnectionHotPlug Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
