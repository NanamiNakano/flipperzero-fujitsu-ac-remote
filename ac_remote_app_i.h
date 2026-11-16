#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_stack.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <notification/notification_messages.h>
#include <hvac_fujitsu.h>

#include "ac_remote_app.h"
#include "scenes/ac_remote_scene.h"
#include "ac_remote_custom_event.h"
#include "views/ac_remote_panel.h"
#include "fujitsu_ac_remote_icons.h"

#define AC_REMOTE_APP_SETTINGS APP_DATA_PATH("settings.txt")

typedef struct {
    uint32_t model;
    uint32_t power;
    uint32_t mode;
    uint32_t temperature;
    uint32_t fan;
    uint32_t swing;
} ACRemoteAppSettings;

struct AC_RemoteApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    ViewStack* view_stack;
    ACRemotePanel* ac_remote_panel;
    Submenu* submenu;
    ACRemoteAppSettings app_state;
    HvacFujitsu* hvac_fujitsu;
};

typedef enum {
    AC_RemoteAppViewStack,
    AC_RemoteAppViewSubmenu,
} AC_RemoteAppView;

bool ac_remote_load_settings(ACRemoteAppSettings* app_state);
bool ac_remote_store_settings(ACRemoteAppSettings* app_state);
