#pragma once
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL      GPIO_NUM_9
#define I2C_MASTER_SDA      GPIO_NUM_8
#define I2C_PORT            I2C_NUM_0

#define MUX_ADDR            0x70
#define MUX_CHANNEL_LENGTH  I2C_ADDR_BIT_LEN_7
#define UINT_MUX_LENGTH     7

namespace Mux {

    SemaphoreHandle_t muxMutex = nullptr;

    // BUS HANDLE
    i2c_master_bus_handle_t busHandle = nullptr;
    // MUX HANDLE
    i2c_master_dev_handle_t muxHandle = nullptr;

    inline esp_err_t setup (){
        // Configuration function for I2C

        //Main I2C Config
        i2c_master_bus_config_t i2cMasterConfig = {
            .i2c_port = I2C_PORT,                   //I2C PORT
            .sda_io_num = I2C_MASTER_SDA,           //SDA PIN
            .scl_io_num = I2C_MASTER_SCL,           //SCL PIN
            .clk_source = I2C_CLK_SRC_DEFAULT,      //CLOCK SOURCE
            .glitch_ignore_cnt = GPIO_NUM_7,        //NOISE FILTER THRESHOLD
            .intr_priority = 0,                     //INTERRUPT PRIORITY
            .trans_queue_depth = 0,                 //TRANSACTION QUEUE FOR ASYNC
            .flags = {
                .enable_internal_pullup = true,     //INTERNAL PULL-UP RESISTOR
            },
        };

        // Master Bus for I2C, if there's an error throw it to the declaration
        esp_err_t status = i2c_new_master_bus(&i2cMasterConfig, &busHandle);
        if (status != ESP_OK) { return status; }

        // Mux Configuration
        i2c_device_config_t muxConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = MUX_ADDR,
            .scl_speed_hz = 10000,
        };

        // Add the mux to the I2C Master Bus, return if !ESP_OK
        status = i2c_master_bus_add_device(busHandle, &muxConfig, &muxHandle);
        if (status != ESP_OK) { return status; }

        // Instantiate the Mux Mutex
        muxMutex = xSemaphoreCreateMutex();

        //Everything was instantiated without failures
        return ESP_OK;
    }

    inline esp_err_t selectChannel (const uint8_t channel) {
        // Select a target channel on the mux

        // The channel for the mux is invalid
        if (channel > UINT_MUX_LENGTH || channel < 0) {
            return ESP_ERR_INVALID_ARG;
        }

        // Target channel
        uint8_t controlRegister = (1 << channel);

        esp_err_t ret = i2c_master_transmit(muxHandle, &controlRegister, 1, -1);
        
        if (ret != ESP_OK) {
            return ret;
        }

        // Channel was successfully created
        return ESP_OK;
    }
}