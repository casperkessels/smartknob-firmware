#pragma once
#include "../app.h"

enum ClimateAppMode : uint8_t
{
    CLIMATE_AUTO = 0,
    LIGHT_SWITCH = 1,
    MODE_COUNT = 2
};

class ClimateApp : public App
{
public:
    ClimateApp(SemaphoreHandle_t mutex, char *app_id, char *friendly_name, char *entity_id);
    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;
    int8_t navigationNext() override;

private:
    void initScreen();
    void initTemperatureArc();
    void updateTemperatureArc();
    void updateModeIcon();
    void initLightSwitch();
    void updateLightSwitch();

    float adjusted_sub_position = 0;
    bool first_run = false;

    ClimateAppMode mode = ClimateAppMode::CLIMATE_AUTO;
    ClimateAppMode last_mode = ClimateAppMode::CLIMATE_AUTO;

    uint8_t climate_saved_position = 25; // Default temperature
    uint8_t light_saved_position = 0;    // Default light state

    // Change these to uint8_t to match motor config
    uint8_t CLIMATE_APP_MIN_TEMP = 16;
    uint8_t CLIMATE_APP_MAX_TEMP = 35;
    uint8_t current_temperature = 20;
    uint8_t target_temperature = 25;
    uint8_t last_target_temperature = 25;

    // Light switch state
    uint8_t current_light_position = 0;
    uint8_t last_light_position = 0;
    bool light_state = false;

    const uint16_t MIN_ANGLE = 0;
    const uint16_t MAX_ANGLE = 240;
    const uint16_t ROTATION_ANGLE = (180 - MAX_ANGLE + 360) / 2;
    const uint8_t ARC_WIDTH = 16;
    uint16_t ONE_STEP_ANGLE = MAX_ANGLE / (CLIMATE_APP_MAX_TEMP - CLIMATE_APP_MIN_TEMP) + 1;

    // Climate UI elements
    lv_obj_t *climate_screen;
    lv_obj_t *state_label;
    lv_obj_t *target_temp_label;
    lv_obj_t *current_temp_label;
    lv_obj_t *mode_auto_icon;
    lv_obj_t *mode_cool_icon;
    lv_obj_t *mode_heat_icon;
    lv_obj_t *mode_air_icon;
    lv_obj_t *temperature_arc;
    lv_obj_t **temperature_dots;

    // Light switch UI elements
    lv_obj_t *light_screen;
    lv_obj_t *arcs[4];
    lv_obj_t *light_bulb;

    const lv_color_t arc_inactive_color = LV_COLOR_MAKE(0x47, 0x47, 0x47);
    const lv_color_t arc_active_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
    const lv_color_t inactive_color = LV_COLOR_MAKE(0x47, 0x47, 0x47);
    const lv_color_t auto_active_color = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
    const lv_color_t cool_active_color = LV_COLOR_MAKE(0x50, 0x64, 0xC8);
    const lv_color_t dark_cool_active_color = LV_COLOR_MAKE(0x3E, 0x4E, 0x9C);
    const lv_color_t heat_active_color = LV_COLOR_MAKE(0xFF, 0x80, 0x00);
    const lv_color_t dark_heat_active_color = LV_COLOR_MAKE(0xC7, 0x6D, 0x12);
    const lv_color_t air_active_color = LV_COLOR_MAKE(0xB4, 0xFF, 0x00);

    // Mode indicator icons for climate screen
    lv_obj_t *climate_mode_auto_icon;
    lv_obj_t *climate_mode_cool_icon;

    // Mode indicator icons for light switch screen
    lv_obj_t *light_mode_auto_icon;
    lv_obj_t *light_mode_cool_icon;
};