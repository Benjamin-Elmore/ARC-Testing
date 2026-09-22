#pragma once
#include <functional>
#include <unordered_map>

namespace SensorUtils {
    constexpr uint8_t UNUSED_MUX = 0xFF;

    //STRUCTURE OF A FUNCTION CALLBACK
    using FunctionCallback = void(*)();

    typedef enum {
        SENSOR_OK = 0,            // Operation completed successfully.
        SENSOR_ERR_NOT_READY,     // No usable reading/state exists yet.
        SENSOR_ERR_IO,            // Hardware or bus transaction failed.
        SENSOR_ERR_TIMEOUT,       // Operation exceeded its allowed time.
        SENSOR_ERR_RANGE,         // Reading fell outside configured bounds.
        SENSOR_ERR_NO_READER,     // update() has no hardware reader callback.
        SENSOR_ERR_DISABLED,      // Operation refused because sensor is disabled.
        SENSOR_ERR_FAULT,         // Sensor is latched in its fault state.
        SENSOR_ERR_ARG,           // Command argument was missing or invalid.
        SENSOR_ERR_NOT_FOUND,     // Requested named method does not exist.
        SENSOR_ERR_FULL,          // A fixed-capacity registry has no free slot.
    } sensor_status_t;

    static inline const char *sensor_status_str(sensor_status_t s) {
    switch (s) {
        case SENSOR_OK:             return "ok";
        case SENSOR_ERR_NOT_READY:  return "not_ready";
        case SENSOR_ERR_IO:         return "io";
        case SENSOR_ERR_TIMEOUT:    return "timeout";
        case SENSOR_ERR_RANGE:      return "range";
        case SENSOR_ERR_NO_READER:  return "no_reader";
        case SENSOR_ERR_DISABLED:   return "disabled";
        case SENSOR_ERR_FAULT:      return "fault";
        case SENSOR_ERR_ARG:        return "bad_arg";
        case SENSOR_ERR_NOT_FOUND:  return "not_found";
        case SENSOR_ERR_FULL:       return "full";
    }
    return "?";  // Defensive fallback for an invalid/out-of-range enum value.
}

    struct FunctionNode {
        // Node structure for function linked list

        FunctionCallback function;
        FunctionNode* next = nullptr;

        // CONSTRUCTOR: Intake the function, next pointer will be added in
        // subsequent declaration
        FunctionNode(SensorUtils::FunctionCallback func) : function(func){};
    };

    struct AppendedMethod {
        char* methodName;
        FunctionCallback method;

        AppendedMethod(char* name, FunctionCallback method) : methodName(name), method(method){};
    };

    struct AppendedVariable {
        char* variableName;
        uint8_t variable;

        AppendedVariable(char* name, uint8_t variable) : variableName(name), variable(variable){};
    };

    FunctionNode* linkFunctionCallback (std::initializer_list<FunctionCallback>& functionList) {
        // Create a linked-list for functions being used

        if (functionList.size() == 0) { return nullptr; }

        FunctionNode* head = nullptr;
        FunctionNode* tail;

        for (const FunctionCallback& function : functionList) {
            FunctionNode* newNode = new FunctionNode(function);
            if (head == nullptr){
                head = newNode;
                tail = newNode;
            } else {
                tail->next = newNode;
                tail = newNode;
            }
        }

        return head;
    }
}