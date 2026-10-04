#pragma once

#include <Arduino.h>

struct BmsTelemetry {
    bool     connected          = false;
    String   bms_type           = "None";
    String   device_name        = "";
    String   mac_address        = "";
    uint32_t last_update        = 0;

    // Cells
    uint8_t  cell_count         = 8;
    float    cell_voltages[32]  = {0.0f};
    float    min_cell_v         = 0.0f;
    float    max_cell_v         = 0.0f;
    float    delta_cell_v       = 0.0f;
    uint8_t  min_cell_idx       = 0;
    uint8_t  max_cell_idx       = 0;

    // Pack totals
    float    total_voltage      = 0.0f; // V
    float    current            = 0.0f; // A (signed: + charge, - discharge)
    float    power              = 0.0f; // W
    float    soc                = 0.0f; // %
    float    capacity_remain    = 0.0f; // Ah
    float    capacity_nominal   = 0.0f; // Ah
    uint32_t cycle_count        = 0;

    // Temperatures
    float    temp_mos           = 0.0f; // °C
    float    temp_sensor1       = 0.0f; // °C
    float    temp_sensor2       = 0.0f; // °C

    // Balancer
    bool     balancing_active   = false;
    float    balancing_current  = 0.0f; // A

    // Switches
    bool     switch_charging    = false;
    bool     switch_discharging = false;
    bool     switch_balancer    = false;

    // Alarms / Status
    uint32_t raw_errors         = 0;
    String   errors_str         = "OK";
};
