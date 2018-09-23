#include "HatButton.h"
#include "mbed.h"

HatButton::HatButton(PinName pin, Direction dir, void (*updateCb)(Direction, bool))
: InterruptIn(pin, PullUp), _dir(dir), _updateCb(updateCb) {
    this->rise(callback(this, &HatButton::onRise));
    this->fall(callback(this, &HatButton::onFall));
}

void HatButton::onRise() {
    this->_updateCb(_dir, false);
}

void HatButton::onFall() {
    this->_updateCb(_dir, true);
}
