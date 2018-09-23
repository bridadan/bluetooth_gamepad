#ifndef HAT_BUTTON_H
#define HAT_BUTTON_H

#include "mbed.h"

class HatButton : public InterruptIn {
    public:
        enum Direction {
            UP = 0,
            RIGHT,
            DOWN,
            LEFT
        };

        HatButton(PinName pin, Direction dir, void (*updateCb)(Direction, bool));
        virtual void onRise();
        virtual void onFall();

    protected:
        Direction _dir;
        void (*_updateCb)(Direction, bool);
};

#endif // HAT_BUTTON_H
