#include "car_climate.h"

CarClimateApp::CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    temperature = 20.0f;
    seat_heat = 0;
    fan_speed = 0;
    current_setting = Setting::TEMPERATURE;

    updateMotorConfig();

    LV_IMG_DECLARE(x80_thermostat);
    LV_IMG_DECLARE(x40_thermostat);

    big_icon = x80_thermostat;
    small_icon = x40_thermostat;

    initScreen();
}

void CarClimateApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    // Initialize temperature arc and label
    temp_arc = lv_arc_create(screen);
    lv_obj_set_size(temp_arc, 200, 200);
    lv_arc_set_rotation(temp_arc, 135);
    lv_arc_set_bg_angles(temp_arc, 0, 270);
    lv_arc_set_range(temp_arc, 160, 300); // 16.0°C to 30.0°C
    lv_obj_center(temp_arc);

    temp_label = lv_label_create(screen);
    lv_obj_set_style_text_font(temp_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 0);

    // Initialize seat heat arc and label
    seat_heat_arc = lv_arc_create(screen);
    lv_obj_set_size(seat_heat_arc, 200, 200);
    lv_arc_set_rotation(seat_heat_arc, 135);
    lv_arc_set_bg_angles(seat_heat_arc, 0, 270);
    lv_arc_set_range(seat_heat_arc, 0, 3);
    lv_obj_add_flag(seat_heat_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(seat_heat_arc);

    seat_heat_label = lv_label_create(screen);
    lv_obj_set_style_text_font(seat_heat_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(seat_heat_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(seat_heat_label, LV_OBJ_FLAG_HIDDEN);

    // Initialize fan speed arc and label
    fan_speed_arc = lv_arc_create(screen);
    lv_obj_set_size(fan_speed_arc, 200, 200);
    lv_arc_set_rotation(fan_speed_arc, 135);
    lv_arc_set_bg_angles(fan_speed_arc, 0, 270);
    lv_arc_set_range(fan_speed_arc, 0, 3);
    lv_obj_add_flag(fan_speed_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(fan_speed_arc);

    fan_speed_label = lv_label_create(screen);
    lv_obj_set_style_text_font(fan_speed_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(fan_speed_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(fan_speed_label, LV_OBJ_FLAG_HIDDEN);

    // Initialize mode icons
    LV_IMG_DECLARE(x20_mode_auto);
    LV_IMG_DECLARE(x20_mode_heat);
    LV_IMG_DECLARE(x20_mode_cool);

    for (int i = 0; i < 3; i++)
    {
        mode_icons[i] = lv_img_create(screen);
        lv_img_set_src(mode_icons[i], i == 0 ? &x20_mode_auto : (i == 1 ? &x20_mode_heat : &x20_mode_cool));
        lv_obj_align(mode_icons[i], LV_ALIGN_BOTTOM_MID, (i - 1) * 40, -20);
    }

    updateLabels();
    updateArcs();
    updateModeIcons();
}

EntityStateUpdate CarClimateApp::updateStateFromKnob(PB_SmartKnobState state)
{
    SemaphoreGuard lock(mutex_);
    int new_value = state.current_position;

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        temperature = 16.0f + (new_value * 0.1f);
        break;
    case Setting::SEAT_HEAT:
        seat_heat = new_value;
        break;
    case Setting::FAN_SPEED:
        fan_speed = new_value;
        break;
    }

    updateLabels();
    updateArcs();
    updateModeIcons();
    updateMotorConfig();

    EntityStateUpdate new_state;
    sprintf(new_state.app_id, "%s", app_id);
    sprintf(new_state.entity_id, "%s", entity_id);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "temperature", temperature);
    cJSON_AddNumberToObject(json, "seat_heat", seat_heat);
    cJSON_AddNumberToObject(json, "fan_speed", fan_speed);
    cJSON_AddNumberToObject(json, "current_setting", static_cast<int>(current_setting));

    char *json_string = cJSON_PrintUnformatted(json);
    sprintf(new_state.state, "%s", json_string);

    cJSON_free(json_string);
    cJSON_Delete(json);

    new_state.changed = true;
    sprintf(new_state.app_slug, "%s", APP_SLUG_CAR_CLIMATE);

    return new_state;
}

void CarClimateApp::updateDisplay(const PB_SmartKnobState &state)
{
    SemaphoreGuard lock(mutex_);
    int new_value = state.current_position;

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        temperature = 16.0f + (new_value * 0.1f);
        break;
    case Setting::SEAT_HEAT:
        seat_heat = new_value;
        break;
    case Setting::FAN_SPEED:
        fan_speed = new_value;
        break;
    }

    updateLabels();
    updateArcs();
    updateModeIcons();
    updateMotorConfig();
    triggerMotorConfigUpdate();
    updateStateFromKnob(state);
}

int8_t CarClimateApp::navigationNext()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        current_setting = Setting::SEAT_HEAT;
        break;
    case Setting::SEAT_HEAT:
        current_setting = Setting::FAN_SPEED;
        break;
    case Setting::FAN_SPEED:
        current_setting = Setting::TEMPERATURE;
        break;
    }
    updateMotorConfig();
    updateModeIcons();
    updateLabels();
    updateArcs();
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}

void CarClimateApp::updateLabels()
{
    char buf[16];
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        snprintf(buf, sizeof(buf), "%.1f°C", temperature);
        lv_label_set_text(temp_label, buf);
        break;
    case Setting::SEAT_HEAT:
        snprintf(buf, sizeof(buf), "%d", seat_heat);
        lv_label_set_text(seat_heat_label, buf);
        break;
    case Setting::FAN_SPEED:
        snprintf(buf, sizeof(buf), "%d", fan_speed);
        lv_label_set_text(fan_speed_label, buf);
        break;
    }
}

void CarClimateApp::updateArcs()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        lv_arc_set_value(temp_arc, (int)(temperature * 10) - 160);
        break;
    case Setting::SEAT_HEAT:
        lv_arc_set_value(seat_heat_arc, seat_heat);
        break;
    case Setting::FAN_SPEED:
        lv_arc_set_value(fan_speed_arc, fan_speed);
        break;
    }
}

void CarClimateApp::updateModeIcons()
{
    for (int i = 0; i < 3; i++)
    {
        if (i == static_cast<int>(current_setting))
        {
            lv_obj_clear_flag(mode_icons[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_img_recolor(mode_icons[i], lv_color_hex(0xFFFFFF), 0);
        }
        else
        {
            lv_obj_add_flag(mode_icons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_add_flag(temp_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(seat_heat_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fan_speed_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(seat_heat_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fan_speed_label, LV_OBJ_FLAG_HIDDEN);

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        lv_obj_clear_flag(temp_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case Setting::SEAT_HEAT:
        lv_obj_clear_flag(seat_heat_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(seat_heat_label, LV_OBJ_FLAG_HIDDEN);
        break;
    case Setting::FAN_SPEED:
        lv_obj_clear_flag(fan_speed_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fan_speed_label, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

void CarClimateApp::updateMotorConfig()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        motor_config.min_position = 0;
        motor_config.max_position = 140;
        motor_config.position = (int)((temperature - 16.0f) * 10);
        motor_config.position_width_radians = 8.225806452 * PI / 120;
        break;
    case Setting::SEAT_HEAT:
    case Setting::FAN_SPEED:
        motor_config.min_position = 0;
        motor_config.max_position = 3;
        motor_config.position = current_setting == Setting::SEAT_HEAT ? seat_heat : fan_speed;
        motor_config.position_width_radians = 8.225806452 * PI / 4; // Increased step size
        break;
    }
    motor_config.position_nonce = motor_config.position;
    motor_config.detent_strength_unit = 2;
    motor_config.endstop_strength_unit = 1;
    motor_config.snap_point = 1.1;
}

void CarClimateApp::triggerMotorConfigUpdate()
{
    if (motor_notifier != nullptr)
    {
        motor_notifier->requestUpdate(motor_config);
    }
}
