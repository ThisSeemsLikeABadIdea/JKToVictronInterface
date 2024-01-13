



/**
 * JKBMS.h
 * 
 * JKBMS interface, a library for the ESP32 platform
 * Heavily plaguarized from the ESP home library and some 
 * from an STM32 repo. (DIY BMS) 
 * 
 * Pins used GPIO 16 for TX, 17 for RX and a GND
 * the plug on the JKBMS is the RS485 plug
 * it is a 1.25 pitch JST connector
 * White wire to TX, Red wire to RX, Black wire to GND. Pins are 1 - GND, 2 - TX, 3 - rX, VCC (this states bat v which is potentially 50+v so be careful[not tested])
 * 
 * @author Creator lmnewey 
 * @version 0.0.0
 * @license MIT
 */

#ifndef JKBMSINTERFACE_H
#define JKBMSINTERFACE_H


// #ifdef __cplusplus
// extern "C" {
//#endif

#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include <stdint.h>

#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>

#ifdef __cplusplus
#include <vector>
#include <iostream>

extern "C" {
#endif

// C-compatible functions, define method/function calls here to access data from the main thread or Victron thread
const uint8_t* get_vector_data();
size_t get_vector_size();
float get_cell_voltage(int cellnum);
float get_pack_voltage();
float get_pack_current();
float get_cell_count();
int get_cell_H_n();
 int get_cell_L_n();
 int get_cell_H_v();
 int get_cell_L_v();
int get_temps(int tempnum);
float get_state_of_Charge();

#ifdef __cplusplus
}

class JKBMS {
public:
    void start();
    bool printOutPut;
    // Add other member functions and variables as needed

    // Example method to access the vector
    std::vector<uint8_t>& getMyVector();
private:
    int portNum;
    int rx_buffer_size;
    std::vector<uint8_t> myVector; // The vector you want to access from C
};
#endif

#endif // JKBMSINTERFACE_H



// from the ESPHOME implementation, keep for reference we may want to follow some of these examples for alarms etc
// int taskDelay;
// float power_tube_temperature_sensor_ ;
//  float temperature_sensor_1_sensor_ ;
//   float temperature_sensor_2_sensor_ ;
//   float total_voltage ;
//    float current_sensor_ ;
//    float current ;
//   float power ;
//   uint8_t raw_battery_remaining_capacity ;
//   float capacity_remaining_sensor_ ;
//   float temperature_sensors_sensor_ ;
//   float charging_cycles_sensor_ ;
//   float total_charging_cycle_capacity_sensor_ ;
//   float battery_strings_sensor_ ;
//   uint16_t raw_errors_bitmask ;
//   float errors_bitmask_sensor_ ;
//   std::string errors_text_sensor_ ;
//   float total_voltage_overvoltage_protection_sensor_ ;
//   float total_voltage_undervoltage_protection_sensor_ ;
//   float cell_voltage_overvoltage_protection_sensor_ ;
//   float cell_voltage_overvoltage_recovery_sensor_;
//   float cell_voltage_overvoltage_delay_sensor_ ;
//   float cell_voltage_undervoltage_protection_sensor_ ;
//   float cell_voltage_undervoltage_recovery_sensor_ ;
//   float cell_voltage_undervoltage_delay_sensor_ ;
//   float cell_pressure_difference_protection_sensor_ ;
//   float discharging_overcurrent_protection_sensor_ ;
//   float discharging_overcurrent_delay_sensor_ ;
//   float charging_overcurrent_protection_sensor_ ;
//   float charging_overcurrent_delay_sensor_ ;
//   float balance_starting_voltage_sensor_ ;
//   float balance_opening_pressure_difference_sensor_ ;
//   bool balancing_switch_binary_sensor_ ;
//   float power_tube_temperature_protection_sensor_ ;
//   float power_tube_temperature_recovery_sensor_ ;
//   float temperature_sensor_temperature_protection_sensor_ ;
//   float temperature_sensor_temperature_recovery_sensor_ ;
//   float temperature_sensor_temperature_difference_protection_sensor_ ;
//   float charging_high_temperature_protection_sensor_ ;
//   float discharging_high_temperature_protection_sensor_ ;
//   float charging_low_temperature_protection_sensor_ ;
//   float charging_low_temperature_recovery_sensor_ ;
//   float discharging_low_temperature_protection_sensor_ ;
//   float discharging_low_temperature_recovery_sensor_ ;
//   uint32_t raw_total_battery_capacity_setting ;
//   float total_battery_capacity_setting_sensor_ ;
//   float capacity_remaining_derived_sensor_ ;
//   bool charging_switch_binary_sensor_ ;
//   bool discharging_switch_binary_sensor_ ;
//   float current_calibration_sensor_ ;
//   float device_address_sensor_ ;
//   uint8_t raw_battery_type ;
//   std::string battery_type_text_sensor_ ;    
//   float sleep_wait_time_sensor_ ;
//   float alarm_low_volume_sensor_ ;
//   std::string password_text_sensor_ ;
//   bool dedicated_charger_switch_binary_sensor_ ;
//   std::string device_type_text_sensor_ ;
//   float total_runtime_sensor_ ;
//   std::string software_version_text_sensor_ ;
//   float actual_battery_capacity_sensor_ ;
//   std::string manufacturer_text_sensor_ ;
//   float protocol_version_sensor_ ;
//   int years;
//   int days;
//   int hours;


// #ifdef __cplusplus
// }
// #endif
// class JkModbusDevice {
//  public:
//   // void set_parent(JkModbus *parent) { parent_ = parent; }
//   // void set_address(uint8_t address) { address_ = address; }
//   // virtual void on_jk_modbus_data(const uint8_t &function, const std::vector<uint8_t> &data) = 0;

//   // void send(int8_t function, uint8_t address, uint8_t value) { this->parent_->send(function, address, value); }
//   // void write_register(uint8_t address, uint8_t value) { this->parent_->write_register(address, value); }
//   // void read_registers() { this->parent_->read_registers(); }

//  protected:
//   friend JkModbus;

//   // JkModbus *parent_;
//   // uint8_t address_;
// };
