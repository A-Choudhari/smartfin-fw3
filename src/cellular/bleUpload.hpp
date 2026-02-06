/**
 * @file bleUpload.hpp
 * @author Antigravity
 * @brief Standalone BLE Upload Task for testing Nordic UART Service workflow
 * @version 0.1
 * @date 2026-02-05
 * 
 * This follows the Particle Gen 3 BLE documentation for Peripheral UART.
 */

#ifndef __BLE_UPLOAD_HPP__
#define __BLE_UPLOAD_HPP__

#include "task.hpp"
#include "Particle.h"
#include "product.hpp"
#include "sys/led.hpp"

/**
 * @class BleUpload
 * @brief Handles the BLE data upload task for testing.
 * 
 * This class implements the Nordic UART Service (NUS) to allow
 * data streaming over BLE to a mobile app.
 */
class BleUpload : public Task
{
public:
    /**
     * @brief Initializes BLE advertising and services.
     */
    void init(void) override;

    /**
     * @brief Executes the BLE upload loop.
     * 
     * @return The next state (e.g., STATE_DEEP_SLEEP when finished).
     */
    STATES_e run(void) override;

    /**
     * @brief Stops advertising and cleans up.
     */
    void exit(void) override;

    LEDStatus status;

private:
    bool initSuccess;
    
    // UUIDs for Nordic UART Service
    static const BleUuid serviceUuid;
    static const BleUuid rxUuid;
    static const BleUuid txUuid;

    // Static buffers/callbacks if needed by the BLE API
    static void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
};

#endif // __BLE_UPLOAD_HPP__
