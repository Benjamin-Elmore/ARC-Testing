#pragma once

#include <array>
#include <unordered_map>
#include <string_view>
#include <cstring>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

#include "LockGuard.h"

#define I2C_MASTER_SCL      GPIO_NUM_9
#define I2C_MASTER_SDA      GPIO_NUM_8
#define I2C_PORT            I2C_NUM_0

#define MUX_ADDR            0x70
#define MUX_CHANNEL_LENGTH  I2C_ADDR_BIT_LEN_7
#define UINT_MUX_LENGTH     7
#define MUX_CHANNEL_CT 8

struct SensorRegistry {
    //Sensor registry and mux

    uint8_t muxNumber{};
    uint8_t muxChannel{};
    i2c_master_dev_handle_t sensorHandle{};

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

            this->_muxCurrentChannels.at(i) = 0xF;

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

    esp_err_t addSensorClassToBus (std::string_view name, uint8_t muxChannel, uint8_t i2cAddress) {

        if (this->_registryMap.contains(name)) { return ESP_ERR_INVALID_ARG; }

        SensorRegistry newSensorRegistry {
            static_cast<uint8_t>(muxChannel / MUX_CHANNEL_CT),                // MUX Index
            static_cast<uint8_t>(muxChannel % MUX_CHANNEL_CT)                 // MUX Channel within Index
        };

        // Add the new sensor, sensorHandle will be added after config is completed
        
        i2c_device_config_t sensorConfig = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = i2cAddress,
            .scl_speed_hz = 100000,
        };

        esp_err_t addConfigStatus = i2c_master_bus_add_device(
            this->_busHandle,                                   // Bus handle
            &sensorConfig,                                      // Sensor config obj
            &newSensorRegistry.sensorHandle                     // Sensor handle in the temp registry
        );

        this->_registryMap.emplace(name, newSensorRegistry);

        return addConfigStatus;
    }

    inline esp_err_t selectMuxChannel (const std::string_view sensorName) {
        // Select a target channel on the mux
        // NOTE: Use only if a mux exists for the I2CBus instance

        const auto sensorIt = this->_registryMap.find(sensorName);

        if (sensorIt == this->_registryMap.end()) {
            return ESP_ERR_INVALID_ARG;
        }

        SensorRegistry& targetSensor = sensorIt->second;

        i2c_master_dev_handle_t muxHandle = this->_muxHandles.at(targetSensor.muxNumber);

        // The channel for the mux is invalid
        if (targetSensor.muxChannel > UINT_MUX_LENGTH) {
            return ESP_ERR_INVALID_ARG;
        }

        // Target channel
        uint8_t controlRegister = (1 << targetSensor.muxChannel);

        // Transmit the data through a stored mux handle within i2c bus
        esp_err_t ret = i2c_master_transmit(this->_muxHandles.at(targetSensor.muxNumber), &controlRegister, 1, -1);
        
        if (ret != ESP_OK){
            this->_errorStatus = ret;
        } else {
            this->_muxCurrentChannels.at(targetSensor.muxNumber) = targetSensor.muxChannel;
        }

        // Return status
        return ret;
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

    SemaphoreHandle_t& getMutex(){ return this->_busMutex; }
    
    esp_err_t readRegister(const std::string_view name, const uint8_t regAddr, uint8_t *data, const size_t length) {
        //Read the data within the register, returns an esp status of the optical

        // If the name of the sensor isn't in the registry, return the error
        if (!this->_registryMap.contains(name)) { return ESP_ERR_INVALID_ARG; }
        
        SensorRegistry existingRegistry = this->_registryMap.at(name);

        //Write, return the success
        return i2c_master_transmit_receive(existingRegistry.sensorHandle, &regAddr, 1, data, length, 1000);
    }

    esp_err_t writeRegister(const std::string_view name, uint8_t regAddr, uint8_t data) {
        //Write data at a specific register to the optical Sensor

        // If the name of the sensor isn't in the registry, return the error
        if (!this->_registryMap.contains(name)) { return ESP_ERR_INVALID_ARG; }

        SensorRegistry existingRegistry = this->_registryMap.at(name);

        uint8_t writeBuffer[2] = {regAddr, data};
        
        return i2c_master_transmit(existingRegistry.sensorHandle, writeBuffer, sizeof(writeBuffer), 1000);
    }

    template<size_t S>
    esp_err_t writeArrayRegister(const std::string_view name, const uint8_t regAddr, const uint8_t (&array)[S]) {
        // Write an array of any length to a target

        // If the name of the sensor isn't in the registry, return the error
        if (!this->_registryMap.contains(name)) { return ESP_ERR_INVALID_ARG; }

        SensorRegistry existingRegistry = this->_registryMap.at(name);

        // Create the writeBuffer with the registry
        // as the first element
        uint8_t writeBuffer[S + 1]{regAddr};

        // Copy over the passed array
        std::memcpy(writeBuffer + 1, array, S);

        return i2c_master_transmit(existingRegistry.sensorHandle, writeBuffer, sizeof(writeBuffer), 1000);
    }


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
    std::array<uint8_t, N> _muxCurrentChannels{};

    std::unordered_map<std::string_view, SensorRegistry> _registryMap{};
    
    // Status of the mux
    esp_err_t _errorStatus = ESP_OK;
};