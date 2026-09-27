#pragma once

//FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Mux.h"

class LockGuard {
    //Class structure that automatically locks the mutex that
    //is passed in by reference

    public:
        //CONSTRUCTOR: Take the mutex if it exists and if it able to be taken
        explicit LockGuard(SemaphoreHandle_t& mutex, uint8_t delayIfFail = 50): _mutexReference(mutex) {
            if (this->_mutexReference != nullptr && xSemaphoreTake(this->_mutexReference, portMAX_DELAY) != pdFALSE) {
                this->_isLocked = true;
            }
        }

        //DESTRUCTOR: unlocks the mutex
        //This will automatically be called when the LockGuard goes out of scope
        //of its declaration, freeing the mutex
        ~LockGuard() noexcept { xSemaphoreGive(this->_mutexReference); }

        //COPY PREVENTION
        //Prevent the LockGuard from being copied to ensure the mutex is secure
        //behind this class instance
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

        bool isMutexLocked() {
            return this->_isLocked;
        }
    private:
        SemaphoreHandle_t& _mutexReference;
        bool _isLocked = false;
};