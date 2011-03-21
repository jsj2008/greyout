#include "Physics.h"

#include <vector>

#include "SimpleDirection.h"
#include "Colour.h"
#include "NumberUtility.h"
#include "GFX.h"
#include "Vector2di.h"

#include "BaseUnit.h"
#include "Level.h"

// you can do funky x-gravity, but the collision checking would need some tinkering to make it work
// it currently checks the y-directions last for a reason...
#define DEFAULT_GRAVITY Vector2df(0,2)
#define DEFAULT_MAXIMUM Vector2df(24,24) // this should be the size of a tile if that's applicable

Physics* Physics::self = 0;

Physics::Physics()
{
    gravity = DEFAULT_GRAVITY;
    maximum = DEFAULT_MAXIMUM;
}

Physics::~Physics()
{
    //
}

Physics* Physics::GetSingleton()
{
    if (not self)
        self = new Physics();
    return self;
}

void Physics::applyPhysics(BaseUnit* const unit) const
{
    // Acceleration
    if (unit->acceleration[0].x > 0)
    {
        unit->velocity.x += min(max(unit->acceleration[1].x - unit->velocity.x,0.0f),unit->acceleration[0].x);
    }
    else if (unit->acceleration[0].x < 0)
    {
        unit->velocity.x += max(min(unit->acceleration[1].x - unit->velocity.x,0.0f),unit->acceleration[0].x);
    }
    if (unit->acceleration[0].y > 0)
    {
        unit->velocity.y += min(max(unit->acceleration[1].y - unit->velocity.y,0.0f),unit->acceleration[0].y);
    }
    else if (unit->acceleration[0].y < 0)
    {
        unit->velocity.y += max(min(unit->acceleration[1].y - unit->velocity.y,0.0f),unit->acceleration[0].y);
    }

    // Reset acceleration
    if (unit->velocity.x == unit->acceleration[1].x)
    {
        unit->acceleration[0] = Vector2df(0,0);
        unit->acceleration[1] = Vector2df(0,0);
    }
    if (unit->velocity.y == unit->acceleration[1].y)
    {
        unit->acceleration[0] = Vector2df(0,0);
        unit->acceleration[1] = Vector2df(0,0);
    }

    // Gravity
    if (not unit->flags.hasFlag(BaseUnit::ufNoGravity))
        unit->velocity += gravity;

    // Check for max
    unit->velocity.x = NumberUtility::signMin(unit->velocity.x,maximum.x);
    unit->velocity.y = NumberUtility::signMin(unit->velocity.y,maximum.y);
}

/** NOTICE:
New implementation, checking each direction of movement individually. This keeps
the correction of gravity induced movement separate from sideways movement
correction, which is desired in platformers. (assuming gravity in y-direction)
**/
void Physics::unitMapCollision(const Level* const parent, SDL_Surface* const level, BaseUnit* const unit, const Vector2df& mapOffset) const
{
    /// TODO: Implement step-size and check diBOTTOMLEFT and -RIGHT in x-direction, too
    /// compare to y-correction values and step-size

    CollisionObject collisionDir;
    Vector2df correction(0,0);
    Vector2di pixelCorrection(0,0); // unit will be moved by this step until no collision occurs
    vector<SimpleDirection> checkPoints; // the points being checked

    /// clear data of previous check
    unit->collisionInfo.clear();

    /// x-direction
    checkPoints.push_back(diTOPLEFT);
    checkPoints.push_back(diTOPRIGHT);
    checkPoints.push_back(diLEFT);
    checkPoints.push_back(diRIGHT);

    // check which pixels are colliding
    for (vector<SimpleDirection>::const_iterator dir = checkPoints.begin(); dir != checkPoints.end(); ++dir)
    {
        Vector2df pixel = unit->getPixel((*dir));
        pixel.x += unit->velocity.x;
        pixel = parent->transformCoordinate(pixel);

        // out of bounds check
        if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
            continue;

        Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);

        if (unit->checkCollisionColour(colColour))
        {
            // we have a collision
            CollisionEntry entry;
            entry.dir = (*dir);
            entry.pixel = pixel;
            entry.col = colColour;
            entry.correction = Vector2df(0,0);
            collisionDir.entries.push_back(entry);
            int temp = dir->xDirection();
            // only use direction if not set before or aiming in the same direction as velocity
            if (pixelCorrection.x == 0 || NumberUtility::sign(unit->velocity.x) == temp)
                pixelCorrection.x = temp * -1;
        }
    }

    // move unit until the collision is solved (maximum by unit's velocity as we
    // are assuming the unit was in a no-collision state before)
    bool stillColliding = (collisionDir.entries.size() > 0); // don't check when there are no colliding pixels
    int correctionX = 0; // total correction to solve collision in this direction
    while (stillColliding && abs(correctionX) <= maximum.x)
    {
        correctionX += pixelCorrection.x;

        vector<CollisionEntry>::const_iterator entryPtr;
        for (entryPtr = collisionDir.entries.begin(); entryPtr != collisionDir.entries.end(); ++entryPtr)
        {
            Vector2df pixel = entryPtr->pixel;
            pixel.x += correctionX;
            pixel = parent->transformCoordinate(pixel);
            if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
                continue;

            Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);
            if (unit->checkCollisionColour(colColour))
                break;
        }
        if (entryPtr == collisionDir.entries.end()) // all pixel have been checked, so no new collision has been found
            stillColliding = false;
    }
    if (abs(correctionX) > maximum.x) // correction is out of bounds -> disregard
        correctionX = 0;

    correction.x = correctionX;
    unit->collisionInfo.entries.insert(unit->collisionInfo.entries.end(),collisionDir.entries.begin(),collisionDir.entries.end());


    /// y-direction

    pixelCorrection = Vector2di(0,0);
    checkPoints.clear();
    collisionDir.clear();
    checkPoints.push_back(diTOP);
    checkPoints.push_back(diBOTTOMLEFT);
    checkPoints.push_back(diBOTTOM);
    checkPoints.push_back(diBOTTOMRIGHT);
    checkPoints.push_back(diTOPLEFT);
    checkPoints.push_back(diTOPRIGHT);

    for (vector<SimpleDirection>::const_iterator dir = checkPoints.begin(); dir != checkPoints.end(); ++dir)
    {
        Vector2df pixel = unit->getPixel((*dir));
        pixel += unit->velocity;
        pixel.x += correctionX;
        pixel = parent->transformCoordinate(pixel);

        if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
            continue;

        Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);

        if (unit->checkCollisionColour(colColour))
        {
            // we have a collision
            CollisionEntry entry;
            entry.dir = (*dir);
            entry.pixel = pixel;
            entry.col = colColour;
            entry.correction = Vector2df(0,0);
            collisionDir.entries.push_back(entry);
            int temp = dir->yDirection();
            // only use direction if not set before or aiming in the same direction as velocity
            if (pixelCorrection.y == 0 || NumberUtility::sign(unit->velocity.y) == temp)
                pixelCorrection.y = temp * -1;
        }
    }

    stillColliding = (collisionDir.entries.size() > 0); // don't check when there are no colliding pixels
    int correctionY = 0; // total correction to solve collision in this direction
    while (stillColliding && abs(correctionY) <= maximum.y)
    {
        correctionY += pixelCorrection.y;

        vector<CollisionEntry>::const_iterator entryPtr;
        for (entryPtr = collisionDir.entries.begin(); entryPtr != collisionDir.entries.end(); ++entryPtr)
        {
            Vector2df pixel = entryPtr->pixel;
            pixel.y += correctionY;
            pixel = parent->transformCoordinate(pixel);
            if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
                continue;

            Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);
            if (unit->checkCollisionColour(colColour))
                break;
        }
        if (entryPtr == collisionDir.entries.end()) // all pixel have been checked, so no new collision has been found
            stillColliding = false;
    }

    if (abs(correctionY) > maximum.y) // correction is out of bounds -> disregard
        correctionY = 0;

    correction.y = correctionY;
    unit->collisionInfo.entries.insert(unit->collisionInfo.entries.end(),collisionDir.entries.begin(),collisionDir.entries.end());

    unit->collisionInfo.correction = correction;

    unit->hitMap(correction);
}

/** NOTICE:
The following implementation checks both directions at once instead of one after
another. This, of course, is more accurate, probably a tad faster and has less
quirks and assumptions than the above implementation (and also leads to less copy-pasted
code). BUT on the other hand does not work that well for platformer-type games or
at least not in this, simple implementation.
There are some (different) quirks which I would have to work around and therefore
decided to scrap it. (for example, you "stick" to surfaces)
The code is preserved though and fully functionable, so just replace the above
function with the one below to see how this changes things.
Feel free to use for anything or ask me questions about it (as it is not as well
commented as the above function).
**/
/// TODO: Implement latest changes with pixel transformation
/*
void Physics::unitMapCollision(SDL_Surface* const level, BaseUnit* const unit, const Vector2df& mapOffset) const
{
    Vector2df correction(0,0);
    Vector2di pixelCorrection(0,0); // unit will be moved in this steps until no collision occurs

    /// clear data of previous check
    unit->collisionInfo.clear();

    /// Determine colliding pixels
    // diLEFT to diBOTTOMRIGHT
    for (int dir = 1; dir <= 8; ++dir)
    {
        // current collision pixel
        Vector2df pixel = unit->getPixel(dir) + unit->velocity;
        // pixelCorrection for this pixel (e.g. diTOPLEFT would be corrected to the bottom-right)
        Vector2df temp = SimpleDirection(dir).vectorDirection() * (-1.0f);

        if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
            continue;

        Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);

        if (unit->checkCollisionColour(colColour))
        {
            // we have a collision
            CollisionEntry entry;
            entry.dir = dir;
            entry.pixel = pixel;
            entry.col = colColour;
            entry.correction = Vector2df(0,0);
            unit->collisionInfo.entries.push_back(entry);
            // only use direction if not set before or aiming in the same direction as velocity
            if (temp.x != 0 && (pixelCorrection.x == 0 || NumberUtility::sign(unit->velocity.x)*-1 == temp.x))
                pixelCorrection.x = temp.x;
            if (temp.y != 0 && (pixelCorrection.y == 0 || NumberUtility::sign(unit->velocity.y)*-1 == temp.y))
                pixelCorrection.y = temp.y;
        }
    }

    /// Calculate correction by moving unit back pixel per pixel
    Vector2df unitVel = unit->velocity;
    bool stillColliding = true;
    while (stillColliding)
    {
        // only correct current movement
        if (unitVel.x ==  0)
            pixelCorrection.x = 0;
        if (unitVel.y == 0)
            pixelCorrection.y = 0;
        if (pixelCorrection.x == 0 && pixelCorrection.y == 0)
        {
            // usually this should not happen as it means there already has been
            // an unsolved collision on the last iteration, but better check for
            // it anyway to prevent an infinite loop
            break;
        }
        unitVel += pixelCorrection;

        vector<CollisionEntry>::const_iterator entryPtr;
        for (entryPtr = unit->collisionInfo.entries.begin(); entryPtr != unit->collisionInfo.entries.end(); ++entryPtr)
        {
            Vector2df pixel = entryPtr->pixel + unitVel - unit->velocity; // unitVel - unitVelocity = pixelCorrection * iteration
            if (pixel.x < 0 || pixel.y < 0 || pixel.x > GFX::getXResolution() || pixel.y > GFX::getYResolution())
                continue;

            Colour colColour = GFX::getPixel(level,pixel.x,pixel.y);
            // if unit is still colliding break here and go into next iteration
            if (unit->checkCollisionColour(colColour))
                break;
        }
        if (entryPtr == unit->collisionInfo.entries.end()) // all pixel have been checked, so no new collision has been found
            stillColliding = false;
    }
    correction = unitVel - unit->velocity;

    unit->collisionInfo.correction = correction;

    unit->hitMap();
}
*/


void Physics::playerUnitCollision(const Level* const parent, BaseUnit* const player, BaseUnit* const unit) const
{
    /// TODO: Optimize this and remove redundant stuff (aka this is a mess!)
    if (not unit->checkCollisionColour(player->col) && not player->checkCollisionColour(unit->col))
        return;

    Vector2df proPosPlayer = player->position + player->velocity;
    Vector2df proPosUnit = unit->position + unit->velocity;
    float xPos = max(proPosPlayer.x, proPosUnit.x);
    float yPos = max(proPosPlayer.y, proPosUnit.y);
    float xPosMax = min(proPosPlayer.x + player->getWidth(), proPosUnit.x + unit->getWidth());
    float yPosMax = min(proPosPlayer.y + player->getHeight(), proPosUnit.y + unit->getHeight());
    if (xPosMax > xPos && yPosMax > yPos)
    {
        float diffX = min(proPosPlayer.x + player->getWidth(), proPosUnit.x + unit->getWidth()) - max(proPosPlayer.x, proPosUnit.x);
        float diffY = min(proPosPlayer.y + player->getHeight(), proPosUnit.y + unit->getHeight()) - max(proPosPlayer.y, proPosUnit.y);
        SimpleDirection dir;
        if (player->position.x < unit->position.x)
            dir = diLEFT;
        else
            dir = diRIGHT;
        unit->hitUnit(make_pair(dir,Vector2df(diffX,diffY)),player);
    }

    Vector2df pos2 = parent->boundsCheck(player);
    if (pos2 != player->position)
    {
        Vector2df proPosPlayer = pos2 + player->velocity;
        float xPos = max(proPosPlayer.x, proPosUnit.x);
        float yPos = max(proPosPlayer.y, proPosUnit.y);
        float xPosMax = min(proPosPlayer.x + player->getWidth(), proPosUnit.x + unit->getWidth());
        float yPosMax = min(proPosPlayer.y + player->getHeight(), proPosUnit.y + unit->getHeight());
        if (xPosMax > xPos && yPosMax > yPos)
        {
            float diffX = min(proPosPlayer.x + player->getWidth(), proPosUnit.x + unit->getWidth()) - max(proPosPlayer.x, proPosUnit.x);
            float diffY = min(proPosPlayer.y + player->getHeight(), proPosUnit.y + unit->getHeight()) - max(proPosPlayer.y, proPosUnit.y);
            SimpleDirection dir;
            if (pos2.x < unit->position.x)
                dir = diLEFT;
            else
                dir = diRIGHT;
            unit->hitUnit(make_pair(dir,Vector2df(diffX,diffY)),player);
        }
    }
}

bool Physics::checkUnitCollision(const Level* const parent, const BaseUnit* const unitA, const BaseUnit* const unitB) const
{
    SDL_Rect rectA = {unitA->position.x, unitA->position.y, unitA->getWidth(), unitA->getHeight()};
    SDL_Rect rectB = {unitB->position.x, unitB->position.y, unitB->getWidth(), unitB->getHeight()};
    if (rectCheck(rectA,rectB))
        return true;

    // check warped position
    Vector2df posA2 = parent->boundsCheck(unitA);
    Vector2df posB2 = parent->boundsCheck(unitB);
    if (posA2 != unitA->position || posB2 != unitB->position)
    {
        rectA.x = posA2.x;
        rectA.y = posA2.y;
        rectB.x = posB2.x;
        rectB.y = posB2.y;
        if (rectCheck(rectA,rectB))
            return true;
    }
    return false;
}

///

bool Physics::rectCheck(const SDL_Rect& rectA, const SDL_Rect& rectB) const
{
    if (((rectB.x - rectA.x) < rectA.w && (rectA.x - rectB.x) < rectB.w) &&
        ((rectB.y - rectA.y) < rectA.h && (rectA.y - rectB.y) < rectB.h))
        return true;
    return false;
}