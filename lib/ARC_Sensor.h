// BASE INCLUDES
#pragma once
#include <stdio.h>
#include <functional>
#include <unordered_map>
#include <string_view>

// RTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// OUTSIDE IMPORTS
#include "SensorUtils.h"
#include "LockGuard.h"

// MAXIMUM VALUES PER SENSOR
#ifndef SENSOR_MAX_FILTERS
#define SENSOR_MAX_FILTERS 4  // Maximum filter callbacks stored per sensor.
#endif
#ifndef SENSOR_MAX_METHODS
#define SENSOR_MAX_METHODS 6  // Maximum named command callbacks per sensor.
#endif
#ifndef SENSOR_MAX_ON_SETUP
#define SENSOR_MAX_ON_SETUP 4  // Maximum named command callbacks per sensor.
#endif
#ifndef SENSOR_MAX_ON_LOOP
#define SENSOR_MAX_ON_LOOP 4  // Maximum named command callbacks per sensor.
#endif
#ifndef SENSOR_USER_SLOTS
#define SENSOR_USER_SLOTS  4  // Number of per-sensor float scratch values.
#endif
#ifndef DEFAULT_PRIORITY
#define DEFAULT_PRIORITY 1
#endif
#ifndef LOCK_FAIL_DELAY
#define LOCK_FAIL_DELAY 50
#endif

enum FunctionReferences {
    ON_SETUP,
    ON_LOOP
};

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
                uint8_t priority = DEFAULT_PRIORITY,                                            // SENSOR TASK PRIORITY
                std::initializer_list <SensorUtils::FunctionCallback> appendedOnSetup = {},     // STARTUP FUNCTIONS
                std::initializer_list<SensorUtils::FunctionCallback> appendedOnLoop = {},       // LOOP FUNCTIONS
                std::initializer_list<SensorUtils::AppendedMethod> appendedMethods = {}         // APPENDED METHODS
            )
            : _name(name), _mutexReference(mutexReference), _muxChannel(muxChannel), _priority(priority)
        {

            // SETUP AND LOOP SETUP
            // the FuncHead fields hold the head of the linked list
            if (appendedOnSetup.size() != 0){
                _setupFuncHead = SensorUtils::linkFunctionCallback(appendedOnSetup);
            }
            if (appendedOnLoop.size() != 0){
                _loopFuncHead = SensorUtils::linkFunctionCallback(appendedOnLoop);
            }
            
            // APPENDED METHODS SETUP
            for (const SensorUtils::AppendedMethod& method : appendedMethods) {
                this->appendMethod(method);
            }
        }

        // DESTRUCTOR:
        // Default for the sensor
        ~Sensor() = default;
        
        void setup() {
            // Setup the sensor, and start the RTOS task
            // When a child class inherits this class, the super
            // of setup() must be called.

            // Execute the functions in the "on startup" list
            // Only completed if there are functions in the list
            if (this->_setupFuncHead != nullptr){
                this->executeFunctionList(ON_SETUP);
            }

            // Create the RTOS Task and store it in taskObject
            xTaskCreatePinnedToCore(
                _taskEntry,             //Task Loop
                this->_name,            //Loop Name
                4096,                   //Stack size
                this,                   //Ptr to task object
                1,                      //Priority
                &(this->_taskObject),   //taskHandle_t in SensorParent
                this->_core             //Core
            );
        }

        void appendMethod(SensorUtils::AppendedMethod function) {
            // Append a method to the hash table

            if (this->_appendedMethods.size() >= SENSOR_MAX_METHODS) {
                this->_status = SensorUtils::SENSOR_ERR_FULL;
            }
            
            this->_appendedMethods.emplace(function.methodName, function.method);
        }

    protected:
        // Meant to be interfaced with in child classes

        // TASK OBJECT
        TaskHandle_t _taskObject;

        // Constructor-defined variables
        char* _name;
        SemaphoreHandle_t& _mutexReference;

        //OPTIONAL MEMBERS:
        uint8_t _muxChannel;

        // SETUP AND LOOP FUNCTIONS, stored in linked list
        // Pointers to the head of each function list:
        SensorUtils::FunctionNode* _setupFuncHead = nullptr;
        SensorUtils::FunctionNode* _loopFuncHead = nullptr;

        //APPENDED METHODS, stored in hash table
        std::unordered_map<std::string_view, SensorUtils::FunctionCallback> _appendedMethods;

        void executeFunctionList(FunctionReferences targetList) {
            // Execute all of the functions in the linked list

            SensorUtils::FunctionNode* iterator = nullptr;

            switch(targetList){
                case ON_LOOP:
                    // Loop Functions after data pulling
                    iterator = this->_loopFuncHead;
                    break;

                case ON_SETUP:
                    //Setup functions before creating the RTOS task
                    iterator = this->_setupFuncHead;
                    break;

                default:
                    return;
            }

            // No functions located within the list
            if (iterator == nullptr) {return;}

            // Execute the listed functions
            while (iterator->next != nullptr) {
                iterator->function();
                iterator = iterator->next;
            }
        }

        // TASK LOOP utilized within the actual RTOS task
        void taskLoop() {
            // Loop on RTOS TASK

            {
                LockGuard loopLock = LockGuard(this->_mutexReference);
                if (!loopLock.isMutexLocked()) {
                    // Run a delay on the RTOS task, the mutex is not
                    // currently available
                    vTaskDelay(pdMS_TO_TICKS(LOCK_FAIL_DELAY));
                } else {
                    
                    //Execute Loop Functions
                    this->executeFunctionList(ON_LOOP);
                }
            }
        }

    private:
        //STATUS FIELDS

        uint8_t _core{1};
        uint8_t _priority{DEFAULT_PRIORITY};

        bool _exceededMaximums{false};
        SensorUtils::sensor_status_t _status {SensorUtils::SENSOR_ERR_NOT_READY};

        // WRAPPER ON TASK LOOP for RTOS Task
        static void _taskEntry(void* ptr) {
            reinterpret_cast<Sensor*>(ptr)->taskLoop();
        }
};
