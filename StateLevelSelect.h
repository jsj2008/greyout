#ifndef STATELEVELSELECT_H
#define STATELEVELSELECT_H

#include <vector>
#include <SDL/SDL_mutex.h>
#include <SDL/SDL_thread.h>

#include "BaseState.h"
#include "AnimatedSprite.h"
#include "Rectangle.h"
#include "Vector2di.h"
#include "FileLister.h"
#include "Text.h"

#include "Chapter.h"

/**
Displays chapters and levels graphically
Chapters will be displayed by an image specified in the chapter file or fallback text
Levels will be displayed by rendering of the actual level
Currently there is no overflow detection present so this might crash due to memory
limits when more than 100 maps are present (depending on size of memory)
**/

struct PreviewData;

class StateLevelSelect : public BaseState
{
    public:
        StateLevelSelect();
        virtual ~StateLevelSelect();

        virtual void init();
        virtual void clear();
        virtual void clearLevelListing();
        virtual void clearChapterListing();

        virtual void userInput();
        virtual void update();
        virtual void render();

        // renders the passed preview images (either chapters or levels) to a
        // target surface, addOffset is the number of preview images to skip
        // rendering (which have been rendered in the last cycle to speed things up)
        virtual void renderPreviews(const vector<PreviewData>& data, SDL_Surface* const target, CRint addOffset);

        // set the browsing directories for chapters and levels
        // setChapterDirectory will be called upon entering this state,
        // setLevelDirectory will be called when selecting the level folder item
        // these functions use FileLister objects to fill the appropriate vectors with data
        virtual void setLevelDirectory(CRstring dir);
        virtual void setChapterDirectory(CRstring dir);

        // similar to setLevelDirectory, but takes the currently selected chapter
        // to fill the data vector
        virtual void exploreChapter(CRstring filename);

        enum LevelSelectState
        {
            lsChapter,
            lsIntermediate, // when selecting a chapter and chosing between play and explore
            lsIntermediateLevel, // selecting play or speedrun on a level
            lsLevel
        };
        LevelSelectState state;

    protected:
        // switches between states and resets certain variables
        void switchState(const LevelSelectState& toState);

        // checks whether the passed selection is valid in the context of the
        // currently displayed data (i.e. it does not go out of bounds)
        void checkSelection(const vector<PreviewData>& data, Vector2di& sel);

        // Thread functions, which load SDL_Surfaces* from chapter or level
        // data to display the preview images and save them in the vectors
        static int loadLevelPreviews(void* data);
        static int loadChapterPreviews(void* data);

        AnimatedSprite bg;
        AnimatedSprite error;
        AnimatedSprite loading;
        AnimatedSprite locked;
        AnimatedSprite arrows;
        Rectangle cursor;
        Rectangle menu;
        Rectangle overlay;
        // preview images will be drawn onto this surface for less redraws needed
        SDL_Surface* previewDraw;
        #ifdef _DEBUG
        Text fpsDisplay;
        #endif
        Text imageText; // fallback text when encountering chapter without image
        Text title;

        // map<filename,pair<surface, hasbeenloaded>
        // if hasBeenLoaded is true but surface is NULL an error occurred while loading
        vector<PreviewData> levelPreviews;
        vector<PreviewData> chapterPreviews;

        // mutex to prevent sharing violations between main and loading thread
        SDL_mutex* levelLock;
        SDL_Thread* levelThread;
        bool abortLevelLoading; // if true level preview generation will exit

        SDL_mutex* chapterLock;
        SDL_Thread* chapterThread;
        bool abortChapterLoading; // if true chapter preview generation will exit

        Vector2di size;
        Vector2di spacing;
        Vector2di selection;
        int intermediateSelection;
        int gridOffset; // the topmost column to render (=offset)
        int lastDraw; // indicates the last drawn preview image to speed up rendering
        bool firstDraw; // if true the all preview images will be rendered (also set on scrolling)
        Vector2di lastPos, mousePos;

        FileLister levelLister;
        FileLister dirLister;
        // the currently active chapter (or NULL if level folder)
        Chapter* exChapter;
    private:

};

#endif // STATELEVELSELECT_H



