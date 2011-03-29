#include "BasePlayer.h"

#include "Level.h"

#define SPRITESHEET_W 8
#define SPRITESHEET_H 6
#define FRAMERATE DECI_SECONDS

BasePlayer::BasePlayer(Level* newParent) : ControlUnit(newParent)
{
    canJump = false;
    flags.addFlag(ufMissionObjective);
    fallCounter.init(500,MILLI_SECONDS);
}

BasePlayer::~BasePlayer()
{
    //
}

///---public---

// Custom load implementation to ensure the unit is initialized properly after loading
bool BasePlayer::load(const PARAMETER_TYPE& params)
{
    bool result = BaseUnit::load(params);

    if (imageOverwrite[0] == 0)
    {
        imageOverwrite = "images/player/black_big.png";
    }
    SDL_Surface* surf = getSurface(imageOverwrite);
    AnimatedSprite* temp;
    loadFrames(surf,0,1,false,"stand");
    loadFrames(surf,1,7,true,"fallRight");
    loadFrames(surf,8,3,false,"turnRight");
    temp = loadFrames(surf,12,4,true,"pushRight");
    temp->setPlayMode(pmPulse);
    temp = loadFrames(surf,16,6,true,"runRight");
    temp->setPlayMode(pmPulse);
    loadFrames(surf,22,2,false,"jumpRight");
    loadFrames(surf,23,1,false,"flyRight");
    loadFrames(surf,25,7,true,"fallLeft");
    loadFrames(surf,32,3,false,"turnLeft");
    temp = loadFrames(surf,36,4,true,"pushLeft");
    temp->setPlayMode(pmPulse);
    temp = loadFrames(surf,40,6,true,"runLeft");
    temp->setPlayMode(pmPulse);
    loadFrames(surf,46,2,false,"jumpLeft");
    loadFrames(surf,47,1,false,"flyLeft");

    setSpriteState("stand");

    return result;
}

void BasePlayer::update()
{
    if (collisionInfo.correction.y < 0) // on the ground
    {
        if (currentState == "stand")
        {
            if (velocity.x < 0)
                setSpriteState("runLeft");
            else if (velocity.x > 0)
                setSpriteState("runRight");
        }
    }
    else // air
    {
        if (currentState == "stand")
        {
            if (fallCounter.hasFinished() || not fallCounter.isStarted())
            {
                if (direction > 0)
                    setSpriteState("fallRight");
                else
                    setSpriteState("fallLeft");
            }
            else if (not canJump)
            {
                if (direction > 0)
                    setSpriteState("flyRight");
                else
                    setSpriteState("flyLeft");
            }
        }
    }

    BaseUnit::update();

#ifdef _DEBUG
    parent->debugString += "P: " + StringUtility::vecToString(position) + "\n" +
                           "V: " + StringUtility::vecToString(velocity) + "\n" +
                           "A: " + StringUtility::vecToString(acceleration[0]) + " to " + StringUtility::vecToString(acceleration[1]) + "\n" +
                           "C: " + StringUtility::vecToString(collisionInfo.correction) + "\n" +
                           "S: " + currentState + " (" + StringUtility::intToString(currentSprite->getCurrentFrame()) + ")\n";
#endif

    if ((int)velocity.y != 0)
        canJump = false;
}

void BasePlayer::render(SDL_Surface* screen)
{
    BaseUnit::render(screen);

    if (currentSprite->getLooping() || currentSprite->hasFinished())
    {
        setSpriteState("stand");
    }
}

void BasePlayer::hitMap(const Vector2df& correctionOverride)
{
    BaseUnit::hitMap(correctionOverride);

    if (correctionOverride.y < 0)
        canJump = true;
}


void BasePlayer::control(SimpleJoy* input)
{
    if (input->isLeft())
    {
        if (direction > 0 || (int)velocity.x == 0)
        {
            setSpriteState("turnLeft",true);
            states["runLeft"]->setCurrentFrame(0);
        }
        acceleration[0].x = -2;
        acceleration[1].x = -8;
    }
    else if (input->isRight())
    {
        if (direction < 0 || (int)velocity.x == 0)
        {
            setSpriteState("turnRight",true);
            states["runRight"]->setCurrentFrame(0);
        }
        acceleration[0].x = 2;
        acceleration[1].x = 8;
    }
    else
    {
        if (canJump)
            setSpriteState("stand");
        acceleration[0].x = 0;
        acceleration[1].x = 0;
        velocity.x = 0;
    }/*
    if (input->isUp())
    {
        acceleration[0].y = -2;
        acceleration[1].y = -16;
    }
    else if (input->isDown())
    {
        acceleration[0].y = 2;
        acceleration[1].y = 16;
    }
    else
    {
        acceleration[0].y = 0;
        acceleration[1].y = 0;
        velocity.y = 0;
    }*/
    if (input->isA() && canJump)
    {
        velocity.y = -14;
        canJump = false;
        if (direction > 0)
            setSpriteState("jumpRight",true);
        else
            setSpriteState("jumpLeft",true);
        fallCounter.start();
        input->resetA();
    }
}

///---private---

AnimatedSprite* BasePlayer::loadFrames(SDL_Surface* const surf, CRint skip, CRint num, CRbool loop, CRstring state)
{
    AnimatedSprite* temp = new AnimatedSprite;
    temp->loadFrames(surf,SPRITESHEET_W,SPRITESHEET_H,skip,num);
    temp->setTransparentColour(MAGENTA);
    temp->setFrameRate(FRAMERATE);
    temp->setLooping(loop);
    states[state] = temp;
    return temp;
}
