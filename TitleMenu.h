#ifndef TITLEMENU_H
#define TITLEMENU_H

#include "BaseState.h"
#include "PenjinTypes.h"
#include "AnimatedSprite.h"

#ifdef _DEBUG
#include "Text.h"
#endif

class TitleMenu : public BaseState
{
    public:
        TitleMenu();
        virtual ~TitleMenu();

        virtual void init();
        virtual void userInput();
        virtual void update();
        virtual void render();
    protected:

    private:
        void setSelection(CRbool immediate);
        void incSelection();
        void decSelection();
        void doSelection();
        void inverse(SDL_Surface* const surf, const SDL_Rect& rect);

        AnimatedSprite bg;
        vector<SDL_Surface*> inverseBG;
        AnimatedSprite marker;
        int selection;
        SDL_Rect invertRegion;
        int currentFps;

        #ifdef _DEBUG
        Text fpsDisplay;
        #endif
};


#endif // TITLEMENU_H

