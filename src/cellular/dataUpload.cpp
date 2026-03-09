#include "dataUpload.hpp"

#include "cli/conio.hpp"
#include "cli/flog.hpp"
#include "cellular/sf_cloud.hpp"
#include "encoding/base85.h"
#include "encoding/base64.h"

#include "consts.hpp"
#include "system.hpp"
#include "product.hpp"
#include "sleepTask.hpp"

#include "Particle.h"

#if BLE_UPLOAD_ENABLED
// Nordic UART Service UUIDs
const BleUuid DataUpload::serviceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
// const BleUuid DataUpload::rxUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const BleUuid DataUpload::txUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

// BLE Characteristics
BleCharacteristic DataUpload::txCharacteristic("tx", BleCharacteristicProperty::NOTIFY, DataUpload::txUuid, DataUpload::serviceUuid);
// BleCharacteristic DataUpload::rxCharacteristic("rx", BleCharacteristicProperty::WRITE_WO_RSP, DataUpload::rxUuid, DataUpload::serviceUuid);


#endif

void DataUpload::init(void)
{
    status.setColor(SF_DUP_RGB_LED_COLOR);
    status.setPattern(SF_DUP_CONNECT_RGB_LED_PATTERN);
    status.setPeriod(SF_DUP_RGB_LED_PERIOD);
    status.setPriority(SF_DUP_RGB_LED_PRIORITY);
    status.setActive();
    SF_OSAL_printf("UPLOAD" __NL__);
    Serial.flush();

    this->initSuccess = 1;
    
#if BLE_UPLOAD_ENABLED
    BLE.addCharacteristic(txCharacteristic);
    // BLE.addCharacteristic(rxCharacteristic);
    // rxCharacteristic.onDataReceived(DataUpload::onDataReceived, this);
    
    BleAdvertisingData advData;
    advData.appendServiceUUID(serviceUuid);
    advData.appendLocalName("Smartfin");
    
    if (BLE.advertise(&advData) == 0) {
        SF_OSAL_printf("BLE start" __NL__);
    } else {
        SF_OSAL_printf("BLE fail" __NL__);
    }
    Serial.flush();
#endif
    
    // Cellular wait moved to after BLE start to allow immediate advertising
    SF_OSAL_printf("Cell c" __NL__);
    Serial.flush();
    
#if BLE_UPLOAD_ENABLED
    // Use a shorter timeout in init if BLE is enabled to allow quicker testing
    if (sf::cloud::wait_connect(30000))
    {
        SF_OSAL_printf("Cell F (BLE active)" __NL__);
    } else {
        SF_OSAL_printf("Cell OK" __NL__);
        Particle.syncTime();
    }
    // Stay success for BLE
    this->initSuccess = 1;
#else
    if (sf::cloud::wait_connect(SF_CELL_SIGNAL_TIMEOUT_MS))
    {
        this->initSuccess = 0;
        SF_OSAL_printf("Cell F" __NL__);
    } else {
        SF_OSAL_printf("Cell OK" __NL__);
        Particle.syncTime();
    }
#endif
    Serial.flush();
}

int DataUpload::preparePacket(uint8_t* buffer, char* ascii, char* name, int& encodedLen) {
    memset(buffer, 0, SF_PACKET_SIZE);
    int nBytes = pSystemDesc->pRecorder->getLastPacket(buffer, SF_PACKET_SIZE, name, DU_PUBLISH_ID_NAME_LEN);
    
    if (nBytes < 0) {
        if (nBytes == -2) FLOG_AddError(FLOG_UPL_OPEN_FAIL, 0);
        return nBytes; // Error codes map nicely
    }

    // Alignment
    if (nBytes % 4 != 0) nBytes += 4 - (nBytes % 4);
    encodedLen = nBytes;

    memset(ascii, 0, SF_RECORD_SIZE + 1);
    size_t nBytesToSend = SF_RECORD_SIZE + 1;
    int retval;

#if SF_UPLOAD_ENCODING == SF_UPLOAD_BASE64URL
    retval = urlsafe_b64_encode(buffer, nBytes, ascii, &nBytesToSend);
#else
    retval = b64_encode(buffer, nBytes, ascii, &nBytesToSend);
#endif

    if (retval) {
        SF_OSAL_printf("Enc err: %d" __NL__, retval);
        return -4; // Internal error code for encoding failure
    }
    return 0;
}

STATES_e DataUpload::can_upload(void)
{
    if (!pSystemDesc->pRecorder->hasData())
    {
        return pSystemDesc->flags->hasCharger ? STATE_CHARGE : STATE_DEEP_SLEEP;
    }

    if (!sf::cloud::is_connected())
    {
        if (sf::cloud::wait_connect(SF_CELL_SIGNAL_TIMEOUT_MS)) return STATE_DEEP_SLEEP;
    }

    if (pSystemDesc->pWaterSensor->getCurrentStatus()) return STATE_DEPLOYED;
    if (pSystemDesc->pBattery->getVCell() < SF_BATTERY_UPLOAD_VOLTAGE) return STATE_DEEP_SLEEP;

    return STATE_UPLOAD;
}

#if BLE_UPLOAD_ENABLED
STATES_e DataUpload::tryBleUpload(void)
{
    uint8_t bin[SF_PACKET_SIZE];
    char ascii[SF_RECORD_SIZE + 1];
    char name[DU_PUBLISH_ID_NAME_LEN + 1];
    int encodedLen;
    size_t uploaded = 0;
    
    SF_OSAL_printf("BLE..." __NL__);
    
    system_tick_t startTime = millis();
    while (!BLE.connected()) {
        Particle.process();
        if (millis() - startTime > BLE_UPLOAD_TIMEOUT_MS) {
            SF_OSAL_printf("BLE TO" __NL__);
            return STATE_UPLOAD;
        }
        if (pSystemDesc->pWaterSensor->getCurrentStatus()) return STATE_DEPLOYED;
        delay(10);
    }
    
    SF_OSAL_printf("BLE C" __NL__);
    status.setPattern(SF_DUP_RGB_LED_PATTERN);
    status.setPeriod(SF_DUP_RGB_LED_PERIOD / 2);

    while (BLE.connected() && pSystemDesc->pRecorder->hasData()) {
        int res = preparePacket(bin, ascii, name, encodedLen);
        if (res < 0) return (res == -2) ? STATE_DEEP_SLEEP : STATE_CLI;

        txCharacteristic.setValue((uint8_t *)ascii, strlen(ascii));
        SF_OSAL_printf("BLE: %s" __NL__, name);
        uploaded++;

        if (pSystemDesc->pRecorder->popLastPacket(encodedLen) < 0) {
            SF_OSAL_printf("Pop err" __NL__);
            break;
        }
        Particle.process();
        delay(50);
    }

    SF_OSAL_printf("BLE D: %u" __NL__, uploaded);
    FLOG_AddError(FLOG_UPL_COUNT, uploaded);
    return STATE_DEEP_SLEEP;
}
#endif

STATES_e DataUpload::run(void)
{
    uint8_t bin[SF_PACKET_SIZE];
    char ascii[SF_RECORD_SIZE + 1];
    char name[DU_PUBLISH_ID_NAME_LEN + 1];
    int encodedLen;
    size_t uploaded = 0;

    if (!this->initSuccess) {
        SF_OSAL_printf("Init F" __NL__);
        return STATE_DEEP_SLEEP;
    }

#if BLE_UPLOAD_ENABLED
    STATES_e bleResult = tryBleUpload();
    if (bleResult != STATE_UPLOAD) return bleResult;
    SF_OSAL_printf("Try cell" __NL__);
#endif

    status.setPattern(SF_DUP_RGB_LED_PATTERN);
    status.setPeriod(SF_DUP_RGB_LED_PERIOD / 2);
    status.setActive();

    while (can_upload() == STATE_UPLOAD)
    {
        int res = preparePacket(bin, ascii, name, encodedLen);
        if (res < 0) return (res == -2) ? STATE_DEEP_SLEEP : STATE_CLI;

        SF_OSAL_printf("Pub: %s" __NL__, name);
        int retval = sf::cloud::publish_blob(name, ascii);
        uploaded++;

        if (retval != sf::cloud::SUCCESS) {
            SF_OSAL_printf("Pub fail: %d" __NL__, retval);
            return (retval == sf::cloud::NOT_CONNECTED || retval == sf::cloud::PUBLISH_FAIL) ? STATE_DEEP_SLEEP : STATE_CLI;
        }

        if (pSystemDesc->pRecorder->popLastPacket(encodedLen) < 0) {
            SF_OSAL_printf("Pop err" __NL__);
            return STATE_CLI;
        }
        Particle.process();
    }
    FLOG_AddError(FLOG_UPL_COUNT, uploaded);
    return STATE_DEEP_SLEEP;
}

void DataUpload::exit(void)
{
#if BLE_UPLOAD_ENABLED
    BLE.stopAdvertising();
#endif
    sf::cloud::wait_disconnect(5000);
    status.setActive(false);
}

// In smartfin-fw2/src/dataUpload::DataUpload::exitState(void), we return based on the water sensor state.  If the system is in the water, we redeploy, otherwise we go to sleep.
/*
STATES_e DataUpload::exitState(void)
{
    return STATE_DEEP_SLEEP;
}
*/