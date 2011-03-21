#ifndef STATETITLE_H
#define STATETITLE_H

#include "BaseState.h"
#include "Text.h"
#include "Sprite.h"
#include "Timer.h"

class StateTitle : public BaseState
{
    public:
        StateTitle();
        virtual ~StateTitle();

        virtual void render();
        virtual void update();

        virtual void userInput();

        Text text;
        Sprite pandora;
        Sprite penjin;
        Timer counter;
};

#endif