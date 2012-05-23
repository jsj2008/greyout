#include "GreySurfaceCache.h"

#include <iostream>

GreySurfaceCache::GreySurfaceCache() : SurfaceCache()
{
    #ifdef _DEBUG
    superVerbose = true;
    #else
    superVerbose = false;
    #endif
}

GreySurfaceCache::~GreySurfaceCache()
{
    //
}

SDL_Surface* GreySurfaceCache::loadSurface(CRstring filename, CRstring pathOverwrite, CRbool optimize)
{
    if (pathOverwrite[0] == 0) // no overwrite specified
        return loadSurface(filename,optimize);

    if (superVerbose)
        printf("Trying to load custom image \"%s%s\"\n",pathOverwrite.c_str(),filename.c_str());

    verbose = false;
    SDL_Surface* surface = loadSurface(pathOverwrite + filename, optimize);
    verbose = true;

    if (surface == errorSurface || surface == NULL)
    {
        if (superVerbose)
            printf("Custom image not found, loading default!\n");
        surface = loadSurface(filename,optimize);
    }

    return surface;
}
