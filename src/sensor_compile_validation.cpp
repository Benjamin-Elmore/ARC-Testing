#include "ARC_Sensor.h"

namespace {
void validationCallback() {}

// These functions are compiled, never executed. In particular, setup() starts
// a task that must not outlive its Sensor or I2CBus objects.
template<size_t N>
void validateSensorApi(I2CBus<N>& bus, uint8_t muxChannel) {
    char sensorName[] = "validation_sensor";
    char methodName[] = "validation_method";
    char variableName[] = "validation_variable";

    Sensor<N> sensor(
        sensorName,
        bus,
        0x17,
        SensorPreconfig::NONE,
        muxChannel,
        DEFAULT_PRIORITY,
        {validationCallback},
        {validationCallback},
        {SensorUtils::AppendedMethod{methodName, validationCallback}},
        {SensorUtils::AppendedVariable{variableName, 42}},
        validationCallback
    );

    sensor.appendSetupFunc(validationCallback);
    sensor.appendLoopFunc(validationCallback);
    sensor.executeAppendedMethod(methodName);
    (void)sensor.getAppendedVariable(variableName);
    sensor.setup();
}
} // namespace

// Referencing both specializations forces their constructors and setup/task
// paths to compile even though app_main does not call this function.
void validateSensorCompilation(I2CBus<0>& directBus, I2CBus<1>& muxBus) {
    validateSensorApi(directBus, SensorUtils::UNUSED_MUX);
    validateSensorApi(muxBus, 0);
}
