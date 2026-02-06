/**
 * @file bleUpload.cpp
 * @author Antigravity
 * @brief Standalone BLE Upload Task for testing Nordic UART Service workflow
 * @version 0.1
 * @date 2026-02-05
 */

#include "cellular/bleUpload.hpp"
#include "cellular/sf_cloud.hpp" // for error codes if needed
#include "cellular/encoding/base64.h"
#include "consts.hpp"
#include "system.hpp"
#include "product.hpp"
#include "cli/flog.hpp"
#include "cli/conio.hpp"

// Nordic UART Service UUIDs
const BleUuid BleUpload::serviceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
const BleUuid BleUpload::rxUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const BleUuid BleUpload::txUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

// Characteristics
static BleCharacteristic txCharacteristic("tx", BleCharacteristicProperty::NOTIFY, txUuid, serviceUuid);
static BleCharacteristic rxCharacteristic("rx", BleCharacteristicProperty::WRITE_WO_RSP, rxUuid, serviceUuid);

void BleUpload::onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context) {
    // Optional: Handle incoming commands from the phone
    SF_OSAL_printf("BLE Received %u bytes" __NL__, len);
}

void BleUpload::init(void) {
    this->initSuccess = false;

    // Set LED status
    status.setColor(SF_DUP_RGB_LED_COLOR);
    status.setPattern(SF_DUP_CONNECT_RGB_LED_PATTERN);
    status.setPeriod(SF_DUP_RGB_LED_PERIOD);
    status.setPriority(SF_DUP_RGB_LED_PRIORITY);
    status.setActive();

    // Add Services and Characteristics
    BLE.addCharacteristic(txCharacteristic);
    BLE.addCharacteristic(rxCharacteristic);

    rxCharacteristic.onDataReceived(BleUpload::onDataReceived, this);

    // Set Advertising Data
    BleAdvertisingData advData;
    advData.appendServiceUUID(serviceUuid);
    advData.appendLocalName("Smartfin-BLE");

    if (BLE.advertise(&advData) != 0) {
        SF_OSAL_printf("BLE Advertising failed!" __NL__);
        return;
    }

    SF_OSAL_printf("BLE Advertising started. Waiting for connection..." __NL__);
    this->initSuccess = true;
}

STATES_e BleUpload::run(void) {
    uint8_t binary_packet_buffer[SF_PACKET_SIZE];
    char ascii_record_buffer[SF_RECORD_SIZE + 1];
    char publishName[DU_PUBLISH_ID_NAME_LEN + 1];
    int nBytesToEncode;
    size_t nBytesToSend;
    int retval;

    if (!this->initSuccess) {
        FLOG_AddError(FLOG_SYS_STARTSTATE_JUSTIFICATION, 0x0501);
        return STATE_DEEP_SLEEP;
    }

    // Wait for connection
    while (!BLE.connected()) {
        Particle.process();
        delay(10);
        
        // Timeout or water check?
        if (pSystemDesc->pWaterSensor->getCurrentStatus()) {
            return STATE_DEPLOYED;
        }

        // Just to prevent infinite hang in some contexts
        if (Serial.peek() == 'q') {
             return STATE_CLI;
        }
    }

    SF_OSAL_printf("BLE Peer Connected!" __NL__);
    status.setPattern(SF_DUP_RGB_LED_PATTERN);
    status.setPeriod(SF_DUP_RGB_LED_PERIOD / 2);

    // Data Streaming Loop
    while (BLE.connected()) {
        if (!pSystemDesc->pRecorder->hasData()) {
            SF_OSAL_printf("All data uploaded via BLE." __NL__);
            return STATE_DEEP_SLEEP;
        }

        memset(binary_packet_buffer, 0, SF_PACKET_SIZE);
        nBytesToEncode = pSystemDesc->pRecorder->getLastPacket(binary_packet_buffer, SF_PACKET_SIZE, publishName, DU_PUBLISH_ID_NAME_LEN);
        
        if (nBytesToEncode < 0) {
            SF_OSAL_printf("Failed to retrieve data from recorder: %d" __NL__, nBytesToEncode);
            return STATE_CLI;
        }

        // Encode to Base64 (to maintain text-stream compatibility with existing server)
        memset(ascii_record_buffer, 0, SF_RECORD_SIZE + 1);
        nBytesToSend = SF_RECORD_SIZE + 1;
        if ((retval = urlsafe_b64_encode(binary_packet_buffer, nBytesToEncode, ascii_record_buffer, &nBytesToSend))) {
            SF_OSAL_printf("Failed to encode: %d" __NL__, retval);
            return STATE_CLI;
        }

        // Send via BLE Notify
        // Note: BLE notifications have a size limit (MTU). Particle Device OS handles splitting 
        // if using newer MTU, but for older we might need to chunk.
        // Assuming modern Device OS BLE handling.
        txCharacteristic.setValue((uint8_t*)ascii_record_buffer, nBytesToSend);

        SF_OSAL_printf("Sent %u bytes via BLE: %s" __NL__, nBytesToSend, publishName);

        // Acknowledge/Pop from recorder
        pSystemDesc->pRecorder->popLastPacket(nBytesToEncode);
        
        Particle.process();
        delay(50); // Small delay to avoid saturating BLE buffer
    }

    SF_OSAL_printf("BLE Disconnected." __NL__);
    return STATE_DEEP_SLEEP;
}

void BleUpload::exit(void) {
    BLE.stopAdvertising();
    // BLE.disconnectAll(); // Optional: force disconnect
    status.setActive(false);
    SF_OSAL_printf("Exiting BleUpload state" __NL__);
}
