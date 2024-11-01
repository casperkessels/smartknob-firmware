#include "climate.h"

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

    LV_IMG_DECLARE(x80_thermostat);
    LV_IMG_DECLARE(x40_thermostat);
    LV_IMG_DECLARE(x80_lightbulb_outline);
    LV_IMG_DECLARE(x80_lightbulb_filled);
    LV_IMG_DECLARE(x80_fan_filled);

    big_icon = x80_thermostat;
    small_icon = x40_thermostat;

    // Create screens for both modes
    climate_screen = lv_obj_create(screen);
    light_screen = lv_obj_create(screen);

    // Remove all default styles that might cause borders or padding
    lv_obj_remove_style_all(climate_screen);
    lv_obj_remove_style_all(light_screen);

    // Set exact size to match display
    lv_obj_set_size(climate_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_size(light_screen, LV_HOR_RES, LV_VER_RES);

    // Set position to (0,0) to avoid any offsets
    lv_obj_set_pos(climate_screen, 0, 0);
    lv_obj_set_pos(light_screen, 0, 0);

    // Set background colors
    lv_obj_set_style_bg_color(climate_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(climate_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(light_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(light_screen, LV_OPA_COVER, 0);

    // Remove any borders
    lv_obj_set_style_border_width(climate_screen, 0, 0);
    lv_obj_set_style_border_width(light_screen, 0, 0);

    // Remove any padding
    lv_obj_set_style_pad_all(climate_screen, 0, 0);
    lv_obj_set_style_pad_all(light_screen, 0, 0);

    // Hide light screen initially
    lv_obj_add_flag(light_screen, LV_OBJ_FLAG_HIDDEN);

    initScreen();
    updateTemperatureArc();
    initLightSwitch();
}

int8_t ClimateApp::navigationNext()
{
    if (mode == ClimateAppMode::CLIMATE_AUTO)
    {
        mode = ClimateAppMode::LIGHT_SWITCH;

        // Switch to light switch mode
        lv_obj_add_flag(climate_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(light_screen, LV_OBJ_FLAG_HIDDEN);

        // Update motor config for light switch - now with 4 steps (0-3)
        motor_config = PB_SmartKnobConfig{
            current_light_position,
            0,
            current_light_position,
            0,
            3, // Changed from 1 to 3 to allow for 4 positions (0,1,2,3)
            25 * PI / 180,
            2,
            1,
            1.1,
            "light",
            0,
            {},
            0,
            27,
        };
    }
    else
    {
        mode = ClimateAppMode::CLIMATE_AUTO;

        // Save light switch state
        // light_saved_position = current_light_position;

        // Switch back to climate mode
        lv_obj_clear_flag(climate_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(light_screen, LV_OBJ_FLAG_HIDDEN);

        // Restore climate motor config with its saved state
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
    }

    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);
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
    else
    {
        // Light switch logic
        current_light_position = state.current_position;
        light_saved_position = current_light_position;

        if (last_light_position != current_light_position)
        {
            updateLightSwitch();

            sprintf(new_state.app_id, "%s", app_id);
            sprintf(new_state.entity_id, "%s", entity_id);

            cJSON *json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "state", light_state);

            char *json_string = cJSON_PrintUnformatted(json);
            sprintf(new_state.state, "%s", json_string);

            cJSON_free(json_string);
            cJSON_Delete(json);

            last_light_position = current_light_position;
            new_state.changed = true;
            sprintf(new_state.app_slug, "%s", APP_SLUG_LIGHT_SWITCH);
        }
    }

    first_run = true;
    return new_state;
}

void ClimateApp::initLightSwitch()
{
    SemaphoreGuard lock(mutex_);

    // Setup light switch UI elements
    const int ARC_TOTAL_SPAN = 25; // Keep these values the same to maintain appearance
    const int ARC_GAP = 10;
    const int ARC_SIZE = ARC_TOTAL_SPAN - ARC_GAP;

    for (int i = 0; i < 4; i++)
    {
        arcs[i] = lv_arc_create(light_screen); // Make sure we use light_screen as parent
        lv_obj_remove_style_all(arcs[i]);      // Remove default styles
        lv_obj_set_size(arcs[i], 210, 210);

        int base_rotation = 270 - (ARC_TOTAL_SPAN * 2);
        int start_angle = i * ARC_TOTAL_SPAN;

        lv_arc_set_rotation(arcs[i], base_rotation);
        lv_arc_set_bg_angles(arcs[i], start_angle, start_angle + ARC_SIZE);
        lv_arc_set_value(arcs[i], 100);
        lv_obj_center(arcs[i]);

        lv_obj_remove_style(arcs[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arcs[i], 24, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arcs[i], 24, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arcs[i], arc_inactive_color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arcs[i], arc_inactive_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arcs[i], true, LV_PART_INDICATOR);
    }

    light_bulb = lv_img_create(light_screen);
    lv_obj_remove_style_all(light_bulb); // Remove default styles
    lv_img_set_src(light_bulb, &big_icon);
    lv_obj_set_style_img_recolor_opa(light_bulb, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(light_bulb, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
    lv_obj_center(light_bulb);
}

void ClimateApp::updateLightSwitch()
{
    SemaphoreGuard lock(mutex_);

    for (int i = 0; i < 4; i++)
    {
        lv_color_t color = (i <= current_light_position) ? arc_active_color : arc_inactive_color;
        lv_obj_set_style_arc_color(arcs[i], color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arcs[i], color, LV_PART_INDICATOR);
    }

    if (current_light_position == 0)
    {
        lv_img_set_src(light_bulb, &big_icon);
        lv_obj_set_style_bg_color(light_screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    }
    else
    {
        lv_img_set_src(light_bulb, &big_icon);
        uint8_t brightness = ((current_light_position + 1) * 63);
        lv_obj_set_style_bg_color(light_screen, LV_COLOR_MAKE(brightness / 3, brightness / 3, 0), 0);
    }
}

void ClimateApp::initScreen()
{
    {
        SemaphoreGuard lock(mutex_);

        target_temp_label = lv_label_create(climate_screen);
        current_temp_label = lv_label_create(climate_screen);

        // All the mode icons should also be on climate_screen
        mode_auto_icon = lv_img_create(climate_screen);
        mode_cool_icon = lv_img_create(climate_screen);
        mode_heat_icon = lv_img_create(climate_screen);
        mode_air_icon = lv_img_create(climate_screen);

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

        LV_IMG_DECLARE(x20_mode_auto);
        LV_IMG_DECLARE(x20_mode_cool);
        LV_IMG_DECLARE(x20_mode_heat);
        LV_IMG_DECLARE(x20_mode_air);

        mode_auto_icon = lv_img_create(climate_screen);
        lv_img_set_src(mode_auto_icon, &x20_mode_auto);
        lv_obj_add_style(mode_auto_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align(mode_auto_icon, LV_ALIGN_BOTTOM_MID, -30, -10);
        lv_obj_set_style_img_recolor(mode_auto_icon, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), LV_PART_MAIN);

        mode_cool_icon = lv_img_create(climate_screen);
        lv_img_set_src(mode_cool_icon, &x20_mode_cool);
        lv_obj_add_style(mode_cool_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align_to(mode_cool_icon, mode_auto_icon, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

        mode_heat_icon = lv_img_create(climate_screen);
        lv_img_set_src(mode_heat_icon, &x20_mode_heat);
        lv_obj_add_style(mode_heat_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align_to(mode_heat_icon, mode_cool_icon, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

        mode_air_icon = lv_img_create(climate_screen);
        lv_img_set_src(mode_air_icon, &x20_mode_air);
        lv_obj_add_style(mode_air_icon, (lv_style_t *)&SK_X20_ICON_STYLE, LV_PART_MAIN);
        lv_obj_align_to(mode_air_icon, mode_heat_icon, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
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

                lv_obj_t *min_temp_label = lv_label_create(screen);
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

                lv_obj_t *max_temp_label = lv_label_create(screen);
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
    // {
    //     SemaphoreGuard lock(mutex_);

    //     switch (mode)
    //     {
    //     case ClimateAppMode::CLIMATE_AUTO:
    //         lv_obj_set_style_img_recolor(mode_cool_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_heat_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_air_icon, inactive_color, LV_PART_MAIN);

    //         if (current_temperature < target_temperature)
    //         {
    //             lv_obj_set_style_img_recolor(mode_heat_icon, heat_active_color, LV_PART_MAIN);
    //             // lv_label_set_text(state_label, "Heating");
    //         }
    //         else if (current_temperature > target_temperature)
    //         {
    //             lv_obj_set_style_img_recolor(mode_cool_icon, cool_active_color, LV_PART_MAIN);
    //             // lv_label_set_text(state_label, "Cooling");
    //         }
    //         else if (current_temperature == target_temperature)
    //         {
    //             lv_obj_set_style_img_recolor(mode_air_icon, air_active_color, LV_PART_MAIN);
    //             // lv_label_set_text(state_label, "idle");
    //         }

    //         lv_obj_set_style_img_recolor(mode_auto_icon, auto_active_color, LV_PART_MAIN);
    //         break;
    //     case ClimateAppMode::CLIMATE_COOL:
    //         lv_obj_set_style_img_recolor(mode_heat_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_air_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_auto_icon, inactive_color, LV_PART_MAIN);

    //         lv_obj_set_style_img_recolor(mode_cool_icon, cool_active_color, LV_PART_MAIN);
    //         // lv_label_set_text(state_label, "Cooling");
    //         break;
    //     case ClimateAppMode::CLIMATE_HEAT:
    //         lv_obj_set_style_img_recolor(mode_air_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_auto_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_cool_icon, inactive_color, LV_PART_MAIN);

    //         lv_obj_set_style_img_recolor(mode_heat_icon, heat_active_color, LV_PART_MAIN);
    //         // lv_label_set_text(state_label, "Heating");
    //         break;
    //     case ClimateAppMode::CLIMATE_FAN_ONLY:
    //         lv_obj_set_style_img_recolor(mode_auto_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_cool_icon, inactive_color, LV_PART_MAIN);
    //         lv_obj_set_style_img_recolor(mode_heat_icon, inactive_color, LV_PART_MAIN);

    //         lv_obj_set_style_img_recolor(mode_air_icon, air_active_color, LV_PART_MAIN);
    //         // lv_label_set_text(state_label, "idle");
    //         break;
    //     }

    //     // lv_obj_align_to(state_label, target_temp_label, LV_ALIGN_OUT_TOP_MID, 0, -2);
    //     lv_obj_align_to(current_temp_label, target_temp_label, LV_ALIGN_OUT_BOTTOM_MID, 0, -4);
    // }
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