#include <stdio.h>
#include "ARC_Sensor.h"
#include "Mux.h"
#include "OpticalUtils.h"
#include "I2CBus.h"

extern "C" void app_main(void)
{
    Sensor mySensor("Optical", Mux::muxMutex);


    for(;;){
        
    }
}