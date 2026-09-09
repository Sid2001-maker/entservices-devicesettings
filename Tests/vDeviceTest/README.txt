AVInput (HDMI Input) L3 vDevice Test Suite
==========================================

JSON-RPC functional test suite for the org.rdk.AVInput plugin (HDMI Input),
modelled on the PowerManager L3 vDevice suite.

EXECUTION
---------
cd source_code/AVInput_vDeviceTests

with timing   : python3 SuiteManager.py -t avinput
without timing: python3 SuiteManager.py avinput

Scenario suite (multi-step user journeys):
  python3 SuiteManager.py avinput_scenarios
  python3 SuiteManager.py -t avinput_scenarios

Run a single test (repeatable):
  python3 SuiteManager.py avinput --test TCID05_StartStopInput
  python3 SuiteManager.py avinput_scenarios --test TCID20_EdidProvisioningWorkflow

Default Actions:
Plugin activation is done by default before suite execution:
- avinput -> Controller.1.activate(callsign=org.rdk.AVInput)
- Init_AVInput_Populate is run to establish a known baseline (reads
  numberOfInputs / getInputDevices and issues a best-effort stopInput).

Disable default activation only if needed:
- export AUTO_ACTIVATE_PLUGINS=0

ENDPOINTS / DEFAULTS
--------------------
- MW JSON-RPC        : http://127.0.0.1:9998/jsonrpc
- HDMI vComponent API: http://127.0.0.1:8082/api/postKVP

Useful overrides:
- TARGET_HOST                 (applies to both endpoints)
- JSONRPC_PORT
- HDMIIN_VCOMPONENT_PORT
- WPEFRAMEWORK_JSONRPC_URL     (full URL, highest priority)
- HDMIIN_VCOMPONENT_API_URL    (full URL, highest priority)
- HDMIIN_CMD_BASE              (directory of vComponent command YAMLs)

Examples:

# inside QEMU guest (services on localhost)
python3 SuiteManager.py avinput

# from host against QEMU target IP
export TARGET_HOST=127.0.0.1
export JSONRPC_PORT=9998
export HDMIIN_VCOMPONENT_PORT=8082
python3 SuiteManager.py avinput
# then forward the ports via QEMU:
#   hostfwd=tcp:127.0.0.1:9998-:9998,hostfwd=tcp:127.0.0.1:8082-:8082
# and set WPEFramework "binding":"0.0.0.0" in /etc/WPEFramework/config.json.

TEST CASES
----------
TCID01_NumberOfInputs          - numberOfInputs returns a valid port count.
TCID02_GetInputDevices         - getInputDevices lists id/locator/connected.
TCID03_ConnectionHotPlug       - vComponent connect/disconnect -> onDevicesChanged
                                 reflected in getInputDevices.
TCID04_SignalChange            - vComponent NO_SIGNAL/UNSTABLE/LOCKED ->
                                 onSignalChanged path; LOCKED yields a video mode.
TCID05_StartStopInput          - startInput/stopInput + onInputStatusChanged.
TCID06_VideoStreamInfoUpdate   - vComponent format change -> currentVideoMode updates.
TCID07_EdidVersionRoundTrip    - set/get EDID version (HDMI2.0 / HDMI1.4).
TCID08_ReadWriteEdid           - readEDID / writeEDID / re-read round-trip.
TCID09_Edid2AllmSupport        - EDID-2.0 gated ALLM-in-EDID set/get.
TCID10_GameFeatureAllmStatus   - getSupportedGameFeatures + getGameFeatureStatus.
TCID11_AviContentTypeUpdate    - vComponent AVI InfoFrame (GAME) ->
                                 aviContentTypeUpdate path + health.
TCID12_VrrSupportAndFrameRate  - EDID-2.0 gated VRR set/get + getVRRFrameRate.
TCID13_GetSpd                  - vComponent SPD InfoFrame -> getSPD / getRawSPD.
TCID14_HdmiVersion             - getHdmiVersion returns capability version.
TCID15_SetVideoRectangle       - setVideoRectangle (full-screen + PIP).
TCID16_ContentProtected        - contentProtected returns HDCP-protected boolean.
TCID17_InvalidParameterHandling- malformed portId/typeOfInput handled gracefully.

SCENARIO TEST CASES (suite: avinput_scenarios)
----------------------------------------------
Multi-step user journeys chaining several APIs, with baseline capture and
restore in a finally block so each scenario leaves the device as it found it.

TCID18_PortEnumerationConsistency - numberOfInputs vs getInputDevices cross-check
                                   (count, contiguous ids, locator/id match).
TCID19_InputSwitchingLifecycle    - direct source switching across all ports
                                   without an intervening stop + stop idempotency.
TCID20_EdidProvisioningWorkflow   - readEDID -> setEdidVersion -> writeEDID ->
                                   re-read; validates base64 + 128-byte blocks.
TCID21_GameModeProvisioning       - Game Mode on/off via ALLM-in-EDID, with ALLM
                                   status queried in both states.
TCID22_VrrProvisioning            - VRR advertisement toggled on/off and verified.
TCID23_MultiPortIndependence      - opposing settings on 2 ports; asserts no
                                   cross-contamination between ports.
TCID24_PresentationWindowWorkflow - full-screen -> PIP -> move -> restore -> stop,
                                   plus post-stop geometry call handling.
TCID25_SourceInspectionWorkflow   - one-pass diagnostics report (HDMI version,
                                   SPD, raw SPD, video mode, content protection).
TCID26_AudioMixingWorkflow        - requestAudioMix true/false + mixer level sweep
                                   including both 0/100 extremes.
TCID27_NegativeAndBoundaryHandling- non-numeric + out-of-range portId, unknown
                                   typeOfInput, invalid edidVersion, then health.
TCID28_SettingsPersistAcrossRestart- provision, deactivate/activate the plugin,
                                   confirm EDID version/ALLM/VRR persisted.
TCID29_DisconnectedPortBehaviour  - locks in graceful degradation on a port with
                                   no source (empty video mode, not errors).

VERIFIED JSON-RPC CONTRACT
--------------------------
Parameter spelling and response shapes in AVInput_Curl.py / AVInput_Helpers.py
were confirmed against a live vDevice run (wpeframework_vdevice.log, 21/21 pass):

  portId          : INTEGER (e.g. 0), not a string
  startInput      : {portId, typeOfInput, requestAudioMix, plane, topMost}
  getInputDevices : returns the same list under BOTH "devices" and "deviceList"
  numberOfInputs  : {"numberOfInputs":2,"success":true}
  readEDID        : {"EDID":"<base64>","success":true}
  getEdidVersion  : {"edidVersion":"HDMI2.0","success":true}
  getEdid2AllmSupport : {"allmSupport":true,"success":true}
  getVRRSupport   : {"vrrSupport":false,"success":true}
  getHdmiVersion  : {"HdmiCapabilityVersion":"2.1","success":true}
  getSPD/getRawSPD: {"HDMISPD":"...","success":true}
  contentProtected: {"isContentProtected":true,"success":true}
  currentVideoMode: {"currentVideoMode":"","success":true}  <- empty w/o a source

Known vDevice behaviours the scenarios accommodate:
  - Both HDMI ports report connected:false (no physical source on the bench).
  - startInput succeeds even on a disconnected port.
  - currentVideoMode is an empty string when no source is attached.
  - getSupportedGameFeatures returns {"supportedGameFeatures":[],"success":false}
    on the vDevice, so scenarios rely on getGameFeatureStatus instead.

SCENARIO HOOKS (vComponent)
---------------------------
Event-driven cases (TCID03/04/06/11/12/13) drive the HDMI Input vComponent via
utils.send_vcomponent_command() posting the YAML files under
vcomponent_configurations/commands/ to the postKVP control plane
(default port 8082; override with HDMIIN_VCOMPONENT_PORT). The YAML command
contract mirrors the rdk-halif HDMI controller command templates
(connection_status, signal_status, videoformat_change, aviinfo_frame,
vrr_status, spdinfo_frame).

NOTE ON ASYNC EVENTS
--------------------
This curl-based suite registers each notification (onDevicesChanged,
onSignalChanged, onInputStatusChanged, videoStreamInfoUpdate,
gameFeatureStatusUpdate, aviContentTypeUpdate) to exercise the plugin
subscription path, then validates the resulting observable state through the
available getters (getInputDevices, currentVideoMode, getVRRFrameRate, getSPD).
Capturing the asynchronous JSON-RPC notification payloads themselves requires a
persistent Thunder event listener (RAFT); wire that in where byte-for-byte
callback payload assertions are required.

NOTES
-----
- JSON-RPC parameter spelling in AVInput_Curl.py follows the working curl set in
  curl_extract.txt (audioMix / planeType / topMost / portId as string). If a
  target build exposes different parameter spelling, adjust the builders in
  AVInput_Curl.py (single source of truth).

Troubleshooting:
- On "connection refused", verify WPEFramework JSON-RPC is reachable using the
  endpoint overrides above.
- If vComponent-driven tests fail with non-200 HTTP, verify the HDMI Input
  vComponent control plane is running and HDMIIN_VCOMPONENT_PORT is correct.
