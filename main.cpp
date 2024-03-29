/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <events/mbed_events.h>
#include <mbed.h>
#include "ble/BLE.h"
#include "SecurityManager.h"
#include "LittleFileSystem.h"

#include "JoystickService.h"
#include "HatButton.h"

JoystickService *hidServicePtr;
uint8_t _hidReport[6] = {0};

events::EventQueue queue;

static const uint8_t DEVICE_NAME[] = "Gamepad";
static const uint8_t MIN_AXES_DELTA = 5;

HeapBlockDevice hbd(8192, 512);
LittleFileSystem fs("fs");
DigitalOut led(LED1);

AnalogIn a_x0(A5);
AnalogIn a_y0(A4);
AnalogIn a_x1(A3);
AnalogIn a_y1(A2);

bool hatButtonState[4] = {false};

enum HatDirection {
    DIR_IDLE = -1,
    DIR_UP,
    DIR_UP_RIGHT,
    DIR_RIGHT,
    DIR_DOWN_RIGHT,
    DIR_DOWN,
    DIR_DOWN_LEFT,
    DIR_LEFT,
    DIR_UP_LEFT,
};

void update_button() {
    if (hidServicePtr) {
        hidServicePtr->copyReport(_hidReport);
        hidServicePtr->sendCallback();
    }
}

int8_t hatDirection = DIR_IDLE;

class Button : public InterruptIn {
    public:
        Button(PinName pin, unsigned int btnNumber)
        : InterruptIn(pin, PullUp), _btnNumber(btnNumber), _btnOffset(_btnNumber % 8) {
            if (_btnNumber <= 7) {
                _reportIndex = 0;
            } else {
                _reportIndex = 1;
            }

            this->rise(queue.event(this, &Button::onRise));
            this->fall(queue.event(this, &Button::onFall));
        }

        virtual void onRise() {
            // TODO deibounce
            _hidReport[_reportIndex] &= ~(1 << _btnOffset);
            update_button();
        }

        virtual void onFall() {
            // TODO debounce
            _hidReport[_reportIndex] |= 1 << _btnOffset;
            update_button();
        }

    protected:
        unsigned int _btnNumber;
        unsigned int _btnOffset;
        unsigned int _reportIndex;
};

void update_hat_direction(HatButton::Direction dir, bool pressed) {

    hatButtonState[dir] = pressed;

    if (hatButtonState[HatButton::UP]) {
        if (hatButtonState[HatButton::RIGHT]) {
            hatDirection = DIR_UP_RIGHT;
        } else if (hatButtonState[HatButton::LEFT]) {
            hatDirection = DIR_UP_LEFT;
        } else {
            hatDirection = DIR_UP;
        }
    } else if (hatButtonState[HatButton::DOWN]) {
        if (hatButtonState[HatButton::RIGHT]) {
            hatDirection = DIR_DOWN_RIGHT;
        } else if (hatButtonState[HatButton::LEFT]) {
            hatDirection = DIR_DOWN_LEFT;
        } else {
            hatDirection = DIR_DOWN;
        }
    } else if (hatButtonState[HatButton::LEFT]) {
        hatDirection = DIR_LEFT;
    } else if (hatButtonState[HatButton::RIGHT]) {
        hatDirection = DIR_RIGHT;
    } else {
        hatDirection = DIR_IDLE;
    }

    // TODO debounce
    _hidReport[1] = (hatDirection & 0xF) << 4;
    queue.call(update_button);
}

Button btn0(P0_11, 0);
Button btn1(P0_12, 1);
Button btn2(P0_13, 2);
Button btn3(P0_14, 3);
Button btn4(P0_15, 4);
Button btn5(P0_16, 5);
Button btn6(P0_26, 6);
Button btn7(P0_27, 7);
HatButton hat0(P0_22, HatButton::UP, &update_hat_direction);
HatButton hat1(P0_23, HatButton::RIGHT, &update_hat_direction);
HatButton hat2(P0_24, HatButton::DOWN, &update_hat_direction);
HatButton hat3(P0_25, HatButton::LEFT, &update_hat_direction);


AnalogIn *axes[] = { &a_x0, &a_y0, &a_x1, &a_y1 };
uint8_t axes_initial[4];
uint8_t axes_previous[4];


const uint8_t AXIS_MAX = 255;

unsigned int map(unsigned int x, unsigned int in_min, unsigned int in_max, unsigned int out_min, unsigned int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

uint8_t read_initial_axis(unsigned int axis) {
    return axes[axis]->read() * AXIS_MAX;
}

uint8_t read_axis(unsigned int axis) {
    uint8_t val = axes[axis]->read() * AXIS_MAX;
    return 128 + (val - axes_initial[axis]);
    //return map(val, 50, 220, 0, AXIS_MAX);
}

int update_handle;

void read_analog_sticks() {
    int val;
    bool update = false;
    for (unsigned int i = 0; i < 4; i++) {
        val = read_axis(i);
        if (abs((int)axes_previous[i] - val) >= MIN_AXES_DELTA) {
            _hidReport[2 + i] = val;
            axes_previous[i] = val;
            update = true;
        }
    }

    if (update) {
        queue.call(update_button);
    }
}

void blink(void) {
    led = !led;
}

class SMDevice : private mbed::NonCopyable<SMDevice>,
                 public SecurityManager::EventHandler
{
public:
    /* event handler functions */

    /** Respond to a pairing request. This will be called by the stack
     * when a pairing request arrives and expects the application to
     * call acceptPairingRequest or cancelPairingRequest */
    virtual void pairingRequest(ble::connection_handle_t connectionHandle) {
        printf("Pairing requested. Authorising.\r\n");
        BLE::Instance().securityManager().acceptPairingRequest(connectionHandle);
    }

    /** Inform the application of a successful pairing. Terminate the demonstration. */
    virtual void pairingResult(
        ble::connection_handle_t connectionHandle,
        SecurityManager::SecurityCompletionStatus_t result
    ) {
        if (result == SecurityManager::SEC_STATUS_SUCCESS) {
            printf("Pairing successful\r\n");
        } else {
            printf("Pairing failed\r\n");
        }

        update_handle = queue.call_every(40, &read_analog_sticks);
    }

    /** Inform the application of change in encryption status. This will be
     * communicated through the serial port */
    virtual void linkEncryptionResult(
        ble::connection_handle_t connectionHandle,
        ble::link_encryption_t result
    ) {
        if (result == ble::link_encryption_t::ENCRYPTED) {
            printf("Link ENCRYPTED\r\n");
        } else if (result == ble::link_encryption_t::ENCRYPTED_WITH_MITM) {
            printf("Link ENCRYPTED_WITH_MITM\r\n");
        } else if (result == ble::link_encryption_t::NOT_ENCRYPTED) {
            printf("Link NOT_ENCRYPTED\r\n");
        }
    }
};

SMDevice securityManagerEventHandler;

/** Schedule processing of events from the BLE in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    queue.call(mbed::callback(&context->ble, &BLE::processEvents));
}

/** End demonstration unexpectedly. Called if timeout is reached during advertising,
 * scanning or connection initiation */
void on_timeout(const Gap::TimeoutSource_t source) {
    printf("Unexpected timeout - aborting \r\n");
    queue.break_dispatch();
}

/** This is called by Gap to notify the application we connected,
 *  in our case it immediately requests a change in link security */
void on_connect(const Gap::ConnectionCallbackParams_t *connection_event) {
    BLE& ble = BLE::Instance();
    ble_error_t error;

    /* Request a change in link security. This will be done
     * indirectly by asking the master of the connection to
     * change it. Depending on circumstances different actions
     * may be taken by the master which will trigger events
     * which the applications should deal with. */
    error = ble.securityManager().setLinkSecurity(
        connection_event->handle,
        SecurityManager::SECURITY_MODE_ENCRYPTION_NO_MITM
    );

    if (error) {
        printf("Error during SM::setLinkSecurity %d\r\n", error);
        return;
    }
};

/** This is called by Gap to notify the application we disconnected,
 *  in our case it ends the demonstration. */
void on_disconnect(const Gap::DisconnectionCallbackParams_t *event) {
    BLE& ble = BLE::Instance();
    ble_error_t error;
    printf("Disconnected - demonstration ended \r\n");
    queue.cancel(update_handle);
    error = ble.gap().startAdvertising();
    if (error) {
        printf("Error during Gap::startAdvertising.\r\n");
    } else {
        printf("Started advertising\r\n");
    }
};

void start() {
    /* Set up and start advertising */
    BLE& ble = BLE::Instance();
    ble_error_t error;
    GapAdvertisingData advertising_data;

    /* add advertising flags */
    advertising_data.addFlags(GapAdvertisingData::LE_GENERAL_DISCOVERABLE
                              | GapAdvertisingData::BREDR_NOT_SUPPORTED);

    static const uint16_t uuid16_list[] =  {GattService::UUID_HUMAN_INTERFACE_DEVICE_SERVICE};

    advertising_data.addData(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS,
        (uint8_t *)uuid16_list, sizeof(uuid16_list));


    /* add device name */
    advertising_data.addData(
        GapAdvertisingData::COMPLETE_LOCAL_NAME,
        DEVICE_NAME,
        sizeof(DEVICE_NAME)
    );

    error = ble.gap().setAdvertisingPayload(advertising_data);

    if (error) {
        printf("Error during Gap::setAdvertisingPayload\r\n");
        return;
    }

    error = ble.gap().setAppearance(GapAdvertisingData::JOYSTICK);

    /* advertise to everyone */
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    /* how many milliseconds between advertisements, lower interval
     * increases the chances of being seen at the cost of more power */
    ble.gap().setAdvertisingInterval(20);
    ble.gap().setAdvertisingTimeout(0);

    error = ble.gap().startAdvertising();

    if (error) {
        printf("Error during Gap::startAdvertising.\r\n");
        return;
    }

    /** This tells the stack to generate a pairingRequest event
     * which will require this application to respond before pairing
     * can proceed. Setting it to false will automatically accept
     * pairing. */
    ble.securityManager().setPairingRequestAuthorisation(true);
};


/** This is called when BLE interface is initialised and starts the demonstration */
void on_init_complete(BLE::InitializationCompleteCallbackContext *event) {
    BLE& ble = BLE::Instance();
    ble_error_t error;

    if (event->error) {
        printf("Error during the initialisation\r\n");
        return;
    }

    /* If the security manager is required this needs to be called before any
     * calls to the Security manager happen. */
    error = ble.securityManager().init(true, false, SecurityManager::IO_CAPS_NONE, NULL, true, "/fs/bt.db");

    if (error) {
        printf("Error during init %d\r\n", error);
        return;
    }

    /* Tell the security manager to use methods in this class to inform us
     * of any events. Class needs to implement SecurityManagerEventHandler. */
    ble.securityManager().setSecurityManagerEventHandler(&securityManagerEventHandler);

    /* print device address */
    Gap::AddressType_t addr_type;
    Gap::Address_t addr;
    ble.gap().getAddress(&addr_type, addr);
    printf("Device address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    /* when scanning we want to connect to a peer device so we need to
     * attach callbacks that are used by Gap to notify us of events */
    ble.gap().onConnection(&on_connect);
    ble.gap().onDisconnection(&on_disconnect);

    hidServicePtr = new JoystickService(ble);

    /* start test in 500 ms */
    queue.call_in(500, &start);
};

int main() {
    /* to show we're running we'll blink every 500ms */
    queue.call_every(500, &blink);

    for (unsigned int i = 0; i < 4; i++) {
        axes_initial[i] = read_initial_axis(i);
        _hidReport[2 + i] = axes_initial[i];
    }

    BLE& ble = BLE::Instance();

    // Mount and/or format the filesystem for storing persistent pairing info
    int err = fs.mount(&hbd);
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err) {
        // Reformat if we can't mount the filesystem
        // this should only happen on the first boot
        printf("No filesystem found, formatting... ");
        fflush(stdout);
        err = fs.reformat(&hbd);
        printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }

    // Start bluetooth and the gamepad service
    printf("\r\n PERIPHERAL \r\n\r\n");

    ble_error_t error;

    if (ble.hasInitialized()) {
        printf("Ble instance already initialised.\r\n");
        return -1;
    }

    /* this will inform us of all events so we can schedule their handling
     * using our event queue */
    FunctionPointerWithContext<BLE::OnEventsToProcessCallbackContext*> schedule_fp(schedule_ble_events);
    ble.onEventsToProcess(schedule_fp);

    /* handle timeouts, for example when connection attempts fail */
    FunctionPointerWithContext<Gap::TimeoutSource_t> timeout_fp(on_timeout);
    ble.gap().onTimeout(timeout_fp);

    error = ble.init(on_init_complete);

    if (error) {
        printf("Error returned by BLE::init.\r\n");
        return -1;
    }

    /* this will not return until shutdown */
    printf("Dispatching queue\r\n");
    queue.dispatch_forever();

    if (ble.hasInitialized()) {
        ble.shutdown();
    }

    return 0;
}

