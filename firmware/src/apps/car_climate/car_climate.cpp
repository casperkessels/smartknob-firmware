#include "car_climate.h"
#include "assets/images/icons.h"
#include "lvgl.h" // Add this line to include the LVGL library

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

    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, 200, 200);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_center(arc);

    temp_label = lv_label_create(screen);
    lv_obj_set_style_text_font(temp_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -20);

    seat_label = lv_label_create(screen);
    lv_obj_set_style_text_font(seat_label, &roboto_light_mono_24pt, 0);
    lv_obj_align(seat_label, LV_ALIGN_CENTER, 0, 40);

    fan_label = lv_label_create(screen);
    lv_obj_set_style_text_font(fan_label, &roboto_light_mono_24pt, 0);
    lv_obj_align(fan_label, LV_ALIGN_CENTER, 0, 80);

    icon_temp = lv_img_create(screen);
    icon_seat = lv_img_create(screen);
    icon_fan = lv_img_create(screen);

    // Use placeholder icons for now
    lv_img_set_src(icon_temp, LV_SYMBOL_SETTINGS);
    lv_img_set_src(icon_seat, LV_SYMBOL_SETTINGS);
    lv_img_set_src(icon_fan, LV_SYMBOL_REFRESH);

    lv_obj_align(icon_temp, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_align(icon_seat, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_align(icon_fan, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

    updateLabels();
    updateArc();
    updateIcons();
}

void CarClimateApp::updateIcons()
{
    lv_obj_clear_flag(icon_temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(icon_seat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(icon_fan, LV_OBJ_FLAG_HIDDEN);

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        lv_obj_add_flag(icon_seat, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(icon_fan, LV_OBJ_FLAG_HIDDEN);
        break;
    case Setting::SEAT_HEAT:
        lv_obj_add_flag(icon_temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(icon_fan, LV_OBJ_FLAG_HIDDEN);
        break;
    case Setting::FAN_SPEED:
        lv_obj_add_flag(icon_temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(icon_seat, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    lv_obj_set_style_text_color(icon_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(icon_seat, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(icon_fan, lv_color_hex(0xFFFFFF), 0);
}

EntityStateUpdate CarClimateApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;
    int new_value = state.current_position;

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        temperature = 16 + (new_value * 0.5f);
        if (temperature < 16)
            temperature = 16;
        if (temperature > 28)
            temperature = 28;
        break;
    case Setting::SEAT_HEAT:
        seat_heat = new_value;
        if (seat_heat < 0)
            seat_heat = 0;
        if (seat_heat > 3)
            seat_heat = 3;
        break;
    case Setting::FAN_SPEED:
        fan_speed = new_value;
        if (fan_speed < 0)
            fan_speed = 0;
        if (fan_speed > 3)
            fan_speed = 3;
        break;
    }

    updateLabels();
    updateArc();
    updateIcons();
    updateMotorConfig();

    // Prepare new_state with the updated values
    sprintf(new_state.app_id, "%s", app_id);
    sprintf(new_state.entity_id, "%s", entity_id);
    char state_json[128];
    snprintf(state_json, sizeof(state_json),
             "{\"temperature\":%.1f,\"seat_heat\":%d,\"fan_speed\":%d}",
             temperature, seat_heat, fan_speed);
    sprintf(new_state.state, "%s", state_json);
    new_state.changed = true;

    return new_state;
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
    updateIcons();
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}

void CarClimateApp::updateLabels()
{
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", temperature);
    lv_label_set_text(temp_label, temp_str);
    lv_label_set_text_fmt(seat_label, "Seat: %d", seat_heat);
    lv_label_set_text_fmt(fan_label, "Fan: %d", fan_speed);
}

void CarClimateApp::updateArc()
{
    int32_t arc_value;
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        arc_value = (temperature - 16) * 2 * 270 / 24;
        break;
    case Setting::SEAT_HEAT:
    case Setting::FAN_SPEED:
        arc_value = current_setting == Setting::SEAT_HEAT ? seat_heat : fan_speed;
        arc_value = arc_value * 270 / 3;
        break;
    }
    lv_arc_set_value(arc, arc_value);
}

void CarClimateApp::updateMotorConfig()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        motor_config.min_position = 0;
        motor_config.max_position = 24;
        motor_config.position = (temperature - 16) * 2;
        motor_config.position_width_radians = 8.225806452 * PI / 120;
        break;
    case Setting::SEAT_HEAT:
    case Setting::FAN_SPEED:
        motor_config.min_position = 0;
        motor_config.max_position = 3;
        motor_config.position = current_setting == Setting::SEAT_HEAT ? seat_heat : fan_speed;
        motor_config.position_width_radians = 8.225806452 * PI / 60;
        break;
    }
    motor_config.position_nonce = motor_config.position;
    motor_config.detent_strength_unit = 2;
    motor_config.endstop_strength_unit = 1;
    motor_config.snap_point = 1.1;
}