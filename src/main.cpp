#include <stdio.h>
#include "ARC_Sensor.h"
#include "I2CBus.h"

extern "C" void app_main(void)
{
    
    std::array<uint8_t, 1> muxes{0x70};
    I2CBus<1> bus(GPIO_NUM_9, GPIO_NUM_8, I2C_NUM_0, muxes);

    std::string_view name{"otos"};
    uint8_t buffer[6]{};

    bus.addSensorClassToBus(name, 0, 0x17);
    bus.selectMuxChannel(name);
    bus.readRegister(name, 0x20, buffer, sizeof(buffer));
    bus.writeRegister(name, 0x10, 0);
    bus.writeArrayRegister(name, 0x10, buffer);

    for(;;){
        
    }
}