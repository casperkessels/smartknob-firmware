#pragma once
#include "../app.h"

enum ClimateControlAppMode : uint8_t
{
    CLIMATE_TEMP = 0,
    FAN_SPEED = 1,
    SEAT_HEATING = 2,
    MODE_COUNT = 3
};

class ClimateControlApp : public App
{
public:
    ClimateControlApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_);
    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;
    int8_t navigationNext() override;

private:
    // Core methods
    void initScreen();
    void initTemperatureArc();
    void updateTemperatureArc();
    void updateModeIcon();
    void initFanSpeed();
    void updateFanSpeed();
    void initSeatHeating();
    void updateSeatHeating();

    // State variables
    float adjusted_sub_position = 0;
    bool first_run = false;

    ClimateControlAppMode mode = ClimateControlAppMode::CLIMATE_TEMP;
    ClimateControlAppMode last_mode = ClimateControlAppMode::CLIMATE_TEMP;

    // Saved positions
    uint8_t climate_saved_position = 20;
    uint8_t fan_speed_saved_position = 0;
    uint8_t seat_heating_saved_position = 0;

    // Temperature ranges and current values
    const uint8_t CLIMATE_APP_MIN_TEMP = 16;
    const uint8_t CLIMATE_APP_MAX_TEMP = 25;
    uint8_t current_temperature = 20;
    uint8_t target_temperature = 25;
    uint8_t last_target_temperature = 25;

    // Fan speed states
    uint8_t current_fan_speed_position = 3;
    uint8_t last_fan_speed_position = 0;
    bool fan_speed_state = false;

    // Seat heating states
    uint8_t current_seat_heating_position = 0;
    uint8_t last_seat_heating_position = 0;
    bool seat_heating_state = false;

    // UI Constants
    const uint16_t MIN_ANGLE = 0;
    const uint16_t MAX_ANGLE = 240;
    const uint16_t ROTATION_ANGLE = (180 - MAX_ANGLE + 360) / 2;
    const uint8_t ARC_WIDTH = 16;
    uint16_t ONE_STEP_ANGLE = MAX_ANGLE / (CLIMATE_APP_MAX_TEMP - CLIMATE_APP_MIN_TEMP) + 1;

    // Climate screen elements
    lv_obj_t *climate_screen;
    lv_obj_t *state_label;
    lv_obj_t *target_temp_label;
    lv_obj_t *current_temp_label;
    lv_obj_t *temperature_arc;
    lv_obj_t **temperature_dots;

    // Fan screen elements
    lv_obj_t *fan_speed_screen;
    // lv_obj_t *fan_speed_arcs[5];
    lv_obj_t *fan_speed_bulb;

    lv_obj_t *fan_speed_arc;
    lv_obj_t **fan_speed_dots;
    lv_obj_t *fan_speed_label;

    // Seat heating elements
    lv_obj_t *seat_heating_screen;
    lv_obj_t *seat_heating_arcs[4];
    lv_obj_t *seat_heating_bulb;
    lv_obj_t *seat_heating_bg_arc;

    // Icons for each mode
    const lv_img_dsc_t *temp_img;
    const lv_img_dsc_t *fan_img;
    const lv_img_dsc_t *seat_img;

    // Mode icons for climate screen
    lv_obj_t *climate_mode_auto_icon;
    lv_obj_t *climate_mode_cool_icon;
    lv_obj_t *climate_mode_heat_icon;

    // Mode icons for fan screen
    lv_obj_t *fan_speed_mode_auto_icon;
    lv_obj_t *fan_speed_mode_cool_icon;
    lv_obj_t *fan_speed_mode_heat_icon;

    // Mode icons for seat heating
    lv_obj_t *seat_heating_mode_auto_icon;
    lv_obj_t *seat_heating_mode_cool_icon;
    lv_obj_t *seat_heating_mode_heat_icon;

    // Colors
    const lv_color_t arc_inactive_color = LV_COLOR_MAKE(0x47, 0x47, 0x47);
    const lv_color_t arc_active_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
    const lv_color_t inactive_color = LV_COLOR_MAKE(0x47, 0x47, 0x47);
    const lv_color_t auto_active_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
    const lv_color_t cool_active_color = LV_COLOR_MAKE(0x50, 0x64, 0xC8);
    const lv_color_t dark_cool_active_color = LV_COLOR_MAKE(0x3E, 0x4E, 0x9C);
    const lv_color_t heat_active_color = LV_COLOR_MAKE(0xFF, 0x80, 0x00);
    const lv_color_t dark_heat_active_color = LV_COLOR_MAKE(0xC7, 0x6D, 0x12);
    const lv_color_t air_active_color = LV_COLOR_MAKE(0xB4, 0xFF, 0x00);
};