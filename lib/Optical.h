#pragma once
#include <string.h>

#include "driver/i2c.h"
#include "Mux.h"

// I2C Address for the Optical Sensor
#define OTOS_I2C_ADDRESS 0x17
#define OTOS_OFFSET_WRITE 0x10

namespace Optical {
    // Optical Functions that read and write to its registers

    esp_err_t setOffSets(uint16_t offX, uint16_t offY, uint16_t offH) {
        // Set the offsets for the optical sensor

        // Setup the array set of offsets
        uint8_t offsetWrite[6] = {offX, offX >> 8, offY, offY >> 8, offH, offH >> 8};

        //Write, return the success
        return writeArrayRegister(OTOS_OFFSET_WRITE, offsetWrite);
    }

    esp_err_t readRegister(uint8_t regAddr, uint8_t *data, size_t length) {
        //Read the data within the register, returns an esp status of the optical

        //Write, return the success
        return i2c_master_write_read_device(
            I2C_PORT, 
            OTOS_I2C_ADDRESS,
            &regAddr,
            1,
            data,
            length,
            pdMS_TO_TICKS(1000)
        );
    }

    esp_err_t writeRegister(uint8_t regAddr, uint8_t data) {
        //Write data at a specific register to the optical Sensor

        uint8_t writeBuffer[2] = {regAddr, data};
        
        return i2c_master_write_to_device(
            I2C_PORT,
            OTOS_I2C_ADDRESS,
            writeBuffer,
            sizeof(writeBuffer),
            pdMS_TO_TICKS(1000)
        );
    }

    esp_err_t writeArrayRegister(const uint8_t regAddr, const uint8_t array[]) {
        // Write an array of any length to a target

        if (sizeof(array) == 0) { return; }

        uint8_t arrayLength = sizeof(array) / sizeof(array[0]);

        //Create an array with the register at location 0
        uint8_t registeredArray[arrayLength + 1];
        registeredArray[0] = regAddr;

        // Copy the array data
        memcpy(&registeredArray[1], array, arrayLength);

        return i2c_master_write_to_device(
            I2C_PORT,
            OTOS_I2C_ADDRESS,
            array,
            sizeof(array),
            pdMS_TO_TICKS(1000)
        );
    }
};