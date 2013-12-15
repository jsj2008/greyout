#include "StateLevelSelect.h"

#include "GFX.h"
#include <limits.h>

#include "MyGame.h"
#include "Level.h"
#include "userStates.h"
#include "GreySurfaceCache.h"
#include "LevelLoader.h"
#include "MusicCache.h"
#include "gameDefines.h"
#include "Savegame.h"

#define PREVIEW_COUNT_X 4
#define PREVIEW_COUNT_Y 3
#ifdef _MEOW
#define SPACING_X 10
#define OFFSET_Y 18
#define TITLE_FONT_SIZE 24
#define IMAGE_FONT_SIZE 12
#define CURSOR_BORDER 2
#else
#define SPACING_X 20
#define OFFSET_Y 35
#define TITLE_FONT_SIZE 48
#define IMAGE_FONT_SIZE 24
#define CURSOR_BORDER 5
#endif


#define INTERMEDIATE_MENU_ITEM_COUNT 2
#define INTERMEDIATE_LEVEL_MENU_ITEM_COUNT 2
#define INTERMEDIATE_MENU_SPACING 20

#define DEFAULT_LEVEL_FOLDER ((string)"levels/")
#define DEFAULT_CHAPTER_FOLDER ((string)"chapters/")

map<string,SDL_Surface*> StateLevelSelect::previewCache;


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
    gridOffsetLast = 0;
    lastDraw = 0;
    firstDraw = true;
    selection = Vector2di(0,0);
    intermediateSelection = 0;
    lastPos = Vector2di(-1,-1);
    mousePos = Vector2di(0,0);
    mouseInBounds = false;

    // graphic stuff
    // TODO: Use 320x240 bg here (and make that)
    bg.loadFrames(SURFACE_CACHE->loadSurface("images/menu/error_bg_800_480.png"),1,1,0,0);
    bg.disableTransparentColour();
    bg.setPosition(0,0);
    loading.loadFrames(SURFACE_CACHE->loadSurface("images/general/loading.png",true),1,1,0,0);
    loading.disableTransparentColour();
    loading.setScaleX((float)size.x / loading.getWidth());
    loading.setScaleY((float)size.y / loading.getHeight());
    error.loadFrames(SURFACE_CACHE->loadSurface("images/general/error.png",true),1,1,0,0);
    error.disableTransparentColour();
    error.setScaleX((float)size.x / error.getWidth());
    error.setScaleY((float)size.y / error.getHeight());
    locked.loadFrames(SURFACE_CACHE->loadSurface("images/general/locked.png",true),1,1,0,0);
    locked.disableTransparentColour();
    locked.setScaleX((float)size.x / locked.getWidth());
    locked.setScaleY((float)size.y / locked.getHeight());
    arrows.loadFrames(SURFACE_CACHE->loadSurface("images/general/arrows.png"),2,1,0,0);
    arrows.setTransparentColour(MAGENTA);
    cursor.setDimensions(size.x + 2*CURSOR_BORDER,size.y + 2*CURSOR_BORDER);
    cursor.setColour(ORANGE);
    previewDraw = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
    SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
    SDL_SetColorKey(previewDraw, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(previewDraw->format,255,0,255));
    imageText.loadFont(GAME_FONT,IMAGE_FONT_SIZE);
    imageText.setUpBoundary(size);
    imageText.setWrapping(true);
    imageText.setColour(WHITE);
    title.loadFont(GAME_FONT,TITLE_FONT_SIZE);
    title.setAlignment(LEFT_JUSTIFIED);
    title.setColour(WHITE);
    title.setPosition(0,OFFSET_Y - TITLE_FONT_SIZE);
    title.setUpBoundary(Vector2di(GFX::getXResolution(),OFFSET_Y));
    menu.setDimensions(GFX::getXResolution(),OFFSET_Y);
    menu.setColour(BLACK);
    menu.setPosition(0,0);
    overlay.setDimensions(GFX::getXResolution(),GFX::getYResolution());
    overlay.setColour(BLACK);
    overlay.setPosition(0,0);
    overlay.setAlpha(100);

    // thread stuff
    levelLock = SDL_CreateMutex();
    levelThread = NULL;
    abortLevelLoading = false;
    chapterLock = SDL_CreateMutex();
    chapterThread = NULL;
    abortChapterLoading = false;

    levelLister.addFilter("txt");
    dirLister.addFilter("DIR");
    state = lsChapter;
    setChapterDirectory(DEFAULT_CHAPTER_FOLDER);
    exChapter = NULL;
}

StateLevelSelect::~StateLevelSelect()
{
    clear();
    SDL_FreeSurface(previewDraw);
    SDL_DestroyMutex(levelLock);
    SDL_DestroyMutex(chapterLock);
}

/// ---public---

void StateLevelSelect::init()
{
    MUSIC_CACHE->playMusic("music/title_menu.ogg");
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
    {
        SDL_WaitThread(levelThread,status);
        levelThread = NULL;
    }

    levelPreviews.clear();
    delete exChapter;
    exChapter = NULL;
}

void StateLevelSelect::clearChapterListing()
{
    abortChapterLoading = true;
    int* status = NULL;
    if (chapterThread)
    {
        SDL_WaitThread(chapterThread,status);
        chapterThread = NULL;
    }

    chapterPreviews.clear();
}

void StateLevelSelect::userInput()
{
	mousePos = input->getMouse();
    if (state != lsIntermediate && state != lsIntermediateLevel) // grid navigation
    {
    	Vector2di oldSel = selection;
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
        if (input->getMouseWheelDelta() != 0)
		{
			gridOffset -= input->getMouseWheelDelta();
			lastPos = Vector2di(-1,-1);
			if (state == lsLevel)
			{
				checkGridOffset( levelPreviews, gridOffset );
			}
			else if (state == lsChapter)
			{
				checkGridOffset( chapterPreviews, gridOffset );
			}
		}

        if (mousePos != lastPos)
		{
			mouseInBounds = true;
			Vector2di newPos(-1,-1);
			newPos.x = floor( (float)(mousePos.x - spacing.x) / (float)( size.x + spacing.x ) );
			newPos.y = floor( (float)(mousePos.y - spacing.y / 2.0f - OFFSET_Y) / (float)( size.y + spacing.y) );
			if (newPos.y >= 0 && newPos.y < PREVIEW_COUNT_Y)
				newPos.y += gridOffset;
			else
			{
				mouseInBounds = false;
				newPos.y = -1;
			}
			if (newPos.x >= 0 && newPos.y >= 0 && newPos != selection)
			{
				selection = newPos;
			}

			lastPos = mousePos;
		}
		if (input->isLeftClick())
		{
			if (mousePos.y > OFFSET_Y + spacing.y * PREVIEW_COUNT_Y + size.y * PREVIEW_COUNT_Y)
			{
				++gridOffset;
				if (state == lsLevel)
				{
					checkGridOffset( levelPreviews, gridOffset );
				}
				else if (state == lsChapter)
				{
					checkGridOffset( chapterPreviews, gridOffset );
				}
			}
			else if (mousePos.y < OFFSET_Y + spacing.y)
			{
				--gridOffset;
				if (state == lsLevel)
				{
					checkGridOffset( levelPreviews, gridOffset );
				}
				else if (state == lsChapter)
				{
					checkGridOffset( chapterPreviews, gridOffset );
				}
			}
		}
		if (state == lsLevel)
		{
			checkSelection( levelPreviews, selection );
		}
		else if (state == lsChapter)
		{
			checkSelection( chapterPreviews, selection );
		}
		if (selection != oldSel)
			MUSIC_CACHE->playSound("sounds/level_select.wav");
    }
    else // intermediate menu navigation
    {
    	int menuItemCount = 0;
    	if (state == lsIntermediate)
		{
			menuItemCount = INTERMEDIATE_MENU_ITEM_COUNT;
			if (StringUtility::stringToBool(SAVEGAME->getData("speedrun")))
				++menuItemCount;
		}
		else
			menuItemCount = INTERMEDIATE_LEVEL_MENU_ITEM_COUNT;
        if (input->isUp())
        {
            if (intermediateSelection > 0)
            {
                --intermediateSelection;
            }
        }
        else if (input->isDown())
        {
            if (intermediateSelection < menuItemCount-1)
            {
                ++intermediateSelection;
            }
        }
        if (mousePos != lastPos)
		{
			int pos = (GFX::getYResolution() - INTERMEDIATE_MENU_SPACING * (menuItemCount-1) - OFFSET_Y * menuItemCount) / 2;
			int temp = -1;
			for (int I = 0; I < menuItemCount; ++I)
			{
				if (mousePos.y >= pos && mousePos.y <= pos + OFFSET_Y)
				{
					temp = I;
					break;
				}
				pos += OFFSET_Y + INTERMEDIATE_MENU_SPACING;
			}
			if ( temp != -1 )
			{
				intermediateSelection = temp;
				mouseInBounds = true;
			}
			else
				mouseInBounds = false;

			lastPos = mousePos;
		}

        if ( ACCEPT_KEY || ( input->isLeftClick() && mouseInBounds ) )
        {
			int value = selection.y * PREVIEW_COUNT_X + selection.x;
        	if (state == lsIntermediate)
			{
				switch (intermediateSelection)
				{
				case 0: // play
					ENGINE->playChapter(chapterPreviews[value].filename);
					MUSIC_CACHE->playSound("sounds/drip.wav");
					break;
				case 1: // explore
					exploreChapter(chapterPreviews[value].filename);
					switchState(lsLevel);
					break;
				case 2: // time attack (speedrun!)
					ENGINE->startChapterTrial();
					ENGINE->playChapter(chapterPreviews[value].filename,0);
					MUSIC_CACHE->playSound("sounds/drip.wav");
					break;
				default: // nothing
					break;
				}
            }
            else
			{
				if (intermediateSelection == 1)
					ENGINE->timeTrial = true;

				abortLevelLoading = true;
				if (not exChapter) // level from level folder
					ENGINE->playSingleLevel(levelPreviews[value].filename,STATE_LEVELSELECT);
				else // exploring chapter -> start chapter at selected level
				{
					if (value <= exChapter->getProgress())
						ENGINE->playChapter(exChapter->filename,value);
				}
				MUSIC_CACHE->playSound("sounds/drip.wav");
			}
        }
        else if (CANCEL_KEY || input->isRightClick())
        {
        	if (state == lsIntermediate)
				switchState(lsChapter);
			else
				switchState(lsLevel);
        }
        input->resetKeys();
    }

    if (state == lsLevel)
    {
        if (CANCEL_KEY || input->isRightClick()) // return to chapter selection
        {
            abortLevelLoading = true;
            switchState(lsChapter);
            input->resetKeys();
            return;
        }
        if ( ACCEPT_KEY || ( input->isLeftClick() && mouseInBounds ) )
        {
			if (StringUtility::stringToBool(SAVEGAME->getData("speedrun")))
			{
				switchState(lsIntermediateLevel);
				input->resetKeys();
				return;
			}
			else
			{
				int value = selection.y * PREVIEW_COUNT_X + selection.x;
				abortLevelLoading = true;
				if (not exChapter) // level from level folder
					ENGINE->playSingleLevel(levelPreviews[value].filename,STATE_LEVELSELECT);
				else // exploring chapter -> start chapter at selected level
				{
					if (value <= exChapter->getProgress())
						ENGINE->playChapter(exChapter->filename,value);
				}
				MUSIC_CACHE->playSound("sounds/drip.wav");
			}
        }
    }
    else if (state == lsChapter)
    {
        if (CANCEL_KEY || input->isRightClick()) // return to menu
        {
            abortLevelLoading = true;
            setNextState(STATE_MAIN);
            input->resetKeys();
            MUSIC_CACHE->playSound("sounds/menu_back.wav");
            return;
        }
        if (ACCEPT_KEY || ( input->isLeftClick() && mouseInBounds ) )
        {
            int value = selection.y * PREVIEW_COUNT_X + selection.x;
            if (value == 0) // level folder
            {
                setLevelDirectory(DEFAULT_LEVEL_FOLDER);
                switchState(lsLevel);
            }
            else // show play/explore menu
            {
                if (chapterPreviews[value].surface && chapterPreviews[value].hasBeenLoaded)
                {
                    switchState(lsIntermediate);
                    input->resetKeys();
                    return;
                }
            }
        }
    }

    input->resetKeys();
}

void StateLevelSelect::update()
{
    if (state != lsIntermediate && state != lsIntermediateLevel)
    {
        // check selection and adjust grid offset
        if (selection.y < gridOffset)
            --gridOffset;
        else if (selection.y >= gridOffset + PREVIEW_COUNT_Y)
            ++gridOffset;

        if (gridOffset != gridOffsetLast)
        {
            // redraw previews on scroll
        	gridOffsetLast = gridOffset;
            firstDraw = true;
            lastDraw = 0;
            SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
        }

        cursor.setPosition(selection.x * size.x + (selection.x + 1) * spacing.x - CURSOR_BORDER,
                           OFFSET_Y + (selection.y - gridOffset) * size.y + (selection.y - gridOffset + 1) * spacing.y - CURSOR_BORDER);

    }
}

void StateLevelSelect::render()
{
    if (state != lsIntermediate && state != lsIntermediateLevel)
    {
        GFX::clearScreen();

        bg.render();
        cursor.render();
        menu.render();

        vector<PreviewData>* activeData;

        switch (state)
        {
        case lsChapter:
            title.print("CHAPTERS");
            activeData = &chapterPreviews;
            break;
        case lsLevel:
            title.print("LEVELS");
            activeData = &levelPreviews;
            break;
        default:
            break;
        }

        // render previews to temp surface (checks for newly generated previews and renders them only)
        renderPreviews(*activeData,previewDraw,lastDraw - gridOffset * PREVIEW_COUNT_X);
        // blit temp surface
        SDL_BlitSurface(previewDraw,NULL,GFX::getVideoSurface(),NULL);

        // show arrows
        if (gridOffset > 0)
        {
            arrows.setCurrentFrame(0);
            arrows.setPosition((GFX::getXResolution() - arrows.getWidth()) / 2, OFFSET_Y + 5);
            arrows.render();
        }
        if (activeData->size() - gridOffset * PREVIEW_COUNT_X > PREVIEW_COUNT_X * PREVIEW_COUNT_Y)
        {
            arrows.setCurrentFrame(1);
            arrows.setPosition((GFX::getXResolution() - arrows.getWidth()) / 2, GFX::getYResolution() - arrows.getHeight() - 5);
            arrows.render();
        }
    }
    else
    {
        // previewDraw now holds the background image
        SDL_BlitSurface(previewDraw,NULL,GFX::getVideoSurface(),NULL);

		vector<string> items;
        if (state == lsIntermediate)
		{
			items.push_back("PLAY");
			items.push_back("EXPLORE");
			if (StringUtility::stringToBool(SAVEGAME->getData("speedrun")))
				items.push_back("SPEEDRUN");
		}
		else
		{
			items.push_back("PLAY");
			items.push_back("SPEEDRUN");
		}

        // OFFSET_Y is also the height of the rectangle used
        int pos = (GFX::getYResolution() - INTERMEDIATE_MENU_SPACING * (items.size()-1) - OFFSET_Y * items.size()) / 2;

        for (int I = 0; I < items.size(); ++I)
        {
            menu.setPosition(0,pos);
            title.setPosition(0,pos + OFFSET_Y - TITLE_FONT_SIZE);
            if (I == intermediateSelection)
            {
                menu.setColour(WHITE);
                title.setColour(BLACK);
            }
            else
            {
                menu.setColour(BLACK);
                title.setColour(WHITE);
            }
            menu.render();
            title.print(items[I]);
            pos += OFFSET_Y + INTERMEDIATE_MENU_SPACING;
        }

    }

}

void StateLevelSelect::renderPreviews(const vector<PreviewData>& data, SDL_Surface* const target, CRint addOffset)
{
    int numOffset = gridOffset * PREVIEW_COUNT_X;
    bool reachedLoaded = false; // set when the first not loaded surface is found

    int maxUnlocked = 0;
    if (state == lsLevel && exChapter)
    {
        maxUnlocked = exChapter->getProgress();
    }

    for (int I = numOffset + max(addOffset,0); (I < (numOffset + PREVIEW_COUNT_X * PREVIEW_COUNT_Y)) && (I < data.size()); ++I)
    {
        // calculate position of object to render
        Vector2di pos;
        pos.x = (I % PREVIEW_COUNT_X) * size.x + (I % PREVIEW_COUNT_X + 1) * spacing.x;
        pos.y = OFFSET_Y + ((I - numOffset) / PREVIEW_COUNT_X) * size.y + ((I - numOffset) / PREVIEW_COUNT_X + 1) * spacing.y;

        SDL_mutexP(levelLock);
        if (data[I].hasBeenLoaded)
        {
            if (data[I].surface) // render preview
            {
                SDL_Rect rect = {pos.x,pos.y,0,0};
                SDL_BlitSurface(data[I].surface,NULL,target,&rect);
                SDL_mutexV(levelLock);
            }
            else // render error or locked image
            {
                SDL_mutexV(levelLock);
                if (state == lsLevel && exChapter && (I > maxUnlocked)) // locked
                {
                    // actually we need to add an offset here as we need to preserve the top-left
                    // corner (Penjin scales centred), but I rather scale the images properly
                    // than add code to compensate for my stupidity in that regard
                    locked.setPosition(pos);
                    locked.render(target);
                }
                else
                {
                    error.setPosition(pos);
                    error.render(target);
                }
            }
            if (not reachedLoaded)
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

    //#ifdef _DEBUG
    PreviewData bench = {BENCHMARK_LEVEL,NULL,false};
    levelPreviews.push_back(bench);
    //#endif

    // initialize map
    for (vector<string>::const_iterator file = files.begin(); file < files.end(); ++file)
    {
        PreviewData temp = {dir + (*file),NULL,false};
        levelPreviews.push_back(temp);
    }
    printf("%i files found in level directory\n",levelPreviews.size());

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
    for (vector<string>::iterator iter = files.begin(); iter != files.end();)
    {
        if ((*iter) == "." || (*iter) == ".." || (*iter) == dir)
            iter = files.erase(iter);
        else
            ++iter;
    }

    // add single level folder
    SDL_Surface* img = SURFACE_CACHE->loadSurface("images/general/levelfolder.png");
    if (img->w != size.x || img->h != size.y)
    {
        img = zoomSurface(img,(float)size.x / (float)img->w , (float)size.y / (float)img->h, SMOOTHING_OFF);
    }
    else
    {
        // remove image from cache to prevent double freeing
        SURFACE_CACHE->removeSurface("images/general/levelfolder.png",false);
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
    printf("%i chapters found\n",chapterPreviews.size()-1);

    abortChapterLoading = false;
    chapterThread = SDL_CreateThread(StateLevelSelect::loadChapterPreviews,this);
}

void StateLevelSelect::exploreChapter(CRstring filename)
{
    clearLevelListing();
    exChapter = new Chapter;

    if (not exChapter->loadFromFile(filename))
    {
        ENGINE->stateParameter = "ERROR loading chapter!";
        setNextState(STATE_ERROR);
    }

    for (vector<string>::const_iterator file = exChapter->levels.begin(); file != exChapter->levels.end(); ++file)
    {
        PreviewData temp = {exChapter->path + (*file),NULL,false};
        levelPreviews.push_back(temp);
    }
    printf("Chapter has %i levels\n",levelPreviews.size());

    abortLevelLoading = false;
    levelThread = SDL_CreateThread(StateLevelSelect::loadLevelPreviews,this);
}

/// ---protected---

void StateLevelSelect::switchState(const LevelSelectState& toState)
{
    if (toState != lsIntermediate && toState != lsIntermediateLevel)
    {
        title.setColour(WHITE);
        menu.setColour(BLACK);
        menu.setPosition(0,0);
        lastDraw = 0;
        firstDraw = true;
        SDL_FillRect(previewDraw, NULL, SDL_MapRGB(previewDraw->format,255,0,255));
    }

    if (toState == lsChapter)
    {
    	if (state == lsLevel)
		{
            selection = Vector2di(0,0);
            gridOffset = 0;
        }
        title.setPosition(0,OFFSET_Y - TITLE_FONT_SIZE);
        title.setAlignment(LEFT_JUSTIFIED);
    }
    else if (toState == lsLevel)
    {
    	if (state == lsIntermediate || state == lsChapter)
		{
            selection = Vector2di(0,0);
            gridOffset = 0;
        }
        title.setPosition(GFX::getXResolution(),OFFSET_Y - TITLE_FONT_SIZE);
        title.setAlignment(RIGHT_JUSTIFIED);
    }
    else if (toState == lsIntermediate || toState == lsIntermediateLevel)
    {
        overlay.render();
        SDL_BlitSurface(GFX::getVideoSurface(),NULL,previewDraw,NULL);
        intermediateSelection = 0;
        title.setAlignment(CENTRED);
    }

    state = toState;
}

void StateLevelSelect::checkSelection(const vector<PreviewData>& data, Vector2di& sel)
{
    if (sel.y < 0)
        sel.y = 0;
    else
    {
        int rows = ceil((float)data.size() / (float)PREVIEW_COUNT_X);
        if (sel.y >= rows)
		{
            sel.y = rows - 1;
            mouseInBounds = false;
		}
    }

    if (sel.x < 0)
        sel.x = 0;
    else
    {
        int cols = min(PREVIEW_COUNT_X,(int)data.size() - sel.y * PREVIEW_COUNT_X);
        if (sel.x >= cols)
		{
            sel.x = cols - 1;
            mouseInBounds = false;
		}
    }
}

void StateLevelSelect::checkGridOffset(const vector<PreviewData>& data, int& offset)
{
	if ( offset < 0 )
		offset = 0;
	else
	{
		int temp = max( 0.0f, ceil((float)data.size() / (float)PREVIEW_COUNT_X) - PREVIEW_COUNT_Y );
		if ( offset > temp )
			offset = temp;
	}
	if ( selection.y < offset )
		selection.y = offset;
	else if ( selection.y >= offset + PREVIEW_COUNT_Y )
		selection.y = offset + PREVIEW_COUNT_Y - 1;
}


int StateLevelSelect::loadLevelPreviews(void* data)
{
    printf("Generating level previews images\n");
    StateLevelSelect* self = (StateLevelSelect*)data;
    vector<PreviewData>::iterator iter = self->levelPreviews.begin();
    int levelNumber = 0;
    int maxUnlocked = 0;
    if (self->exChapter)
        maxUnlocked = self->exChapter->getProgress();

    while (not self->abortLevelLoading && iter != self->levelPreviews.end())
    {
    	map<string,SDL_Surface*>::iterator cachedImage = self->previewCache.find( (*iter).filename );
    	if ( cachedImage != self->previewCache.end() )
		{
            SDL_mutexP(self->levelLock);
            (*iter).surface = cachedImage->second;
            (*iter).hasBeenLoaded = true;
            SDL_mutexV(self->levelLock);
		}
		else
		{
			Level* level;
			if (self->exChapter) // we are browsing a chapter
			{
				// check whether the level has been unlocked, else don't load
				if (levelNumber <= maxUnlocked)
					level = LEVEL_LOADER->loadLevelFromFile(iter->filename,self->exChapter->path);
				else
					level = NULL;
			}
			else
				level = LEVEL_LOADER->loadLevelFromFile(iter->filename);
			if (level)
			{
				SDL_Surface* temp = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
				level->init();
				level->update(); // update once to properly position units
				level->render(temp); // render at full resolution
				// scale down, then delete full resolution copy
				SDL_Surface* surf = zoomSurface(temp,(float)self->size.x / GFX::getXResolution(), (float)self->size.y / GFX::getYResolution(), SMOOTHING_OFF);
				SDL_FreeSurface(temp);
				self->previewCache[iter->filename] = surf;
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
				(*iter).surface = NULL;
				(*iter).hasBeenLoaded = true;
				SDL_mutexV(self->levelLock);
				if (LEVEL_LOADER->errorString[0] != 0)
					printf("ERROR: %s\n",LEVEL_LOADER->errorString.c_str());
			}
			delete level;
		}
        ++iter;
        ++levelNumber;
    }
    if (self->abortLevelLoading)
    {
        printf("Aborted level preview generation\n");
        self->abortLevelLoading = false;
    }
    else
        printf("Finished generating level preview images\n");
    return 0;
}

int StateLevelSelect::loadChapterPreviews(void* data)
{
    StateLevelSelect* self = (StateLevelSelect*)data;

    printf("Loading chapter preview images\n");
    vector<PreviewData>::iterator iter = self->chapterPreviews.begin();
    ++iter; // skip level folder
    Chapter chapter;

    while (not self->abortChapterLoading && iter != self->chapterPreviews.end())
    {
    	map<string,SDL_Surface*>::iterator cachedImage = self->previewCache.find( iter->filename );

    	if ( cachedImage != self->previewCache.end() )
		{
			SDL_mutexP(self->chapterLock);
			(*iter).surface = cachedImage->second;
			(*iter).hasBeenLoaded = true;
			SDL_mutexV(self->chapterLock);
		}
		else
		{
			if (not chapter.loadFromFile(iter->filename))
			{
				// oops, we have error
				SDL_mutexP(self->chapterLock);
				(*iter).hasBeenLoaded = true;
				SDL_mutexV(self->chapterLock);
				printf("ERROR: %s\n",chapter.errorString.c_str());
				++iter;
				continue;
			}

			// loaded chapter fine
			if (chapter.imageFile[0] != 0) // we have an image file
			{
				SDL_Surface* img = SURFACE_CACHE->loadSurface(chapter.imageFile,chapter.path,true);
				if (img) // load successful
				{
					if (img->w != self->size.x || img->h != self->size.y) // scale if needed
					{
						img = zoomSurface(img,(float)self->size.x / (float)img->w , (float)self->size.y / (float)img->h, SMOOTHING_OFF);
					}
					else // remove the surface from the cache to prevent double freeing
					{
						SURFACE_CACHE->removeSurface(chapter.imageFile,false);
						SURFACE_CACHE->removeSurface(chapter.path + chapter.imageFile,false);
					}
					self->previewCache[iter->filename] = img;
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
			self->previewCache[iter->filename] = surf;
			SDL_mutexP(self->chapterLock);
			(*iter).surface = surf;
			(*iter).hasBeenLoaded = true;
			SDL_mutexV(self->chapterLock);
		}
        ++iter;
    }
    if (self->abortChapterLoading)
        printf("Aborted chapter preview generation\n");
    else
        printf("Finished generating chapter preview images\n");
    return 0;
}
