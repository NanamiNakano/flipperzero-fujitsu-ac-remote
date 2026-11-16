#include "../ac_remote_app_i.h"

/* Remote families, with the other remotes each one covers. Picking the wrong
 * one can lock the A/C up, so they are spelled out rather than guessed at. */
static const char* const model_label[] = {
    [HvacFujitsuModelArrah2e] = "AR-RAH2E / RAH2U",
    [HvacFujitsuModelArdb1] = "AR-DB1 / DL10",
    [HvacFujitsuModelArreb1e] = "AR-REB1E / RAH1U",
    [HvacFujitsuModelArjw2] = "AR-JW2 / DL3",
    [HvacFujitsuModelArry4] = "AR-RY4",
    [HvacFujitsuModelArrew4e] = "AR-REW4E / REW1E",
};

static void ac_remote_scene_model_submenu_callback(void* context, uint32_t index) {
    AC_RemoteApp* ac_remote = context;
    uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeModelSelected, index);
    view_dispatcher_send_custom_event(ac_remote->view_dispatcher, event);
}

void ac_remote_scene_model_on_enter(void* context) {
    AC_RemoteApp* ac_remote = context;
    Submenu* submenu = ac_remote->submenu;

    submenu_set_header(submenu, "Remote type");
    for(size_t model = 0; model < COUNT_OF(model_label); model++) {
        submenu_add_item(
            submenu,
            model_label[model],
            model,
            ac_remote_scene_model_submenu_callback,
            context);
    }
    submenu_set_selected_item(submenu, ac_remote->app_state.model);

    view_dispatcher_switch_to_view(ac_remote->view_dispatcher, AC_RemoteAppViewSubmenu);
}

bool ac_remote_scene_model_on_event(void* context, SceneManagerEvent event) {
    AC_RemoteApp* ac_remote = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        uint16_t event_type;
        int16_t event_value;
        ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
        if(event_type == AC_RemoteCustomEventTypeModelSelected) {
            ac_remote->app_state.model = event_value;
            scene_manager_next_scene(ac_remote->scene_manager, AC_RemoteSceneFujitsu);
            consumed = true;
        }
    }
    return consumed;
}

void ac_remote_scene_model_on_exit(void* context) {
    AC_RemoteApp* ac_remote = context;
    submenu_reset(ac_remote->submenu);
}
