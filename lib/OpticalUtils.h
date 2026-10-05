#pragma once
#include <string.h>

#include "driver/i2c_master.h"
#include "Mux.h"
#include "Vec3.h"

// I2C Address for the Optical Sensor
#define OTOS_I2C_ADDRESS 0x17

#define OTOS_REG_OFFSETS        0x10  // 6 bytes (Off_X, Off_Y, Off_H)
#define OTOS_REG_POS            0x20  // 6 bytes (Pos_X, Pos_Y, Pos_H)
#define OTOS_REG_VEL            0x26  // 6 bytes (Vel_X, Vel_Y, Vel_H)
#define OTOS_REG_ACC            0x2C  // 6 bytes (Acc_X, Acc_Y, Acc_H)

namespace Optical {
    // Optical Functions that read and write to its registers

    i2c_master_dev_handle_t opticalHandle = nullptr;

    inline esp_err_t setup(const uint8_t channel) {
        if (Mux::busHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        // Ensure that the mux is on the correct channel before completing the
        // setup
        esp_err_t channelValidation = Mux::selectChannel(channel);

        if (channelValidation != ESP_OK) { return channelValidation; }

        i2c_device_config_t opticalConfig{};
        opticalConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        opticalConfig.device_address = OTOS_I2C_ADDRESS;
        opticalConfig.scl_speed_hz = 100000;

        return i2c_master_bus_add_device(Mux::busHandle, &opticalConfig, &opticalHandle);
    }

    inline esp_err_t readRegister(uint8_t regAddr, uint8_t *data, size_t length) {
        //Read the data within the register, returns an esp status of the optical

        if (opticalHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        //Write, return the success
        return i2c_master_transmit_receive(opticalHandle, &regAddr, 1, data, length, 1000);
    }

    inline esp_err_t writeRegister(uint8_t regAddr, uint8_t data) {
        //Write data at a specific register to the optical Sensor

        if (opticalHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        uint8_t writeBuffer[2] = {regAddr, data};
        
        return i2c_master_transmit(opticalHandle, writeBuffer, sizeof(writeBuffer), 1000);
    }

    int16_t convertTo16Bits(const uint8_t* data) {
        const uint16_t converted =
            static_cast<uint16_t>(data[0]) |            // Rightmost data
            (static_cast<uint16_t>(data[1] << 8));      // Leftmost data

        return static_cast<int16_t>(converted);
    }

    template<size_t S>
    esp_err_t writeArrayRegister(const uint8_t regAddr, const uint8_t (&array)[S]) {
        // Write an array of any length to a target

        if (opticalHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        // Create the writeBuffer with the registry
        // as the first element
        uint8_t writeBuffer[S + 1]{regAddr};

        // Copy over the passed array
        memcpy(writeBuffer + 1, array, S);

        return i2c_master_transmit(opticalHandle, writeBuffer, sizeof(writeBuffer), 1000);
    }

    template <typename S>
    inline esp_err_t getOpticalPosition(Vec3<S> &packet) {
        // Get the positioning from the optical sensor
        // Packet data is stored in vec3,
        // ESP status is returned

        if (opticalHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        uint8_t opticalData[6]{};

        esp_err_t status = readRegister(OTOS_REG_POS, opticalData, sizeof(opticalData)/ sizeof(opticalData[0]));

        if(status != ESP_OK){
            return status;
        }

        packet.x = static_cast<S>(Optical::convertTo16Bits(&opticalData[0]));     // X Positioning
        packet.y = static_cast<S>(Optical::convertTo16Bits(&opticalData[2]));     // Y Positioning
        packet.z = static_cast<S>(Optical::convertTo16Bits(&opticalData[4]));     // Heading

        return ESP_OK;
    }

    inline esp_err_t setOffSets(uint16_t offX, uint16_t offY, uint16_t offH) {
        // Set the offsets for the optical sensor

        if (opticalHandle == nullptr) { return ESP_ERR_INVALID_STATE; }

        // Setup the array set of offsets
        uint8_t offsetWrite[6] = {
            (uint8_t)offX, (uint8_t)(offX >> 8),
            (uint8_t)offY, (uint8_t)(offY >> 8),
            (uint8_t)offH, (uint8_t)(offH >> 8)
        };

        //Write, return the success
        return writeArrayRegister<6>(OTOS_REG_OFFSETS, offsetWrite);
    }
};