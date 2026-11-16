#pragma once

#include <infrared_transmit.h>
#include <infrared_worker.h>

#include "furi_hal.h"

#define HVAC_FUJITSU_HDR_MARK 3324
#define HVAC_FUJITSU_HDR_SPACE 1574
#define HVAC_FUJITSU_BIT_MARK 448
#define HVAC_FUJITSU_ONE_SPACE 1182
#define HVAC_FUJITSU_ZERO_SPACE 390
#define HVAC_FUJITSU_MIN_GAP 8100

#define HVAC_FUJITSU_PACKET_SIZE 16
#define HVAC_FUJITSU_PACKET_SIZE_SHORT 7

#define HVAC_FUJITSU_TEMPERATURE_MIN 16
#define HVAC_FUJITSU_TEMPERATURE_MAX 30

/* Remote families, as emulated by IRremoteESP8266's fujitsu_ac_remote_model_t.
 * Enumerated from zero so the UI can cycle and index by value. */
typedef enum {
    HvacFujitsuModelArrah2e, // AR-RAH2E, AR-RAH2U, AR-RAC1E, AR-RAE1E, AR-RCE1E, AR-REG1U, AR-RCL1E
    HvacFujitsuModelArdb1, // AR-DB1, AR-DL10, AR-JW17, AR-JW19
    HvacFujitsuModelArreb1e, // AR-REB1E, AR-REB4E, AR-RAH1U
    HvacFujitsuModelArjw2, // AR-JW2, AR-DL3
    HvacFujitsuModelArry4, // AR-RY4
    HvacFujitsuModelArrew4e, // AR-REW4E, AR-REW1E
} HvacFujitsuModel;

typedef enum {
    HvacFujitsuPowerOn,
    HvacFujitsuPowerOff,
} HvacFujitsuPower;

typedef enum {
    HvacFujitsuModeAuto,
    HvacFujitsuModeCool,
    HvacFujitsuModeDry,
    HvacFujitsuModeFan,
    HvacFujitsuModeHeat,
} HvacFujitsuMode;

typedef enum {
    HvacFujitsuFanSpeedAuto,
    HvacFujitsuFanSpeedHigh,
    HvacFujitsuFanSpeedMed,
    HvacFujitsuFanSpeedLow,
    HvacFujitsuFanSpeedQuiet,
} HvacFujitsuFanSpeed;

typedef enum {
    HvacFujitsuSwingOff,
    HvacFujitsuSwingVert,
    HvacFujitsuSwingHoriz,
    HvacFujitsuSwingBoth,
} HvacFujitsuSwing;

typedef struct HvacFujitsu HvacFujitsu;

HvacFujitsu* hvac_fujitsu_init(HvacFujitsuModel model);
void hvac_fujitsu_deinit(HvacFujitsu* hvac);
void hvac_fujitsu_set_model(HvacFujitsu* hvac, HvacFujitsuModel model);
void hvac_fujitsu_power(HvacFujitsu* hvac, HvacFujitsuPower power);
void hvac_fujitsu_set_mode(HvacFujitsu* hvac, HvacFujitsuMode mode);
void hvac_fujitsu_set_fan_speed(HvacFujitsu* hvac, HvacFujitsuFanSpeed speed);
void hvac_fujitsu_set_temperature(HvacFujitsu* hvac, uint8_t temp);
void hvac_fujitsu_set_swing(HvacFujitsu* hvac, HvacFujitsuSwing swing);
void hvac_fujitsu_step_vert(HvacFujitsu* hvac);
void hvac_fujitsu_step_horiz(HvacFujitsu* hvac);
void hvac_fujitsu_toggle_swing_vert(HvacFujitsu* hvac);
void hvac_fujitsu_toggle_swing_horiz(HvacFujitsu* hvac);
void hvac_fujitsu_set_id(HvacFujitsu* hvac, uint8_t id);
void hvac_fujitsu_send(HvacFujitsu* hvac);

/** Highest swing value the model transmits in its long code. */
HvacFujitsuSwing hvac_fujitsu_model_max_swing(HvacFujitsuModel model);

/** True if the model carries the swing setting as state, false if it only steps the louvre. */
bool hvac_fujitsu_model_has_swing_state(HvacFujitsuModel model);

/** True if the model accepts the horizontal step and toggle commands. */
bool hvac_fujitsu_model_has_step_horiz(HvacFujitsuModel model);

/** Remote family name, for display. */
const char* hvac_fujitsu_model_name(HvacFujitsuModel model);
