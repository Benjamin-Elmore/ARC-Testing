#pragma once
#include "driver/i2c_master.h"
#include <array>

template <size_t N>
class I2CBus {
public:
    I2CBus(
        gpio_num_t gpioSCL,
        gpio_num_t gpioSDA,
        i2c_port_t i2cPort,
        std::array<uint8_t, N> muxAddresses = {}
    ) : _gpioSCL(gpioSCL), _gpioSDA(gpioSDA), _i2cPort(i2cPort), _muxAddresses(muxAddresses) {}

    ~I2CBus() = default;

    void setupI2CBus() {
        // Setup the i2c bus for this class and add multiplexers if there
        // any in the array

        i2c_master_bus_config_t i2cMasterConfig = {
            .i2c_port = _i2cPort,                   //I2C PORT
            .sda_io_num = _gpioSDA,           //SDA PIN
            .scl_io_num = _gpioSCL,           //SCL PIN
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

            i2c_device_config_t muxConfig = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = _muxAddresses.at(i),
                .scl_speed_hz = 10000,
            };

            // Add mux to the bus
            this->confirmStatus(i2c_master_bus_add_device(this->_busHandle, &muxConfig, &this->_muxHandles.at(i)));
        }
    }

    void confirmStatus(esp_err_t status) {
        if (status != ESP_OK && this->_sensorErrorStatus == ESP_OK) {
            this->_sensorErrorStatus = status;
        }
    }

private:
    i2c_master_bus_handle_t _busHandle = nullptr;
    gpio_num_t _gpioSCL{};
    gpio_num_t _gpioSDA{};
    uint8_t _i2cPort{};

    std::array<uint8_t, N> _muxAddresses{};
    std::array<i2c_master_dev_handle_t, N> _muxHandles{};

    esp_err_t _sensorErrorStatus = ESP_OK;
};