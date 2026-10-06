#pragma once

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

namespace i2cUtils {
    
    inline int16_t convertTo16Bits(const uint8_t* data) {
        const uint16_t converted =
            static_cast<uint16_t>(data[0]) |            // Rightmost data
            (static_cast<uint16_t>(data[1] << 8));      // Leftmost data

        return static_cast<int16_t>(converted);
    }
}