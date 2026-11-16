#include "ac_remote_app_i.h"

#include <furi.h>
#include <furi_hal.h>

static bool ac_remote_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ac_remote_app_back_event_callback(void* context) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void ac_remote_app_tick_event_callback(void* context) {
    furi_assert(context);
    AC_RemoteApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

bool ac_remote_load_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    uint32_t version = 0;
    bool success = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(!furi_string_equal(header, "AC Remote") || (version != 1)) break;
        if(!flipper_format_read_uint32(ff, "Model", &app_state->model, 1)) break;
        if(app_state->model > HvacFujitsuModelArrew4e) break;
        if(!flipper_format_read_uint32(ff, "Power", &app_state->power, 1)) break;
        if(app_state->power > HvacFujitsuPowerOff) break;
        if(!flipper_format_read_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(app_state->mode > HvacFujitsuModeHeat) break;
        if(!flipper_format_read_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(app_state->temperature > HVAC_FUJITSU_TEMPERATURE_MAX) break;
        if(app_state->temperature < HVAC_FUJITSU_TEMPERATURE_MIN) break;
        if(!flipper_format_read_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(app_state->fan > HvacFujitsuFanSpeedQuiet) break;
        if(!flipper_format_read_uint32(ff, "Swing", &app_state->swing, 1)) break;
        if(app_state->swing > HvacFujitsuSwingBoth) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(header);
    flipper_format_free(ff);
    return success;
}

bool ac_remote_store_settings(ACRemoteAppSettings* app_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;
    do {
        if(!flipper_format_file_open_always(ff, AC_REMOTE_APP_SETTINGS)) break;
        if(!flipper_format_write_header_cstr(ff, "AC Remote", 1)) break;
        if(!flipper_format_write_comment_cstr(ff, "")) break;
        if(!flipper_format_write_uint32(ff, "Model", &app_state->model, 1)) break;
        if(!flipper_format_write_uint32(ff, "Power", &app_state->power, 1)) break;
        if(!flipper_format_write_uint32(ff, "Mode", &app_state->mode, 1)) break;
        if(!flipper_format_write_uint32(ff, "Temperature", &app_state->temperature, 1)) break;
        if(!flipper_format_write_uint32(ff, "Fan", &app_state->fan, 1)) break;
        if(!flipper_format_write_uint32(ff, "Swing", &app_state->swing, 1)) break;
        success = true;
    } while(false);
    furi_record_close(RECORD_STORAGE);
    flipper_format_free(ff);
    return success;
}

AC_RemoteApp* ac_remote_app_alloc() {
    AC_RemoteApp* app = malloc(sizeof(AC_RemoteApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ac_remote_scene_handlers, app);
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ac_remote_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ac_remote_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, ac_remote_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->view_stack = view_stack_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AC_RemoteAppViewStack, view_stack_get_view(app->view_stack));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AC_RemoteAppViewSubmenu, submenu_get_view(app->submenu));

    app->ac_remote_panel = ac_remote_panel_alloc();

    if(!ac_remote_load_settings(&app->app_state)) {
        app->app_state.model = HvacFujitsuModelArrah2e;
        app->app_state.mode = HvacFujitsuModeAuto;
        app->app_state.fan = HvacFujitsuFanSpeedAuto;
        app->app_state.swing = HvacFujitsuSwingOff;
        app->app_state.power = HvacFujitsuPowerOff;
        app->app_state.temperature = 24;
    }

    scene_manager_next_scene(app->scene_manager, AC_RemoteSceneModel);
    return app;
}

void ac_remote_app_free(AC_RemoteApp* app) {
    furi_assert(app);

    ac_remote_store_settings(&app->app_state);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewStack);
    view_dispatcher_remove_view(app->view_dispatcher, AC_RemoteAppViewSubmenu);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    view_stack_free(app->view_stack);
    submenu_free(app->submenu);
    ac_remote_panel_free(app->ac_remote_panel);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t ac_remote_app(void* p) {
    UNUSED(p);
    AC_RemoteApp* ac_remote_app = ac_remote_app_alloc();
    view_dispatcher_run(ac_remote_app->view_dispatcher);
    ac_remote_app_free(ac_remote_app);
    return 0;
}
