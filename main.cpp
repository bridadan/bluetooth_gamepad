
#include "mbed.h"

#include "ble/BLE.h"

#include "JoystickService.h"

#include "examples_common.h"

JoystickService *hidServicePtr;

static const char DEVICE_NAME[] = "uJoy";
static const char SHORT_DEVICE_NAME[] = "joystick0";

int hatVal = 0;
bool hatNeutral = false;
bool connected = false;
bool sendUpdate = false;

DigitalOut heartbeat_led(LED1);

uint8_t _hidReport[6];

void heartbeat_func() {
    heartbeat_led = !heartbeat_led;
    if (connected && hidServicePtr) {
        if (_hidReport[0]) {
            _hidReport[0] = 0;
        } else {
            _hidReport[0] = 1;
        }

        hidServicePtr->copyReport(_hidReport);
        hidServicePtr->sendCallback();
    }
}

void onDisconnect(const Gap::DisconnectionCallbackParams_t *params)
{
    connected = false;
    BLE::Instance().gap().startAdvertising(); // restart advertising
}

void onConnect(const Gap::ConnectionCallbackParams_t *params)
{
    connected = true;
}

void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext* context) {
    BLE &ble_device_inst = BLE::Instance();
    mbed_event_queue()->call(Callback<void()>(&ble_device_inst, &BLE::processEvents));
}

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble_device_inst   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        /* In case of error, forward the error handling to onBleInitError */
        printf("Received error!\n");
        return;
    }

    /* Ensure that it is the default instance of BLE */
    if(ble_device_inst.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        printf("mismatch!\n");
        return;
    }

    initializeSecurity(ble_device_inst);
    ble_device_inst.gap().onDisconnection(onDisconnect);
    ble_device_inst.gap().onConnection(onConnect);


    JoystickService joystickService(ble_device_inst);
    hidServicePtr = &joystickService;

    ble_device_inst.gap().accumulateAdvertisingPayload(GapAdvertisingData::JOYSTICK);

    ble_device_inst.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME,
                                           (const uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble_device_inst.gap().accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                           (const uint8_t *)SHORT_DEVICE_NAME, sizeof(SHORT_DEVICE_NAME));

    ble_device_inst.gap().setDeviceName((const uint8_t *)DEVICE_NAME);

    initializeHOGP(ble_device_inst);
}

int main()
{
    EventQueue *queue = mbed_event_queue();
    queue->call_every(2000, heartbeat_func);

    _hidReport[0] = 0;
    _hidReport[1] = 0;
    _hidReport[2] = 0;
    _hidReport[3] = 0;
    _hidReport[4] = 0;
    _hidReport[5] = 0;

    BLE &ble_device_inst = BLE::Instance();

    ble_device_inst.onEventsToProcess(scheduleBleEventsProcessing);
    ble_device_inst.init(&bleInitComplete);
    queue->dispatch_forever();
    return 0;
}
