#include "light_switch.h"

LightSwitchApp::LightSwitchApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    motor_config = PB_SmartKnobConfig{
        current_position,
        0,
        current_position,
        0,
        3, // Changed from 1 to 3 for 4 steps (0,1,2,3)
        30 * PI / 180,
        1,   // detent strength
        1,   // endstop strength
        1.1, // snap point
        "",
        0,
        {},
        0,
        27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_lightbulb_outline);
    LV_IMG_DECLARE(x40_lightbulb_outline);
    // LV_IMG_DECLARE(x80_lightbulb_filled);
    LV_IMG_DECLARE(x80_fan_filled);

    big_icon = x80_lightbulb_outline;
    big_icon_active = x80_fan_filled;
    small_icon = x40_lightbulb_outline;

    initScreen();
}

void LightSwitchApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    // Constants for arc positioning
    const int ARC_TOTAL_SPAN = 25;                 // Total degrees each arc section spans
    const int ARC_GAP = 10;                        // Size of gap in degrees
    const int ARC_SIZE = ARC_TOTAL_SPAN - ARC_GAP; // Actual size of each arc

    // Create all 4 arcs
    for (int i = 0; i < 4; i++)
    {
        arcs[i] = lv_arc_create(screen);
        lv_obj_set_size(arcs[i], 210, 210);

        // Calculate positioning
        // Base rotation is still centered at 270 degrees (bottom)
        int base_rotation = 270 - (ARC_TOTAL_SPAN * 2); // Start of first arc
        int start_angle = i * ARC_TOTAL_SPAN;

        lv_arc_set_rotation(arcs[i], base_rotation);
        lv_arc_set_bg_angles(arcs[i], start_angle, start_angle + ARC_SIZE); // Leave gap between arcs
        lv_arc_set_value(arcs[i], 100);                                     // Full arc
        lv_obj_center(arcs[i]);

        // Remove the knob part
        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);

        // Set arc width
        lv_obj_set_style_arc_width(arcs[i], 24, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arcs[i], 24, LV_PART_INDICATOR);

        // Set initial colors
        lv_obj_set_style_arc_color(arcs[i], arc_inactive_color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arcs[i], arc_inactive_color, LV_PART_INDICATOR);

        // Optional: Round the ends of the arcs for a smoother look
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_INDICATOR);
    }

    // Create light bulb icon and label (rest of initialization remains the same)
    light_bulb = lv_img_create(screen);
    lv_img_set_src(light_bulb, &big_icon);
    lv_obj_set_style_img_recolor_opa(light_bulb, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(light_bulb, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
    lv_obj_center(light_bulb);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, friendly_name);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -48);
}

// float rubberBandEasing(float value, float bound)
// {
//     if (abs(value) > bound)
//     {
//         return bound + (cbrt(abs(value) - bound)) * (value > 0 ? 1 : -1);
//     }
//     return value;
// }

// Define a global or class-level variable to track the previous sub_position_unit
float previous_sub_position_unit = 0.0f;

EntityStateUpdate LightSwitchApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return new_state;
    }

    current_position = state.current_position;
    sub_position_unit = state.sub_position_unit;

    motor_config.position_nonce = current_position;
    motor_config.position = current_position;

    if (last_position != current_position || first_run)
    {
        {
            SemaphoreGuard lock(mutex_);

            // Update arcs based on position
            for (int i = 0; i < 4; i++)
            {
                // Arc is active if its position is less than or equal to current position
                lv_color_t color = (i <= current_position) ? arc_active_color : arc_inactive_color;
                lv_obj_set_style_arc_color(arcs[i], color, LV_PART_MAIN);
                lv_obj_set_style_arc_color(arcs[i], color, LV_PART_INDICATOR);
            }

            // Update light bulb and background
            if (current_position == 0)
            {
                lv_img_set_src(light_bulb, &big_icon);
                lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
            }
            else
            {
                lv_img_set_src(light_bulb, &big_icon_active);
                uint8_t brightness = ((current_position + 1) * 63); // Adjusted for 4 levels
                lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(brightness / 3, brightness / 3, 0), 0);
            }
        }

        // Update state for external systems
        if (first_run)
        {
            sprintf(new_state.app_id, "%s", app_id);
            sprintf(new_state.entity_id, "%s", entity_id);

            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "level", current_position);

            char *json_string = cJSON_PrintUnformatted(json);
            sprintf(new_state.state, "%s", json_string);

            cJSON_free(json_string);
            cJSON_Delete(json);

            new_state.changed = true;
            sprintf(new_state.app_slug, "%s", APP_SLUG_LIGHT_SWITCH);
        }

        last_position = current_position;
    }

    first_run = true;
    return new_state;
}

void LightSwitchApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *new_state = cJSON_Parse(mqtt_state_update.state);
    cJSON *on = cJSON_GetObjectItem(new_state, "on");

    if (on != NULL)
    {
        current_position = on->valueint;
        motor_config.position = current_position;
        motor_config.position_nonce = current_position + 1; // TODO: LOOK INTO THIS WEIRD WORK AROUND (NEEDED FOR LIGHT SWITCH NOT TO TOGGLE STATE OF LIGHT TO OFF IF SET TO ON IN HASS, KINDA)
        state_sent_from_hass = true;
    }

    cJSON_Delete(new_state);

    last_position = current_position;

    {
        SemaphoreGuard lock(mutex_);

        if (current_position == 0)
        {
            lv_img_set_src(light_bulb, &big_icon);
            lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
            lv_obj_set_style_arc_color(arc_, dark_arc_bg, LV_PART_MAIN);
        }
        else
        {
            lv_img_set_src(light_bulb, &big_icon_active);
            lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0xFF, 0x9E, 0x00), 0);
            lv_obj_set_style_arc_color(arc_, lv_color_mix(dark_arc_bg, LV_COLOR_MAKE(0xFF, 0x9E, 0x00), 128), LV_PART_MAIN);
        }

        if (current_position == 0)
        {
            lv_arc_set_value(arc_, abs(adjusted_sub_position) * 100);
        }
        else
        {
            lv_arc_set_value(arc_, 100 - abs(adjusted_sub_position) * 100);
        }
    }
}

void LightSwitchApp::updateStateFromSystem(AppState state) {}