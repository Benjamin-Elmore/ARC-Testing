#pragma once
#include <stdio.h>
#include <functional>

#include "SensorUtils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class Sensor{
    public:
        // CONSTRUCTOR:
        // Establish the fields for the sensor to be accessed later
        // If the Sensor is utilized through the mux, then provide the channel that
        // the mux exists on.
        Sensor (
                char* name,                                                                     // NAME of the Sensor
                SemaphoreHandle_t& mutexReference,                                              // MUTEX for the sensor
                uint8_t muxChannel = SensorUtils::UNUSED_MUX,                                   // MUX channel
                std::initializer_list <SensorUtils::FunctionCallback> appendedOnSetup = {},     // STARTUP FUNCTIONS
                std::initializer_list<SensorUtils::FunctionCallback> appendedOnLoop = {}        // LOOP FUNCTIONS
            )
            : _name(name), _mutexReference(mutexReference), _muxChannel(muxChannel)
        {

            // Function members, configured into a linked list
            // the FuncHead fields hold the head of the linked list
            if (appendedOnSetup.size() != 0){
                setupFuncHead = SensorUtils::linkFunctionCallback(appendedOnSetup);
            }
            if (appendedOnLoop.size() != 0){
                loopFuncHead = SensorUtils::linkFunctionCallback(appendedOnLoop);
            }
        }

        // DESTRUCTOR:
        // Default for the sensor
        ~Sensor() = default;

        void setup();

        void readRaw();

    protected:
        // Meant to be interfaced with in child classes

        // TASK OBJECT
        TaskHandle_t _taskObject;

        // Constructor-defined variables
        char* _name;
        SemaphoreHandle_t& _mutexReference;

        //OPTIONAL MEMBERS:
        uint8_t _muxChannel;

        // ORDERED LINKED LIST of functions that run after setup and loop
        // Pointers to the head of each function list:
        SensorUtils::FunctionNode* setupFuncHead = nullptr;
        SensorUtils::FunctionNode* loopFuncHead = nullptr;

        // TASK LOOP utilized within the actual RTOS task
        void taskLoop();
    private:
        // Wrapper on task loop for RTOS Task
        static void _taskEntry(void* ptr) {
            reinterpret_cast<Sensor*>(ptr)->taskLoop();
        }
};
