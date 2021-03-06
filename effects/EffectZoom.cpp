/*
	Greyout - a colourful platformer about love

	Greyout is Copyright (c)2011-2014 Janek Schäfer

	This file is part of Greyout.

	Greyout is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Greyout is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Please direct any feedback, questions or comments to
	Janek Schäfer (foxblock), foxblock_at_gmail_dot_com
*/

#include "EffectZoom.h"

EffectZoom::EffectZoom(CRint time, const Vector2df& pos, const Vector2df& newSize, const Colour& col, CRbool inverted) : BaseEffect()
{
	if (pos.x < 0 || pos.x > GFX::getXResolution() || pos.y < 0 || pos.y > GFX::getYResolution())
		printf("WARNING: Position passed to EffectZoom is out of bounds (only a position on the screen makes sense here): %f.2,%f.2\n",pos.x,pos.y);

	surf = SDL_CreateRGBSurface(SDL_SWSURFACE,GFX::getXResolution(),GFX::getYResolution(),GFX::getVideoSurface()->format->BitsPerPixel,0,0,0,0);
	if (not inverted)
	{
		if (col != MAGENTA)
		{
			rect.setColour(MAGENTA);
			SDL_SetColorKey(surf, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(GFX::getVideoSurface()->format,255,0,255));
		}
		else
		{
			rect.setColour(GREEN);
			SDL_SetColorKey(surf, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(GFX::getVideoSurface()->format,0,255,0));
		}
		zoomCol = col;
		if (time > 0) // zoom in
		{
			rect.setDimensions(GFX::getXResolution(), GFX::getYResolution());
			rect.setPosition(0,0);
		}
		else // zoom out
		{
			rect.setDimensions(0,0);
			rect.setPosition(pos);
		}
	}
	else
	{
		rect.setColour(col);
		if (col != MAGENTA)
		{
			SDL_SetColorKey(surf, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(GFX::getVideoSurface()->format,255,0,255));
			zoomCol = MAGENTA;
		}
		else
		{
			SDL_SetColorKey(surf, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(GFX::getVideoSurface()->format,0,255,0));
			zoomCol = GREEN;
		}
		if (time > 0) // zoom in
		{
			rect.setDimensions(GFX::getXResolution(), GFX::getYResolution());
			rect.setPosition(0,0);
		}
		else // zoom out
		{
			rect.setDimensions(0,0);
			rect.setPosition(pos);
		}
	}

	zoomTime = time;
	position = pos;
	size = newSize;
	type = etZoom;

	timer = abs(time);
}

EffectZoom::~EffectZoom()
{
	SDL_FreeSurface(surf);
}

void EffectZoom::update()
{
	if (timer > 0)
	{
		// calculate factor to determine scaling (dependant on position in zoom)
		// preserve aspect ratio of given size
		float factor = 1.0f;
		if (zoomTime > 0) // zoom in
			factor = max(max(position.x, (float)GFX::getXResolution() - position.x) / size.x, max(position.y, (float)GFX::getYResolution() - position.y) / size.y) * ((float)timer / (float)zoomTime);
		else
			factor = max(max(position.x, (float)GFX::getXResolution() - position.x) / size.x, max(position.y, (float)GFX::getYResolution() - position.y) / size.y) * (1.0f + (float)timer / (float)zoomTime);
		float halfSizeX = size.x * factor;
		float halfSizeY = size.y * factor;
		float diffX[2] = {max(0 - (position.x - halfSizeX),0.0f), max(position.x + halfSizeX - GFX::getXResolution(),0.0f) }; // > 0 when out of screen's bounds to cut off part which is not shown anyway
		float diffY[2] = {max(0 - (position.y - halfSizeY),0.0f), max(position.y + halfSizeY - GFX::getYResolution(),0.0f) };

		rect.setDimensions(halfSizeX * 2 - diffX[0] - diffX[1], halfSizeY * 2 - diffY[0] - diffY[1]);
		rect.setPosition(position.x - halfSizeX + diffX[0], position.y - halfSizeY + diffY[0]);
		--timer;
	}
	else
		finished = true;
}

void EffectZoom::render()
{
	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = GFX::getXResolution();
	area.h = GFX::getYResolution();
	SDL_FillRect(surf,&area,zoomCol.getSDL_Uint32Colour(surf));
	rect.render(surf);
	SDL_BlitSurface(surf,&area,GFX::getVideoSurface(),&area);
}
