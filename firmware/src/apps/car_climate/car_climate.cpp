#include "car_climate.h"

CarClimateApp::CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    temperature = 20.0f;
    fan_speed = 0;
    seat_heat = 0;
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

    temp_label = lv_label_create(screen);
    lv_obj_set_style_text_font(temp_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -40);

    fan_label = lv_label_create(screen);
    lv_obj_set_style_text_font(fan_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(fan_label, LV_ALIGN_CENTER, 0, 20);

    seat_label = lv_label_create(screen);
    lv_obj_set_style_text_font(seat_label, &roboto_light_mono_48pt, 0);
    lv_obj_align(seat_label, LV_ALIGN_CENTER, 0, 60);

    updateLabels();
}

void CarClimateApp::updateLabels()
{
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f", temperature);
    lv_label_set_text_fmt(temp_label, "%s°C", temp_str);
    lv_label_set_text_fmt(fan_label, "Fan: %d", fan_speed);
    lv_label_set_text_fmt(seat_label, "Seat: %d", seat_heat);

    lv_obj_set_style_text_color(temp_label, current_setting == Setting::TEMPERATURE ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(fan_label, current_setting == Setting::FAN_SPEED ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(seat_label, current_setting == Setting::SEAT_HEAT ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
}

void CarClimateApp::updateDisplay(const PB_SmartKnobState &state)
{
    SemaphoreGuard lock(mutex_);
    int new_value = state.current_position;

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        temperature = 16 + (new_value * 0.5f);
        break;
    case Setting::FAN_SPEED:
        fan_speed = new_value;
        break;
    case Setting::SEAT_HEAT:
        seat_heat = new_value;
        break;
    }

    updateLabels();
    updateMotorConfig();
    triggerMotorConfigUpdate();
}

int8_t CarClimateApp::navigationNext()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        current_setting = Setting::FAN_SPEED;
        break;
    case Setting::FAN_SPEED:
        current_setting = Setting::SEAT_HEAT;
        break;
    case Setting::SEAT_HEAT:
        current_setting = Setting::TEMPERATURE;
        break;
    }
    updateMotorConfig();
    updateLabels();
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}

void CarClimateApp::updateMotorConfig()
{
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        motor_config.min_position = 0;
        motor_config.max_position = 32;
        motor_config.position = (temperature - 16) * 2;
        motor_config.position_width_radians = 8.225806452 * PI / 120;
        break;
    case Setting::FAN_SPEED:
        motor_config.min_position = 0;
        motor_config.max_position = 4;
        motor_config.position = fan_speed;
        motor_config.position_width_radians = 8.225806452 * PI / 60;
        break;
    case Setting::SEAT_HEAT:
        motor_config.min_position = 0;
        motor_config.max_position = 4;
        motor_config.position = seat_heat;
        motor_config.position_width_radians = 8.225806452 * PI / 60;
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