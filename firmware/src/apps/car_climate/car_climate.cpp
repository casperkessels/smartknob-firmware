#include "car_climate.h"

CarClimateApp::CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    updateMotorConfig();

    LV_IMG_DECLARE(x80_blind);
    LV_IMG_DECLARE(x40_blind);

    big_icon = x80_blind;
    small_icon = x40_blind;

    initScreen();
}

void CarClimateApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    temp_label = lv_label_create(screen);
    // lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_48, 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -40);

    fan_label = lv_label_create(screen);
    // lv_obj_set_style_text_font(fan_label, &lv_font_montserrat_24, 0);
    lv_obj_align(fan_label, LV_ALIGN_CENTER, 0, 20);

    seat_label = lv_label_create(screen);
    // lv_obj_set_style_text_font(seat_label, &lv_font_montserrat_24, 0);
    lv_obj_align(seat_label, LV_ALIGN_CENTER, 0, 60);

    updateLabels();
}

void CarClimateApp::updateDisplay(const PB_SmartKnobState &state)
{
    SemaphoreGuard lock(mutex_);

    int new_value = state.current_position;

    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        temperature = new_value;
        break;
    case Setting::FAN_SPEED:
        fan_speed = new_value;
        break;
    case Setting::SEAT_HEAT:
        seat_heat = new_value;
        break;
    }

    updateLabels();
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
    return 0;
}

void CarClimateApp::updateLabels()
{
    lv_label_set_text_fmt(temp_label, "%d°C", temperature);
    lv_label_set_text_fmt(fan_label, "Fan: %d", fan_speed);
    lv_label_set_text_fmt(seat_label, "Seat: %d", seat_heat);

    lv_obj_set_style_text_color(temp_label, current_setting == Setting::TEMPERATURE ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(fan_label, current_setting == Setting::FAN_SPEED ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_color(seat_label, current_setting == Setting::SEAT_HEAT ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
}

void CarClimateApp::updateMotorConfig()
{
    int min_value, max_value;
    switch (current_setting)
    {
    case Setting::TEMPERATURE:
        min_value = 16;
        max_value = 30;
        motor_config.position = temperature;
        break;
    case Setting::FAN_SPEED:
        min_value = 1;
        max_value = 5;
        motor_config.position = fan_speed;
        break;
    case Setting::SEAT_HEAT:
        min_value = 0;
        max_value = 3;
        motor_config.position = seat_heat;
        break;
    }

    motor_config.min_position = min_value;
    motor_config.max_position = max_value;
    motor_config.position_width_radians = 8.225806452 * PI / 120;
    motor_config.detent_strength_unit = 2;
    motor_config.endstop_strength_unit = 1;
    motor_config.snap_point = 1.1;
}