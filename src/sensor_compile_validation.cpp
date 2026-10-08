#include "ARC_Sensor.h"
#include <type_traits>
#include <utility>

// namespace {
// template <size_t N>
// void validationCallback(Sensor<N>& sensor) {
//     uint8_t data[6]{};
//     (void)sensor.readDeviceRegister(0x20, data, sizeof(data));
//     (void)sensor.writeDeviceRegister(0x10, uint8_t{0});
// }

// // These functions are compiled, never executed. In particular, setup() starts
// // a task that must not outlive its Sensor or I2CBus objects.
// template<size_t N>
// void validateSensorApi(I2CBus<N>& bus, uint8_t muxChannel) {
//     static_assert(!std::is_copy_constructible_v<Sensor<N>>);
//     static_assert(!std::is_copy_assignable_v<Sensor<N>>);
//     static_assert(std::is_same_v<
//         decltype(&validationCallback<N>), SensorUtils::FunctionCallback<N>>);
//     static_assert(std::is_same_v<
//         decltype(std::declval<Sensor<N>&>().readDeviceRegister(
//             uint8_t{}, static_cast<uint8_t*>(nullptr), size_t{})), esp_err_t>);
//     static_assert(std::is_same_v<
//         decltype(std::declval<Sensor<N>&>().writeDeviceRegister(
//             uint8_t{}, uint8_t{})), esp_err_t>);

//     char sensorName[] = "validation_sensor";
//     char methodName[] = "validation_method";
//     char variableName[] = "validation_variable";

//     Sensor<N> sensor(
//         sensorName,
//         bus,
//         0x17,
//         SensorPreconfig::NONE,
//         muxChannel,
//         DEFAULT_PRIORITY,
//         {validationCallback<N>},
//         {validationCallback<N>},
//         {SensorUtils::AppendedMethod<N>{methodName, validationCallback<N>}},
//         {SensorUtils::AppendedVariable{variableName, 42}},
//         validationCallback<N>
//     );

//     sensor.appendSetupFunc(validationCallback<N>);
//     sensor.appendLoopFunc(validationCallback<N>);
//     sensor.appendMethod(methodName, validationCallback<N>);
//     sensor.appendVariable(variableName, uint8_t{42});
//     sensor.executeAppendedMethod(methodName);
//     (void)sensor.getAppendedVariable(variableName);
//     sensor.setAppendedVariable(variableName, uint8_t{7});
//     (void)sensor.getAddressI2C();
//     (void)sensor.getName();
//     (void)sensor.getMuxI2C();
//     validationCallback(sensor);
//     sensor.setup();

//     // Also instantiate the constructor with its optional arguments omitted.
//     Sensor<N> defaultSensor(sensorName, bus, 0x17);
// }
// } // namespace

// // Referencing both specializations forces their constructors and setup/task
// // paths to compile even though app_main does not call this function.
// void validateSensorCompilation(I2CBus<0>& directBus, I2CBus<1>& muxBus) {
//     validateSensorApi(directBus, SensorUtils::UNUSED_MUX);
//     validateSensorApi(muxBus, 0);
// }
