"""
/**
 * @file utils.py
 * @brief utils.py
 *
 * @testcase utils
 * @details Provides shared utility functions and constants used across all AVInput
 *          (HDMI Input) L3 test cases, including JSON-RPC command dispatch, HDMI
 *          vComponent YAML execution (scenario hooks), curl-based API invocation,
 *          and structured pass/fail logging helpers.
 *
 * @precondition
 *  - WPEFramework JSON-RPC endpoint is reachable at WPEFRAMEWORK_JSONRPC_URL.
 *
 * @dependencies
 *  - Standard Python libraries: os, json, subprocess, pathlib
 */
"""

import os
import time
import json
import subprocess
from pathlib import Path


# Base paths for HDMI Input vComponent YAML commands (scenario hooks).
# Prefer testcase-local YAMLs, fallback to /etc paths, and allow env overrides.
_BASE_DIR = Path(__file__).resolve().parent
_LOCAL_HDMIIN_CMD_BASE = _BASE_DIR / "vcomponent_configurations" / "commands"


def _pick_existing_dir(primary, fallback):
    if primary.is_dir():
        return str(primary)
    return fallback


HDMIIN_CMD_BASE = os.environ.get("HDMIIN_CMD_BASE") or _pick_existing_dir(
    _LOCAL_HDMIIN_CMD_BASE,
    "/etc/avinput/vcomponent_configurations/commands",
)

# Endpoint selection for local/QEMU execution.
# - TARGET_HOST sets both MW and vComponent host in one place.
# - Explicit URL env vars take precedence.
TARGET_HOST = os.environ.get("TARGET_HOST", "127.0.0.1")
JSONRPC_PORT = os.environ.get("JSONRPC_PORT", "9998")
# HDMI Input vComponent control plane defaults to 8082. Override via
# HDMIIN_VCOMPONENT_PORT / HDMIIN_VCOMPONENT_API_URL for target-specific deployments.
HDMIIN_VCOMPONENT_PORT = os.environ.get("HDMIIN_VCOMPONENT_PORT", "8082")
WPEFRAMEWORK_JSONRPC_URL = (
    os.environ.get("WPEFRAMEWORK_JSONRPC_URL")
    or os.environ.get("JSONRPC_URL")
    or f"http://{TARGET_HOST}:{JSONRPC_PORT}/jsonrpc"
)
HDMIIN_VCOMPONENT_API_URL = (
    os.environ.get("HDMIIN_VCOMPONENT_API_URL")
    or f"http://{TARGET_HOST}:{HDMIIN_VCOMPONENT_PORT}/api/postKVP"
)

# ---------- ANSI COLOR CONSTANTS ----------
RESET = "\033[0m"
BOLD = "\033[1m"

RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
CYAN = "\033[96m"

# ---------- LOG HELPERS ----------
def _emit_log(message):
    print(message, flush=True)


def log_info(msg):
    _emit_log(f"{CYAN}{msg}{RESET}")

def log_success(msg):
    _emit_log(f"{GREEN}{BOLD}{msg}{RESET}")

def log_warning(msg):
    _emit_log(f"{YELLOW}{msg}{RESET}")

def log_error(msg):
    _emit_log(f"{RED}{BOLD}{msg}{RESET}")


def log_with_timing(msg, elapsed_time):
    """Append timing info only when AVINPUT_TIMING_ENABLED is set."""
    if os.environ.get("AVINPUT_TIMING_ENABLED"):
        return f"{msg} time consumed: {elapsed_time:.3f}s"
    return msg


def send_jsonrpc_command(method, params=None, request_id=1, timeout=5):
    """Send a JSON-RPC request to WPEFramework and return parsed response dict.
    Returns None when request fails or response is not JSON.
    """
    payload = {
        "jsonrpc": "2.0",
        "id": request_id,
        "method": method,
    }
    if params is not None:
        payload["params"] = params

    cmd = [
        "curl", "-sS", "--max-time", str(timeout),
        "-H", "Content-Type: application/json",
        "-X", "POST",
        "--data", json.dumps(payload),
        WPEFRAMEWORK_JSONRPC_URL,
    ]

    try:
        result = subprocess.run(
            cmd,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if result.returncode != 0:
            return None
        body = (result.stdout or "").strip()
        if not body:
            return None
        return json.loads(body)
    except Exception:
        return None


def activate_plugin(callsign, timeout_seconds=40):
    """Activate an RDK plugin via Controller.1.activate.
    Returns True on success, False otherwise.
    """
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        response = send_jsonrpc_command(
            "Controller.1.activate",
            params={"callsign": callsign},
            request_id=1234567890,
        )
        if not response or "error" in response:
            time.sleep(1)
            continue
        return "result" in response
    return False


def send_curl_command(curl_command):
    """Send a curl command list and return the first valid JSON response line."""
    output_response = ""
    try:
        result = subprocess.run(
            curl_command,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        response = result.stdout or ""
        for line in response.splitlines():
            try:
                json.loads(line)
                output_response = line
                break
            except json.JSONDecodeError:
                pass

        if len(output_response) < 5:
            output_response = "< No response from WPEFramework >"
    except Exception as exc:
        _emit_log(f"Inside utils.py : Exception in send_curl_command function: {exc}")
    finally:
        return output_response


def send_vcomponent_command(yaml_file_path):
    """Post a YAML command file to the HDMI Input vComponent HTTP API.

    Uses: curl -sS -X POST -H "Content-Type: application/x-yaml"
               --data-binary @<yaml_file> http://<host>:8082/api/postKVP
    Returns (http_code: int, body: str). http_code 200 indicates success.
    Some vComponent builds close the connection without an HTTP response after
    applying YAML (CURLE_GOT_NOTHING 52); that is treated as accepted.
    """
    try:
        if not Path(yaml_file_path).is_file():
            return 0, f"YAML file not found: {yaml_file_path}"

        cmd = [
            "curl", "-sS", "-w", "\n%{http_code}",
            "-X", "POST",
            "-H", "Content-Type: application/x-yaml",
            "--data-binary", f"@{yaml_file_path}",
            HDMIIN_VCOMPONENT_API_URL,
        ]
        result = subprocess.run(
            cmd,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        stdout = result.stdout or ""
        parts = stdout.rsplit("\n", 1)
        if len(parts) == 2:
            body = parts[0]
            http_code_str = parts[1].strip()
        else:
            body = stdout.strip()
            http_code_str = "0"
        try:
            http_code = int(http_code_str)
        except ValueError:
            http_code = 0
        if (
            http_code == 0
            and result.returncode == 52
            and "Empty reply from server" in (result.stderr or "")
        ):
            return 200, "Empty reply from server (accepted)"
        if http_code == 0 and result.stderr.strip():
            body = result.stderr.strip()
        return http_code, body
    except Exception as exc:
        return 0, f"Exception in send_vcomponent_command: {exc}"


def parse_result(curl_response):
    """Parse a JSON-RPC curl response string and return the 'result' field.
    Returns None on transport/parse error or when no result field is present.
    """
    if not curl_response or curl_response.startswith("< No response"):
        return None
    try:
        body = json.loads(curl_response)
    except json.JSONDecodeError:
        return None
    if not isinstance(body, dict) or "result" not in body:
        return None
    return body.get("result")


def is_ok(curl_response):
    """Return True if a JSON-RPC response is valid and carries a 'result' (no 'error')."""
    if not curl_response or curl_response.startswith("< No response"):
        return False
    try:
        body = json.loads(curl_response)
    except json.JSONDecodeError:
        return False
    if not isinstance(body, dict):
        return False
    return "result" in body and "error" not in body


def responded(curl_response):
    """True when the plugin returned a JSON body (not the no-response sentinel)."""
    return bool(curl_response) and not curl_response.startswith("< No response")
