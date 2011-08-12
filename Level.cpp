#include "Level.h"

#include "StringUtility.h"

#include "BaseUnit.h"
#include "ControlUnit.h"
#include "PixelParticle.h"
#include "Physics.h"
#include "MyGame.h"
#include "userStates.h"
#include "SurfaceCache.h"
#include "effects/Hollywood.h"
#include "MusicCache.h"
#include "Dialogue.h"
#include "Savegame.h"

#ifdef _MEOW
#define NAME_TEXT_SIZE 24
#define NAME_RECT_HEIGHT 18
#else
#define NAME_TEXT_SIZE 48
#define NAME_RECT_HEIGHT 35
#endif

#define PAUSE_MENU_ITEM_COUNT 5
#define TIME_TRIAL_ITEM_COUNT 3
#ifdef _MEOW
#define PAUSE_MENU_SPACING 4
#define PAUSE_MENU_OFFSET_Y -20
#else
#define PAUSE_MENU_SPACING 10
#define PAUSE_MENU_OFFSET_Y 0
#endif
#define PAUSE_MENU_OFFSET_X 20
#ifdef _MEOW
#define PAUSE_VOLUME_SLIDER_SIZE 125
#else
#define PAUSE_VOLUME_SLIDER_SIZE 400
#endif

#ifdef _MEOW
#define TIME_TRIAL_OFFSET_Y 5
#define TIME_TRIAL_SPACING_Y 0
#define TIME_TRIAL_MENU_OFFSET_Y 30
#else
#define TIME_TRIAL_OFFSET_Y 10
#define TIME_TRIAL_SPACING_Y 10
#define TIME_TRIAL_MENU_OFFSET_Y 80
#endif

map<string,int> Level::stringToFlag;
map<string,int> Level::stringToProp;

Level::Level()
{
    // set-up static string to int conversion maps
    if (stringToFlag.empty())
    {
        stringToFlag["repeatx"] = lfRepeatX;
        stringToFlag["repeaty"] = lfRepeatY;
        stringToFlag["scrollx"] = lfScrollX;
        stringToFlag["scrolly"] = lfScrollY;
        stringToFlag["disableswap"] = lfDisableSwap;
        stringToFlag["keepcentred"] = lfKeepCentred;
        stringToFlag["scalex"] = lfScaleX;
        stringToFlag["scaley"] = lfScaleY;
        stringToFlag["splitx"] = lfSplitX;
        stringToFlag["splity"] = lfSplitY;
        stringToFlag["drawpattern"] = lfDrawPattern;
    }

    if (stringToProp.empty())
    {
        stringToProp["image"] = lpImage;
        stringToProp["flags"] = lpFlags;
        stringToProp["filename"] = lpFilename;
        stringToProp["offset"] = lpOffset;
        stringToProp["background"] = lpBackground;
        stringToProp["boundaries"] = lpBoundaries;
        stringToProp["name"] = lpName;
        stringToProp["music"] = lpMusic;
        stringToProp["dialogue"] = lpDialogue;
    }

    levelImage = NULL;
    collisionLayer = NULL;
    levelFileName = "";
    chapterPath = "";
    errorString = "";
    drawOffset = Vector2df(0,0);
    startingOffset = drawOffset;
    idCounter = 0;

    eventTimer.init(1000,MILLI_SECONDS);
    eventTimer.setRewind(STOP_AND_REWIND);
    winCounter = 0;
    firstLoad = true;
    trialEnd = false;

#ifdef _DEBUG
    debugText.loadFont("fonts/unispace.ttf",12);
    debugText.setColour(GREEN);
    debugString = "";
    fpsDisplay.loadFont("fonts/unispace.ttf",24);
    fpsDisplay.setColour(GREEN);
    fpsDisplay.setPosition(GFX::getXResolution(),0);
    fpsDisplay.setAlignment(RIGHT_JUSTIFIED);
#endif
    nameText.loadFont("fonts/Lato-Bold.ttf",NAME_TEXT_SIZE);
    nameText.setColour(WHITE);
    nameText.setAlignment(CENTRED);
    nameText.setUpBoundary(Vector2di(GFX::getXResolution(),GFX::getYResolution()));
    nameText.setPosition(0.0f,(GFX::getYResolution() - NAME_RECT_HEIGHT) / 2.0f + NAME_RECT_HEIGHT - NAME_TEXT_SIZE);
    nameRect.setDimensions(GFX::getXResolution(),NAME_RECT_HEIGHT);
    nameRect.setPosition(0.0f,(GFX::getYResolution() - NAME_RECT_HEIGHT) / 2.0f);
    nameRect.setColour(BLACK);
    nameTimer.init(2000,MILLI_SECONDS);

    if (ENGINE->timeTrial)
    {
        timeTrialText.loadFont("fonts/Lato-Bold.ttf",NAME_TEXT_SIZE * 1.5);
        timeTrialText.setColour(WHITE);
        timeTrialText.setAlignment(RIGHT_JUSTIFIED);
        timeTrialText.setUpBoundary(Vector2di(GFX::getXResolution()-PAUSE_MENU_OFFSET_X,GFX::getYResolution()-10));
        timeTrialText.setPosition(PAUSE_MENU_OFFSET_X,TIME_TRIAL_OFFSET_Y);
    }

    cam.parent = this;

    pauseSurf = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
    pauseSelection = 0;
    overlay.setDimensions(GFX::getXResolution(),GFX::getYResolution());
    overlay.setPosition(0,0);
    overlay.setColour(BLACK);
    overlay.setAlpha(100);

    hidex = NULL;
    hidey = NULL;
}

Level::~Level()
{
    SDL_FreeSurface(collisionLayer);
    for (vector<ControlUnit*>::iterator curr = players.begin(); curr != players.end(); ++curr)
    {
        delete (*curr);
    }
    players.clear();
    for (vector<ControlUnit*>::iterator curr = removedPlayers.begin(); curr != removedPlayers.end(); ++curr)
    {
        delete (*curr);
    }
    removedPlayers.clear();
    for (vector<BaseUnit*>::iterator curr = units.begin(); curr != units.end(); ++curr)
    {
        delete (*curr);
    }
    units.clear();
    for (vector<BaseUnit*>::iterator curr = removedUnits.begin(); curr != removedUnits.end(); ++curr)
    {
        delete (*curr);
    }
    removedUnits.clear();
    for (vector<PixelParticle*>::iterator curr = effects.begin(); curr != effects.end(); ++curr)
    {
        delete (*curr);
    }
    effects.clear();
    delete hidex;
    delete hidey;

    // reset background colour
    GFX::setClearColour(BLACK);
}

/// ---public---

bool Level::load(const list<PARAMETER_TYPE >& params)
{
    for (list<PARAMETER_TYPE >::const_iterator value = params.begin(); value != params.end(); ++value)
    {
        if (not processParameter(make_pair(value->first,value->second)) && value->first != "class")
        {
            string className = params.front().second;
            cout << "Warning: Unprocessed parameter \"" << value->first << "\" on level \"" << className << "\"" << endl;
        }
    }

    if (not levelImage)
    {
        errorString = "Error: No image has been specified! (critical)";
        return false;
    }
    else
    {
        collisionLayer = SDL_CreateRGBSurface(SDL_SWSURFACE,levelImage->w,levelImage->h,GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
    }
    if (getWidth() < GFX::getXResolution())
    {
        hidex = new Rectangle;
        hidex->setDimensions((GFX::getXResolution() - getWidth()) / 2.0f,GFX::getYResolution());
        hidex->setColour(GFX::getClearColour());
    }
    if (getHeight() < GFX::getYResolution())
    {
        hidey = new Rectangle;
        hidey->setDimensions(GFX::getXResolution() - max((int)GFX::getXResolution() - getWidth(),0),(GFX::getYResolution() - getHeight()) / 2.0f);
        hidey->setColour(GFX::getClearColour());
    }

    return true;
}

void Level::reset()
{
    for (vector<ControlUnit*>::iterator player = removedPlayers.begin(); player != removedPlayers.end();)
    {
        players.push_back(*player);
        player = removedPlayers.erase(player);
    }
    for (vector<BaseUnit*>::iterator unit = removedUnits.begin(); unit != removedUnits.end();)
    {
        units.push_back(*unit);
        unit = removedUnits.erase(unit);
    }

    for (vector<ControlUnit*>::iterator player = players.begin(); player != players.end(); ++player)
    {
        (*player)->reset();
    }
    for (vector<BaseUnit*>::iterator unit = units.begin(); unit != units.end(); ++unit)
    {
        (*unit)->reset();
    }
    for (vector<PixelParticle*>::iterator curr = effects.begin(); curr != effects.end(); ++curr)
    {
        delete (*curr);
    }
    effects.clear();

    firstLoad = false;
    ENGINE->restartCounter++;

    init();
}

void Level::init()
{
    winCounter = 0;
    for (vector<ControlUnit*>::const_iterator I = players.begin(); I != players.end(); ++I)
    {
        if ((*I)->flags.hasFlag(BaseUnit::ufMissionObjective))
            ++winCounter;
    }
    if (winCounter == 0)
        winCounter = 1; // prevent insta-win

    if (firstLoad)
    {
        nameTimer.start(2000);
        if (ENGINE->currentState != STATE_LEVELSELECT)
            EFFECTS->fadeIn(1000);
    }
    timeCounter = 0;
    newRecord = false;

    drawOffset = startingOffset;
    SDL_BlitSurface(levelImage,NULL,collisionLayer,NULL);
}

void Level::userInput()
{
    input->update();

#ifdef PLATFORM_PC
    if (input->isQuit())
    {
        nullifyState();
        return;
    }
#endif

    if (input->isStart())
    {
        pauseToggle();
        return;
    }
    if (input->isB() && input->isY())
    {
        for (vector<ControlUnit*>::iterator iter = players.begin(); iter != players.end(); ++iter)
        {
            (*iter)->explode();
        }
        lose();
        input->resetKeys();
    }


    if ((input->isL() || input->isR()) && not flags.hasFlag(lfDisableSwap))
    {
        if (players.size() > 1)
        {
            swapControl();
        }
        input->resetL();
        input->resetR();
    }

    for (vector<ControlUnit*>::iterator curr = players.begin(); curr != players.end(); ++curr)
    {
        if ((*curr)->takesControl)
            (*curr)->control(input);
    }
    input->resetA();
    input->resetX();
}

void Level::update()
{
#ifdef _DEBUG
    debugString = "";
#endif
    ++timeCounter;

    // Check for units to be removed, also reset temporary data
    for (vector<ControlUnit*>::iterator player = players.begin(); player != players.end();)
    {
        (*player)->resetTemporary();
        if ((*player)->toBeRemoved)
        {
            clearUnitFromCollision(collisionLayer,*player);
            removedPlayers.push_back(*player);
            player = players.erase(player);
        }
        else
        {
            ++player;
        }
    }
    for (vector<BaseUnit*>::iterator unit = units.begin();  unit != units.end();)
    {
        (*unit)->resetTemporary();
        if ((*unit)->toBeRemoved)
        {
            clearUnitFromCollision(collisionLayer,*unit);
            removedUnits.push_back(*unit);
            unit = units.erase(unit);
        }
        else
        {
            ++unit;
        }
    }
    for (vector<PixelParticle*>::iterator part = effects.begin();  part != effects.end();)
    {
        (*part)->resetTemporary();
        if ((*part)->toBeRemoved)
        {
            delete (*part);
            part = effects.erase(part);
        }
        else
        {
            ++part;
        }
    }

    // particle-map collision
    // and update (velocity, gravity, etc.)
    for (vector<PixelParticle*>::iterator curr = effects.begin(); curr != effects.end(); ++curr)
    {
        PHYSICS->applyPhysics((*curr));
        PHYSICS->particleMapCollision(this,collisionLayer,(*curr));
        (*curr)->update();
    }


    // physics (acceleration, friction, etc)
    for (vector<BaseUnit*>::iterator unit = units.begin();  unit != units.end(); ++unit)
    {
        adjustPosition((*unit));
        PHYSICS->applyPhysics((*unit));
    }
    // cache unit collision data for ALL units
    for (vector<ControlUnit*>::iterator player = players.begin(); player != players.end(); ++player)
    {
        clearUnitFromCollision(collisionLayer,*player);
        adjustPosition((*player));
        PHYSICS->applyPhysics((*player));
        for (vector<BaseUnit*>::iterator unit = units.begin();  unit != units.end(); ++unit)
        {
            PHYSICS->playerUnitCollision(this,(*player),(*unit));
        }
    }
    for (vector<BaseUnit*>::iterator unit = units.begin(); unit != units.end(); ++unit)
    {
        vector<BaseUnit*>::iterator next = unit;
        for (vector<BaseUnit*>::iterator unit2 = ++next; unit2 != units.end(); ++unit2)
            PHYSICS->playerUnitCollision(this,(*unit2),(*unit));

        for (vector<UnitCollisionEntry>::iterator item = (*unit)->collisionInfo.units.begin();
            item != (*unit)->collisionInfo.units.end(); ++item)
        {
            if (not item->unit->flags.hasFlag(BaseUnit::ufNoUnitCollision) && item->unit->hitUnitCheck(*unit))
                (*unit)->hitUnit(*item);
        }
    }

    // TODO: Clearing and redrawing units causes problems with the order
    // Make the map collision routine check every pixel also for a unit collision
    // instead and handle that (instead of a separate unit collision test)
    // also if a sinlge pixel only collides with the unit itself disregard that

    // map collision
    for (vector<BaseUnit*>::iterator curr = units.begin(); curr != units.end(); ++curr)
    {
        clearUnitFromCollision(collisionLayer,(*curr));
        // check for overwritten units by last clearUnitFromCollision call and redraw them
        for (vector<UnitCollisionEntry>::iterator item = (*curr)->collisionInfo.units.begin();
        item != (*curr)->collisionInfo.units.end(); ++item)
        {
            if (not item->unit->isPlayer)
                renderUnit(collisionLayer,item->unit,Vector2df(0,0));
        }

        if (not (*curr)->flags.hasFlag(BaseUnit::ufNoMapCollision))
        {
            PHYSICS->unitMapCollision(this,collisionLayer,(*curr));
        }

        // else still update unit on collision surface for player-map collision
        if (not (*curr)->flags.hasFlag(BaseUnit::ufNoUpdate))
            (*curr)->update();
        renderUnit(collisionLayer,(*curr),Vector2df(0,0));
    }

    // player-map collision
    for (vector<ControlUnit*>::iterator curr = players.begin(); curr != players.end(); ++curr)
    {
        // players should always have map collision enabled, so don't check for that here
        PHYSICS->unitMapCollision(this,collisionLayer,(*curr));
        (*curr)->update();
    }

    // other update stuff
    if (flags.hasFlag(lfKeepCentred))
        cam.centerOnUnit(getFirstActivePlayer());
    eventTimer.update();
    DIALOGUE->update();

    if (winCounter <= 0)
    {
        win();
    }

    EFFECTS->update();
}

void Level::render()
{
    //GFX::clearScreen();

    render(GFX::getVideoSurface());

    // if level is smaller hide outside area
    if (hidex) // left and right
    {
        if (flags.hasFlag(lfRepeatX) && flags.hasFlag(lfDrawPattern))
        {
            SDL_Rect src;
            src.w = ((int)GFX::getXResolution() - getWidth()) / 2.0f;
            src.h = min((int)GFX::getYResolution(), getHeight());
            src.x = getWidth() - src.w;
            src.y = max(drawOffset.y,0.0f);
            SDL_Rect dst;
            dst.x = 0;
            dst.y = ((int)GFX::getYResolution() - src.h) / 2.0f;
            SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
            src.x = 0;
            dst.x = (int)GFX::getXResolution() - src.w;
            SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
        }
        else
        {
            hidex->setPosition(0,0);
            hidex->render();
            hidex->setPosition(getWidth() - drawOffset.x,0.0f);
            hidex->render();
        }
    }
    if (hidey) // top and bottom
    {
        if (flags.hasFlag(lfRepeatY) && flags.hasFlag(lfDrawPattern))
        {
            SDL_Rect src;
            src.w = min((int)GFX::getXResolution(), getWidth());
            src.h = ((int)GFX::getYResolution() - getHeight()) / 2.0f;
            src.x = max(drawOffset.x,0.0f);
            src.y = getHeight() - src.h;
            SDL_Rect dst;
            dst.x = ((int)GFX::getXResolution() - src.w) / 2.0f;
            dst.y = 0;
            SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
            src.y = 0;
            dst.y = (int)GFX::getYResolution() - src.h;
            SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
        }
        else
        {
            hidey->setPosition(max(-drawOffset.x,0.0f),0.0f);
            hidey->render();
            hidey->setPosition(max(-drawOffset.x,0.0f),getHeight() - drawOffset.y);
            hidey->render();
        }
    }
    if (hidex && hidey && flags.hasFlag(lfRepeatX) && flags.hasFlag(lfRepeatY)
        && flags.hasFlag(lfDrawPattern)) // corners
    {
        SDL_Rect src;
        src.w = ((int)GFX::getXResolution() - getWidth()) / 2.0f;
        src.h = ((int)GFX::getYResolution() - getHeight()) / 2.0f;
        src.x = getWidth() - src.w;
        src.y = getHeight() - src.h;
        SDL_Rect dst;
        dst.x = 0;
        dst.y = 0;
        SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
        src.x = 0;
        dst.x = (int)GFX::getXResolution() - src.w;
        SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
        src.y = 0;
        dst.y = (int)GFX::getYResolution() - src.h;
        SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
        src.x = getWidth() - src.w;
        dst.x = 0;
        SDL_BlitSurface(collisionLayer,&src,GFX::getVideoSurface(),&dst);
    }

    // scaling (very unoptimized and slow!)
    // TODO: Implement properly
    if (flags.hasFlag(lfScaleX) && getWidth() != GFX::getXResolution() ||
            flags.hasFlag(lfScaleY) && getHeight() != GFX::getYResolution())
    {
        static float fx = (float)GFX::getXResolution() / (float)getWidth();
        static float fy = (float)GFX::getYResolution() / (float)getHeight();
        SDL_Surface* scaled;
        SDL_Rect scaleRect;
        if (not flags.hasFlag(lfScaleX))
        {
            scaled = zoomSurface(GFX::getVideoSurface(),fy,fy,SMOOTHING_OFF);
            scaleRect.x = (float)-drawOffset.x * fy;
            scaleRect.y = (float)-drawOffset.y * fy;
        }
        else if (not flags.hasFlag(lfScaleY))
        {
            scaled = zoomSurface(GFX::getVideoSurface(),fx,fx,SMOOTHING_OFF);
            scaleRect.x = (float)-drawOffset.x * fx;
            scaleRect.y = (float)-drawOffset.y * fx + (scaled->h - GFX::getYResolution());
        }
        else
        {
            scaled = zoomSurface(GFX::getVideoSurface(),fx,fy,SMOOTHING_OFF);
            scaleRect.x = (float)-drawOffset.x * fx;
            scaleRect.y = (float)-drawOffset.y * fy;
        }
        scaleRect.w = GFX::getXResolution();
        scaleRect.h = GFX::getYResolution();
        SDL_BlitSurface(scaled,&scaleRect,GFX::getVideoSurface(),NULL);
        SDL_FreeSurface(scaled);
    }

    // draw level name overlay
    if (firstLoad)
    {
        if (name[0] != 0 && not nameTimer.hasFinished())
        {
            nameRect.render();
            nameText.print(name);
        }
    }

    DIALOGUE->render();

    EFFECTS->render();

    // draw the cursor
    Vector2df pos = input->getMouse();
    GFX::setPixel(pos,RED);
    GFX::setPixel(pos+Vector2df(1,1),RED);
    GFX::setPixel(pos+Vector2df(-1,1),RED);
    GFX::setPixel(pos+Vector2df(1,-1),RED);
    GFX::setPixel(pos+Vector2df(-1,-1),RED);
    GFX::renderPixelBuffer();

#ifdef _DEBUG
    debugString += "Players alive: " + StringUtility::intToString(players.size()) + "\n";
    debugString += "Units alive: " + StringUtility::intToString(units.size()) + "\n";
    debugString += "Particles: " + StringUtility::intToString(effects.size()) + "\n";
    debugText.setPosition(10,10);
    debugText.print(debugString);
    fpsDisplay.print(StringUtility::intToString((int)MyGame::getMyGame()->getFPS()));
#endif
}

void Level::render(SDL_Surface* screen)
{
    // split screen on small screens and multiple players
    if (not hidex && not hidey && (flags.hasFlag(lfSplitX) || flags.hasFlag(lfSplitY)) && players.size() > 1 && not playersVisible())
    {
        // TODO: Proper algorithm here, not centring on one player, but "between" both and smooth
        // disconnection of screen parts (make X and Y split automatically maybe)

        // caching values
        int count = players.size();
        bool horizontally = flags.hasFlag(lfSplitX); // else vertically
        int size = 0;
        if (horizontally)
            size = GFX::getYResolution() / count;
        else
            size = GFX::getXResolution() / count;

        // players don't get drawn to the collision surface for collision testing
        for (vector<ControlUnit*>::iterator curr = players.begin(); curr != players.end(); ++curr)
        {
            renderUnit(collisionLayer,(*curr),Vector2df(0,0));
        }

        // sort players (not efficient, but we are talking low numbers here, usually =2)
        vector<ControlUnit*> temp;
        for (vector<ControlUnit*>::const_iterator curr = players.begin(); curr != players.end(); ++curr)
        {
            vector<ControlUnit*>::iterator ins;
            for (ins = temp.begin(); ins != temp.end(); ++ins)
            {
                if (horizontally)
                {
                    if ((*ins)->position.y > (*curr)->position.y)
                        break;
                }
                else
                {
                    if ((*ins)->position.x > (*curr)->position.x)
                        break;
                }
            }
            temp.insert(ins,*curr);
        }

        for (int I = 0; I < count; ++I)
        {
            SDL_Rect src;
            SDL_Rect dst;
            if (horizontally)
            {
                src.w = GFX::getXResolution();
                src.h = size;
                dst.x = 0;
                dst.y = size * I;
            }
            else
            {
                src.w = size;
                src.h = GFX::getYResolution();
                dst.x = size * I;
                dst.y = 0;
            }
            src.x = max(temp[I]->position.x - src.w / 2.0f,0.0f);
            src.y = max(temp[I]->position.y - src.h / 2.0f,0.0f);
            SDL_BlitSurface(collisionLayer,&src,screen,&dst);
        }
        temp.clear();
    }
    else
    {
        SDL_Rect src;
        SDL_Rect dst;
        dst.x = max(-drawOffset.x,0.0f);
        dst.y = max(-drawOffset.y,0.0f);
        src.x = max(drawOffset.x,0.0f);
        src.y = max(drawOffset.y,0.0f);
        src.w = min((int)GFX::getXResolution(),getWidth() - src.x);
        src.h = min((int)GFX::getYResolution(),getHeight() - src.y);

        // players don't get drawn to the collision surface for collision testing
        for (vector<ControlUnit*>::iterator curr = players.begin(); curr != players.end(); ++curr)
        {
            renderUnit(collisionLayer,(*curr),Vector2df(0,0));
        }

        SDL_BlitSurface(collisionLayer,&src,screen,&dst);
    }

    // particles
    for (vector<PixelParticle*>::iterator curr = effects.begin(); curr != effects.end(); ++curr)
    {
        (*curr)->updateScreenPosition(drawOffset);
        (*curr)->render(screen);
        GFX::renderPixelBuffer();
    }
}

void Level::onPause()
{
    SDL_BlitSurface(GFX::getVideoSurface(),NULL,pauseSurf,NULL);
    overlay.render(pauseSurf);
    if (trialEnd)
    {
        pauseSelection = 0;
        timeDisplay = 0;
        pauseItems.push_back("RESTART");
        pauseItems.push_back("NEXT");
        pauseItems.push_back("EXIT");
    }
    else
    {
        pauseSelection = 2;
        pauseItems.push_back("MUSIC VOL:");
        pauseItems.push_back("SOUND VOL:");
        pauseItems.push_back("RETURN");
        #ifdef _DEBUG
        pauseItems.push_back("RELOAD");
        #else
        pauseItems.push_back("RESTART");
        #endif
        pauseItems.push_back("EXIT");
    }
    nameText.setAlignment(LEFT_JUSTIFIED);
    input->resetKeys();
}

void Level::onResume()
{
    nameRect.setDimensions(GFX::getXResolution(),NAME_RECT_HEIGHT);
    nameRect.setPosition(0.0f,(GFX::getYResolution() - NAME_RECT_HEIGHT) / 2.0f);
    nameRect.setColour(BLACK);
    nameText.setAlignment(CENTRED);
    nameText.setPosition(0.0f,(GFX::getYResolution() - NAME_RECT_HEIGHT) / 2.0f + NAME_RECT_HEIGHT - NAME_TEXT_SIZE);
    nameText.setColour(WHITE);
    input->resetKeys();
    timeTrialText.setPosition(PAUSE_MENU_OFFSET_X,TIME_TRIAL_OFFSET_Y);
    timeTrialText.setAlignment(RIGHT_JUSTIFIED);
    timeTrialText.setColour(WHITE);
    pauseItems.clear();
}

void Level::pauseInput()
{
    input->update();

#ifdef PLATFORM_PC
    if (input->isQuit())
    {
        nullifyState();
        return;
    }
#endif

    if (trialEnd)
    {
        if (input->isUp() && pauseSelection > 0)
        {
            --pauseSelection;
            input->resetUp();
        }
        if (input->isDown() && pauseSelection < TIME_TRIAL_ITEM_COUNT-1)
        {
            ++pauseSelection;
            input->resetDown();
        }
        if (input->isA() || input->isX())
        {
            switch (pauseSelection)
            {
            case 0:
                reset();
                trialEnd = false;
                pauseToggle();
                break;
            case 1:
                eventTimer.setCallback(this,Level::winCallback);
                eventTimer.start(1000);
                EFFECTS->fadeOut(1000);
                pauseToggle();
                break;
            case 2:
                setNextState(STATE_MAIN);
                MUSIC_CACHE->playSound("sounds/menu_back.wav");
                break;
            default:
                break;
            }
        }
    }
    else
    {
        if (input->isUp() && pauseSelection > 0)
        {
            --pauseSelection;
            input->resetUp();
        }
        if (input->isDown() && pauseSelection < PAUSE_MENU_ITEM_COUNT-1)
        {
            ++pauseSelection;
            input->resetDown();
        }

        if (input->isLeft())
        {
            if (pauseSelection == 0)
            {
                int vol = MUSIC_CACHE->getMusicVolume();
                if (vol > 0)
                    MUSIC_CACHE->setMusicVolume(vol-8);
            }
            else if (pauseSelection == 1)
            {
                int vol = MUSIC_CACHE->getSoundVolume();
                if (vol > 0)
                    MUSIC_CACHE->setSoundVolume(vol-8);
            }
        }
        else if (input->isRight())
        {
            if (pauseSelection == 0)
            {
                int vol = MUSIC_CACHE->getMusicVolume();
                if (vol < MUSIC_CACHE->getMaxVolume())
                    MUSIC_CACHE->setMusicVolume(vol+8);
            }
            else if (pauseSelection == 1)
            {
                int vol = MUSIC_CACHE->getSoundVolume();
                if (vol < MUSIC_CACHE->getMaxVolume())
                    MUSIC_CACHE->setSoundVolume(vol+8);
            }
        }

        if (input->isA() || input->isX())
        {
            switch (pauseSelection)
            {
            case 2:
                pauseToggle();
                break;
            #ifdef _DEBUG
            case 3:
                setNextState(STATE_LEVEL);
                break;
            #else
            case 3:
                pauseToggle();
                for (vector<ControlUnit*>::iterator iter = players.begin(); iter != players.end(); ++iter)
                {
                    (*iter)->explode();
                }
                lose();
                break;
            #endif
            case 4:
                setNextState(STATE_MAIN);
                MUSIC_CACHE->playSound("sounds/menu_back.wav");
                break;
            default:
                break;
            }
        }

        if (input->isStart())
            pauseToggle();
    }
}

void Level::pauseUpdate()
{
    if (trialEnd)
    {
        if (timeDisplay < timeCounter)
            timeDisplay += min(timeCounter - timeDisplay, timeCounter / 30);
        else
        {
            if (newRecord)
                timeTrialText.setColour(ORANGE);
        }
    }
}

void Level::pauseScreen()
{
    SDL_BlitSurface(pauseSurf,NULL,GFX::getVideoSurface(),NULL);

    int offset = 0;
    if (trialEnd)
    {
        offset = TIME_TRIAL_MENU_OFFSET_Y;
    }
    else
    {
        offset = PAUSE_MENU_OFFSET_Y;
    }
    int pos = (GFX::getYResolution() - PAUSE_MENU_SPACING * (PAUSE_MENU_ITEM_COUNT-1)) / 2 + offset;

    // render text and selection
    for (int I = 0; I < pauseItems.size(); ++I)
    {
        nameRect.setPosition(0,pos);
        nameText.setPosition(PAUSE_MENU_OFFSET_X,pos + NAME_RECT_HEIGHT - NAME_TEXT_SIZE);
        if (I == pauseSelection)
        {
            nameRect.setColour(WHITE);
            nameText.setColour(BLACK);
        }
        else
        {
            nameRect.setColour(BLACK);
            nameText.setColour(WHITE);
        }
        nameRect.render();
        nameText.print(pauseItems.at(I));

        if (not trialEnd)
        {
            if (I == 0)
            {
                // render volume sliders
                float factor = (float)MUSIC_CACHE->getMusicVolume() / (float)MUSIC_CACHE->getMaxVolume();
                nameRect.setDimensions((float)PAUSE_VOLUME_SLIDER_SIZE * factor,NAME_RECT_HEIGHT);
                nameRect.setPosition((int)GFX::getXResolution() - PAUSE_VOLUME_SLIDER_SIZE - PAUSE_MENU_OFFSET_X,pos);
                if (pauseSelection == 0)
                    nameRect.setColour(BLACK);
                else
                    nameRect.setColour(WHITE);
                nameRect.render();
                nameRect.setDimensions(GFX::getXResolution(),NAME_RECT_HEIGHT);
            }
            else if (I == 1)
            {
                float factor = (float)MUSIC_CACHE->getSoundVolume() / (float)MUSIC_CACHE->getMaxVolume();
                nameRect.setDimensions(PAUSE_VOLUME_SLIDER_SIZE * factor,NAME_RECT_HEIGHT);
                nameRect.setPosition((int)GFX::getXResolution() - PAUSE_VOLUME_SLIDER_SIZE - PAUSE_MENU_OFFSET_X,pos);
                if (pauseSelection == 1)
                    nameRect.setColour(BLACK);
                else
                    nameRect.setColour(WHITE);
                nameRect.render();
                nameRect.setDimensions(GFX::getXResolution(),NAME_RECT_HEIGHT);
                pos += 20; // extra offset
            }
        }

        pos += NAME_RECT_HEIGHT + PAUSE_MENU_SPACING;
    }

    // in time trial mode also render text
    if (ENGINE->timeTrial)
    {
        timeTrialText.setAlignment(LEFT_JUSTIFIED);
        timeTrialText.setPosition(PAUSE_MENU_OFFSET_X,TIME_TRIAL_OFFSET_Y);
        timeTrialText.print("TIME: ");
        timeTrialText.setAlignment(RIGHT_JUSTIFIED);
        if (trialEnd)
            timeTrialText.print(ticksToTimeString(timeDisplay));
        else
            timeTrialText.print(ticksToTimeString(timeCounter));
        if (not trialEnd || timeDisplay == timeCounter)
        {
            timeTrialText.setAlignment(LEFT_JUSTIFIED);
            timeTrialText.setPosition(PAUSE_MENU_OFFSET_X,TIME_TRIAL_OFFSET_Y + TIME_TRIAL_SPACING_Y + NAME_TEXT_SIZE * 1.5);
            timeTrialText.print("BEST: ");
            timeTrialText.setAlignment(RIGHT_JUSTIFIED);
            timeTrialText.print(ticksToTimeString(SAVEGAME->getLevelStats(levelFileName).time));
        }
    }

#ifdef _DEBUG
    fpsDisplay.print(StringUtility::intToString((int)MyGame::getMyGame()->getFPS()));
#endif
}

int Level::getWidth() const
{
    if (levelImage)
        return levelImage->w;
    return -1;
}

int Level::getHeight() const
{
    if (levelImage)
        return levelImage->h;
    return -1;
}

Vector2df Level::transformCoordinate(const Vector2df& coord) const
{
    Vector2df result = coord;

    // cache some values
    int width = getWidth();
    int height = getHeight();

    // assuming the coordinate is not THAT much out of bounds (else should use modulo)
    if (flags.hasFlag(lfRepeatX))
    {
        if (result.x >= width)
            result.x -= width;
        else if (result.x < 0)
            result.x += width;
    }
    if (flags.hasFlag(lfRepeatY))
    {
        if (result.y >= height)
            result.y -= height;
        else if (result.y < 0)
            result.y += height;
    }
    return result;
}

Vector2df Level::boundsCheck(const BaseUnit* const unit) const
{
    Vector2df result = unit->position;

    // cache some values
    int width = getWidth();
    int height = getHeight();

    if (flags.hasFlag(lfRepeatX))
    {
        if (result.x < 0)
            result.x += width;
        else if (result.x + unit->getWidth() > width)
            result.x -= width;
    }
    if (flags.hasFlag(lfRepeatY))
    {
        if (result.y < 0)
            result.y += height;
        else if (result.y + unit->getHeight() > height)
            result.y -= height;
    }
    return result;
}

ControlUnit* Level::getFirstActivePlayer() const
{
    for (vector<ControlUnit*>::const_iterator unit = players.begin(); unit != players.end(); ++unit)
    {
        if ((*unit)->takesControl)
            return (*unit);
    }
    return NULL;
}

void Level::swapControl()
{
    bool valid = false;
    // check whether there is a player without control
    for (vector<ControlUnit*>::iterator unit = players.begin(); unit != players.end(); ++unit)
    {
        if (not (*unit)->takesControl)
        {
            valid = true;
            break;
        }
    }

    if (valid)
    {
        for (vector<ControlUnit*>::iterator unit = players.begin(); unit != players.end(); ++unit)
        {
            (*unit)->takesControl = not (*unit)->takesControl;
            if ((*unit)->takesControl)
                (*unit)->setSpriteState("wave",true);
        }
    }
}

void Level::lose()
{
    if (not eventTimer.isStarted())
    {
        eventTimer.setCallback(this,Level::loseCallback);
        eventTimer.start(750);
    }
}

void Level::win()
{
    if (not eventTimer.isStarted())
    {
        Savegame::LevelStats stats = {timeCounter};
        newRecord = SAVEGAME->setLevelStats(levelFileName,stats);
        if (ENGINE->timeTrial)
        {
            trialEnd = true;
            pauseToggle();
        }
        else
        {
            eventTimer.setCallback(this,Level::winCallback);
            eventTimer.start(1000);
            EFFECTS->fadeOut(1000);
        }
    }
}

void Level::addParticle(const BaseUnit* const caller, const Colour& col, const Vector2df& pos, const Vector2df& vel, CRint lifeTime)
{
    PixelParticle* temp = new PixelParticle(this,lifeTime);
    // copy the collision colours from the calling unit to mimic behaviour
    temp->collisionColours.insert(caller->collisionColours.begin(),caller->collisionColours.end());
    temp->position = pos;
    temp->velocity = vel;
    temp->col = col;
    effects.push_back(temp);
}

/// ---protected---

void Level::clearUnitFromCollision(SDL_Surface* const surface, BaseUnit* const unit)
{
    clearRectangle(surface,unit->position.x,unit->position.y,unit->getWidth(),unit->getHeight());

    Vector2df pos2 = boundsCheck(unit);
    if (pos2 != unit->position)
    {
        clearRectangle(surface,pos2.x,pos2.y,unit->getWidth(),unit->getHeight());
    }
}

void Level::clearRectangle(SDL_Surface* const surface, CRfloat posX, CRfloat posY, CRint width, CRint height)
{
    SDL_Rect unitRect;
    unitRect.x = max(posX,0.0f);
    unitRect.y = max(posY,0.0f);
    unitRect.w = min(surface->w - unitRect.x,width - max((int)-posX,0));
    unitRect.h = min(surface->h - unitRect.y,height - max((int)-posY,0));

    SDL_BlitSurface(levelImage,&unitRect,surface,&unitRect);
}

void Level::renderUnit(SDL_Surface* const surface, BaseUnit* const unit, const Vector2df& offset)
{
    unit->updateScreenPosition(offset);
    unit->render(surface);
    Vector2df pos2 = boundsCheck(unit);
    if (pos2 != unit->position)
    {
        Vector2df temp = unit->position;
        unit->position = pos2;
        unit->updateScreenPosition(offset);
        unit->render(surface);
        unit->position = temp;
    }
}

void Level::adjustPosition(BaseUnit* const unit)
{
    // 1 = out of right/bottom bounds, -1 = out of left/top bounds
    int boundsX = (unit->position.x > getWidth()) - (unit->position.x + unit->getWidth() < 0);
    int boundsY = (unit->position.y > getHeight()) - (unit->position.y + unit->getHeight() < 0);
    bool changed = false;

    if (boundsX != 0 && flags.hasFlag(lfRepeatX))
    {
        unit->position.x -= getWidth() * boundsX;
        changed = true;
    }
    if (boundsY != 0 && flags.hasFlag(lfRepeatY))
    {
        unit->position.y -= getHeight() * boundsY;
        changed = true;
    }
    if ((boundsX + boundsY != 0) && not changed)
    {
        unit->position = unit->startingPosition;
        changed = true;
    }
}

bool Level::processParameter(const PARAMETER_TYPE& value)
{
    bool parsed = true;

    switch (stringToProp[value.first])
    {
    case lpImage:
    {
        bool fromCache;
        levelImage = SURFACE_CACHE->getSurface(value.second,chapterPath,fromCache);
        if (levelImage)
        {
            if (levelImage->w < GFX::getXResolution())
                drawOffset.x = ((int)levelImage->w - (int)GFX::getXResolution()) / 2.0f;
            if (levelImage->h < GFX::getYResolution())
                drawOffset.y = ((int)levelImage->h - (int)GFX::getYResolution()) / 2.0f;
            startingOffset = drawOffset;
        }
        else
            parsed = false;
        break;
    }
    case lpFlags:
    {
        vector<string> props;
        StringUtility::tokenize(value.second,props,",");
        for (vector<string>::const_iterator str = props.begin(); str < props.end(); ++str)
            flags.addFlag(stringToFlag[*str]);
        break;
    }
    case lpFilename:
    {
        levelFileName = value.second;
        break;
    }
    case lpOffset:
    {
        vector<string> token;
        StringUtility::tokenize(value.second,token,DELIMIT_STRING);
        if (token.size() != 2)
        {
            parsed = false;
            break;
        }
        drawOffset.x = StringUtility::stringToFloat(token.at(0));
        drawOffset.y = StringUtility::stringToFloat(token.at(1));
        startingOffset = drawOffset;
        break;
    }
    case lpBackground:
    {
        int val = StringUtility::stringToInt(value.second);
        if (val > 0 || value.second == "0") // passed parameter is a numeric colour code
            GFX::setClearColour(Colour(val));
        else // string colour code
            GFX::setClearColour(Colour(value.second));
        break;
    }
    case lpBoundaries:
    {
        cam.disregardBoundaries = not StringUtility::stringToBool(value.second);
        break;
    }
    case lpName:
    {
        name = StringUtility::upper(value.second);
        break;
    }
    case lpMusic:
    {
        if (ENGINE->currentState != STATE_LEVELSELECT)
            MUSIC_CACHE->playMusic(value.second,chapterPath);
        break;
    }
    case lpDialogue:
    {
        if (ENGINE->currentState != STATE_LEVELSELECT)
            DIALOGUE->loadFromFile(chapterPath + value.second);
        break;
    }
    default:
        parsed = false;
    }
    return parsed;
}

string Level::ticksToTimeString(CRint ticks)
{
    if (ticks < 0)
        return "NONE";

    int time = (float)ticks / 30.0f * 100.0f; // convert to centi-seconds
    string cs = "00" + StringUtility::intToString(time % 100);
    string s = "00" + StringUtility::intToString((time / 100) % 60);
    string m = "";
    if (time / 6000 > 0)
    {
        m = StringUtility::intToString(time / 6000) + "'";
    }
    return (m + s.substr(s.length()-2,2) + "''" + cs.substr(cs.length()-2,2));
}

void Level::loseCallback(void* data)
{
    ((Level*)data)->eventTimer.setCallback(data,Level::lose2Callback);
    ((Level*)data)->eventTimer.start(125);
    EFFECTS->fadeOut(125,WHITE);
}

void Level::lose2Callback(void* data)
{
    ((Level*)data)->reset();
    EFFECTS->fadeIn(125,WHITE);
}

void Level::winCallback(void* data)
{
    Level* self = (Level*)data;
    self->setNextState(STATE_NEXT);
}

bool Level::playersVisible() const
{
    SDL_Rect screen = {drawOffset.x,drawOffset.y,GFX::getXResolution(),GFX::getYResolution()};
    for (vector<ControlUnit*>::const_iterator I = players.begin(); I != players.end(); ++I)
    {
        if (not (*I)->position.inRect(screen))
            return false;
    }
    return true;
}
