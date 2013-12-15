#ifndef PHYSICS_H
#define PHYSICS_H

#include <SDL/SDL.h>
#include <vector>

#include "Vector2df.h"

/**
Collision checking class
Testing for collisions between a unit and the level (imaged based, checking corner pixels)
and between two units (rectangular)
Also applies gravity, acceleration and friction to units
See readme file for more details (such as advantages and problems with this implementation)
**/

#define PHYSICS (Physics::GetSingleton())

class Level;
class BaseUnit;
class SimpleDirection;

class Physics
{
private:
    Physics();
    static Physics* self;
public:
    ~Physics();
    static Physics* GetSingleton();

    // apply physical forces such as gravity, friction and acceleration to the unit
    void applyPhysics(BaseUnit* const unit) const;

    // check for a collision between the passed unit and level
    // level - the parent Level, used for bounds checking
    // colImage - the actual image against which we will test
    // unit - the unit to test
    // mapOffset - optional offset parameter
    // will not return anything but set unit->collisionInfo and call unit->hitMap instead
    void unitMapCollision(const Level* const level, SDL_Surface* const colImage, BaseUnit* const unit, const Vector2df& mapOffset = Vector2df(0,0)) const;
    // check for a collision between two units
    // level - Level, used for bounds checking
    // calls unit->hit on hit (does not call player->hit, call that from unit->hit
    // by using the passed parameter instead)
    // see readme for why this is done
    void playerUnitCollision(const Level* const level, BaseUnit* const player, BaseUnit* const unit) const;

    void particleMapCollision(const Level* const level, SDL_Surface* const colImage, BaseUnit* const particle) const;

    // simple rectangular check between two units, returns true on collision
    bool checkUnitCollision(const Level* const level, const BaseUnit* const unitA, const BaseUnit* const unitB) const;

    // resets to initial state
    void reset();

    Vector2df gravity;
    Vector2df maximum; // the maximum, absolute value a unit is allowed to move (limit)
private:
    // check for overlapping rectangles
    bool rectCheck(const SDL_Rect& rectA, const SDL_Rect& rectB) const;

    std::vector<SimpleDirection> checkPointsX;
    std::vector<SimpleDirection> checkPointsY;
};

#endif
