#include "BaseUnit.h"

class Switch : public BaseUnit
{
public:
    Switch(Level* newParent);
    virtual ~Switch();

    virtual bool load(const PARAMETER_TYPE& params);
    virtual void reset();

    virtual bool hitUnitCheck(const BaseUnit* const caller) const;
    virtual void hitUnit(const UnitCollisionEntry& entry);

    virtual void update();
protected:
    virtual bool processParameter(const pair<string,string>& value);

    typedef void (Switch::*FuncPtr)();
    FuncPtr switchOn;
    FuncPtr switchOff;
    void movementOn();
    void movementOff();

    int switchTimer;
    BaseUnit* target;

    enum SwitchProp
    {
        spFunction=BaseUnit::upEOL,
        spTarget,
        bpEOL
    };

    enum SwitchFunction
    {
        sfMovement
    };
    map<string,int> stringToFunc;
private:
};
