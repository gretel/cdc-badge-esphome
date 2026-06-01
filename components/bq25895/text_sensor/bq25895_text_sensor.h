#pragma once

#include "../bq25895.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::bq25895 {

// Bus status text sensor
class BQ25895BusStatusTextSensor : public BQ25895Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t status = (data.registers[BQ25895_REG_STATUS] >> 5) & 0x07;
    const char *status_str = this->get_bus_status_string_(status);
    this->publish_state(status_str);
  }

 protected:
  const char *get_bus_status_string_(uint8_t status) {
    switch (status) {
      case BUS_STATUS_NO_INPUT:
        return "No Input";
      case BUS_STATUS_USB_SDP:
        return "USB SDP";
      case BUS_STATUS_USB_CDP:
        return "USB CDP";
      case BUS_STATUS_USB_DCP:
        return "USB DCP";
      case BUS_STATUS_HVDCP:
        return "HVDCP";
      case BUS_STATUS_ADAPTER:
        return "Adapter";
      case BUS_STATUS_NO_STD_ADAPTER:
        return "Non-Standard Adapter";
      case BUS_STATUS_OTG:
        return "OTG";
      default:
        return "Unknown";
    }
  }
};

// Charge status text sensor
class BQ25895ChargeStatusTextSensor : public BQ25895Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t status = (data.registers[BQ25895_REG_STATUS] >> 3) & 0x03;
    const char *status_str = this->get_charge_status_string_(status);
    this->publish_state(status_str);
  }

 protected:
  const char *get_charge_status_string_(uint8_t status) {
    switch (status) {
      case CHARGE_STATUS_NOT_CHARGING:
        return "Not Charging";
      case CHARGE_STATUS_PRE_CHARGE:
        return "Pre-charge";
      case CHARGE_STATUS_FAST_CHARGE:
        return "Fast Charge";
      case CHARGE_STATUS_CHARGE_DONE:
        return "Charge Done";
      default:
        return "Unknown";
    }
  }
};

// NTC status text sensor
class BQ25895NtcStatusTextSensor : public BQ25895Listener, public text_sensor::TextSensor {
 public:
  void on_data(const BQ25895Data &data) override {
    uint8_t status = data.registers[BQ25895_REG_FAULT] & 0x07;
    const char *status_str = this->get_ntc_status_string_(status);
    this->publish_state(status_str);
  }

 protected:
  const char *get_ntc_status_string_(uint8_t status) {
    switch (status) {
      case 0:
        return "Normal";
      case 2:
        return "Warm";
      case 3:
        return "Cool";
      case 5:
        return "Cold";
      case 6:
        return "Hot";
      default:
        return "Unknown";
    }
  }
};

}  // namespace esphome::bq25895
