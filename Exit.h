#ifndef EXIT_H
#define EXIT_H

#include "BaseUnit.h"

class Exit : public BaseUnit
{
public:
    Exit(Level* newParent);
    virtual ~Exit();

    virtual bool load(const PARAMETER_TYPE& params);

    virtual bool hitUnitCheck(const BaseUnit* const caller) const;
    virtual void hitUnit(const UnitCollisionEntry& entry);
protected:

private:

};


#endif // EXIT_H
