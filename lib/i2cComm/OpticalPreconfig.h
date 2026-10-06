#pragma once
#include "ARC_Sensor.h"
#include "SensorUtils.h"
#include "i2cUtils.h"
#include "I2CBus.h"

// I2C Address for the Optical Sensor
#define OTOS_I2C_ADDRESS 0x17

#define OTOS_REG_OFFSETS        0x10  // 6 bytes (Off_X, Off_Y, Off_H)
#define OTOS_REG_POS            0x20  // 6 bytes (Pos_X, Pos_Y, Pos_H)
#define OTOS_REG_VEL            0x26  // 6 bytes (Vel_X, Vel_Y, Vel_H)
#define OTOS_REG_ACC            0x2C  // 6 bytes (Acc_X, Acc_Y, Acc_H)

namespace OpticalPreconfig {
    
    template <typename S, size_t M>
    inline esp_err_t getOpticalPosition(Vec3<S> &packet, I2CBus<M> &commBus) {
        // Get the positioning from the optical sensor
        // Packet data is stored in vec3,
        // ESP status is returned

        uint8_t opticalData[6]{};

        esp_err_t status = commBus.readRegister(OTOS_REG_POS, opticalData, sizeof(opticalData) / sizeof(opticalData[0]));

        if(status != ESP_OK){
            return status;
        }

        packet.x = static_cast<S>(i2cUtils::convertTo16Bits(&opticalData[0]));     // X Positioning
        packet.y = static_cast<S>(i2cUtils::convertTo16Bits(&opticalData[2]));     // Y Positioning
        packet.z = static_cast<S>(i2cUtils::convertTo16Bits(&opticalData[4]));     // Heading

        return ESP_OK;
    }

    template <size_t M>
    inline esp_err_t setOffSets(I2CBus<M> &commBus, uint16_t offX, uint16_t offY, uint16_t offH) {
        // Set the offsets for the optical sensor

        // Setup the array set of offsets
        uint8_t offsetWrite[6] = {
            (uint8_t)offX, (uint8_t)(offX >> 8),
            (uint8_t)offY, (uint8_t)(offY >> 8),
            (uint8_t)offH, (uint8_t)(offH >> 8)
        };

        //Write, return the success
        return commBus.writeArrayRegister<6>(OTOS_REG_OFFSETS, offsetWrite);
    }
}