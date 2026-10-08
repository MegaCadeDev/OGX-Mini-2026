# Apply a series of patches (in order, each on top of the previous) in DIR, unless the series
# is already applied. A later patch changes the context of earlier ones, so per-patch reverse
# checks stop matching; instead: last patch reverse-applies => whole series applied; first
# patch applies cleanly => apply all in order; anything else (partial / drifted) fails the build.
function(ogxm_apply_patch_series NAME DIR)
    set(_patches ${ARGN})
    list(GET _patches -1 _last)
    list(GET _patches 0 _first)
    execute_process(
        COMMAND git apply --check --reverse --ignore-whitespace ${_last}
        WORKING_DIRECTORY ${DIR}
        RESULT_VARIABLE _already
        OUTPUT_QUIET ERROR_QUIET
    )
    if (_already EQUAL 0)
        message(STATUS "${NAME} patches already applied.")
        return()
    endif ()
    execute_process(
        COMMAND git apply --check --ignore-whitespace ${_first}
        WORKING_DIRECTORY ${DIR}
        RESULT_VARIABLE _clean
        ERROR_VARIABLE _error
    )
    if (NOT _clean EQUAL 0)
        message(FATAL_ERROR "${NAME} patches are partially applied or out of date: ${_error}")
    endif ()
    foreach (_patch IN LISTS _patches)
        message(STATUS "Applying ${NAME} patch: ${_patch}")
        execute_process(
            COMMAND git apply --ignore-whitespace ${_patch}
            WORKING_DIRECTORY ${DIR}
            RESULT_VARIABLE _result
            ERROR_VARIABLE _error
        )
        if (NOT _result EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${NAME} patch ${_patch}: ${_error}")
        endif ()
    endforeach ()
endfunction()

function(apply_lib_patches EXTERNAL_DIR)
    set(BTSTACK_PATCH "${EXTERNAL_DIR}/patches/btstack_l2cap.diff")
    set(BTSTACK_PATH "${EXTERNAL_DIR}/bluepad32/external/btstack")

    message(STATUS "Applying BTStack patch: ${BTSTACK_PATCH}")

    execute_process(
        COMMAND git apply --ignore-whitespace ${BTSTACK_PATCH}
        WORKING_DIRECTORY ${BTSTACK_PATH}
        RESULT_VARIABLE BTSTACK_PATCH_RESULT
        OUTPUT_VARIABLE BTSTACK_PATCH_OUTPUT
        ERROR_VARIABLE BTSTACK_PATCH_ERROR
    )

    if (BTSTACK_PATCH_RESULT EQUAL 0)
        message(STATUS "BTStack patch applied successfully.")
    elseif (BTSTACK_PATCH_ERROR MATCHES "patch does not apply")
        message(STATUS "BTStack patch already applied.")
    else ()
        message(FATAL_ERROR "Failed to apply BTStack patch: ${BTSTACK_PATCH_ERROR}")
    endif ()

    set(BTSTACK_HIDS_REPORTS_PATCH "${EXTERNAL_DIR}/patches/btstack_hids_num_reports.diff")
    message(STATUS "Applying BTStack HIDS reports patch: ${BTSTACK_HIDS_REPORTS_PATCH}")
    execute_process(
        COMMAND git apply --ignore-whitespace ${BTSTACK_HIDS_REPORTS_PATCH}
        WORKING_DIRECTORY ${BTSTACK_PATH}
        RESULT_VARIABLE BTSTACK_HIDS_REPORTS_RESULT
        OUTPUT_VARIABLE BTSTACK_HIDS_REPORTS_OUTPUT
        ERROR_VARIABLE BTSTACK_HIDS_REPORTS_ERROR
    )
    if (BTSTACK_HIDS_REPORTS_RESULT EQUAL 0)
        message(STATUS "BTStack HIDS reports patch applied successfully.")
    elseif (BTSTACK_HIDS_REPORTS_ERROR MATCHES "patch does not apply")
        message(STATUS "BTStack HIDS reports patch already applied.")
    else ()
        message(FATAL_ERROR "Failed to apply BTStack HIDS reports patch: ${BTSTACK_HIDS_REPORTS_ERROR}")
    endif ()

    # BTstack v1.8 queues the HID Control Point command through the GATT query
    # scheduler even though the operation is a write without response. This can
    # leave Xbox BLE controllers connected but unable to complete initialization
    # (HID up, GETs return COMMAND_DISALLOWED / no input).
    # Detect by content — never treat a failed git apply as "already applied"
    # (hunk line drift caused that false positive and left Xbox BLE broken).
    set(BTSTACK_HIDS_CONTROL_POINT_PATCH "${EXTERNAL_DIR}/patches/btstack_hids_control_point.diff")
    set(BTSTACK_HIDS_HOST_C "${BTSTACK_PATH}/src/ble/gatt-service/hids_host.c")
    file(READ "${BTSTACK_HIDS_HOST_C}" _hids_host_c_contents)
    set(_hids_cp_bug
"    client->handle = client->services[client->service_index].control_point_value_handle;
    client->value = value;

    client->state = HIDS_HOST_W2_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE;
    hids_host_request_to_send_next_query(client);")
    set(_hids_cp_fix
"    client->handle = client->services[client->service_index].control_point_value_handle;
    client->value = value;

    client->state = HIDS_HOST_W2_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE;
    hids_host_request_to_send_write_without_response(client);")
    string(FIND "${_hids_host_c_contents}" "${_hids_cp_fix}" _hids_cp_fixed)
    string(FIND "${_hids_host_c_contents}" "${_hids_cp_bug}" _hids_cp_buggy)
    if (_hids_cp_fixed GREATER_EQUAL 0)
        message(STATUS "BTStack HIDS Control Point patch already applied.")
    elseif (_hids_cp_buggy GREATER_EQUAL 0)
        message(STATUS "Applying BTStack HIDS Control Point patch (in-place fix)")
        string(REPLACE "${_hids_cp_bug}" "${_hids_cp_fix}" _hids_host_c_contents "${_hids_host_c_contents}")
        file(WRITE "${BTSTACK_HIDS_HOST_C}" "${_hids_host_c_contents}")
        message(STATUS "BTStack HIDS Control Point patch applied successfully.")
    else ()
        message(STATUS "Applying BTStack HIDS Control Point patch: ${BTSTACK_HIDS_CONTROL_POINT_PATCH}")
        execute_process(
            COMMAND git apply --ignore-whitespace ${BTSTACK_HIDS_CONTROL_POINT_PATCH}
            WORKING_DIRECTORY ${BTSTACK_PATH}
            RESULT_VARIABLE BTSTACK_HIDS_CONTROL_POINT_RESULT
            ERROR_VARIABLE BTSTACK_HIDS_CONTROL_POINT_ERROR
        )
        if (NOT BTSTACK_HIDS_CONTROL_POINT_RESULT EQUAL 0)
            execute_process(
                COMMAND patch -p1 --forward --batch
                WORKING_DIRECTORY ${BTSTACK_PATH}
                INPUT_FILE ${BTSTACK_HIDS_CONTROL_POINT_PATCH}
                RESULT_VARIABLE BTSTACK_HIDS_CONTROL_POINT_RESULT
                ERROR_VARIABLE BTSTACK_HIDS_CONTROL_POINT_ERROR
            )
        endif ()
        if (BTSTACK_HIDS_CONTROL_POINT_RESULT EQUAL 0)
            message(STATUS "BTStack HIDS Control Point patch applied successfully.")
        else ()
            message(FATAL_ERROR "Failed to apply BTStack HIDS Control Point patch: ${BTSTACK_HIDS_CONTROL_POINT_ERROR}")
        endif ()
    endif ()

    set(BLUEPAD32_PATCH "${EXTERNAL_DIR}/patches/bluepad32_uni.diff")
    set(BLUEPAD32_PATH "${EXTERNAL_DIR}/bluepad32")

    message(STATUS "Applying Bluepad32 patch: ${BLUEPAD32_PATCH}")

    execute_process(
        COMMAND git apply --ignore-whitespace ${BLUEPAD32_PATCH}
        WORKING_DIRECTORY ${BLUEPAD32_PATH}
        RESULT_VARIABLE BLUEPAD32_PATCH_RESULT
        OUTPUT_VARIABLE BLUEPAD32_PATCH_OUTPUT
        ERROR_VARIABLE BLUEPAD32_PATCH_ERROR
    )

    if (BLUEPAD32_PATCH_RESULT EQUAL 0)
        message(STATUS "Bluepad32 patch applied successfully.")
    elseif (BLUEPAD32_PATCH_ERROR MATCHES "patch does not apply")
        message(STATUS "Bluepad32 patch already applied.")
    else ()
        message(FATAL_ERROR "Failed to apply Bluepad32 patch: ${BLUEPAD32_PATCH_ERROR}")
    endif ()

    set(BLUEPAD32_8BITDO_PATCH "${EXTERNAL_DIR}/patches/bluepad32_8bitdo_pids.diff")
    message(STATUS "Applying Bluepad32 8BitDo PID patch: ${BLUEPAD32_8BITDO_PATCH}")
    execute_process(
        COMMAND git apply --ignore-whitespace ${BLUEPAD32_8BITDO_PATCH}
        WORKING_DIRECTORY ${BLUEPAD32_PATH}
        RESULT_VARIABLE BLUEPAD32_8BITDO_RESULT
        OUTPUT_VARIABLE BLUEPAD32_8BITDO_OUTPUT
        ERROR_VARIABLE BLUEPAD32_8BITDO_ERROR
    )
    if (BLUEPAD32_8BITDO_RESULT EQUAL 0)
        message(STATUS "Bluepad32 8BitDo PID patch applied successfully.")
    elseif (BLUEPAD32_8BITDO_ERROR MATCHES "patch does not apply")
        message(STATUS "Bluepad32 8BitDo PID patch already applied.")
    else ()
        message(FATAL_ERROR "Failed to apply Bluepad32 8BitDo PID patch: ${BLUEPAD32_8BITDO_ERROR}")
    endif ()

    # Switch parser fixes, applied in order on top of each other:
    # - enable vibration (subcommand 0x48) during setup: Joy-Cons ignore rumble until then;
    # - setup robustness: per-step timeout + retries, stale-reply filtering, one pad in
    #   setup at a time, and timer cleanup before a device's parser data is wiped;
    # - request-sleep: subcommand 0x06/0x00 so a Joy-Con drops the link and sleeps;
    # - pair IMU: a merged Joy-Con pair keeps (and parses) only the selected half's IMU;
    # - rumble intensity: the magnitudes set the amplitudes at a fixed frequency, the same data
    #   on both actuators as SDL sends it (weak = high band, strong = low band), instead of
    #   being encoded as a frequency with a fixed amplitude.
    ogxm_apply_patch_series("Bluepad32 Switch parser" "${BLUEPAD32_PATH}"
        "${EXTERNAL_DIR}/patches/bluepad32_switch_enable_vibration.diff"
        "${EXTERNAL_DIR}/patches/bluepad32_switch_setup_robustness.diff"
        "${EXTERNAL_DIR}/patches/bluepad32_switch_request_sleep.diff"
        "${EXTERNAL_DIR}/patches/bluepad32_switch_pair_imu.diff"
        "${EXTERNAL_DIR}/patches/bluepad32_switch_rumble_intensity.diff")

    # DS4 parser: keep the latest touch points for touchpad passthrough.
    ogxm_apply_patch_series("Bluepad32 DS4 parser" "${BLUEPAD32_PATH}"
        "${EXTERNAL_DIR}/patches/bluepad32_ds4_touchpad.diff")

    # Output queue: byte-stream ring buffer, backport of ricardoquesada/bluepad32 b6531db. The old
    # 32-slot queue dropped packets when full, a queued rumble "stop" among them (stuck rumble).
    ogxm_apply_patch_series("Bluepad32 output queue" "${BLUEPAD32_PATH}"
        "${EXTERNAL_DIR}/patches/bluepad32_output_ring_buffer.diff")

    # Pico SDK 2.1.x still lists BTstack's old hids_client.c; Bluepad32's BTstack
    # v1.8 renamed it to hids_host.c. Patch the SDK cmake when using that tree.
    set(PICO_SDK_HIDS_PATCH "${EXTERNAL_DIR}/patches/pico_sdk_hids_host.diff")
    set(PICO_SDK_PATH_LOCAL "${EXTERNAL_DIR}/pico-sdk")
    if (EXISTS "${PICO_SDK_PATH_LOCAL}/src/rp2_common/pico_btstack/CMakeLists.txt")
        message(STATUS "Applying Pico SDK HIDS host patch: ${PICO_SDK_HIDS_PATCH}")
        execute_process(
            COMMAND git apply --ignore-whitespace ${PICO_SDK_HIDS_PATCH}
            WORKING_DIRECTORY ${PICO_SDK_PATH_LOCAL}
            RESULT_VARIABLE PICO_SDK_HIDS_PATCH_RESULT
            OUTPUT_VARIABLE PICO_SDK_HIDS_PATCH_OUTPUT
            ERROR_VARIABLE PICO_SDK_HIDS_PATCH_ERROR
        )
        if (PICO_SDK_HIDS_PATCH_RESULT EQUAL 0)
            message(STATUS "Pico SDK HIDS host patch applied successfully.")
        elseif (PICO_SDK_HIDS_PATCH_ERROR MATCHES "patch does not apply")
            message(STATUS "Pico SDK HIDS host patch already applied.")
        else ()
            message(FATAL_ERROR "Failed to apply Pico SDK HIDS host patch: ${PICO_SDK_HIDS_PATCH_ERROR}")
        endif ()
    endif ()

    # Pico SDK 2.1.0 places incoming HCI packets too close to the start of the
    # receive buffer for BTstack's configured pre-buffer. Keep the full
    # pre-buffer available so longer Xbox BLE GATT events are not corrupted.
    set(PICO_SDK_CYW43_HCI_PREBUFFER_PATCH "${EXTERNAL_DIR}/patches/pico_sdk_cyw43_hci_prebuffer.diff")
    if (EXISTS "${PICO_SDK_PATH_LOCAL}/src/rp2_common/pico_cyw43_driver/btstack_hci_transport_cyw43.c")
        message(STATUS "Applying Pico SDK CYW43 HCI pre-buffer patch: ${PICO_SDK_CYW43_HCI_PREBUFFER_PATCH}")
        execute_process(
            COMMAND git apply --ignore-whitespace ${PICO_SDK_CYW43_HCI_PREBUFFER_PATCH}
            WORKING_DIRECTORY ${PICO_SDK_PATH_LOCAL}
            RESULT_VARIABLE PICO_SDK_CYW43_HCI_PREBUFFER_RESULT
            OUTPUT_VARIABLE PICO_SDK_CYW43_HCI_PREBUFFER_OUTPUT
            ERROR_VARIABLE PICO_SDK_CYW43_HCI_PREBUFFER_ERROR
        )
        if (PICO_SDK_CYW43_HCI_PREBUFFER_RESULT EQUAL 0)
            message(STATUS "Pico SDK CYW43 HCI pre-buffer patch applied successfully.")
        elseif (PICO_SDK_CYW43_HCI_PREBUFFER_ERROR MATCHES "patch does not apply")
            message(STATUS "Pico SDK CYW43 HCI pre-buffer patch already applied.")
        else ()
            message(FATAL_ERROR "Failed to apply Pico SDK CYW43 HCI pre-buffer patch: ${PICO_SDK_CYW43_HCI_PREBUFFER_ERROR}")
        endif ()
    endif ()

endfunction()
