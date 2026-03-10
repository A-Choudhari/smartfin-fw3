/**
 * @file sf_ble.cpp
 * @brief Particle-backed implementation of the platform-adaptable BLE wrapper.
 * @author Charlie Kushelevsky (ckushelevsky@ucsd.edu)
 * @date 3-9-2026
 */

#include "sf_ble_defs.hpp"
#include "sf_ble.hpp"

#include "cli/conio.hpp"
#include "product.hpp"

#if SF_PLATFORM == SF_PLATFORM_PARTICLE
#include "Particle.h"
#endif


#if SF_PLATFORM == SF_PLATFORM_PARTICLE

namespace
{
    /**
     * @brief Particle-specific implementation 
     */

    
    BleUuid g_serviceUuid(sf::bledefs::SERVICE_UUID);
    BleUuid g_telemetryUuid(sf::bledefs::TELEMETRY_CHAR_UUID);
    BleUuid g_controlUuid(sf::bledefs::CONTROL_CHAR_UUID);

    /**
     * @brief Particle BLE backend that wires characteristics and event hooks.
     */
    class ParticleBleBackend
    {
    public:
        /**
         * @brief Construct BLE characteristics and hook control handler.
         */
        ParticleBleBackend() :
            telemetryCharacteristic(
                "tele",
                BleCharacteristicProperty::NOTIFY,
                g_telemetryUuid,
                g_serviceUuid
            ),
            controlCharacteristic(
                "ctrl",
                BleCharacteristicProperty::WRITE_WO_RSP,
                g_controlUuid,
                g_serviceUuid,
                ParticleBleBackend::onControlReceivedStatic,
                this
            )
        {
        }

        BleCharacteristic telemetryCharacteristic;
        BleCharacteristic controlCharacteristic;

        /** @brief Access singleton backend instance. */
        static ParticleBleBackend& getInstance()
        {
            static ParticleBleBackend instance;
            return instance;
        }

        /**
         * @brief Particle connect callback shim.
         * @param peer Connected peer (unused).
         * @param context Pointer to backend instance.
         */
        static void onConnectedStatic(const BlePeerDevice& peer, void* context)
        {
            (void)peer;
            ParticleBleBackend* self = static_cast<ParticleBleBackend*>(context);
            if (self)
            {
                self->onConnected();
            }
        }

        /**
         * @brief Particle disconnect callback shim.
         * @param peer Disconnected peer (unused).
         * @param context Pointer to backend instance.
         */
        static void onDisconnectedStatic(const BlePeerDevice& peer, void* context)
        {
            (void)peer;
            ParticleBleBackend* self = static_cast<ParticleBleBackend*>(context);
            if (self)
            {
                self->onDisconnected();
            }
        }

        /**
         * @brief Particle write-without-response callback shim.
         * @param data Received payload.
         * @param len Number of bytes in payload.
         * @param peer Peer device (unused).
         * @param context Pointer to backend instance.
         */
        static void onControlReceivedStatic(const uint8_t* data,
                                            size_t len,
                                            const BlePeerDevice& peer,
                                            void* context)
        {
            (void)peer;
            ParticleBleBackend* self = static_cast<ParticleBleBackend*>(context);
            if (self)
            {
                self->onControlReceived(data, len);
            }
        }

        /**
         * @brief Invoke connection callback chain on connect.
         */
        void onConnected()
        {
            SFBLE& ble = SFBLE::getInstance();
            ble.setConnectionCallback(bleConnectionThunk, &ble);
            bleConnectionThunk(true, &ble);
        }

        /**
         * @brief Invoke connection callback chain on disconnect.
         */
        void onDisconnected()
        {
            SFBLE& ble = SFBLE::getInstance();
            ble.setConnectionCallback(bleConnectionThunk, &ble);
            bleConnectionThunk(false, &ble);
        }

        /**
         * @brief Forward control writes to registered handler.
         * @param data Control payload bytes.
         * @param len Payload length.
         */
        void onControlReceived(const uint8_t* data, size_t len)
        {
            SFBLE& ble = SFBLE::getInstance();
            bleControlThunk(data, len, &ble);
        }

        /**
         * @brief Bridge connection events to SFBLE instance.
         * @param isConnected True if connected, false otherwise.
         * @param context Pointer to SFBLE instance.
         */
        static void bleConnectionThunk(bool isConnected, void* context);
        /**
         * @brief Bridge control writes to SFBLE instance.
         * @param data Control payload.
         * @param len Payload length.
         * @param context Pointer to SFBLE instance.
         */
        static void bleControlThunk(const uint8_t* data, size_t len, 
                                                        void* context);
    };

    void ParticleBleBackend::bleConnectionThunk(bool isConnected, void* context)
    {
        SFBLE* ble = static_cast<SFBLE*>(context);
        if (!ble)
        {
            return;
        }

        ble->connected = isConnected;

        if (ble->connectionCallback)
        {
            ble->connectionCallback(isConnected, ble->connectionContext);
        }
    }

    void ParticleBleBackend::bleControlThunk(const uint8_t* data, size_t len, 
                                                                void* context)
    {
        SFBLE* ble = static_cast<SFBLE*>(context);
        if (!ble)
        {
            return;
        }

        if (ble->controlCallback)
        {
            ble->controlCallback(data, len, ble->controlContext);
        }
    }
}

/**
 * @brief Get singleton SFBLE instance.
 * @return Reference to SFBLE.
 */
SFBLE& SFBLE::getInstance(void)
{
    static SFBLE instance;
    return instance;
}

SFBLE::SFBLE() :
    initialized(false),
    connected(false),
    controlCallback(nullptr),
    controlContext(nullptr),
    connectionCallback(nullptr),
    connectionContext(nullptr)
{
}

/**
 * @brief Initialize BLE stack and register characteristics.
 * @return true on success, false otherwise.
 */
bool SFBLE::init(void)
{
    if (this->initialized)
    {
        return true;
    }

#if SF_PLATFORM == SF_PLATFORM_PARTICLE
    ParticleBleBackend& backend = ParticleBleBackend::getInstance();

    BLE.on();
    BLE.setDeviceName(sf::bledefs::DEVICE_NAME);
    BLE.onConnected(ParticleBleBackend::onConnectedStatic, &backend);
    BLE.onDisconnected(ParticleBleBackend::onDisconnectedStatic, &backend);

    // Register characteristics by referencing them before advertising.
    // Particle associates the characteristic with the given service UUID in the
    // BleCharacteristic constructor.
    (void)backend.telemetryCharacteristic;
    (void)backend.controlCharacteristic;

    this->initialized = true;
    return true;
#else
    return false;
#endif
}

/**
 * @brief Begin advertising the Smartfin BLE service.
 * @return true on success, false otherwise.
 */
bool SFBLE::startAdvertising(void)
{
    if (!this->initialized)
    {
        return false;
    }

#if SF_PLATFORM == SF_PLATFORM_PARTICLE
    BleAdvertisingData advData;
    advData.appendServiceUUID(BleUuid(sf::bledefs::SERVICE_UUID));
    BLE.advertise(&advData);
    return true;
#else
    return false;
#endif
}

/**
 * @brief Stop BLE advertising.
 * @return true on success, false otherwise.
 */
bool SFBLE::stopAdvertising(void)
{
    if (!this->initialized)
    {
        return false;
    }

#if SF_PLATFORM == SF_PLATFORM_PARTICLE
    BLE.stopAdvertising();
    return true;
#else
    return false;
#endif
}

/**
 * @brief Report connection status.
 * @return true when a central is connected, else false.
 */
bool SFBLE::isConnected(void) const
{
    return this->connected;
}

/**
 * @brief Notify telemetry payload to connected central.
 * @param pData Pointer to payload.
 * @param len Payload length in bytes.
 * @return true on success, false on failure.
 */
bool SFBLE::notifyTelemetry(const void* pData, size_t len)
{
    if (!this->initialized || !this->connected || !pData)
    {
        return false;
    }

    if (len == 0 || len > sf::bledefs::MAX_NOTIFY_LEN)
    {
        return false;
    }

#if SF_PLATFORM == SF_PLATFORM_PARTICLE
    ParticleBleBackend& backend = ParticleBleBackend::getInstance();
    return backend.telemetryCharacteristic.setValue(
        static_cast<const uint8_t*>(pData),
        len
    );
#else
    return false;
#endif
}

/**
 * @brief Register callback for incoming control data.
 * @param cb Callback function pointer.
 * @param context User context passed to callback.
 */
void SFBLE::setControlCallback(control_rx_callback_t cb, void* context)
{
    this->controlCallback = cb;
    this->controlContext = context;
}

/**
 * @brief Register callback for connection state changes.
 * @param cb Callback function pointer.
 * @param context User context passed to callback.
 */
void SFBLE::setConnectionCallback(connection_callback_t cb, void* context)
{
    this->connectionCallback = cb;
    this->connectionContext = context;
}
