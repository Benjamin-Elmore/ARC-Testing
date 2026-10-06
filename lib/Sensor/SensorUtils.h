#pragma once
#include <functional>
#include <unordered_map>

namespace SensorUtils {
    constexpr uint8_t UNUSED_MUX = 0xFF;
    constexpr uint8_t ADDR_NOT_INCLUDED = 0xFF;
    constexpr uint8_t FAULTY_VARIABLE = 0xFF;

    //STRUCTURE OF A FUNCTION CALLBACK
    using FunctionCallback = void(*)();
    using EspErrorCallback = esp_err_t(*)();

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
        SENSOR_ERR_FUNC_CALL,     // Function was improperly called
        SENSOR_ERR_TASK,          // Failure with task operation
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
        case SENSOR_ERR_FUNC_CALL:  return "call_fail";
        case SENSOR_ERR_TASK:       return "task_fail";
    }
    return "?";  // Defensive fallback for an invalid/out-of-range enum value.
}

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
}