#include "BaseUnit.h"

#include "StringUtility.h"
#include "Random.h"

#include "Level.h"
#include "GreySurfaceCache.h"
#include "MusicCache.h"

map<string,int> BaseUnit::stringToFlag;
map<string,int> BaseUnit::stringToProp;
map<string,int> BaseUnit::stringToOrder;

BaseUnit::BaseUnit(Level* newParent)
{
    // set-up conversion maps
    stringToFlag["nomapcollision"] = ufNoMapCollision;
    stringToFlag["nounitcollision"] = ufNoUnitCollision;
    stringToFlag["nogravity"] = ufNoGravity;
    stringToFlag["invincible"] = ufInvincible;
    stringToFlag["missionobjective"] = ufMissionObjective;
    stringToFlag["noupdate"] = ufNoUpdate;

    stringToProp["class"] = upClass;
    stringToProp["state"] = upState;
    stringToProp["position"] = upPosition;
    stringToProp["velocity"] = upVelocity;
    stringToProp["flags"] = upFlags;
    stringToProp["collision"] = upCollision;
    stringToProp["imageoverwrite"] = upImageOverwrite;
    stringToProp["colour"] = upColour;
    stringToProp["health"] = upHealth;
    stringToProp["id"] = upID;
    stringToProp["order"] = upOrder;

    stringToOrder["idle"] = okIdle;
    stringToOrder["position"] = okPosition;
    stringToOrder["repeat"] = okRepeat;
    stringToOrder["colour"] = okColour;

    currentSprite = NULL;
    position = Vector2df(0.0f,0.0f);
    startingPosition = Vector2df(0.0f,0.0f);
    velocity = Vector2df(0.0f,0.0f);
    startingVelocity = Vector2df(0.0f,0.0f);
    gravity = Vector2df(0.0f,0.0f);
    acceleration[0] = Vector2df(0.0f,0.0f);
    acceleration[1] = Vector2df(0.0f,0.0f);
    toBeRemoved = false;
    parent = newParent;
    tag = "";
    id = "";
    imageOverwrite = "";
    col = WHITE;
    startingColour = WHITE;
    currentState = "";
    startingState = "";
    direction = 0;
    isPlayer = false;
    orderRunning = false;

    currentOrder = 0;
}

BaseUnit::BaseUnit(const BaseUnit& source)
{
    //
}

BaseUnit::~BaseUnit()
{
    collisionInfo.clear();
    collisionColours.clear();
    stringToFlag.clear();
    stringToProp.clear();
    for (map<string,AnimatedSprite*>::iterator iter = states.begin(); iter != states.end(); ++iter)
    {
        delete iter->second;
    }
    states.clear();
    orderList.clear();
}

/// ---public---

bool BaseUnit::load(const list<PARAMETER_TYPE >& params)
{
    for (list<PARAMETER_TYPE >::const_iterator value = params.begin(); value != params.end(); ++value)
    {
        if (not processParameter(make_pair(value->first,value->second)))
        {
            string className = params.front().second;
            cout << "Warning: Unprocessed parameter \"" << value->first << "\" on unit with id \""
                 << id << "\" (" << className << ")" << endl;
        }
    }

    if (id[0] == 0) // set id to increasing number if not set manually
        id = StringUtility::intToString(++(parent->idCounter));

    if (orderList.size() > 0)
    {
        Order temp = {okIdle,"-1"};
        orderList.push_back(temp); // make unit stop after last order (unless a repeat order is specified)
    }

#ifdef _DEBUG
    collisionColours.insert(Colour(GREEN).getIntColour()); // collision indicator pixels else messing things up
#endif

    return true; // Currently returns always true as no critical error checking is done here
}

void BaseUnit::reset()
{
    position = startingPosition;
    velocity = startingVelocity;
    gravity = Vector2df(0,0);
    acceleration[0] = Vector2df(0,0);
    acceleration[1] = Vector2df(0,0);
    collisionInfo.clear();
    toBeRemoved = false;
    if (startingState[0] != 0)
        setSpriteState(startingState,true);
    if (orderList.size() > 0)
    {
        currentOrder = 0;
        processOrder(orderList.front());
    }
    col = startingColour;
}

void BaseUnit::resetTemporary()
{
    collisionInfo.clear();
}

SDL_Surface* BaseUnit::getSurface(CRstring filename, CRbool optimize) const
{
    return SURFACE_CACHE->loadSurface(filename,parent->chapterPath,optimize);
}

int BaseUnit::getHeight() const
{
    if (currentSprite)
        return currentSprite->getHeight();
    else
        return -1;
}

int BaseUnit::getWidth() const
{
    if (currentSprite)
        return currentSprite->getWidth();
    else
        return -1;
}

Vector2di BaseUnit::getSize() const
{
    return Vector2di(getWidth(),getHeight());
}

Vector2df BaseUnit::getPixel(const SimpleDirection& dir) const
{
    switch (dir.value)
    {
    case diLEFT:
        return Vector2df(position.x,position.y + getHeight() / 2.0f);
    case diRIGHT:
        return Vector2df(position.x + getWidth() - 1,position.y + getHeight() / 2.0f);
    case diTOP:
        return Vector2df(position.x + getWidth() / 2.0f, position.y);
    case diBOTTOM:
        return Vector2df(position.x + getWidth() / 2.0f, position.y + getHeight() - 1.0f);
    case diTOPLEFT:
        return position;
    case diTOPRIGHT:
        return Vector2df(position.x + getWidth() - 1.0f, position.y);
    case diBOTTOMLEFT:
        return Vector2df(position.x, position.y + getHeight() - 1.0f);
    case diBOTTOMRIGHT:
        return position + getSize() - Vector2df(1,1);
    default:
        return position + Vector2df(getWidth() / 2.0f,getHeight() / 2.0f);
    }
}

SDL_Rect BaseUnit::getRect() const
{
    SDL_Rect result = {position.x,position.y,getWidth(),getHeight()};
    return result;
}

void BaseUnit::setStartingPosition(const Vector2df& pos)
{
    position = pos;
    startingPosition = pos;
}

void BaseUnit::setStartingVelocity(const Vector2df& vel)
{
    velocity = vel;
    startingVelocity = vel;
}

void BaseUnit::update()
{
    if (not flags.hasFlag(ufInvincible) && not collisionInfo.isHealthy(velocity))
    {
        explode();
        if (flags.hasFlag(ufMissionObjective))
        {
            parent->lose();
        }
    }
    else
    {
        move();
        if (velocity.x > 0.0f)
            direction = 1;
        else if (velocity.x < 0.0f)
            direction = -1;

        if (currentSprite)
            currentSprite->update();
        if (orderRunning && orderTimer > 0 && orderList.size() > 0)
        {
            updateOrder(orderList[currentOrder]);
            if (--orderTimer <= 0)
            {
                ++currentOrder;
                if (currentOrder < orderList.size())
                    processOrder(orderList[currentOrder]);
                else
                {
                    orderRunning = false;
                }
            }
        }
    }
}

void BaseUnit::updateScreenPosition(const Vector2di& offset)
{
    if (currentSprite)
        currentSprite->setPosition(position - offset);
}

void BaseUnit::render(SDL_Surface* surf)
{
    if (currentSprite)
        currentSprite->render(surf);

#ifdef _DEBUG
    for (vector<MapCollisionEntry>::const_iterator temp = collisionInfo.pixels.begin(); temp != collisionInfo.pixels.end(); ++temp)
    {
        GFX::setPixel(temp->pos - parent->drawOffset,GREEN);
    }
    GFX::renderPixelBuffer();
#endif
}

AnimatedSprite* BaseUnit::setSpriteState(CRstring newState, CRbool reset, CRstring fallbackState)
{
    map<string,AnimatedSprite*>::const_iterator state = states.find(newState);
    if (state != states.end()) // set to desired state
    {
        currentSprite = state->second;
        currentState = newState;
    }
    else if (fallbackState[0] != 0) // set to fallback or keep the current one
    {
        state = states.find(fallbackState);
        if (state != states.end())
        {
            currentSprite = state->second;
            currentState = fallbackState;
        }
        else
        {
            cout << "Unrecognized sprite state \"" << newState << "\" and fallback \"" <<
                 fallbackState << "\" on unit with id \"" << id << "\"" << endl;
        }
    }
    else
        cout << "Unrecognized sprite state \"" << newState << "\" on unit with id \"" << id << "\"" << endl;
    if (reset)
        currentSprite->rewind();
    return currentSprite;
}

void BaseUnit::hitMap(const Vector2df& correctionOverride)
{
    if (correctionOverride.y < 0.0f) // hitting the ground
    {
        position.y += velocity.y + correctionOverride.y;
        velocity.y = 0;
        velocity.x += correctionOverride.x;
    }
    else
    {
        velocity += correctionOverride;
    }
    position += collisionInfo.positionCorrection;
}

bool BaseUnit::checkCollisionColour(const Colour& col) const
{
    // NOTE: This effectively always adds the unit's colour to the collision list (done to speed check up)
    if (col == this->col)
        return true;
    set<int>::const_iterator iter = collisionColours.find(col.getIntColour());
    if (iter != collisionColours.end())
        return true;
    return false;
}

bool BaseUnit::hitUnitCheck(const BaseUnit* const caller) const
{
    if (not caller->checkCollisionColour(col) && not checkCollisionColour(caller->col))
        return false;

    return true;
}

void BaseUnit::hitUnit(const UnitCollisionEntry& entry)
{
    //
}

void BaseUnit::explode()
{
    if (currentSprite && parent)
    {
        // go through the image and create a particle for every visible pixel
        // only takes every fourth pixel for speed reasons
        Colour none = currentSprite->getTransparentColour();
        Colour pix = MAGENTA;
        Vector2df vel(0,0);
        int time = 0;
        for (int X = 0; X < currentSprite->getWidth(); X+=2)
        {
            for (int Y = 0; Y < currentSprite->getHeight(); Y+=2)
            {
                pix = currentSprite->getPixel(X,Y);
                if (pix != none)
                {
                    vel.x = Random::nextFloat(-5,5);
                    vel.y = Random::nextFloat(-8,-3);
                    time = Random::nextInt(750,1250);
                    parent->addParticle(this,pix,position + Vector2df(X,Y),vel,time);
                }
            }
        }
        MUSIC_CACHE->playSound("sounds/die.wav",parent->chapterPath);
    }
    toBeRemoved = true;
}

/// ---protected---

bool BaseUnit::processParameter(const PARAMETER_TYPE& value)
{
    bool parsed = true;

    switch (stringToProp[value.first])
    {
    case upClass:
    {
        tag = value.second;
        break;
    }
    case upState:
    {
        startingState = value.second;
        break;
    }
    case upPosition:
    {
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING);
        if (token.size() != 2)
        {
            parsed = false;
            break;
        }
        position.x = StringUtility::stringToFloat(token[0]);
        position.y = StringUtility::stringToFloat(token[1]);
        startingPosition = position;
        break;
    }
    case upVelocity:
    {
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING);
        if (token.size() != 2)
        {
            parsed = false;
            break;
        }
        velocity.x = StringUtility::stringToFloat(token[0]);
        velocity.y = StringUtility::stringToFloat(token[1]);
        startingVelocity = velocity;
        break;
    }
    case upFlags:
    {
        flags.clear();
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING);
        for (vector<string>::const_iterator flag = token.begin(); flag != token.end(); ++flag)
        {
            flags.addFlag(stringToFlag[*flag]);
        }
        break;
    }
    case upCollision:
    {
        collisionColours.clear();
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING);
        for (vector<string>::const_iterator col = token.begin(); col != token.end(); ++col)
        {
            int val = StringUtility::stringToInt(*col);
            if (val > 0 || (*col) == "0") // passed parameter is a numeric colour code
                collisionColours.insert(Colour(val).getIntColour());
            else // string colour code
                collisionColours.insert(Colour(*col).getIntColour());
        }
        break;
    }
    case upImageOverwrite:
    {
        imageOverwrite = value.second;
        break;
    }
    case upColour:
    {
        int val = StringUtility::stringToInt(value.second);
        if (val > 0 || value.second == "0") // passed parameter is a numeric colour code
            col = Colour(val);
        else // string colour code
            col = Colour(value.second);
        collisionColours.insert(col.getIntColour());
        startingColour = col;
        break;
    }
    case upHealth:
    {
        collisionInfo.squashThreshold = StringUtility::stringToInt(value.second);
        break;
    }
    case upID:
    {
        id = value.second;
        break;
    }
    case upOrder:
    {
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING,2);
        if (token.size() != 2)
        {
            if (stringToOrder[token.front()] != okRepeat)
            {
                cout << "Error: Bad order parameter \"" << value.second << "\" on unit id \"" << id << "\"" << endl;
            }
            else
            {
                Order temp = {okRepeat,""};
                orderList.push_back(temp);
            }
            break;
        }
        Order temp = {stringToOrder[token.front()],token.back()};
        orderList.push_back(temp);
        break;
    }
    default:
        parsed = false;
    }

    return parsed;
}

void BaseUnit::move()
{
    position += velocity + gravity;
}

bool BaseUnit::processOrder(Order& next)
{
    bool parsed = true;
    bool badOrder = false;

    vector<string> tokens;
    StringUtility::tokenize(next.value,tokens,DELIMIT_STRING);
    int ticks = 1;
    // This is kinda fucked up, because of the frame based movement
    if (tokens.size() > 0)
    {
        ticks = round(StringUtility::stringToFloat(tokens.front()) / 1000.0f * (float)FRAME_RATE);
    }

    switch (next.key)
    {
    case okIdle:
    {
        velocity = Vector2df(0,0);
        acceleration[0] = Vector2df(0,0);
        acceleration[1] = Vector2df(0,0);
        break;
    }
    case okPosition:
    {
        if (tokens.size() < 3)
        {
            badOrder = true;
            break;
        }
        Vector2df dest = Vector2df(StringUtility::stringToFloat(tokens[1]),StringUtility::stringToFloat(tokens[2]));
        velocity = (dest - position) / (float)ticks; // set speed to pixels per framerate
        break;
    }
    case okRepeat:
    {
        if (currentOrder != 0) // prevent possible endless loop
        {
            currentOrder = 0;
            processOrder(orderList.front());
        }
        return true;
    }
    case okColour:
    {
        if (tokens.size() < 2)
        {
            badOrder = true;
            break;
        }
        tempColour.x = col.red;
        tempColour.y = col.green;
        tempColour.z = col.blue;

        int val = StringUtility::stringToInt(tokens[1]);
        Colour temp;
        if (val > 0 || tokens[1] == "0") // passed parameter is a numeric colour code
            temp = Colour(val);
        else // string colour code
            temp = Colour(tokens[1]);

        tempColourChange.x = (float)(temp.red - col.red) / (float)ticks;
        tempColourChange.y = (float)(temp.green - col.green) / (float)ticks;
        tempColourChange.z = (float)(temp.blue - col.blue) / (float)ticks;
        break;
    }
    default:
        return false;
    }

    if (badOrder)
    {
        cout << "Error: Bad order parameter \"" << next.value << "\" on unit id \"" << id << "\"" << endl;
        orderList.erase(orderList.begin() + currentOrder);
        orderTimer = 1; // process next order in next cycle
        return false;
    }

    orderTimer = ticks;
    orderRunning = true;
    return parsed;
}

bool BaseUnit::updateOrder(const Order& curr)
{
    bool parsed = true;

    switch (curr.key)
    {
    case okColour:
        tempColour += tempColourChange;
        col.red = tempColour.x;
        col.green = tempColour.y;
        col.blue = tempColour.z;
        break;
    default:
        parsed = false;
    }

    return parsed;
}
