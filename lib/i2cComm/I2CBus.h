#pragma once

#include <array>
#include <unordered_map>
#include <string_view>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

#include "ARC_Sensor.h"

#define MUX_CHANNEL_CT 8

struct SensorRegistry {
    //Sensor registry and mux

    uint8_t muxNumber;
    uint8_t muxChannel;

    SensorRegistry(uint8_t addr, uint8_t channel) :
    muxNumber(addr), muxChannel(channel) {}
};

template <size_t N>
class I2CBus {
    // Bus Object for I2C communication to sensors,
    // encoders, and lighting

public:
    I2CBus(
        gpio_num_t gpioSCL,
        gpio_num_t gpioSDA,
        i2c_port_t i2cPort,
        std::array<uint8_t, N> muxAddresses = {}
    ) : _gpioSCL(gpioSCL), _gpioSDA(gpioSDA), _i2cPort(i2cPort), _muxAddresses(muxAddresses) {
        this->setupI2CBus();
    }

    ~I2CBus() = default;

    void setupI2CBus() {
        // Setup the i2c bus for this class and add multiplexers if there
        // any in the array

        // Configuration object for the bus
        i2c_master_bus_config_t i2cMasterConfig = {
            .i2c_port = _i2cPort,                   //I2C PORT
            .sda_io_num = _gpioSDA,                 //SDA PIN
            .scl_io_num = _gpioSCL,                 //SCL PIN
            .clk_source = I2C_CLK_SRC_DEFAULT,      //CLOCK SOURCE
            .glitch_ignore_cnt = GPIO_NUM_7,        //NOISE FILTER THRESHOLD
            .intr_priority = 0,                     //INTERRUPT PRIORITY
            .trans_queue_depth = 0,                 //TRANSACTION QUEUE FOR ASYNC
            .flags = {
                .enable_internal_pullup = true,     //INTERNAL PULL-UP RESISTOR
            },
        };

        // Create the master bus, controlled with the busHandle object
        this->confirmStatus(i2c_new_master_bus(&i2cMasterConfig, &this->_busHandle));

        
        // Include the muxes with the bus if needed, for loop will not run
        // if _muxAddresses is an empty list, as size = 0
        for (int i = 0; i < _muxAddresses.size(); i++) {
            // Create the muxConfig object

            i2c_device_config_t muxConfig = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = _muxAddresses.at(i),
                .scl_speed_hz = 10000,
            };

            // Add mux to the bus
            this->confirmStatus(i2c_master_bus_add_device(this->_busHandle, &muxConfig, &this->_muxHandles.at(i)));
        }

        // Once the muxes are added to the i2c bus, instantiate the RTOS mutex for the bus
        this->_busMutex = xSemaphoreCreateMutex();
    }

    esp_err_t addSensorClassToBus (Sensor& sensor) {

        SensorRegistry newSensorRegistry = new SensorRegistry(
            sensor.getMuxI2C() / MUX_CHANNEL_CT,                // MUX Index
            sensor.getMuxI2C() % MUX_CHANNEL_CT                 // MUX Channel within Index
        );

        this->_registryMap.emplace(sensor.getName(), newSensorRegistry);

        i2c_device_config_t sensorConfig = {
            .dev_addr_length = I2C_ADDR_BIT_7,
            .deviceAddress = sensor.getAddressI2C(),
            .scl_speed_hz = 100000,
        };

        return ESP_OK;
    }

    void confirmStatus(esp_err_t status) {
        // Check the status of the mux, and if it's not ESP_OK,
        // change the bus's status.

        if (status != ESP_OK && this->_errorStatus == ESP_OK) {
            this->_errorStatus = status;
        }
    }

    // Getter for the status of the bus, based on esp_err_t
    esp_err_t getStatus() { return this->_errorStatus; }

private:
    // Handle for I2C bus
    i2c_master_bus_handle_t _busHandle = nullptr;

    // I2C Port and pins
    const gpio_num_t _gpioSCL{};
    const gpio_num_t _gpioSDA{};
    const uint8_t _i2cPort{};

    // Bus Mutex
    SemaphoreHandle_t _busMutex{};

    // Addresses and handles for each mux
    const std::array<uint8_t, N> _muxAddresses{};
    std::array<i2c_master_dev_handle_t, N> _muxHandles{};

    std::unordered_map<std::string_view, Sensor&> _busSensors{};
    std::unordered_map<std::string_view, SensorRegistry> _registryMap{};
    
    // Status of the mux
    esp_err_t _errorStatus = ESP_OK;
};