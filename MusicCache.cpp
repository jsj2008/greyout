#include "MusicCache.h"

#include <iostream>
#include <SDL/SDL_mixer.h>

#include "Music.h"
#include "Sound.h"

using namespace std;

MusicCache* MusicCache::self = NULL;

MusicCache::MusicCache()
{
    fadeDuration = 1000;
    musicPlaying = "";
    nextTrack = "";
    soundVolume = MIX_MAX_VOLUME;
    musicVolume = MIX_MAX_VOLUME;
    setMusicVolume(musicVolume);
    fadeThread = NULL;
}

MusicCache::~MusicCache()
{
    int* result = NULL;
    if (fadeThread)
        SDL_WaitThread(fadeThread,result);
    stopMusic();
    stopSounds();
    clear();
}

MusicCache* MusicCache::getMusicCache()
{
    if (not self)
        self = new MusicCache();
    return self;
}

///---public---

bool MusicCache::playMusic(CRstring filename, CRbool suppressOutput)
{
    map<string,Music*>::iterator iter = music.find(filename);

    int* result = NULL;
    if (fadeThread)
    {
        SDL_WaitThread(fadeThread,result);
        fadeThread = NULL;
    }

    if (iter != music.end()) // found in cache
    {
        if (musicPlaying[0] == 0) // nothing is currently playing
        {
            nextTrack = filename;
            fadeThread = SDL_CreateThread(MusicCache::fadeMusic, iter->second);
        }
        else
        {
            if (musicPlaying != filename)
            {
                nextTrack = filename;
                fadeThread = SDL_CreateThread(MusicCache::fadeMusic, iter->second);
            }
            // else just continue playing of current music
        }
        return true;
    }
    else // load new item
    {
        Music* temp = new Music;

        if (not suppressOutput)
            cout << "Loading new music track to cache \"" << filename << "\"" << endl;

        if (temp->loadMusic(filename) != PENJIN_OK)
        {
            if (not suppressOutput)
                cout << "Error loading music \"" << filename << "\": " <<
                Mix_GetError() << endl;
            delete temp;
            return false;
        }
        music[filename] = temp;
        nextTrack = filename;
        fadeThread = SDL_CreateThread(MusicCache::fadeMusic, temp);
    }
    return true;
}

bool MusicCache::playMusic(CRstring filename, CRstring pathOverwrite)
{
    if (pathOverwrite[0] == 0)
        return playMusic(filename);

    cout << "Trying to load custom music track \"" << pathOverwrite + filename << "\"" << endl;

    if (not playMusic(pathOverwrite + filename,true))
    {
        cout << "Custom music track not found, loading default!" << endl;
        return playMusic(filename);
    }

    cout << "Now playing: \"" << pathOverwrite + filename << "\"" << endl;
    return true;
}

void MusicCache::stopMusic()
{
    if (musicPlaying[0] != 0)
        stop(music.find(musicPlaying)->second);
}

void MusicCache::setMusicVolume(CRint newVol)
{
    Mix_VolumeMusic(newVol);
    musicVolume = newVol;
}

bool MusicCache::playSound(CRstring filename, CRint numLoops, CRbool suppressOutput)
{
    map<string,Sound*>::iterator iter = sounds.find(filename);

    if (iter != sounds.end()) // found in cache
    {
        iter->second->play(numLoops);
        iter->second->setVolume(soundVolume);
    }
    else // load new
    {
        Sound* temp = new Sound;

        if (not suppressOutput)
            cout << "Loading new sound to cache \"" << filename << "\"" << endl;

        if (temp->loadSound(filename) != PENJIN_OK)
        {
            if (not suppressOutput)
                cout << "Error loading sound \"" << filename << "\": " << Mix_GetError << endl;
            delete temp;
            return false;
        }

        sounds[filename] = temp;
        temp->setSimultaneousPlay(true);
        temp->play(numLoops);
        temp->setVolume(soundVolume);
    }

    return true;
}

bool MusicCache::playSound(CRstring filename, CRstring pathOverwrite, CRint numLoops)
{
    if (pathOverwrite[0] == 0)
        return playSound(filename,numLoops);

    cout << "Trying to load custom sound \"" << pathOverwrite + filename << "\"" << endl;

    if (not playSound(pathOverwrite + filename,numLoops,true))
    {
        cout << "Custom sound not found, loading default!" << endl;
        return playSound(filename,numLoops);
    }
    return true;
}

void MusicCache::stopSounds()
{
    for(map<string,Sound*>::iterator iter = sounds.begin(); iter != sounds.end(); ++iter)
    {
        iter->second->stop();
    }
}

void MusicCache::setSoundVolume(CRint newVol)
{
    for(map<string,Sound*>::iterator iter = sounds.begin(); iter != sounds.end(); ++iter)
    {
        iter->second->setVolume(newVol);
    }
    soundVolume = newVol;
}

int MusicCache::getMaxVolume() const
{
    return MIX_MAX_VOLUME;
}

void MusicCache::clear(CRbool clearPlaying)
{
    clearMusic(clearPlaying);
    clearSounds(clearPlaying);
}

void MusicCache::clearMusic(CRbool clearPlaying)
{
    int musicClear = music.size();

    map<string,Music*> playingM;
    for (map<string,Music*>::iterator iter = music.begin(); iter != music.end(); ++iter)
    {
        if (not clearPlaying && iter->second->isPlaying())
            playingM[iter->first] = iter->second;
        else
            delete iter->second;
    }
    music.clear();
    // preserve playing music
    if (not playingM.empty())
    {
        music.insert(playingM.begin(),playingM.end());
        playingM.clear();
    }
    cout << "Cleared music cache - deleted " << (musicClear - music.size())
    << " music tracks (" << music.size() << " still playing)." << endl;
}

void MusicCache::clearSounds(CRbool clearPlaying)
{
    int soundClear = sounds.size();

    map<string,Sound*> playingS;
    for (map<string,Sound*>::iterator iter = sounds.begin(); iter != sounds.end(); ++iter)
    {
        if (not clearPlaying && iter->second->isPlaying())
            playingS[iter->first] = iter->second;
        else
            delete (*iter).second;
    }
    sounds.clear();
    if (not playingS.empty())
    {
        sounds.insert(playingS.begin(),playingS.end());
        playingS.clear();
    }
    cout << "Cleared sound cache - deleted " << (soundClear - sounds.size())
    << " sounds (" << sounds.size() << " still playing)." << endl;
}


bool MusicCache::isLoaded(CRstring filename) const
{
    map<string,Music*>::const_iterator mIter = music.find(filename);
    if (mIter != music.end())
        return true;
    map<string,Sound*>::const_iterator sIter = sounds.find(filename);
    if (sIter != sounds.end())
        return true;
    return false;
}

///---protected---

void MusicCache::play(Music* const item, CRstring file)
{
    item->setVolume(musicVolume);
    if (fadeDuration > 0)
        item->playFadeIn(fadeDuration);
    else
        item->play();
    musicPlaying = file;
    cout << "Now playing: \"" << file << "\"" << endl;
}

void MusicCache::stop(Music* const item)
{
    if (fadeDuration > 0)
        item->fade(fadeDuration);
    else
        item->stop();
    musicPlaying = "";
}

int MusicCache::fadeMusic(void* data)
{
    Music* track = (Music*)data;
    if (MUSIC_CACHE->musicPlaying[0] != 0) // stop current
    {
        MUSIC_CACHE->stop(MUSIC_CACHE->music.find(MUSIC_CACHE->musicPlaying)->second);
    }
    // play next track
    MUSIC_CACHE->play(track,MUSIC_CACHE->nextTrack);
    return 0;
}

///---private---