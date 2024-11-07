#include "climate.h"

LV_IMG_DECLARE(x80_temp);
LV_IMG_DECLARE(x80_fan);
LV_IMG_DECLARE(x80_seat);

ClimateApp::ClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    current_temperature = 20;
    target_temperature = 25;
    uint8_t position_nonce = target_temperature;

    // Initialize default climate motor config
    motor_config = PB_SmartKnobConfig{
        target_temperature,
        0,
        target_temperature,
        CLIMATE_APP_MIN_TEMP,
        CLIMATE_APP_MAX_TEMP,
        8.225806452 * PI / 120,
        2,
        1,
        1.1,
        "",
        0,
        {},
        0,
        27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    // Load images
    LV_IMG_DECLARE(x80_temp);
    LV_IMG_DECLARE(x80_fan);
    LV_IMG_DECLARE(x80_seat);

    temp_img = &x80_temp;
    fan_img = &x80_fan;
    seat_img = &x80_seat;

    big_icon = x80_temp;

    // Create screens for both modes
    climate_screen = lv_obj_create(screen);
    fan_speed_screen = lv_obj_create(screen);

    // Remove all default styles that might cause borders or padding
    lv_obj_remove_style_all(climate_screen);
    lv_obj_remove_style_all(fan_speed_screen);

    // Set exact size to match display
    lv_obj_set_size(climate_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_size(fan_speed_screen, LV_HOR_RES, LV_VER_RES);

    // Set position to (0,0) to avoid any offsets
    lv_obj_set_pos(climate_screen, 0, 0);
    lv_obj_set_pos(fan_speed_screen, 0, 0);

    // Set background colors
    lv_obj_set_style_bg_color(climate_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(climate_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fan_speed_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(fan_speed_screen, LV_OPA_COVER, 0);

    // Remove any borders
    lv_obj_set_style_border_width(climate_screen, 0, 0);
    lv_obj_set_style_border_width(fan_speed_screen, 0, 0);

    // Remove any padding
    lv_obj_set_style_pad_all(climate_screen, 0, 0);
    lv_obj_set_style_pad_all(fan_speed_screen, 0, 0);

    // Hide fan_speed screen initially
    lv_obj_add_flag(fan_speed_screen, LV_OBJ_FLAG_HIDDEN);

    initScreen();
    updateTemperatureArc();
    initFanSpeed();
    initSeatHeating();
    updateModeIcon();
}

int8_t ClimateApp::navigationNext()
{

    if (mode == ClimateAppMode::CLIMATE_AUTO)
    {
        mode = ClimateAppMode::FAN_SPEED;
        lv_obj_add_flag(climate_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fan_speed_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(seat_heating_screen, LV_OBJ_FLAG_HIDDEN);
        // lv_img_set_src(fan_speed_bulb, &fan_img);
        motor_config = PB_SmartKnobConfig{
            current_fan_speed_position,
            0,
            current_fan_speed_position,
            0,
            4, // 5 steps
            20 * PI / 180,
            2,
            1,
            1.1,
            "fan_speed",
            0,
            {},
            0,
            27,
        };
        big_icon = x80_fan;
    }
    else if (mode == ClimateAppMode::FAN_SPEED)
    {
        mode = ClimateAppMode::SEAT_HEATING;
        lv_obj_add_flag(climate_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fan_speed_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(seat_heating_screen, LV_OBJ_FLAG_HIDDEN);
        // lv_img_set_src(seat_heating_bulb, &seat_img);
        motor_config = PB_SmartKnobConfig{
            current_seat_heating_position,
            0,
            current_seat_heating_position,
            0,
            3, // 4 steps
            25 * PI / 180,
            2,
            1,
            1.1,
            "seat_heating",
            0,
            {},
            0,
            27,
        };
        big_icon = x80_seat;
    }
    else if (mode == ClimateAppMode::SEAT_HEATING)
    {
        mode = ClimateAppMode::CLIMATE_AUTO;
        lv_obj_clear_flag(climate_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fan_speed_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(seat_heating_screen, LV_OBJ_FLAG_HIDDEN);
        // lv_img_set_src(target_temp_label, &temp_img);
        motor_config = PB_SmartKnobConfig{
            climate_saved_position,
            0,
            climate_saved_position,
            CLIMATE_APP_MIN_TEMP,
            CLIMATE_APP_MAX_TEMP,
            8.225806452 * PI / 120,
            2,
            1,
            1.1,
            "",
            0,
            {},
            0,
            27,
        };
        big_icon = x80_temp;
    }

    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);
    updateModeIcon();
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}

EntityStateUpdate ClimateApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return new_state;
    }

    if (mode == ClimateAppMode::CLIMATE_AUTO)
    {
        // Climate control logic
        target_temperature = state.current_position;
        climate_saved_position = target_temperature;

        if (last_target_temperature != state.current_position)
        {
            updateTemperatureArc();

            sprintf(new_state.app_id, "%s", app_id);
            sprintf(new_state.entity_id, "%s", entity_id);

            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "mode", mode);
            cJSON_AddNumberToObject(json, "target_temp", target_temperature);
            cJSON_AddNumberToObject(json, "current_temp", current_temperature);

            char *json_string = cJSON_PrintUnformatted(json);
            sprintf(new_state.state, "%s", json_string);

            cJSON_free(json_string);
            cJSON_Delete(json);

            last_target_temperature = target_temperature;
            new_state.changed = true;
            sprintf(new_state.app_slug, "%s", APP_SLUG_CLIMATE);
        }
    }
    else if (mode == ClimateAppMode::SEAT_HEATING)
    {
        current_seat_heating_position = state.current_position;
        seat_heating_saved_position = current_seat_heating_position;

        if (last_seat_heating_position != current_seat_heating_position)
        {
            updateSeatHeating();

            sprintf(new_state.app_id, "%s", app_id);
            sprintf(new_state.entity_id, "%s", entity_id);

            cJSON *json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "state", current_seat_heating_position > 0);
            cJSON_AddNumberToObject(json, "brightness", (current_seat_heating_position * 51)); // 0-255 range for 6 steps

            char *json_string = cJSON_PrintUnformatted(json);
            sprintf(new_state.state, "%s", json_string);

            cJSON_free(json_string);
            cJSON_Delete(json);

            last_seat_heating_position = current_seat_heating_position;
            new_state.changed = true;
            sprintf(new_state.app_slug, "%s", APP_SLUG_LIGHT_SWITCH);
        }
    }
    else
    {
        // Light switch logic
        current_fan_speed_position = state.current_position;
        fan_speed_saved_position = current_fan_speed_position;

        if (last_fan_speed_position != current_fan_speed_position)
        {
            updateFanSpeed();

            sprintf(new_state.app_id, "%s", app_id);
            sprintf(new_state.entity_id, "%s", entity_id);

            cJSON *json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "state", fan_speed_state);

            char *json_string = cJSON_PrintUnformatted(json);
            sprintf(new_state.state, "%s", json_string);

            cJSON_free(json_string);
            cJSON_Delete(json);

            last_fan_speed_position = current_fan_speed_position;
            new_state.changed = true;
            sprintf(new_state.app_slug, "%s", APP_SLUG_LIGHT_SWITCH);
        }
    }

    first_run = true;
    return new_state;
}

void ClimateApp::initFanSpeed()
{
    SemaphoreGuard lock(mutex_);

    const int ARC_TOTAL_SPAN = MAX_ANGLE; // Use same constants as climate for consistency
    uint16_t width = 220;
    uint8_t arc_width = ARC_WIDTH;

    LV_IMG_DECLARE(x20_temp);
    LV_IMG_DECLARE(x20_fan);
    LV_IMG_DECLARE(x20_seat);

    fan_speed_mode_auto_icon = lv_img_create(fan_speed_screen);
    lv_img_set_src(fan_speed_mode_auto_icon, &x20_temp);
    lv_obj_add_style(fan_speed_mode_auto_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(fan_speed_mode_auto_icon, LV_ALIGN_BOTTOM_MID, -40, -10);

    fan_speed_mode_cool_icon = lv_img_create(fan_speed_screen);
    lv_img_set_src(fan_speed_mode_cool_icon, &x20_fan);
    lv_obj_add_style(fan_speed_mode_cool_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(fan_speed_mode_cool_icon, LV_ALIGN_BOTTOM_MID, 0, -10);

    fan_speed_mode_heat_icon = lv_img_create(fan_speed_screen);
    lv_img_set_src(fan_speed_mode_heat_icon, &x20_seat);
    lv_obj_add_style(fan_speed_mode_heat_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(fan_speed_mode_heat_icon, LV_ALIGN_BOTTOM_MID, 40, -10);

    // Create main arc (background)
    fan_speed_arc = lv_arc_create(fan_speed_screen);
    lv_obj_remove_style(fan_speed_arc, NULL, LV_PART_KNOB);
    lv_obj_set_size(fan_speed_arc, width, width);
    lv_obj_center(fan_speed_arc);
    lv_obj_set_style_arc_width(fan_speed_arc, arc_width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(fan_speed_arc, arc_width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(fan_speed_arc, dark_arc_bg, LV_PART_MAIN);
    lv_obj_set_style_arc_color(fan_speed_arc, cool_active_color, LV_PART_INDICATOR);
    lv_arc_set_rotation(fan_speed_arc, ROTATION_ANGLE);
    lv_arc_set_bg_angles(fan_speed_arc, 0, MAX_ANGLE);

    // Create 5 dots to show speed levels
    uint8_t dot_amount = 5; // -2, -1, 0, 1, 2
    uint8_t diameter = 6;
    uint8_t rotation = ROTATION_ANGLE;
    uint16_t start_angle = MIN_ANGLE;
    uint16_t end_angle = MAX_ANGLE;

    fan_speed_dots = (lv_obj_t **)malloc(dot_amount * sizeof(lv_obj_t *));
    assert(fan_speed_dots != NULL);

    start_angle += rotation;
    end_angle += rotation;

    float angle_step = (float)(end_angle - start_angle) / (dot_amount - 1);

    lv_coord_t screen_width = lv_obj_get_width(fan_speed_screen);
    lv_coord_t screen_height = lv_obj_get_height(fan_speed_screen);
    lv_coord_t center_x = screen_width / 2;
    lv_coord_t center_y = screen_height / 2;

    float radius = (width - arc_width) / 2.0;

    // Create the speed indicator dots
    for (int i = 0; i < dot_amount; i++)
    {
        float angle = (start_angle + i * angle_step) * M_PI / 180.0;
        int x = center_x + radius * cos(angle);
        int y = center_y + radius * sin(angle);

        lv_obj_t *circle = lvDrawCircle(diameter, fan_speed_screen);
        lv_obj_set_pos(circle, x - diameter / 2, y - diameter / 2);
        lv_obj_set_style_bg_color(circle, arc_inactive_color, LV_PART_MAIN);

        fan_speed_dots[i] = circle;

        // Add min/max speed labels (optional)
        if (i == 0)
        {
            // Slow fan label
            int x_ = center_x + radius * cos(angle - ONE_STEP_ANGLE * DEG_TO_RAD);
            int y_ = center_y + radius * sin(angle - ONE_STEP_ANGLE * DEG_TO_RAD);

            lv_obj_t *slow_label = lv_label_create(fan_speed_screen);
            lv_obj_set_style_text_font(slow_label, &roboto_semi_bold_mono_12pt, 0);
            lv_label_set_text(slow_label, "-");
            lv_obj_set_style_text_color(slow_label, cool_active_color, LV_PART_MAIN);
            lv_obj_update_layout(slow_label);
            lv_obj_set_pos(slow_label, x_ - lv_obj_get_width(slow_label) / 2, y_ - lv_obj_get_height(slow_label) / 2);
        }
        else if (i == dot_amount - 1)
        {
            // Fast fan label
            int x_ = center_x + radius * cos(angle + ONE_STEP_ANGLE * DEG_TO_RAD);
            int y_ = center_y + radius * sin(angle + ONE_STEP_ANGLE * DEG_TO_RAD);

            lv_obj_t *fast_label = lv_label_create(fan_speed_screen);
            lv_obj_set_style_text_font(fast_label, &roboto_semi_bold_mono_12pt, 0);
            lv_label_set_text(fast_label, "+");
            lv_obj_set_style_text_color(fast_label, heat_active_color, LV_PART_MAIN);
            lv_obj_update_layout(fast_label);
            lv_obj_set_pos(fast_label, x_ - lv_obj_get_width(fast_label) / 2, y_ - lv_obj_get_height(fast_label) / 2);
        }
    }

    fan_speed_bulb = lv_img_create(fan_speed_screen);
    lv_img_set_src(fan_speed_bulb, &big_icon);
    lv_obj_set_style_img_recolor_opa(fan_speed_bulb, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(fan_speed_bulb, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
    lv_obj_center(fan_speed_bulb);
}

void ClimateApp::updateFanSpeed()
{
    SemaphoreGuard lock(mutex_);

    // Convert position 0-4 to fan speed -2 to +2
    int fan_speed = current_fan_speed_position - 2;

    // Update arc indicator
    uint16_t angle_mid = lerp(2, 0, 4, MIN_ANGLE, MAX_ANGLE); // Position of center point
    uint16_t angle_current = lerp(current_fan_speed_position, 0, 4, MIN_ANGLE, MAX_ANGLE);

    if (fan_speed == 0)
    {
        // Show green for neutral position
        lv_arc_set_angles(fan_speed_arc, angle_mid - 1, angle_mid + 1);
        lv_obj_set_style_arc_color(fan_speed_arc, LV_COLOR_MAKE(0x00, 0xCC, 0x00), LV_PART_INDICATOR);
    }
    else if (fan_speed < 0)
    {
        // Show arc from current position to center
        lv_arc_set_angles(fan_speed_arc, angle_current, angle_mid);
        lv_obj_set_style_arc_color(fan_speed_arc, cool_active_color, LV_PART_INDICATOR);
    }
    else
    {
        // Show arc from center to current position
        lv_arc_set_angles(fan_speed_arc, angle_mid, angle_current);
        lv_obj_set_style_arc_color(fan_speed_arc, heat_active_color, LV_PART_INDICATOR);
    }

    // Update dots
    for (int i = 0; i < 5; i++)
    {
        if (i == 2)
        {
            // Center dot - green background when active, dot remains white for contrast
            lv_obj_set_style_bg_color(fan_speed_dots[i], LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
        }
        else if (i < 2)
        {
            // Left side dots (negative speeds)
            if (fan_speed < 0 && i >= (2 + fan_speed))
            {
                lv_obj_set_style_bg_color(fan_speed_dots[i], cool_active_color, LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_bg_color(fan_speed_dots[i], arc_inactive_color, LV_PART_MAIN);
            }
        }
        else
        {
            // Right side dots (positive speeds)
            if (fan_speed > 0 && i <= (2 + fan_speed))
            {
                lv_obj_set_style_bg_color(fan_speed_dots[i], heat_active_color, LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_bg_color(fan_speed_dots[i], arc_inactive_color, LV_PART_MAIN);
            }
        }
    }
}

void ClimateApp::initSeatHeating()
{
    SemaphoreGuard lock(mutex_);

    seat_heating_screen = lv_obj_create(screen);
    lv_obj_remove_style_all(seat_heating_screen);
    lv_obj_set_size(seat_heating_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(seat_heating_screen, 0, 0);
    lv_obj_set_style_bg_color(seat_heating_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(seat_heating_screen, LV_OPA_COVER, 0);
    lv_obj_add_flag(seat_heating_screen, LV_OBJ_FLAG_HIDDEN);

    // Arc configuration
    const int OFF_ARC_SPAN = 50;                // Arc size for OFF indicator
    const int HEAT_ARC_SPAN = 150;              // Total arc size for heating levels
    const int SEGMENT_SPAN = HEAT_ARC_SPAN / 3; // Size of each heating segment
    const int BASE_ROTATION = 150;              // Starting angle
    const int LARGE_GAP = 24;                   // Gap between OFF arc and heating segments
    const int SMALL_GAP = 12;                   // Small gap between heating segments

    // Add mode icons
    LV_IMG_DECLARE(x20_temp);
    LV_IMG_DECLARE(x20_fan);
    LV_IMG_DECLARE(x20_seat);

    seat_heating_mode_auto_icon = lv_img_create(seat_heating_screen);
    lv_img_set_src(seat_heating_mode_auto_icon, &x20_temp);
    lv_obj_add_style(seat_heating_mode_auto_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(seat_heating_mode_auto_icon, LV_ALIGN_BOTTOM_MID, -40, -10);

    seat_heating_mode_cool_icon = lv_img_create(seat_heating_screen);
    lv_img_set_src(seat_heating_mode_cool_icon, &x20_fan);
    lv_obj_add_style(seat_heating_mode_cool_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(seat_heating_mode_cool_icon, LV_ALIGN_BOTTOM_MID, 0, -10);

    seat_heating_mode_heat_icon = lv_img_create(seat_heating_screen);
    lv_img_set_src(seat_heating_mode_heat_icon, &x20_seat);
    lv_obj_add_style(seat_heating_mode_heat_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
    lv_obj_align(seat_heating_mode_heat_icon, LV_ALIGN_BOTTOM_MID, 40, -10);

    // Create the OFF arc (arc 0)
    seat_heating_arcs[0] = lv_arc_create(seat_heating_screen);
    lv_obj_remove_style_all(seat_heating_arcs[0]);
    lv_obj_set_size(seat_heating_arcs[0], 210, 210);
    lv_arc_set_rotation(seat_heating_arcs[0], BASE_ROTATION);
    lv_arc_set_bg_angles(seat_heating_arcs[0], 0, OFF_ARC_SPAN);
    lv_arc_set_value(seat_heating_arcs[0], 100);
    lv_obj_center(seat_heating_arcs[0]);

    lv_obj_remove_style(seat_heating_arcs[0], NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(seat_heating_arcs[0], ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(seat_heating_arcs[0], ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(seat_heating_arcs[0], true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(seat_heating_arcs[0], true, LV_PART_INDICATOR);

    // Create three heating level arcs (arcs 1-3)
    int start_angle = OFF_ARC_SPAN + LARGE_GAP;
    for (int i = 1; i <= 3; i++)
    {
        seat_heating_arcs[i] = lv_arc_create(seat_heating_screen);
        lv_obj_remove_style_all(seat_heating_arcs[i]);
        lv_obj_set_size(seat_heating_arcs[i], 210, 210);
        lv_arc_set_rotation(seat_heating_arcs[i], BASE_ROTATION);

        // Add small gaps between heat segments
        lv_arc_set_bg_angles(seat_heating_arcs[i],
                             start_angle,
                             start_angle + SEGMENT_SPAN - SMALL_GAP);

        lv_arc_set_value(seat_heating_arcs[i], 100);
        lv_obj_center(seat_heating_arcs[i]);

        lv_obj_remove_style(seat_heating_arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(seat_heating_arcs[i], ARC_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_arc_width(seat_heating_arcs[i], ARC_WIDTH, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(seat_heating_arcs[i], true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(seat_heating_arcs[i], true, LV_PART_INDICATOR);

        // Move to next segment position
        start_angle += SEGMENT_SPAN;
    }

    seat_heating_bulb = lv_img_create(seat_heating_screen);
    lv_img_set_src(seat_heating_bulb, &big_icon);
    lv_obj_set_style_img_recolor_opa(seat_heating_bulb, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(seat_heating_bulb, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
    lv_obj_center(seat_heating_bulb);
}

void ClimateApp::updateSeatHeating()
{
    SemaphoreGuard lock(mutex_);

    // Update OFF arc color
    lv_color_t off_arc_active = lv_color_make(0xED, 0xF2, 0xF8);   // #EDF2F8 - Active off state
    lv_color_t off_arc_inactive = lv_color_make(0x45, 0x50, 0x5E); // #45505E - Inactive off state

    lv_color_t off_arc_color = current_seat_heating_position == 0 ? off_arc_active : off_arc_inactive;

    lv_obj_set_style_arc_color(seat_heating_arcs[0], off_arc_color, LV_PART_MAIN);
    lv_obj_set_style_arc_color(seat_heating_arcs[0], off_arc_color, LV_PART_INDICATOR);

    // Update heating segments
    lv_color_t active_color = lv_color_make(0xCA, 0x8E, 0x7E);   // #CA8E7E - Active heating
    lv_color_t inactive_color = lv_color_make(0x49, 0x38, 0x37); // #493837 - Inactive heating

    // Update each heating segment
    for (int i = 1; i <= 3; i++)
    {
        lv_color_t segment_color = (i <= current_seat_heating_position) ? active_color : inactive_color;
        lv_obj_set_style_arc_color(seat_heating_arcs[i], segment_color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(seat_heating_arcs[i], segment_color, LV_PART_INDICATOR);
    }

    // Update background color based on heating level
    // if (current_seat_heating_position == 0)
    // {
    //     lv_img_set_src(seat_heating_bulb, &big_icon);
    //     lv_obj_set_style_bg_color(seat_heating_screen, lv_color_make(0x00, 0x00, 0x00), 0);
    // }
    // else
    // {
    //     lv_img_set_src(seat_heating_bulb, &big_icon);
    //     uint8_t brightness = ((current_seat_heating_position) * 42);
    //     lv_obj_set_style_bg_color(seat_heating_screen, lv_color_make(brightness / 2, brightness / 4, 0), 0);
    // }
}

void ClimateApp::initScreen()
{
    {
        SemaphoreGuard lock(mutex_);

        target_temp_label = lv_label_create(climate_screen);
        current_temp_label = lv_label_create(climate_screen);

        // big_icon = *temp_img;

        // All the mode icons should also be on climate_screen
        climate_mode_auto_icon = lv_img_create(climate_screen);
        climate_mode_cool_icon = lv_img_create(climate_screen);
        climate_mode_heat_icon = lv_img_create(climate_screen);
        // climate_mode_air_icon = lv_img_create(climate_screen);

        target_temp_label = lv_label_create(climate_screen); // Changed from screen to climate_screen
        lv_obj_set_style_text_font(target_temp_label, &roboto_light_mono_48pt, LV_PART_MAIN);
        lv_label_set_text_fmt(target_temp_label, "%d", target_temperature);
        lv_obj_align(target_temp_label, LV_ALIGN_CENTER, 0, -8);

        lv_obj_t *target_temp_degree_symbol_label = lv_label_create(climate_screen); // Changed
        lv_obj_set_style_text_font(target_temp_degree_symbol_label, &roboto_light_mono_48pt, 0);
        lv_label_set_text(target_temp_degree_symbol_label, "°");
        lv_obj_align_to(target_temp_degree_symbol_label, target_temp_label, LV_ALIGN_OUT_RIGHT_MID, -6, 0);

        // state_label = lv_label_create(screen);
        // lv_label_set_text(state_label, "Climate");
        // lv_obj_align_to(state_label, target_temp_label, LV_ALIGN_OUT_TOP_MID, 0, -2);

        current_temp_label = lv_label_create(climate_screen);
        lv_obj_set_style_text_font(current_temp_label, &roboto_light_mono_24pt, 0);
        lv_label_set_text_fmt(current_temp_label, "%d", current_temperature);
        lv_obj_align_to(current_temp_label, target_temp_label, LV_ALIGN_OUT_BOTTOM_MID, 0, -4);

        lv_obj_t *current_temp_degree_symbol_label = lv_label_create(climate_screen);
        lv_obj_set_style_text_font(current_temp_degree_symbol_label, &roboto_light_mono_24pt, 0);
        lv_label_set_text(current_temp_degree_symbol_label, "°");
        lv_obj_align_to(current_temp_degree_symbol_label, current_temp_label, LV_ALIGN_OUT_RIGHT_MID, -2, 0);

        LV_IMG_DECLARE(x20_temp);
        LV_IMG_DECLARE(x20_fan);
        LV_IMG_DECLARE(x20_seat);
        // LV_IMG_DECLARE(x20_mode_air);

        // Climate mode icon (leftmost)
        climate_mode_auto_icon = lv_img_create(climate_screen);
        lv_img_set_src(climate_mode_auto_icon, &x20_temp);
        lv_obj_add_style(climate_mode_auto_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align(climate_mode_auto_icon, LV_ALIGN_BOTTOM_MID, -40, -10);

        // Fan mode icon (middle)
        climate_mode_cool_icon = lv_img_create(climate_screen);
        lv_img_set_src(climate_mode_cool_icon, &x20_fan);
        lv_obj_add_style(climate_mode_cool_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align(climate_mode_cool_icon, LV_ALIGN_BOTTOM_MID, 0, -10);

        // Seat heating mode icon (rightmost)
        climate_mode_heat_icon = lv_img_create(climate_screen);
        lv_img_set_src(climate_mode_heat_icon, &x20_seat);
        lv_obj_add_style(climate_mode_heat_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align(climate_mode_heat_icon, LV_ALIGN_BOTTOM_MID, 40, -10);

        // mode_air_icon = lv_img_create(climate_screen);
        // lv_img_set_src(mode_air_icon, &x20_mode_air);
        // lv_obj_add_style(mode_air_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        // lv_obj_align_to(mode_air_icon, mode_heat_icon, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    }
    initTemperatureArc();
}

void ClimateApp::initTemperatureArc()
{
    {
        SemaphoreGuard lock(mutex_);

        temperature_arc = lv_arc_create(climate_screen); // Changed from screen to climate_screen

        uint16_t width = 220;
        uint8_t arc_width = ARC_WIDTH;

        lv_obj_remove_style(temperature_arc, NULL, LV_PART_KNOB); // REMOVE KNOB
        lv_obj_set_size(temperature_arc, width, width);
        lv_obj_center(temperature_arc);
        lv_obj_set_style_arc_width(temperature_arc, arc_width, LV_PART_MAIN);
        lv_obj_set_style_arc_width(temperature_arc, arc_width, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(temperature_arc, dark_arc_bg, LV_PART_MAIN);
        lv_obj_set_style_arc_color(temperature_arc, air_active_color, LV_PART_INDICATOR);
        lv_arc_set_rotation(temperature_arc, ROTATION_ANGLE);
        lv_arc_set_bg_angles(temperature_arc, 0, MAX_ANGLE);

        // Rotation starts at 3o clock to align with LVGL
        uint8_t dot_amount = (CLIMATE_APP_MAX_TEMP - CLIMATE_APP_MIN_TEMP) + 1;
        uint8_t diameter = 6;
        uint8_t rotation = ROTATION_ANGLE;
        uint16_t start_angle = MIN_ANGLE;
        uint16_t end_angle = MAX_ANGLE;

        temperature_dots = (lv_obj_t **)malloc(dot_amount * sizeof(lv_obj_t *));
        assert(temperature_dots != NULL);

        start_angle += rotation;
        end_angle += rotation;

        float angle_step = (float)(end_angle - start_angle) / (dot_amount - 1);

        lv_coord_t screen_width = lv_obj_get_width(climate_screen);
        lv_coord_t screen_height = lv_obj_get_height(climate_screen);
        lv_coord_t center_x = screen_width / 2;
        lv_coord_t center_y = screen_height / 2;

        float radius = (width - arc_width) / 2.0; // Remove arcs width to center dots in the arc

        for (int i = 0; i < dot_amount; i++)
        {

            float angle = (start_angle + i * angle_step) * M_PI / 180.0;

            int x = center_x + radius * cos(angle);
            int y = center_y + radius * sin(angle);

            lv_obj_t *circle = lvDrawCircle(diameter, climate_screen);
            lv_obj_set_pos(circle, x - diameter / 2, y - diameter / 2); // Adjust position to account for the circle's diameter

            uint8_t temp = i + CLIMATE_APP_MIN_TEMP;
            if (temp == current_temperature)
            {
                lv_obj_set_style_bg_color(circle, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
            }
            else if (temp < current_temperature)
            {
                if (target_temperature <= temp && temp < current_temperature)
                    lv_obj_set_style_bg_color(circle, dark_cool_active_color, LV_PART_MAIN);
                else
                    lv_obj_set_style_bg_color(circle, cool_active_color, LV_PART_MAIN);
            }
            else
            {
                if (current_temperature < temp && temp < target_temperature)
                    lv_obj_set_style_bg_color(circle, dark_heat_active_color, LV_PART_MAIN);
                else
                    lv_obj_set_style_bg_color(circle, heat_active_color, LV_PART_MAIN);
            }

            temperature_dots[i] = circle;

            if (i == 0)
            {
                // Get x & y one step before the first dot
                int x_ = center_x + radius * cos(angle - ONE_STEP_ANGLE * DEG_TO_RAD);
                int y_ = center_y + radius * sin(angle - ONE_STEP_ANGLE * DEG_TO_RAD);

                // Changed from screen to climate_screen
                lv_obj_t *min_temp_label = lv_label_create(climate_screen);
                lv_obj_set_style_text_font(min_temp_label, &roboto_semi_bold_mono_12pt, 0);
                lv_label_set_text_fmt(min_temp_label, "%d", CLIMATE_APP_MIN_TEMP);
                lv_obj_set_style_text_color(min_temp_label, cool_active_color, LV_PART_MAIN);
                lv_obj_update_layout(min_temp_label);
                lv_obj_set_pos(min_temp_label, x_ - lv_obj_get_width(min_temp_label) / 2, y_ - lv_obj_get_height(min_temp_label) / 2);
            }
            else if (i == dot_amount - 1)
            {
                // Get x & y one step after the last dot
                int x_ = center_x + radius * cos(angle + ONE_STEP_ANGLE * DEG_TO_RAD);
                int y_ = center_y + radius * sin(angle + ONE_STEP_ANGLE * DEG_TO_RAD);

                // Changed from screen to climate_screen
                lv_obj_t *max_temp_label = lv_label_create(climate_screen);
                lv_obj_set_style_text_font(max_temp_label, &roboto_semi_bold_mono_12pt, 0);
                lv_label_set_text_fmt(max_temp_label, "%d", CLIMATE_APP_MAX_TEMP);
                lv_obj_set_style_text_color(max_temp_label, heat_active_color, LV_PART_MAIN);
                lv_obj_update_layout(max_temp_label);
                lv_obj_set_pos(max_temp_label, x_ - lv_obj_get_width(max_temp_label) / 2, y_ - lv_obj_get_height(max_temp_label) / 2);
            }
        }
    }
}

void ClimateApp::updateTemperatureArc()
{
    {
        SemaphoreGuard lock(mutex_);

        // Update temperature labels
        lv_label_set_text_fmt(target_temp_label, "%d", target_temperature);
        lv_label_set_text_fmt(current_temp_label, "%d", current_temperature);

        // Update ARC
        uint16_t one_step_angle = ONE_STEP_ANGLE;

        uint16_t angle_current_temp = current_temperature <= CLIMATE_APP_MIN_TEMP ? MIN_ANGLE : lerp(current_temperature, CLIMATE_APP_MIN_TEMP, CLIMATE_APP_MAX_TEMP, MIN_ANGLE, MAX_ANGLE);
        uint16_t angle_target_temp = target_temperature >= CLIMATE_APP_MAX_TEMP ? MAX_ANGLE : lerp(target_temperature, CLIMATE_APP_MIN_TEMP, CLIMATE_APP_MAX_TEMP, MIN_ANGLE, MAX_ANGLE);

        if (angle_target_temp == angle_current_temp)
        {
            lv_obj_set_style_arc_color(temperature_arc, LV_COLOR_MAKE(0x00, 0xCC, 0x00), LV_PART_INDICATOR);
            lv_arc_set_angles(temperature_arc, angle_current_temp, angle_target_temp + 1);
        }
        else if (angle_target_temp > angle_current_temp)
        {
            lv_obj_set_style_arc_color(temperature_arc, heat_active_color, LV_PART_INDICATOR);
            lv_arc_set_angles(temperature_arc, angle_current_temp, angle_target_temp + 1);
        }
        else
        {
            lv_obj_set_style_arc_color(temperature_arc, cool_active_color, LV_PART_INDICATOR);
            lv_arc_set_angles(temperature_arc, angle_target_temp, angle_current_temp + 1);
        }

        // Update DOTS
        for (int i = 0; i < (CLIMATE_APP_MAX_TEMP - CLIMATE_APP_MIN_TEMP + 1); i++)
        {
            uint8_t temp = i + CLIMATE_APP_MIN_TEMP;
            if (temp == current_temperature)
            {
                lv_obj_set_style_bg_color(temperature_dots[i], LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
            }
            else if (temp < current_temperature)
            {
                if (target_temperature <= temp)
                    lv_obj_set_style_bg_color(temperature_dots[i], dark_cool_active_color, LV_PART_MAIN);
                else
                    lv_obj_set_style_bg_color(temperature_dots[i], cool_active_color, LV_PART_MAIN);
            }
            else
            {
                if (temp <= target_temperature)
                    lv_obj_set_style_bg_color(temperature_dots[i], dark_heat_active_color, LV_PART_MAIN);
                else
                    lv_obj_set_style_bg_color(temperature_dots[i], heat_active_color, LV_PART_MAIN);
            }
        }
    }
}

void ClimateApp::updateModeIcon()
{
    SemaphoreGuard lock(mutex_);

    // Set all icons on all screens to inactive color
    lv_obj_set_style_img_recolor(climate_mode_auto_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(climate_mode_cool_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(climate_mode_heat_icon, inactive_color, LV_PART_MAIN);

    lv_obj_set_style_img_recolor(fan_speed_mode_auto_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(fan_speed_mode_cool_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(fan_speed_mode_heat_icon, inactive_color, LV_PART_MAIN);

    lv_obj_set_style_img_recolor(seat_heating_mode_auto_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(seat_heating_mode_cool_icon, inactive_color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor(seat_heating_mode_heat_icon, inactive_color, LV_PART_MAIN);

    // Then highlight the active mode on all screens
    switch (mode)
    {
    case ClimateAppMode::CLIMATE_AUTO:
        lv_obj_set_style_img_recolor(climate_mode_auto_icon, auto_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(fan_speed_mode_auto_icon, auto_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(seat_heating_mode_auto_icon, auto_active_color, LV_PART_MAIN);
        break;

    case ClimateAppMode::FAN_SPEED:
        lv_obj_set_style_img_recolor(climate_mode_cool_icon, cool_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(fan_speed_mode_cool_icon, cool_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(seat_heating_mode_cool_icon, cool_active_color, LV_PART_MAIN);
        break;

    case ClimateAppMode::SEAT_HEATING:
        lv_obj_set_style_img_recolor(climate_mode_heat_icon, heat_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(fan_speed_mode_heat_icon, heat_active_color, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(seat_heating_mode_heat_icon, heat_active_color, LV_PART_MAIN);
        break;
    }
}

void ClimateApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *new_state = cJSON_Parse(mqtt_state_update.state);
    cJSON *mode = cJSON_GetObjectItem(new_state, "mode");
    cJSON *target_temp = cJSON_GetObjectItem(new_state, "target_temp");
    cJSON *current_temp = cJSON_GetObjectItem(new_state, "current_temp");

    if (mode != NULL)
    {
        if (mode->valueint >= 0 && mode->valueint < ClimateAppMode::MODE_COUNT)
        {
            this->mode = static_cast<ClimateAppMode>(mode->valueint);
        }
        else
        {
            LOGE("Invalid mode value: %d", mode->valueint);
            return;
        }
    }

    if (target_temp != NULL)
    {
        target_temperature = target_temp->valueint;
        motor_config.position = target_temperature;
        motor_config.position_nonce = target_temperature;
    }

    if (current_temp != NULL)
    {
        this->current_temperature = current_temp->valueint;
    }

    if (mode != NULL || target_temp != NULL || current_temp != NULL)
    {
        state_sent_from_hass = true;
    }

    cJSON_Delete(new_state);

    updateTemperatureArc();
    updateModeIcon();
}