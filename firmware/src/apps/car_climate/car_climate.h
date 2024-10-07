#pragma once

#include "../app.h"

class CarClimateApp : public App
{
public:
    CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_);
    void initScreen() override;
    void updateDisplay(const PB_SmartKnobState &state);
    int8_t navigationNext() override;
    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;

private:
    enum class Setting
    {
        TEMPERATURE,
        SEAT_HEAT,
        FAN_SPEED
    };

    Setting current_setting;
    float temperature;
    int seat_heat;
    int fan_speed;

    int32_t temperature_position;
    int32_t seat_heat_position;
    int32_t fan_speed_position;

    void setTemperature(float temp);
    void setSeatHeat(int heat);
    void setFanSpeed(int speed);
    int32_t getPositionForCurrentSetting() const;

    lv_obj_t *temp_arc;
    lv_obj_t *temp_label;
    lv_obj_t *seat_heat_arc;
    lv_obj_t *seat_heat_label;
    lv_obj_t *fan_speed_arc;
    lv_obj_t *fan_speed_label;
    lv_obj_t *mode_icons[3];

    void updateLabels();
    void updateArcs();
    void updateModeIcons();
    void updateMotorConfig();
    void triggerMotorConfigUpdate();
    void initializeMode();
};