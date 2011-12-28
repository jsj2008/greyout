#ifndef SOUND_TRIGGER
#define SOUND_TRIGGER

#include "BaseTrigger.h"

class SoundTrigger : public BaseTrigger
{
public:
    SoundTrigger(Level* newParent);
    virtual ~SoundTrigger();

protected:
    virtual void doTrigger(const UnitCollisionEntry& entry);

    virtual bool processParameter(const PARAMETER_TYPE& value);

    enum SoundProp
    {
        spFile=BaseTrigger::upEOL,
        spPlayCount,
        spLoops,
        spEOL
    };

    string filename;
    int playcount;
    int loops;
private:

};


#endif // SOUND_TRIGGER
