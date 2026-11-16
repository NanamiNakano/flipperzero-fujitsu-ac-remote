/*
Ported from IRremoteESP8266, which supports every known Fujitsu remote family:
https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Fujitsu.cpp
https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Fujitsu.h

The encoding logic, the per-model quirks and the checksums below follow that
implementation byte for byte; its unit tests in test/ir_Fujitsu_test.cpp were
used as the reference vectors.

Warning carried over from upstream: using the wrong model may lock up the A/C
unit, requiring it to be physically powered off.
*/

#include "hvac_fujitsu.h"
#define TAG "hvac_fujitsu"

/* Special command codes. Anything else makes the message a full (long) code. */
#define HVAC_FUJITSU_CMD_STAY_ON 0x00
#define HVAC_FUJITSU_CMD_TURN_ON 0x01
#define HVAC_FUJITSU_CMD_TURN_OFF 0x02
#define HVAC_FUJITSU_CMD_ECONO 0x09
#define HVAC_FUJITSU_CMD_POWERFUL 0x39
#define HVAC_FUJITSU_CMD_STEP_VERT 0x6C
#define HVAC_FUJITSU_CMD_TOGGLE_SWING_VERT 0x6D
#define HVAC_FUJITSU_CMD_STEP_HORIZ 0x79
#define HVAC_FUJITSU_CMD_TOGGLE_SWING_HORIZ 0x7A

/* Long code marker written into the Cmd byte, per remote family. */
#define HVAC_FUJITSU_LONG_CODE 0xFE
#define HVAC_FUJITSU_LONG_CODE_SHORT_FRAME 0xFC

#define HVAC_FUJITSU_PROTOCOL_DEFAULT 0x30
#define HVAC_FUJITSU_PROTOCOL_ARREW4E 0x31

#define HVAC_FUJITSU_TIMER_STOP 0x00
#define HVAC_FUJITSU_TIMER_SLEEP 0x01
#define HVAC_FUJITSU_TIMER_OFF 0x02
#define HVAC_FUJITSU_TIMER_ON 0x03

#define HVAC_FUJITSU_TEMPERATURE_OFFSET 16

/* Bit fields of the long code, as byte index, bit offset and bit width. Mirrors
 * union FujitsuProtocol in ir_Fujitsu.h. */
#define HVAC_FUJITSU_ID 2, 4, 2
#define HVAC_FUJITSU_CMD 5, 0, 8
#define HVAC_FUJITSU_REST_LENGTH 6, 0, 8
#define HVAC_FUJITSU_PROTOCOL 7, 0, 8
#define HVAC_FUJITSU_POWER 8, 0, 1
#define HVAC_FUJITSU_FAHRENHEIT 8, 1, 1
#define HVAC_FUJITSU_TEMP 8, 2, 6
#define HVAC_FUJITSU_MODE 9, 0, 3
#define HVAC_FUJITSU_CLEAN 9, 3, 1
#define HVAC_FUJITSU_TIMER_TYPE 9, 4, 2
#define HVAC_FUJITSU_FAN 10, 0, 3
#define HVAC_FUJITSU_SWING 10, 4, 2
#define HVAC_FUJITSU_OFF_TIMER_ENABLE 12, 3, 1
#define HVAC_FUJITSU_ON_TIMER_ENABLE 13, 7, 1
#define HVAC_FUJITSU_FILTER 14, 3, 1
#define HVAC_FUJITSU_UNKNOWN 14, 5, 1
#define HVAC_FUJITSU_OUTSIDE_QUIET 14, 7, 1

/* Timings, averaged from measurements upstream. */
#define HVAC_FUJITSU_FREQUENCY 38000
#define HVAC_FUJITSU_DUTY_CYCLE 0.50f

struct HvacFujitsu {
    uint8_t longcode[HVAC_FUJITSU_PACKET_SIZE];
    uint8_t shortcode[HVAC_FUJITSU_PACKET_SIZE_SHORT];
    uint8_t cmd;
    uint8_t state_length;
    uint8_t state_length_short;
    HvacFujitsuModel model;
};

static uint8_t
    hvac_fujitsu_field_get(const uint8_t* data, size_t byte, uint8_t shift, uint8_t width) {
    return (data[byte] >> shift) & ((1u << width) - 1u);
}

static void hvac_fujitsu_field_set(
    uint8_t* data,
    size_t byte,
    uint8_t shift,
    uint8_t width,
    uint8_t value) {
    const uint8_t mask = ((1u << width) - 1u) << shift;
    data[byte] = (data[byte] & ~mask) | ((value << shift) & mask);
}

/* The off/sleep timer spans byte 11 and the low 3 bits of byte 12, the on timer
 * the high 4 bits of byte 12 and the low 7 bits of byte 13. */
static uint16_t hvac_fujitsu_field_get_off_timer(const uint8_t* data) {
    return data[11] | ((uint16_t)(data[12] & 0b00000111) << 8);
}

static void hvac_fujitsu_field_set_off_timer(uint8_t* data, uint16_t minutes) {
    data[11] = minutes & 0xFF;
    hvac_fujitsu_field_set(data, 12, 0, 3, (minutes >> 8) & 0b111);
}

static uint16_t hvac_fujitsu_field_get_on_timer(const uint8_t* data) {
    return ((data[12] >> 4) & 0b1111) | ((uint16_t)(data[13] & 0b01111111) << 4);
}

static void hvac_fujitsu_field_set_on_timer(uint8_t* data, uint16_t minutes) {
    hvac_fujitsu_field_set(data, 12, 4, 4, minutes & 0b1111);
    hvac_fujitsu_field_set(data, 13, 0, 7, (minutes >> 4) & 0b1111111);
}

static uint8_t hvac_fujitsu_sum_bytes(const uint8_t* data, size_t length) {
    uint8_t sum = 0;
    for(size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

/* Set the requested (special) command part for the message. Unsupported
 * commands fall back to "stay on", leaving a full state message. */
static void hvac_fujitsu_set_cmd(HvacFujitsu* hvac, uint8_t cmd) {
    switch(cmd) {
    case HVAC_FUJITSU_CMD_TURN_OFF:
    case HVAC_FUJITSU_CMD_TURN_ON:
    case HVAC_FUJITSU_CMD_STAY_ON:
    case HVAC_FUJITSU_CMD_STEP_VERT:
    case HVAC_FUJITSU_CMD_TOGGLE_SWING_VERT:
        hvac->cmd = cmd;
        break;
    case HVAC_FUJITSU_CMD_STEP_HORIZ:
    case HVAC_FUJITSU_CMD_TOGGLE_SWING_HORIZ:
        switch(hvac->model) {
        // Only these remotes have horizontal.
        case HvacFujitsuModelArrah2e:
        case HvacFujitsuModelArjw2:
            hvac->cmd = cmd;
            break;
        default:
            hvac->cmd = HVAC_FUJITSU_CMD_STAY_ON;
        }
        break;
    case HVAC_FUJITSU_CMD_ECONO:
    case HVAC_FUJITSU_CMD_POWERFUL:
        switch(hvac->model) {
        // Only these remotes have these commands.
        case HvacFujitsuModelArreb1e:
        case HvacFujitsuModelArrew4e:
            hvac->cmd = cmd;
            break;
        default:
            hvac->cmd = HVAC_FUJITSU_CMD_STAY_ON;
        }
        break;
    default:
        hvac->cmd = HVAC_FUJITSU_CMD_STAY_ON;
    }
}

/* 10C heat shares the clean bit, so it has to be recognised to get the power
 * bit right. Nothing in this app sets it; kept to match upstream exactly. */
static bool hvac_fujitsu_get_10c_heat(const HvacFujitsu* hvac) {
    switch(hvac->model) {
    case HvacFujitsuModelArrah2e:
    case HvacFujitsuModelArrew4e:
        return hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_CLEAN) &&
               hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_POWER) &&
               hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_MODE) == HvacFujitsuModeFan &&
               hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_FAN) ==
                   HvacFujitsuFanSpeedAuto &&
               hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_SWING) == HvacFujitsuSwingOff;
    default:
        return false;
    }
}

static uint8_t hvac_fujitsu_get_timer_type(const HvacFujitsu* hvac) {
    switch(hvac->model) {
    // These models seem to have timer support.
    case HvacFujitsuModelArrah2e:
    case HvacFujitsuModelArreb1e:
        return hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_TIMER_TYPE);
    default:
        return HVAC_FUJITSU_TIMER_STOP;
    }
}

static uint16_t hvac_fujitsu_get_on_timer(const HvacFujitsu* hvac) {
    if(hvac_fujitsu_get_timer_type(hvac) == HVAC_FUJITSU_TIMER_ON) {
        return hvac_fujitsu_field_get_on_timer(hvac->longcode);
    }
    return 0;
}

static uint16_t hvac_fujitsu_get_off_sleep_timer(const HvacFujitsu* hvac) {
    switch(hvac_fujitsu_get_timer_type(hvac)) {
    case HVAC_FUJITSU_TIMER_OFF:
    case HVAC_FUJITSU_TIMER_SLEEP:
        return hvac_fujitsu_field_get_off_timer(hvac->longcode);
    default:
        return 0;
    }
}

/* Update the length (size) of the state code for the current configuration.
 * Returns true if a long code is to be used, false for a short one. */
static bool hvac_fujitsu_update_use_long_or_short(HvacFujitsu* hvac) {
    bool full_cmd = false;
    switch(hvac->cmd) {
    case HVAC_FUJITSU_CMD_TURN_OFF:
    case HVAC_FUJITSU_CMD_ECONO:
    case HVAC_FUJITSU_CMD_POWERFUL:
    case HVAC_FUJITSU_CMD_STEP_VERT:
    case HVAC_FUJITSU_CMD_TOGGLE_SWING_VERT:
    case HVAC_FUJITSU_CMD_STEP_HORIZ:
    case HVAC_FUJITSU_CMD_TOGGLE_SWING_HORIZ:
        hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_CMD, hvac->cmd);
        break;
    default:
        switch(hvac->model) {
        case HvacFujitsuModelArry4:
        case HvacFujitsuModelArrah2e:
        case HvacFujitsuModelArreb1e:
        case HvacFujitsuModelArrew4e:
            hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_CMD, HVAC_FUJITSU_LONG_CODE);
            break;
        case HvacFujitsuModelArdb1:
        case HvacFujitsuModelArjw2:
            hvac_fujitsu_field_set(
                hvac->longcode, HVAC_FUJITSU_CMD, HVAC_FUJITSU_LONG_CODE_SHORT_FRAME);
            break;
        }
        full_cmd = true;
        break;
    }
    return full_cmd;
}

/* Calculate and set the checksum values for the internal state. */
static void hvac_fujitsu_checksum(HvacFujitsu* hvac) {
    if(hvac_fujitsu_update_use_long_or_short(hvac)) { // Is it going to be a long code?
        // Nr. of bytes in the message after this byte.
        hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_REST_LENGTH, hvac->state_length - 7);
        hvac_fujitsu_field_set(
            hvac->longcode,
            HVAC_FUJITSU_PROTOCOL,
            (hvac->model == HvacFujitsuModelArrew4e) ? HVAC_FUJITSU_PROTOCOL_ARREW4E :
                                                       HVAC_FUJITSU_PROTOCOL_DEFAULT);
        hvac_fujitsu_field_set(
            hvac->longcode,
            HVAC_FUJITSU_POWER,
            (hvac->cmd == HVAC_FUJITSU_CMD_TURN_ON) || hvac_fujitsu_get_10c_heat(hvac));

        // These values depend on model
        if(hvac->model != HvacFujitsuModelArreb1e && hvac->model != HvacFujitsuModelArrew4e) {
            hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_OUTSIDE_QUIET, 0);
            if(hvac->model != HvacFujitsuModelArrah2e) {
                hvac_fujitsu_field_set(
                    hvac->longcode, HVAC_FUJITSU_TIMER_TYPE, HVAC_FUJITSU_TIMER_STOP);
            }
        }
        if(hvac->model != HvacFujitsuModelArry4) {
            switch(hvac->model) {
            case HvacFujitsuModelArrah2e:
            case HvacFujitsuModelArrew4e:
                break;
            default:
                hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_CLEAN, false);
            }
            hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_FILTER, false);
        }
        // Set the On/Off/Sleep timer Nr of mins.
        const uint16_t off_timer = hvac_fujitsu_get_off_sleep_timer(hvac);
        const uint16_t on_timer = hvac_fujitsu_get_on_timer(hvac);
        hvac_fujitsu_field_set_off_timer(hvac->longcode, off_timer);
        hvac_fujitsu_field_set_on_timer(hvac->longcode, on_timer);
        // Enable bits for the Off/Sleep and On timers
        hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_OFF_TIMER_ENABLE, off_timer > 0);
        hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_ON_TIMER_ENABLE, on_timer > 0);

        uint8_t checksum = 0;
        uint8_t checksum_complement = 0;
        switch(hvac->model) {
        case HvacFujitsuModelArdb1:
        case HvacFujitsuModelArjw2:
            // These remotes step the louvre instead of carrying a swing state.
            hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_SWING, HvacFujitsuSwingOff);
            checksum = hvac_fujitsu_sum_bytes(hvac->longcode, hvac->state_length - 1);
            checksum_complement = 0x9B;
            break;
        case HvacFujitsuModelArreb1e:
        case HvacFujitsuModelArrah2e:
        case HvacFujitsuModelArry4:
            hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_UNKNOWN, 1);
            // FALL THRU
        default:
            checksum = hvac_fujitsu_sum_bytes(
                hvac->longcode + hvac->state_length_short,
                hvac->state_length - hvac->state_length_short - 1);
        }
        // and negate the checksum and store it in the last byte.
        hvac->longcode[hvac->state_length - 1] = checksum_complement - checksum;
    } else { // short codes
        memcpy(hvac->shortcode, hvac->longcode, hvac->state_length_short);
        switch(hvac->model) {
        case HvacFujitsuModelArry4:
        case HvacFujitsuModelArrah2e:
        case HvacFujitsuModelArreb1e:
        case HvacFujitsuModelArrew4e:
            // The last byte is the inverse of penultimate byte
            hvac->shortcode[hvac->state_length_short - 1] =
                ~hvac->shortcode[hvac->state_length_short - 2];
            break;
        default:
            break; // We don't need to do anything for the others.
        }
    }
}

/* Is the current binary state representation a long or a short code? */
static bool hvac_fujitsu_is_long_code(const HvacFujitsu* hvac) {
    const uint8_t cmd = hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_CMD);
    return cmd == HVAC_FUJITSU_LONG_CODE || cmd == HVAC_FUJITSU_LONG_CODE_SHORT_FRAME;
}

/* Get the length (size) of the state code for the current configuration. */
static uint8_t hvac_fujitsu_get_state_length(HvacFujitsu* hvac) {
    return hvac_fujitsu_update_use_long_or_short(hvac) ? hvac->state_length :
                                                         hvac->state_length_short;
}

/* Reset the state of the remote to a known good state/sequence. */
static void hvac_fujitsu_state_reset(HvacFujitsu* hvac) {
    memset(hvac->longcode, 0, sizeof(hvac->longcode));
    hvac_fujitsu_set_temperature(hvac, 24);
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_FAN, HvacFujitsuFanSpeedHigh);
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_MODE, HvacFujitsuModeCool);
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_SWING, HvacFujitsuSwingBoth);
    hvac->cmd = HVAC_FUJITSU_CMD_TURN_ON;
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_FILTER, false);
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_CLEAN, false);
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_TIMER_TYPE, HVAC_FUJITSU_TIMER_STOP);
    hvac_fujitsu_field_set_on_timer(hvac->longcode, 0);
    hvac_fujitsu_field_set_off_timer(hvac->longcode, 0);
    hvac->longcode[0] = 0x14;
    hvac->longcode[1] = 0x63;
    hvac->longcode[3] = 0x10;
    hvac->longcode[4] = 0x10;
}

void hvac_fujitsu_set_model(HvacFujitsu* hvac, HvacFujitsuModel model) {
    hvac->model = model;
    switch(model) {
    case HvacFujitsuModelArdb1:
    case HvacFujitsuModelArjw2:
        hvac->state_length = HVAC_FUJITSU_PACKET_SIZE - 1;
        hvac->state_length_short = HVAC_FUJITSU_PACKET_SIZE_SHORT - 1;
        break;
    case HvacFujitsuModelArry4:
    case HvacFujitsuModelArrah2e:
    case HvacFujitsuModelArreb1e:
    default:
        hvac->state_length = HVAC_FUJITSU_PACKET_SIZE;
        hvac->state_length_short = HVAC_FUJITSU_PACKET_SIZE_SHORT;
    }
}

HvacFujitsu* hvac_fujitsu_init(HvacFujitsuModel model) {
    HvacFujitsu* hvac = malloc(sizeof(HvacFujitsu));
    hvac_fujitsu_set_model(hvac, model);
    hvac_fujitsu_state_reset(hvac);
    return hvac;
}

void hvac_fujitsu_deinit(HvacFujitsu* hvac) {
    free(hvac);
}

void hvac_fujitsu_power(HvacFujitsu* hvac, HvacFujitsuPower power) {
    hvac_fujitsu_set_cmd(
        hvac,
        (power == HvacFujitsuPowerOn) ? HVAC_FUJITSU_CMD_TURN_ON : HVAC_FUJITSU_CMD_TURN_OFF);
}

void hvac_fujitsu_set_mode(HvacFujitsu* hvac, HvacFujitsuMode mode) {
    // Set the mode to maximum if out of range.
    hvac_fujitsu_field_set(
        hvac->longcode,
        HVAC_FUJITSU_MODE,
        (mode > HvacFujitsuModeHeat) ? HvacFujitsuModeHeat : mode);
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STAY_ON); // No special command involved.
}

void hvac_fujitsu_set_fan_speed(HvacFujitsu* hvac, HvacFujitsuFanSpeed speed) {
    // Set the fan to maximum if out of range.
    hvac_fujitsu_field_set(
        hvac->longcode,
        HVAC_FUJITSU_FAN,
        (speed > HvacFujitsuFanSpeedQuiet) ? HvacFujitsuFanSpeedHigh : speed);
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STAY_ON); // No special command involved.
}

void hvac_fujitsu_set_temperature(HvacFujitsu* hvac, uint8_t temp) {
    uint8_t cur_temp;
    if(temp > HVAC_FUJITSU_TEMPERATURE_MAX) {
        cur_temp = HVAC_FUJITSU_TEMPERATURE_MAX;
    } else if(temp < HVAC_FUJITSU_TEMPERATURE_MIN) {
        cur_temp = HVAC_FUJITSU_TEMPERATURE_MIN;
    } else {
        cur_temp = temp;
    };
    // Only the ARREW4E family has native Fahrenheit support; always use Celsius.
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_FAHRENHEIT, false);
    if(hvac->model == HvacFujitsuModelArrew4e) {
        hvac_fujitsu_field_set(
            hvac->longcode,
            HVAC_FUJITSU_TEMP,
            (cur_temp - HVAC_FUJITSU_TEMPERATURE_OFFSET / 2) * 2);
    } else {
        hvac_fujitsu_field_set(
            hvac->longcode, HVAC_FUJITSU_TEMP, (cur_temp - HVAC_FUJITSU_TEMPERATURE_OFFSET) * 4);
    }
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STAY_ON); // No special command involved.
}

void hvac_fujitsu_set_swing(HvacFujitsu* hvac, HvacFujitsuSwing swing) {
    // Set the mode to max if out of range
    const HvacFujitsuSwing max_swing = hvac_fujitsu_model_max_swing(hvac->model);
    hvac_fujitsu_field_set(
        hvac->longcode, HVAC_FUJITSU_SWING, (swing > max_swing) ? max_swing : swing);
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STAY_ON); // No special command involved.
}

void hvac_fujitsu_step_vert(HvacFujitsu* hvac) {
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STEP_VERT);
}

void hvac_fujitsu_step_horiz(HvacFujitsu* hvac) {
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_STEP_HORIZ);
}

void hvac_fujitsu_toggle_swing_vert(HvacFujitsu* hvac) {
    // Toggle the current setting, then set the matching special command.
    hvac_fujitsu_set_swing(
        hvac, hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_SWING) ^ HvacFujitsuSwingVert);
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_TOGGLE_SWING_VERT);
}

void hvac_fujitsu_toggle_swing_horiz(HvacFujitsu* hvac) {
    hvac_fujitsu_set_swing(
        hvac, hvac_fujitsu_field_get(hvac->longcode, HVAC_FUJITSU_SWING) ^ HvacFujitsuSwingHoriz);
    hvac_fujitsu_set_cmd(hvac, HVAC_FUJITSU_CMD_TOGGLE_SWING_HORIZ);
}

void hvac_fujitsu_set_id(HvacFujitsu* hvac, uint8_t id) {
    hvac_fujitsu_field_set(hvac->longcode, HVAC_FUJITSU_ID, id);
}

HvacFujitsuSwing hvac_fujitsu_model_max_swing(HvacFujitsuModel model) {
    switch(model) {
    // No Horizontal support.
    case HvacFujitsuModelArdb1:
    case HvacFujitsuModelArreb1e:
    case HvacFujitsuModelArry4:
        return HvacFujitsuSwingVert;
    // Has Horizontal support.
    case HvacFujitsuModelArrah2e:
    case HvacFujitsuModelArjw2:
    default:
        return HvacFujitsuSwingBoth;
    }
}

bool hvac_fujitsu_model_has_step_horiz(HvacFujitsuModel model) {
    // Mirrors the models hvac_fujitsu_set_cmd() accepts the horizontal commands for.
    return model == HvacFujitsuModelArrah2e || model == HvacFujitsuModelArjw2;
}

bool hvac_fujitsu_model_has_swing_state(HvacFujitsuModel model) {
    // The long code of these two carries no swing field, see the checksum above.
    return model != HvacFujitsuModelArdb1 && model != HvacFujitsuModelArjw2;
}

const char* hvac_fujitsu_model_name(HvacFujitsuModel model) {
    switch(model) {
    case HvacFujitsuModelArdb1:
        return "AR-DB1";
    case HvacFujitsuModelArreb1e:
        return "AR-REB1E";
    case HvacFujitsuModelArjw2:
        return "AR-JW2";
    case HvacFujitsuModelArry4:
        return "AR-RY4";
    case HvacFujitsuModelArrew4e:
        return "AR-REW4E";
    case HvacFujitsuModelArrah2e:
    default:
        return "AR-RAH2E";
    }
}

void hvac_fujitsu_send(HvacFujitsu* hvac) {
    hvac_fujitsu_checksum(hvac);
    const uint8_t* data = hvac_fujitsu_is_long_code(hvac) ? hvac->longcode : hvac->shortcode;
    const size_t length = hvac_fujitsu_get_state_length(hvac);

    // Header, a mark and a space per bit, then the footer mark and the gap.
    const size_t timings_count = 2 + length * 8 * 2 + 2;
    furi_check(timings_count <= MAX_TIMINGS_AMOUNT);

    uint32_t* timings = malloc(sizeof(uint32_t) * timings_count);
    size_t timings_size = 0;

    timings[timings_size++] = HVAC_FUJITSU_HDR_MARK;
    timings[timings_size++] = HVAC_FUJITSU_HDR_SPACE;
    for(size_t i = 0; i < length; i++) {
        for(uint8_t mask = 0b00000001; mask > 0; mask <<= 1) { // Least significant bit first
            timings[timings_size++] = HVAC_FUJITSU_BIT_MARK;
            timings[timings_size++] =
                (data[i] & mask) ? HVAC_FUJITSU_ONE_SPACE : HVAC_FUJITSU_ZERO_SPACE;
        }
    }
    timings[timings_size++] = HVAC_FUJITSU_BIT_MARK;
    timings[timings_size++] = HVAC_FUJITSU_MIN_GAP;

    infrared_send_raw_ext(
        timings, timings_size, true, HVAC_FUJITSU_FREQUENCY, HVAC_FUJITSU_DUTY_CYCLE);
    free(timings);
}
