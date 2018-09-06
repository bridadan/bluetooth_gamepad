/* mbed Microcontroller Library
 * Copyright (c) 2015 ARM Limited
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

#ifndef JOYSTICK_SERVICE_H
#define JOYSTICK_SERVICE_H

#include "mbed.h"

#include "HIDServiceBase.h"

// TODO integrate this into Gamepad

enum ButtonState
{
    BUTTON_UP,
    BUTTON_DOWN
};

enum JoystickButton
{
    JOYSTICK_BUTTON_1       = 0x1,
    JOYSTICK_BUTTON_2       = 0x2,
};

report_map_t JOYSTICK_REPORT_MAP = {
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,                    // USAGE (Game Pad)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0xa1, 0x01,                    //     COLLECTION (Application)
  0x05, 0x09,                    //     USAGE_PAGE (Button)
  0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
  0x29, 0x0c,                    //     USAGE_MAXIMUM (Button 12)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x95, 0x0c,                    //     REPORT_COUNT (12)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
  0x09, 0x39,                    //     USAGE (Hat switch)
  0x65, 0x14,                    //     UNIT (Eng Rot:Angular Pos)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x07,                    //     LOGICAL_MAXIMUM (7)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x46, 0x3b, 0x01,              //     PHYSICAL_MAXIMUM (315)
  0x75, 0x04,                    //     REPORT_SIZE (4)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x81, 0x42,                    //     INPUT (Data,Var,Abs,Null)
  0x65, 0x00,        //     Unit (None)
  0x09, 0x30,                    //     USAGE (X)
  0x09, 0x31,                    //     USAGE (Y)
  0x09, 0x32,                    //     USAGE (Z)
  0x09, 0x35,                    //     USAGE (Rz)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x95, 0x04,                    //     REPORT_COUNT (4)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0xc0,                          //         END_COLLECTION
  0xc0                           //     END_COLLECTION
};

static uint8_t report[] = { 0, 0, 0, 0, 0, 0};

class JoystickService: public HIDServiceBase
{
public:
    JoystickService(BLE &_ble) :
        HIDServiceBase(_ble,
                       JOYSTICK_REPORT_MAP, sizeof(JOYSTICK_REPORT_MAP),
                       inputReport          = report,
                       outputReport         = NULL,
                       featureReport        = NULL,
                       inputReportLength    = 6,
                       outputReportLength   = 0,
                       featureReportLength  = 0,
                       reportTickerDelay    = 20),
        failedReports (0)
    {
    }

    void copyReport(uint8_t *newReport) {
        for (int i = 0; i < 6; i++) {
            report[i] = newReport[i];
        }
    }

    virtual void sendCallback(void) {
        if (!connected)
            return;

        if (send(report))
            failedReports++;
    }

public:
    uint32_t failedReports;
};

#endif
