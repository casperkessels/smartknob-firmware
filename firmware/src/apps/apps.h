#pragma once

#include <map>

#include "app.h"
#include "../app_config.h"

// include all apps
#include "3d_printer_chamber.h"
#include "blinds.h"
#include "climate.h"
#include "light_dimmer.h"
#include "light_switch.h"
#include "music.h"
#include "settings.h"
#include "stopwatch.h"

// include all "menu" apps
#include "menu.h"
#include "onboarding.h"

// TODO: generate menu based on items in the map
class Apps
{
public:
    Apps();
    void add(uint8_t id, App *app);
    void clear();
    EntityStateUpdate update(AppState state);
    TFT_eSprite *renderActive();
    void setActive(uint8_t id);
    uint8_t navigationNext();
    PB_SmartKnobConfig getActiveMotorConfig();
    void setSprite(TFT_eSprite *spr_);
    void loadApp(uint8_t position, std::string app_slug, std::string app_id, std::string friendly_name);
    void updateMenu();

    void reload(cJSON *apps_);
    void createOnboarding();

private:
    QueueHandle_t mutex;
    std::map<std::string, std::shared_ptr<App>> apps;
    // std::vector<std::unique_ptr<App>> apps;
    uint8_t active_id = 0;

    TFT_eSprite *spr_ = nullptr;
    std::shared_ptr<App> active_app = nullptr;

    TFT_eSprite *rendered_spr_;

    std::shared_ptr<App> find(std::string id);
    void lock();
    void unlock();
};
