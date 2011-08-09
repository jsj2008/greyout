#ifndef CAMERA_H
#define CAMERA_H

#include "PenjinTypes.h"
#include "CountDown.h"
#include "Vector2df.h"

class BaseUnit;
class Level;

/**
Provides functions to move the rendered area around
**/

class Camera
{
    public:
        Camera();
        virtual ~Camera();

        // If time is set to 0 the move will be instant
        virtual void centerOnUnit(const BaseUnit* const unit, CRint time=0);
        virtual void centerOnPos(const Vector2df& pos, CRint time=0);

        Level* parent;
        // If false level boundaries will be taken into account when moving
        // the "camera" so space outside the level is never shown
        bool disregardBoundaries;
    protected:

    private:

};


#endif // CAMERA_H
