#include "../ac_remote_app_i.h"

/* Panel layout mirrors the AR-RAH2U keypad: START/STOP, MASTER CONTROL,
 * FAN CONTROL, SET TEMP, the two airflow-direction SET buttons and SWING. */
typedef enum {
    button_power,
    button_mode,
    button_temp_up,
    button_fan,
    button_temp_down,
    button_swing,
    button_vane_vert,
    button_vane_horiz,
    label_model,
    label_temperature,
} button_id;

static const Icon* power[2][2] = {
    [HvacFujitsuPowerOn] = {&I_power_on_19x20, &I_power_on_hover_19x20},
    [HvacFujitsuPowerOff] = {&I_power_off_19x20, &I_power_off_hover_19x20}};
static const Icon* mode[5][2] = {
    [HvacFujitsuModeAuto] = {&I_auto_19x20, &I_auto_hover_19x20},
    [HvacFujitsuModeCool] = {&I_mode_cool_19x20, &I_mode_cool_hover_19x20},
    [HvacFujitsuModeDry] = {&I_mode_dry_19x20, &I_mode_dry_hover_19x20},
    [HvacFujitsuModeFan] = {&I_mode_fan_19x20, &I_mode_fan_hover_19x20},
    [HvacFujitsuModeHeat] = {&I_mode_heat_19x20, &I_mode_heat_hover_19x20}};
static const Icon* fan[5][2] = {
    [HvacFujitsuFanSpeedAuto] = {&I_auto_19x20, &I_auto_hover_19x20},
    [HvacFujitsuFanSpeedHigh] = {&I_fan_speed_3_19x20, &I_fan_speed_3_hover_19x20},
    [HvacFujitsuFanSpeedMed] = {&I_fan_speed_2_19x20, &I_fan_speed_2_hover_19x20},
    [HvacFujitsuFanSpeedLow] = {&I_fan_speed_1_19x20, &I_fan_speed_1_hover_19x20},
    [HvacFujitsuFanSpeedQuiet] = {&I_fan_silent_19x20, &I_fan_silent_hover_19x20}};
static const Icon* swing[4][2] = {
    [HvacFujitsuSwingOff] = {&I_swing_off_19x20, &I_swing_off_hover_19x20},
    [HvacFujitsuSwingVert] = {&I_swing_vert_19x20, &I_swing_vert_hover_19x20},
    [HvacFujitsuSwingHoriz] = {&I_swing_horiz_19x20, &I_swing_horiz_hover_19x20},
    [HvacFujitsuSwingBoth] = {&I_swing_both_19x20, &I_swing_both_hover_19x20}};

static char buffer[4] = {0};

void ac_remote_scene_universal_common_item_callback(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonSelected, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_scene_universal_common_item_callback_long(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event =
        ac_remote_custom_event_pack(AC_RemoteCustomEventTypeButtonSelectedLong, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_scene_fujitsu_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    const HvacFujitsuModel model = ac_remote->app_state.model;
    ac_remote->hvac_fujitsu = hvac_fujitsu_init(model);

    // The swing range depends on the remote, so a stored value may not fit.
    const HvacFujitsuSwing max_swing = hvac_fujitsu_model_max_swing(model);
    if(ac_remote->app_state.swing > max_swing) {
        ac_remote->app_state.swing = max_swing;
    }

    hvac_fujitsu_set_mode(ac_remote->hvac_fujitsu, ac_remote->app_state.mode);
    hvac_fujitsu_set_fan_speed(ac_remote->hvac_fujitsu, ac_remote->app_state.fan);
    if(hvac_fujitsu_model_has_swing_state(model)) {
        hvac_fujitsu_set_swing(ac_remote->hvac_fujitsu, ac_remote->app_state.swing);
    }
    hvac_fujitsu_set_temperature(ac_remote->hvac_fujitsu, ac_remote->app_state.temperature);

    view_stack_add_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reserve(ac_remote_panel, 3, 4);

    // START/STOP
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_power,
        0,
        0,
        6,
        17,
        power[ac_remote->app_state.power][0],
        power[ac_remote->app_state.power][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 3, 39, &I_label_power_25x5);
    // MASTER CONTROL
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_mode,
        1,
        0,
        39,
        17,
        mode[ac_remote->app_state.mode][0],
        mode[ac_remote->app_state.mode][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 40, 39, &I_label_mode_20x5);
    // SET TEMP.
    ac_remote_panel_add_icon(ac_remote_panel, 0, 59, &I_frame_30x39);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_up,
        0,
        1,
        3,
        47,
        &I_tempup_24x21,
        &I_tempup_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_temp_down,
        0,
        2,
        3,
        89,
        &I_tempdown_24x21,
        &I_tempdown_hover_24x21,
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    // FAN CONTROL
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_fan,
        1,
        1,
        39,
        50,
        fan[ac_remote->app_state.fan][0],
        fan[ac_remote->app_state.fan][1],
        ac_remote_scene_universal_common_item_callback,
        NULL,
        context);
    ac_remote_panel_add_icon(ac_remote_panel, 43, 72, &I_label_fan_14x5);
    // SWING, on the remotes that carry a swing setting at all.
    if(hvac_fujitsu_model_has_swing_state(model)) {
        ac_remote_panel_add_item(
            ac_remote_panel,
            button_swing,
            1,
            2,
            39,
            83,
            swing[ac_remote->app_state.swing][0],
            swing[ac_remote->app_state.swing][1],
            ac_remote_scene_universal_common_item_callback,
            NULL,
            context);
        ac_remote_panel_add_icon(ac_remote_panel, 36, 105, &I_label_swing_24x5);
    }
    // The two airflow direction SET buttons. Short press steps the vane one
    // notch, long press starts or stops the swing on that axis.
    ac_remote_panel_add_item(
        ac_remote_panel,
        button_vane_vert,
        0,
        3,
        1,
        115,
        &I_vane_vert_19x11,
        &I_vane_vert_hover_19x11,
        ac_remote_scene_universal_common_item_callback,
        ac_remote_scene_universal_common_item_callback_long,
        context);
    if(hvac_fujitsu_model_has_step_horiz(model)) {
        ac_remote_panel_add_item(
            ac_remote_panel,
            button_vane_horiz,
            1,
            3,
            22,
            115,
            &I_vane_horiz_19x11,
            &I_vane_horiz_hover_19x11,
            ac_remote_scene_universal_common_item_callback,
            ac_remote_scene_universal_common_item_callback_long,
            context);
    }

    ac_remote_panel_add_label(
        ac_remote_panel, label_model, 6, 11, FontPrimary, hvac_fujitsu_model_name(model));

    snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
    ac_remote_panel_add_label(ac_remote_panel, label_temperature, 4, 82, FontKeyboard, buffer);

    view_set_orientation(view_stack_get_view(ac_remote->view_stack), ViewOrientationVertical);
    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewStack);
}

bool ac_remote_scene_fujitsu_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        uint16_t event_type;
        int16_t event_value;
        ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
        if(event_type == AC_RemoteCustomEventTypeSendCommand) {
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, &sequence_blink_white_100);
            hvac_fujitsu_send(ac_remote->hvac_fujitsu);
            notification_message(notifications, &sequence_blink_stop);
            furi_record_close(RECORD_NOTIFICATION);
        } else if(event_type == AC_RemoteCustomEventTypeButtonSelected) {
            switch(event_value) {
            case button_power:
                ac_remote->app_state.power = (ac_remote->app_state.power == HvacFujitsuPowerOn) ?
                                                 HvacFujitsuPowerOff :
                                                 HvacFujitsuPowerOn;
                hvac_fujitsu_power(ac_remote->hvac_fujitsu, ac_remote->app_state.power);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_power,
                    power[ac_remote->app_state.power][0],
                    power[ac_remote->app_state.power][1]);
                break;
            case button_mode:
                ac_remote->app_state.mode++;
                if(ac_remote->app_state.mode > HvacFujitsuModeHeat) {
                    ac_remote->app_state.mode = HvacFujitsuModeAuto;
                }
                hvac_fujitsu_set_mode(ac_remote->hvac_fujitsu, ac_remote->app_state.mode);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_mode,
                    mode[ac_remote->app_state.mode][0],
                    mode[ac_remote->app_state.mode][1]);
                break;
            case button_fan:
                ac_remote->app_state.fan++;
                if(ac_remote->app_state.fan > HvacFujitsuFanSpeedQuiet) {
                    ac_remote->app_state.fan = HvacFujitsuFanSpeedAuto;
                }
                hvac_fujitsu_set_fan_speed(ac_remote->hvac_fujitsu, ac_remote->app_state.fan);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_fan,
                    fan[ac_remote->app_state.fan][0],
                    fan[ac_remote->app_state.fan][1]);
                break;
            case button_swing:
                ac_remote->app_state.swing++;
                if(ac_remote->app_state.swing >
                   hvac_fujitsu_model_max_swing(ac_remote->app_state.model)) {
                    ac_remote->app_state.swing = HvacFujitsuSwingOff;
                }
                hvac_fujitsu_set_swing(ac_remote->hvac_fujitsu, ac_remote->app_state.swing);
                ac_remote_panel_item_set_icons(
                    ac_remote_panel,
                    button_swing,
                    swing[ac_remote->app_state.swing][0],
                    swing[ac_remote->app_state.swing][1]);
                break;
            case button_vane_vert:
                hvac_fujitsu_step_vert(ac_remote->hvac_fujitsu);
                break;
            case button_vane_horiz:
                hvac_fujitsu_step_horiz(ac_remote->hvac_fujitsu);
                break;
            case button_temp_up:
                if(ac_remote->app_state.temperature < HVAC_FUJITSU_TEMPERATURE_MAX) {
                    ac_remote->app_state.temperature++;
                }
                hvac_fujitsu_set_temperature(
                    ac_remote->hvac_fujitsu, ac_remote->app_state.temperature);
                snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
                ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
                break;
            case button_temp_down:
                if(ac_remote->app_state.temperature > HVAC_FUJITSU_TEMPERATURE_MIN) {
                    ac_remote->app_state.temperature--;
                }
                hvac_fujitsu_set_temperature(
                    ac_remote->hvac_fujitsu, ac_remote->app_state.temperature);
                snprintf(buffer, sizeof(buffer), "%ld", ac_remote->app_state.temperature);
                ac_remote_panel_label_set_string(ac_remote_panel, label_temperature, buffer);
                break;
            default:
                break;
            }
            uint32_t send = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, 0);
            view_dispatcher_send_custom_event(ac_remote->view_dispatcher, send);
        } else if(event_type == AC_RemoteCustomEventTypeButtonSelectedLong) {
            switch(event_value) {
            case button_vane_vert:
                hvac_fujitsu_toggle_swing_vert(ac_remote->hvac_fujitsu);
                break;
            case button_vane_horiz:
                hvac_fujitsu_toggle_swing_horiz(ac_remote->hvac_fujitsu);
                break;
            default:
                break;
            }
            uint32_t send = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeSendCommand, 0);
            view_dispatcher_send_custom_event(ac_remote->view_dispatcher, send);
        }
        consumed = true;
    }
    return consumed;
}

void ac_remote_scene_fujitsu_on_exit(void* context) {
    AC_RemoteApp* ac_remote = context;
    ACRemotePanel* ac_remote_panel = ac_remote->ac_remote_panel;
    hvac_fujitsu_deinit(ac_remote->hvac_fujitsu);
    view_stack_remove_view(ac_remote->view_stack, ac_remote_panel_get_view(ac_remote_panel));
    ac_remote_panel_reset(ac_remote_panel);
}
