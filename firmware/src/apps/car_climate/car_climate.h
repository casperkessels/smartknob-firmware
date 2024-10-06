#pragma once

#include "../app.h"

class CarClimateApp : public App
{
public:
    CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_);
    void initScreen() override;
    void updateDisplay(const PB_SmartKnobState &state);
    int8_t navigationNext() override;

private:
    enum class Setting
    {
        TEMPERATURE,
        FAN_SPEED,
        SEAT_HEAT
    };

    Setting current_setting;
    float temperature;
    int fan_speed;
    int seat_heat;

    lv_obj_t *temp_label;
    lv_obj_t *fan_label;
    lv_obj_t *seat_label;

    void updateLabels();
    void updateMotorConfig();
    void triggerMotorConfigUpdate();
};