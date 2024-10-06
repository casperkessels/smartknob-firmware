#pragma once

#include "../app.h"

class CarClimateApp : public App
{
public:
    CarClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_);
    void initScreen() override;
    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    int8_t navigationNext() override;

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

    lv_obj_t *temp_label;
    lv_obj_t *seat_label;
    lv_obj_t *fan_label;
    lv_obj_t *arc;
    lv_obj_t *icon_temp;
    lv_obj_t *icon_seat;
    lv_obj_t *icon_fan;

    void updateLabels();
    void updateArc();
    void updateIcons();
    void updateMotorConfig();
};