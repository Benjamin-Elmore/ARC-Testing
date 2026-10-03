// BASE INCLUDES
#pragma once
#include <stdio.h>
#include <functional>
#include <unordered_map>
#include <string_view>
#include <list>

// RTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// OUTSIDE IMPORTS
#include "SensorUtils.h"
#include "LockGuard.h"
#include "Vec3.h"
#include "OpticalUtils.h"
#include "OpticalPreconfig.h"
#include "I2CBus.h"

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
#define SENSOR_USER_SLOTS  4  // Number of per-sensor int scratch values.
#endif
#ifndef TASK_STACK_SIZE
#define TASK_STACK_SIZE 2048
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

enum SensorPreconfig {
    NONE,
    OPTICAL,
    COLOR,
    ENCODER
};
template <size_t N>
class Sensor{
    public:
        // CONSTRUCTOR:
        // Establish the fields for the sensor to be accessed later
        // If the Sensor is utilized through the mux, then provide the channel that
        // the mux exists on.
        Sensor (
                char* name,                                                                     // NAME of the Sensor
                I2CBus<N>& sensorCommBus,                                                       // BUS CLASS for Sensor 
                uint8_t deviceAddrI2C,                                                          // I2C ADDRESS
                SensorPreconfig sensorType = SensorPreconfig::NONE,                             // SENSOR PRECONFIG
                uint8_t muxChannel = SensorUtils::UNUSED_MUX,                                   // MUX CHANNEL
                uint8_t priority = DEFAULT_PRIORITY,                                            // SENSOR TASK PRIORITY
                std::initializer_list <SensorUtils::FunctionCallback> appendedOnSetup = {},     // STARTUP FUNCTIONS
                std::initializer_list<SensorUtils::FunctionCallback> appendedOnLoop = {},       // LOOP FUNCTIONS
                std::initializer_list<SensorUtils::AppendedMethod> appendedMethods = {},        // APPENDED METHODS
                std::initializer_list<SensorUtils::AppendedVariable> appendedVariables = {},    // APPENDED VARIABLES
                SensorUtils::FunctionCallback SensorReadFunction = NULL                         // READ FUNCTION
            )
            : _name(name), _sensorCommBusI2C(sensorCommBus), _muxChannel(muxChannel), _priority(priority)
        {

            if (sensorType != SensorPreconfig::NONE){
                //TODO: Add preconfig function for reader
            }
            // SETUP AND LOOP SETUP
            // the FuncHead fields hold the head of the linked list
            if (appendedOnSetup.size() != 0){
                // Appended functions are in the init list

                for (const SensorUtils::FunctionCallback func : appendedOnSetup) {
                    this->_setupFuncs.push_back(func);
                }
            }
            if (appendedOnLoop.size() != 0){
                // Appended functions are in the init list

                for (const SensorUtils::FunctionCallback func : appendedOnLoop) {
                    this->_loopFuncs.push_back(func);
                }
            }
            
            // APPENDED METHODS SETUP
            for (const SensorUtils::AppendedMethod& method : appendedMethods) {
                this->appendMethod(method.methodName, method.method);
            }
            for (const SensorUtils::AppendedVariable& variable : appendedVariables) {
                this->appendVariable(variable.variableName, variable.variable);
            }
        }

        // DESTRUCTOR:
        // Default for the sensor
        ~Sensor() = default;
        
        void setup() {
            // Setup the sensor, and start the RTOS task
            // When a child class inherits this class, the super
            // of setup() must be called.

            // Add the sensor to the I2C bus with its address identifier
            this->_sensorCommBusI2C.addSensorClassToBus(
                this->getName(),
                this->_muxChannel,
                this->_devAddrI2C
            );

            // Execute the functions in the "on startup" list
            // Only completed if there are functions in the list
            if (!this->_setupFuncs.empty()){
                this->executeFunctionList(this->_setupFuncs);
            }

            // Create the RTOS Task and store it in taskObject
            xTaskCreatePinnedToCore(
                _taskEntry,             //Task Loop
                this->_name,            //Loop Name
                TASK_STACK_SIZE,        //Stack size
                this,                   //Ptr to task object
                this->_priority,        //Priority
                &(this->_taskObject),   //taskHandle_t in SensorParent
                this->_core             //Core
            );
        }

        void appendMethod(char* functionName, SensorUtils::FunctionCallback function) {
            // Append a method to the hash table

            if (this->_appendedMethods.size() >= SENSOR_MAX_METHODS) {
                this->_status = SensorUtils::SENSOR_ERR_FULL;
            }
            
            this->_appendedMethods.emplace(functionName, function);
        }

        void appendVariable(char* variableName, uint8_t variable) {
            // Append a variable to the hash table

            if (this->_appendedVariables.size() >= SENSOR_USER_SLOTS) {
                this->_status = SensorUtils::SENSOR_ERR_FULL;
            }
            
            this->_appendedVariables.emplace(variableName, variable);
        }

        void executeAppendedMethod(char* methodName) {
            this->_appendedMethods.at(methodName)();
        }

        uint8_t getAppendedVariable(char* variableName) {
            return this->_appendedVariables.at(variableName);
        }

        void appendSetupFunc(SensorUtils::FunctionCallback function) {
            this->_setupFuncs.push_back(function);
        }
        void appendLoopFunc(SensorUtils::FunctionCallback function) {
            this->_setupFuncs.push_back(function);
        }

        uint8_t getAddressI2C() { return this->_devAddrI2C; }

        std::string_view getName() { return this->_name; }

        uint8_t getMuxI2C() { return this->_muxChannel; }

    protected:
        // Meant to be interfaced with in child classes

        // TASK OBJECT
        TaskHandle_t _taskObject;

        // Constructor-defined variables
        char* _name;
        SemaphoreHandle_t& _mutexReference;

        SensorUtils::FunctionCallback readFunction = NULL;

        //OPTIONAL MEMBERS:
        uint8_t _muxChannel = SensorUtils::UNUSED_MUX;
        uint8_t _devAddrI2C = SensorUtils::ADDR_NOT_INCLUDED;

        // SETUP AND LOOP FUNCTIONS, stored in linked list
        // Pointers to the head of each function list:
        std::list<SensorUtils::FunctionCallback> _setupFuncs{};
        std::list<SensorUtils::FunctionCallback> _loopFuncs{};

        //APPENDED METHODS AND VARIABLES, stored in hash tables
        std::unordered_map<std::string_view, SensorUtils::FunctionCallback> _appendedMethods = {};
        std::unordered_map<std::string_view, uint8_t> _appendedVariables = {};

        void executeFunctionList(std::list<SensorUtils::FunctionCallback>& funcList) {
            // Execute all of the functions in the linked list

            for (const SensorUtils::FunctionCallback func : funcList){
                func();
            }
        }

        // TASK LOOP utilized within the actual RTOS task
        void taskLoop() {
            // Loop on RTOS TASK
            for (;;) {
                {
                    // Lock the bus mutex with a Lockguard, if it is not currently available
                    // we will run a delay on the mutex
                    LockGuard loopLock = LockGuard(this->_sensorCommBusI2C.getMutex());
                    if (!loopLock.isMutexLocked()) {
                        // Run a delay on the RTOS task, the mutex is not
                        // currently available
                        vTaskDelay(pdMS_TO_TICKS(LOCK_FAIL_DELAY));

                    } else if (this->readFunction != NULL) {
                        // Run the preconfigured read function
                        this->readFunction();
                    }
                }

                //Execute Loop Functions outside of mutex lock
                if (!this->_loopFuncs.empty()){
                    this->executeFunctionList(this->_loopFuncs);
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

    private:
        //STATUS FIELDS

        I2CBus<N>& _sensorCommBusI2C;

        uint8_t _core{1};
        uint8_t _priority{DEFAULT_PRIORITY};

        bool _exceededMaximums{false};
        SensorUtils::sensor_status_t _status {SensorUtils::SENSOR_ERR_NOT_READY};

        // WRAPPER ON TASK LOOP for RTOS Task
        static void _taskEntry(void* ptr) {
            reinterpret_cast<Sensor*>(ptr)->taskLoop();
        }
};
