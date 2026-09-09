"""
/**
 * @file TCID28_SettingsPersistAcrossRestart.py
 * @brief L3 AVInput scenario testcase.
 *
 * @testcase TCID28_SettingsPersistAcrossRestart
 * @details Scenario: provisioned HDMI port settings must survive a plugin
 *          restart (middleware persistence, not just in-memory cache).
 *          Sequence:
 *            provision port 0 (EDID 2.0 + ALLM true + VRR true)
 *            -> Controller.deactivate(org.rdk.AVInput)
 *            -> Controller.activate(org.rdk.AVInput)
 *            -> re-read all three settings and confirm they survived
 *            -> restore baseline
 *          This exercises the HostPersistence-backed store rather than the HAL.
 *
 * @precondition
 *  - org.rdk.AVInput plugin is active and reachable via JSON-RPC endpoint.
 *  - The Controller plugin permits deactivate/activate of org.rdk.AVInput.
 *
 * @dependencies
 *  - utils.py, AVInput_Curl.py, AVInput_Helpers.py, SuiteManager.py
 *
 * @expected_result
 *  - EDID version, ALLM and VRR settings are unchanged after the restart.
 *
 * @pass_criteria
 *  - All three settings survive the restart; run_test() returns True.
 *
 * @failure_criteria
 *  - Restart fails, or any setting reverts after reactivation.
 */
"""

import time
import os

from utils import (
    send_curl_command,
    is_ok,
    activate_plugin,
    log_info,
    log_success,
    log_error,
    log_warning,
)
import AVInput_Curl as AVInputApis
from AVInput_Helpers import (
    result_success,
    parse_edid_version,
    parse_allm_support,
    parse_vrr_support,
)

PORT = 0


def _snapshot():
    return {
        "edidVersion": parse_edid_version(send_curl_command(AVInputApis.get_edid_version(PORT))),
        "allmSupport": parse_allm_support(send_curl_command(AVInputApis.get_edid2_allm_support(PORT))),
        "vrrSupport": parse_vrr_support(send_curl_command(AVInputApis.get_vrr_support(PORT))),
    }


def run_test():
    start_time = time.perf_counter()

    baseline = _snapshot()
    log_info(f"Baseline settings: {baseline}")

    try:
        log_info("Step 1: provision port 0 (EDID 2.0 + ALLM true + VRR true)")
        for label, cmd in (
            ("setEdidVersion(HDMI2.0)", AVInputApis.set_edid_version(PORT, AVInputApis.EDID_VERSION_20)),
            ("setEdid2AllmSupport(true)", AVInputApis.set_edid2_allm_support(PORT, True)),
            ("setVRRSupport(true)", AVInputApis.set_vrr_support(PORT, True)),
        ):
            resp = send_curl_command(cmd)
            log_warning(f"{label} response: {resp}")
            if not result_success(resp):
                log_error(f"TCID28_SettingsPersistAcrossRestart Failed ❌ ({label} rejected)")
                return False

        before = _snapshot()
        log_info(f"Settings before restart: {before}")

        log_info("Step 2: deactivate org.rdk.AVInput")
        deact = send_curl_command(AVInputApis.controller_deactivate())
        log_warning(f"deactivate response: {deact}")
        if not is_ok(deact):
            log_error("TCID28_SettingsPersistAcrossRestart Failed ❌ (plugin deactivate rejected)")
            return False
        time.sleep(3)

        log_info("Step 3: reactivate org.rdk.AVInput")
        if not activate_plugin(AVInputApis.CALLSIGN):
            log_error("TCID28_SettingsPersistAcrossRestart Failed ❌ (plugin reactivate failed)")
            return False
        log_info("Waiting 6s for plugin to fully initialise...")
        time.sleep(6)
        log_success("✅ Plugin restarted")

        log_info("Step 4: re-read settings after restart")
        after = _snapshot()
        log_info(f"Settings after restart: {after}")

        for key, expected in (("edidVersion", AVInputApis.EDID_VERSION_20),
                              ("allmSupport", True),
                              ("vrrSupport", True)):
            if after.get(key) != expected:
                log_error(
                    f"TCID28_SettingsPersistAcrossRestart Failed ❌ "
                    f"({key} did not persist: expected {expected}, got {after.get(key)})"
                )
                return False

        log_success("✅ EDID version, ALLM and VRR all persisted across plugin restart")
    finally:
        if baseline.get("edidVersion") in (AVInputApis.EDID_VERSION_14, AVInputApis.EDID_VERSION_20):
            send_curl_command(AVInputApis.set_edid_version(PORT, baseline["edidVersion"]))
        if isinstance(baseline.get("allmSupport"), bool):
            send_curl_command(AVInputApis.set_edid2_allm_support(PORT, baseline["allmSupport"]))
        if isinstance(baseline.get("vrrSupport"), bool):
            send_curl_command(AVInputApis.set_vrr_support(PORT, baseline["vrrSupport"]))

    elapsed_time = time.perf_counter() - start_time
    msg = "TCID28_SettingsPersistAcrossRestart Passed ✅"
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        log_success(f"{msg} time consumed: {elapsed_time:.3f}s")
    else:
        log_success(msg)
    return True
