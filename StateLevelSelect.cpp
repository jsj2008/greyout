#include "StateLevelSelect.h"

#include "GFX.h"

#include "MyGame.h"
#include "Level.h"
#include "userStates.h"
#include "SurfaceCache.h"
#include "LevelLoader.h"
#include "Chapter.h"

#define PREVIEW_COUNT_X 4
#define PREVIEW_COUNT_Y 3
#define SPACING_X 20
#define OFFSET_Y 40

#define CURSOR_BORDER 5

#define DEFAULT_LEVEL_FOLDER ((string)"levels/")
#define DEFAULT_CHAPTER_FOLDER ((string)"chapters/")
#define DEFAULT_CHAPTER_INFO_FILE ((string)"info.txt")


struct PreviewData
{
    string filename;
    SDL_Surface* surface;
    bool hasBeenLoaded;
};

StateLevelSelect::StateLevelSelect()
{
    // set up the grid
    spacing.x = SPACING_X;
    size.x = (GFX::getXResolution() - spacing.x * (PREVIEW_COUNT_X + 1)) / PREVIEW_COUNT_X;
    size.y = (float)size.x * ((float)GFX::getYResolution() / (float)GFX::getXResolution());
    spacing.y = (GFX::getYResolution() - OFFSET_Y - size.y * PREVIEW_COUNT_Y) / (PREVIEW_COUNT_Y + 1);
    gridOffset = 0;
    lastDraw = 0;
    firstDraw = true;
    selection = Vector2di(0,0);

    // graphic stuff
    bool fromCache;
    bg.loadFrames(SURFACE_CACHE->getSurface("images/menu/error_bg_800_480.png",fromCache),1,1,0,0);
    bg.disableTransparentColour();
    bg.setPosition(0,0);
    loading.loadFrames(SURFACE_CACHE->getSurface("images/general/loading.png",fromCache,true),1,1,0,0);
    loading.disableTransparentColour();
    loading.setScaleX((float)size.x / loading.getWidth());
    loading.setScaleY((float)size.y / loading.getHeight());
    error.loadFrames(SURFACE_CACHE->getSurface("images/general/error.png",fromCache,true),1,1,0,0);
    error.disableTransparentColour();
    error.setScaleX((float)size.x / error.getWidth());
    error.setScaleY((float)size.y / error.getHeight());
    arrows.loadFrames(SURFACE_CACHE->getSurface("images/general/arrows.png",fromCache),2,1,0,0);
    arrows.setTransparentColour(MAGENTA);
    cursor.setDimensions(size.x + 2*CURSOR_BORDER,size.y + 2*CURSOR_BORDER);
    cursor.setColour(ORANGE);
    previewDraw = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
    SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
    SDL_SetColorKey(previewDraw, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(previewDraw->format,255,0,255));
    imageText.loadFont("fonts/Lato-Bold.ttf",24);
    imageText.setUpBoundary(size);
    imageText.setWrapping(true);
    imageText.setColour(WHITE);

    // thread stuff
    levelLock = SDL_CreateMutex();
    levelThread = NULL;
    abortLevelLoading = false;
    chapterLock = SDL_CreateMutex();
    chapterThread = NULL;
    abortChapterLoading = false;
    coutLock = SDL_CreateMutex();

    levelLister.addFilter("txt");
    dirLister.addFilter("DIR");
    state = lsChapter;
    setChapterDirectory(DEFAULT_CHAPTER_FOLDER);
    chapterPath = "";
}

StateLevelSelect::~StateLevelSelect()
{
    clear();
    SDL_FreeSurface(previewDraw);
    SDL_DestroyMutex(levelLock);
    SDL_DestroyMutex(chapterLock);
    SDL_DestroyMutex(coutLock);
}

/// ---public---

void StateLevelSelect::init()
{
#ifdef _DEBUG
    //fpsDisplay.loadFont("fonts/Lato-Bold.ttf",24);
    fpsDisplay.setColour(GREEN);
    fpsDisplay.setPosition(GFX::getXResolution(),0);
    fpsDisplay.setAlignment(RIGHT_JUSTIFIED);
#endif
}

void StateLevelSelect::clear()
{
    clearLevelListing();
    clearChapterListing();
}

void StateLevelSelect::clearLevelListing()
{
    abortLevelLoading = true;
    int* status = NULL;
    if (levelThread)
        SDL_WaitThread(levelThread,status);

    vector<PreviewData>::iterator iter;
    for (iter = levelPreviews.begin(); iter != levelPreviews.end(); ++iter)
    {
        if (not (*iter).hasBeenLoaded) // stop when reaching end of loaded levels
            break;
        SDL_FreeSurface((*iter).surface);
    }
    levelPreviews.clear();
    chapterPath = "";
}

void StateLevelSelect::clearChapterListing()
{
    abortChapterLoading = true;
    int* status = NULL;
    if (chapterThread)
        SDL_WaitThread(chapterThread,status);

    vector<PreviewData>::iterator iter;
    for (iter = chapterPreviews.begin(); iter != chapterPreviews.end(); ++iter)
    {
        if (not (*iter).hasBeenLoaded) // stop when reaching end of loaded levels
            break;
        SDL_FreeSurface((*iter).surface);
    }
    chapterPreviews.clear();
}

void StateLevelSelect::userInput()
{
    input->update();

#ifdef PLATFORM_PC
    if (input->isQuit())
    {
        nullifyState();
        return;
    }
#endif
    if (state != lsIntermediate)
    {
        if (input->isLeft())
        {
            --selection.x;
        }
        else if (input->isRight())
        {
            ++selection.x;
        }
        if (input->isUp())
        {
            --selection.y;
        }
        if (input->isDown())
        {
            ++selection.y;
        }
    }

    if (state == lsLevel)
    {
        if (input->isStart() || input->isB())
        {
            abortLevelLoading = true;
            setNextState(STATE_MAIN);
            return;
        }
        if (input->isA())
        {
            int value = selection.y * PREVIEW_COUNT_X + selection.x;
            // make sure the map has not returned an error on load already
            if (levelPreviews.at(value).surface && levelPreviews.at(value).hasBeenLoaded)
            {
                abortLevelLoading = true;
                if (chapterPath[0] == 0) // level from level folder
                    ENGINE->playSingleLevel(levelPreviews.at(value).filename,STATE_LEVELSELECT);
                else // exploring chapter -> start chapter at selected level
                    ENGINE->playChapter(chapterPath + DEFAULT_CHAPTER_INFO_FILE,value);
            }
        }
        checkSelection(levelPreviews,selection);
    }
    else if (state == lsChapter)
    {
        if (input->isStart() || input->isB())
        {
            setNextState(STATE_MAIN);
            return;
        }
        if (input->isA())
        {
            int value = selection.y * PREVIEW_COUNT_X + selection.x;
            if (value == 0) // level folder
            {
                setLevelDirectory(DEFAULT_LEVEL_FOLDER);
                switchState();
            }
            else
            {
                if (chapterPreviews.at(value).surface && chapterPreviews.at(value).hasBeenLoaded)
                {
                    exploreChapter(chapterPreviews.at(value).filename);
                    switchState();
                }
            }
        }
        checkSelection(chapterPreviews,selection);
    }

    input->resetKeys();
}

void StateLevelSelect::update()
{
    // check selection and adjust grid offset
    bool changed = false;
    if (selection.y < gridOffset)
    {
        --gridOffset;
        changed = true;
    }
    else if (selection.y >= gridOffset + PREVIEW_COUNT_Y)
    {
        ++gridOffset;
        changed = true;
    }
    if (changed)
    {
        firstDraw = true;
        lastDraw = 0;
        SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
    }

    cursor.setPosition(selection.x * size.x + (selection.x + 1) * spacing.x - CURSOR_BORDER,
                       OFFSET_Y + (selection.y - gridOffset) * size.y + (selection.y - gridOffset + 1) * spacing.y - CURSOR_BORDER);
}

void StateLevelSelect::render()
{
    GFX::clearScreen();

    bg.render();
    cursor.render();

    switch (state)
    {
    case lsChapter:
        renderPreviews(chapterPreviews,previewDraw,lastDraw - gridOffset * PREVIEW_COUNT_X);
        break;
    case lsLevel:
        renderPreviews(levelPreviews,previewDraw,lastDraw - gridOffset * PREVIEW_COUNT_X);
        break;
    default:
        break;
    }
    SDL_BlitSurface(previewDraw,NULL,GFX::getVideoSurface(),NULL);

    if (gridOffset > 0)
    {
        arrows.setCurrentFrame(0);
        arrows.setPosition(GFX::getXResolution() - arrows.getWidth(), OFFSET_Y);
        arrows.render();
    }
    if (levelPreviews.size() - gridOffset * PREVIEW_COUNT_X > PREVIEW_COUNT_X * PREVIEW_COUNT_Y)
    {
        arrows.setCurrentFrame(1);
        arrows.setPosition(GFX::getXResolution() - arrows.getWidth(), GFX::getYResolution() - arrows.getHeight());
        arrows.render();
    }

#ifdef _DEBUG
    //fpsDisplay.print(StringUtility::intToString(MyGame::getMyGame()->getFPS()));
#endif
}

void StateLevelSelect::renderPreviews(const vector<PreviewData>& data, SDL_Surface* const target, CRint addOffset)
{
    int numOffset = gridOffset * PREVIEW_COUNT_X;
    bool reachedLoaded = false; // set when the first not loaded surface is found
    for (int I = numOffset + max(addOffset,0); I < (numOffset + PREVIEW_COUNT_X * PREVIEW_COUNT_Y); ++I)
    {
        if (I >= data.size())
            break;

        // calculate position of object to render
        Vector2di pos;
        pos.x = (I % PREVIEW_COUNT_X) * size.x + (I % PREVIEW_COUNT_X + 1) * spacing.x;
        pos.y = OFFSET_Y + ((I - numOffset) / PREVIEW_COUNT_X) * size.y + ((I - numOffset) / PREVIEW_COUNT_X + 1) * spacing.y;

        SDL_mutexP(levelLock);
        if (data.at(I).hasBeenLoaded)
        {
            if (data.at(I).surface) // render preview
            {
                SDL_Rect rect = {pos.x,pos.y,0,0};
                SDL_BlitSurface(data.at(I).surface,NULL,target,&rect);
                SDL_mutexV(levelLock);
            }
            else // render error
            {
                SDL_mutexV(levelLock);
                error.setPosition(pos);
                error.render(target);
            }
            lastDraw = I;
        }
        else // render loading image
        {
            SDL_mutexV(levelLock);
            if (not reachedLoaded)
            {
                lastDraw = I;
                reachedLoaded = true;
                // will only render loading pics from here on, but we need to keep the lastDraw variable
            }
            if (firstDraw) // only draw loading images once
            {
                loading.setPosition(pos);
                loading.render(target);
            }
        }
    }
    firstDraw = false;
}

void StateLevelSelect::setLevelDirectory(CRstring dir)
{
    clearLevelListing();
    levelLister.setPath(dir);
    vector<string> files;
    files = levelLister.getListing();

    files.erase(files.begin()); // delete first element which is the current folder
    // initialize map
    for (vector<string>::const_iterator file = files.begin(); file < files.end(); ++file)
    {
        PreviewData temp = {dir + (*file),NULL,false};
        levelPreviews.push_back(temp);
    }
    cout << levelPreviews.size() << " files found in level directory" << endl;

    abortLevelLoading = false;
    levelThread = SDL_CreateThread(StateLevelSelect::loadLevelPreviews, this);

    files.clear();
}

void StateLevelSelect::setChapterDirectory(CRstring dir)
{
    clearChapterListing();
    dirLister.setPath(dir);
    vector<string> files;
    files = dirLister.getListing();

    // erase dir, . and ..
    files.erase(files.begin());
    files.erase(files.begin());
    files.erase(files.begin());

    // add single level folder
    bool fromCache;
    SDL_Surface* img = SURFACE_CACHE->getSurface("images/general/levelFolder.png",fromCache,true);
    if (img->w != size.x || img->h != size.y)
    {
        img = rotozoomSurfaceXY(img,0,(float)size.x / (float)img->w , (float)size.y / (float)img->h, SMOOTHING_OFF);
    }
    PreviewData temp = {DEFAULT_LEVEL_FOLDER,img,true};
    chapterPreviews.push_back(temp);

    // set paths to info.txt files and initialize map
    for (vector<string>::iterator item = files.begin(); item < files.end(); ++item)
    {
        (*item) = dir + (*item) + "/" + DEFAULT_CHAPTER_INFO_FILE;
        PreviewData temp2 = {(*item),NULL,false};
        chapterPreviews.push_back(temp2);
    }
    cout << chapterPreviews.size()-1 << " chapters found" << endl;

    abortChapterLoading = false;
    chapterThread = SDL_CreateThread(StateLevelSelect::loadChapterPreviews,this);
}

void StateLevelSelect::exploreChapter(CRstring filename)
{
    clearLevelListing();
    Chapter chapter;

    if (not chapter.loadFromFile(filename))
    {
        ENGINE->stateParameter = "Error loading chapter!";
        setNextState(STATE_ERROR);
    }

    chapterPath = chapter.path;

    for (vector<string>::const_iterator file = chapter.levels.begin(); file != chapter.levels.end(); ++file)
    {
        PreviewData temp = {chapter.path + (*file),NULL,false};
        levelPreviews.push_back(temp);
    }
    cout << "Chapter has " << levelPreviews.size() << " levels" << endl;

    abortLevelLoading = false;
    levelThread = SDL_CreateThread(StateLevelSelect::loadLevelPreviews,this);
}

/// ---protected---

void StateLevelSelect::switchState()
{
    if (state == lsLevel)
        state = lsChapter;
    else
        state = lsLevel;
    gridOffset = 0;
    lastDraw = 0;
    firstDraw = true;
    selection = Vector2di(0,0);
    SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
}

void StateLevelSelect::checkSelection(const vector<PreviewData>& data, Vector2di& sel)
{
    if (sel.y < 0)
        sel.y = 0;
    else
    {
        int max = ceil((float)data.size() / (float)PREVIEW_COUNT_X);
        if (sel.y >= max)
            sel.y = max - 1;
    }

    if (sel.x < 0)
        sel.x = 0;
    else
    {
        int max = min(PREVIEW_COUNT_X,(int)data.size() - sel.y * PREVIEW_COUNT_X);
        if (sel.x >= max)
            sel.x = max - 1;
    }
}

int StateLevelSelect::loadLevelPreviews(void* data)
{
    cout << "Generating level previews images" << endl;
    StateLevelSelect* self = (StateLevelSelect*)data;
    vector<PreviewData>::iterator iter = self->levelPreviews.begin();

    while (not self->abortLevelLoading && iter != self->levelPreviews.end())
    {
        string filename = (*iter).filename;

        Level* level = LEVEL_LOADER->loadLevelFromFile(filename,self->chapterPath);
        if (level)
        {
            SDL_Surface* temp = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
            level->update(); // update once to properly position units
            level->render(temp); // render at full resolution
            // scale down, then delete full resolution copy
            SDL_Surface* surf = rotozoomSurfaceXY(temp,0,(float)self->size.x / GFX::getXResolution(), (float)self->size.y / GFX::getYResolution(), SMOOTHING_OFF);
            SDL_FreeSurface(temp);
            SDL_mutexP(self->levelLock);
            // set results for drawing
            (*iter).surface = surf;
            (*iter).hasBeenLoaded = true;
            SDL_mutexV(self->levelLock);
        }
        else // error on load
        {
            SDL_mutexP(self->levelLock);
            // set loaded to true, but keep surface == NULL indicating an error
            (*iter).hasBeenLoaded = true;
            SDL_mutexV(self->levelLock);
            cout << LEVEL_LOADER->errorString << endl;
        }
        delete level;
        ++iter;
    }
    SDL_mutexP(self->coutLock);
    if (self->abortLevelLoading)
    {
        cout << "Aborted level preview generation" << endl;
        self->abortLevelLoading = false;
    }
    else
        cout << "Finished level generating preview images" << endl;
    SDL_mutexV(self->coutLock);
    return 0;
}

int StateLevelSelect::loadChapterPreviews(void* data)
{
    StateLevelSelect* self = (StateLevelSelect*)data;

    SDL_mutexP(self->coutLock);
    cout << "Loading chapter preview images" << endl;
    SDL_mutexV(self->coutLock);
    vector<PreviewData>::iterator iter = self->chapterPreviews.begin();
    ++iter; // skip level folder
    Chapter chapter;

    while (not self->abortChapterLoading && iter != self->chapterPreviews.end())
    {
        string filename = (*iter).filename;

        if (not chapter.loadFromFile(filename))
        {
            // oops, we have error
            SDL_mutexP(self->chapterLock);
            (*iter).hasBeenLoaded = true;
            SDL_mutexV(self->chapterLock);
            cout << chapter.errorString << endl;
            ++iter;
            continue;
        }

        // loaded chapter fine
        if (chapter.imageFile[0] != 0) // we have an image file
        {
            bool fromCache;
            SDL_Surface* img = SURFACE_CACHE->getSurface(chapter.imageFile,chapter.path,fromCache);
            if (img) // load successful
            {
                if (img->w != self->size.x || img->h != self->size.y) // scale if needed
                {
                    img = rotozoomSurfaceXY(img,0,(float)self->size.x / (float)img->w , (float)self->size.y / (float)img->h, SMOOTHING_OFF);
                }
                SDL_mutexP(self->chapterLock);
                (*iter).surface = img;
                (*iter).hasBeenLoaded = true;
                SDL_mutexV(self->chapterLock);
                ++iter;
                continue;
            }
        }
        // reaching here we either have no image file or loading it resulted in an error
        // render text instead
        SDL_Surface* surf = SDL_CreateRGBSurface(SDL_SWSURFACE,self->size.x,self->size.y,GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
        SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format,0,0,0));
        self->imageText.print(surf,chapter.name);
        SDL_mutexP(self->chapterLock);
        (*iter).surface = surf;
        (*iter).hasBeenLoaded = true;
        SDL_mutexV(self->chapterLock);
        ++iter;
    }
    SDL_mutexP(self->coutLock);
    if (self->abortChapterLoading)
        cout << "Aborted chapter preview generation" << endl;
    else
        cout << "Finished generating chapter preview images" << endl;
    SDL_mutexV(self->coutLock);
    return 0;
}