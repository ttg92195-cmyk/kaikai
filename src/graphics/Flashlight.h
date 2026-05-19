#pragma once
#include "raylib.h"
#include "../utils/Constants.h"

class Flashlight {
public:
    Flashlight();
    void update(float deltaTime, Vector3 playerPos, Vector3 playerForward, float battery);
    void render() const;

    void toggle();
    bool isOn() const;
    float getBatteryLevel() const;
    void recharge(float amount);

    // Visual accessors
    float getIntensity() const;       // affected by battery level and flicker
    Vector3 getPosition() const;
    Vector3 getDirection() const;

private:
    bool on = true;
    float battery = DEFAULT_BATTERY;
    float flickerTimer = 0.0f;
    float flickerIntensity = 1.0f;
    Vector3 position = {0, 0, 0};
    Vector3 direction = {0, 0, -1};

    void updateFlicker(float deltaTime, float batteryLevel);
};
