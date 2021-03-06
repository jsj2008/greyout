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

#include "BasePlayer.h"

#include "Level.h"

#define JUMP_TO_FALL_FRAMES 30
#define WALK_TO_FALL_FRAMES 10

BasePlayer::BasePlayer(Level* newParent) : ControlUnit(newParent)
{
	canJump = false;
	isJumping = false;
	flags.addFlag(ufMissionObjective);
	fallCounter = 0;
	activelyMoving = false;
}

BasePlayer::~BasePlayer()
{
	//
}

///---public---

// Custom load implementation to ensure the unit is initialized properly after loading
bool BasePlayer::load(list<PARAMETER_TYPE >& params)
{
	bool result = BaseUnit::load(params);

	if (imageOverwrite[0] == 0)
	{
		imageOverwrite = "images/player/black_big.png";
	}

	if (startingState[0] == 0 || startingState == "default")
	{
		if (takesControl)
			startingState = "wave";
		else
			startingState = "stand";
	}
	setSpriteState(startingState,true);

	return result;
}

void BasePlayer::resetTemporary()
{
	BaseUnit::resetTemporary();

	if (currentSprite->getLoops() == -1 || currentSprite->hasFinished())
	{
		setSpriteState("stand");
	}
}

void BasePlayer::update()
{
	if (collisionInfo.correction.y < 0) // on the ground
	{
		if (currentState == "stand")
		{
			if (velocity.x < 0.0f)
				setSpriteState("runleft");
			else if (velocity.x > 0.0f)
				setSpriteState("runright");
		}
	}
	else // air
	{
		if ( fallCounter > 0 )
			--fallCounter;
		if (currentState == "stand")
		{
			if (fallCounter == 0)
			{
				if (direction < 0)
					setSpriteState("fallleft");
				else
					setSpriteState("fallright");
			}
			else
			{
				if (isJumping)
				{
					if (direction < 0)
						setSpriteState("flyleft");
					else
						setSpriteState("flyright");
				}
				else
				{
					if (velocity.x < 0.0f)
						setSpriteState("runleft");
					else if (velocity.x > 0.0f)
						setSpriteState("runright");
				}
			}
		}
	}

	BaseUnit::update();

	if ((int)velocity.y != 0)
		canJump = false;
}

void BasePlayer::hitMap(const Vector2df& correctionOverride)
{
	BaseUnit::hitMap(correctionOverride);

	if (correctionOverride.y < 0.0f)
	{
		isJumping = false;
		fallCounter = 0;
	}
	else if (correctionOverride.y > 0.0f)
	{
		acceleration[0].y = 0;
		acceleration[1].y = 0;
	}
	else if (lastYCorrection != 0.0f && !isJumping)
	{
		fallCounter = WALK_TO_FALL_FRAMES;
	}
	lastYCorrection = correctionOverride.y;
}


void BasePlayer::control(SimpleJoy* input)
{
	if (!input)
		return;

	if (input->isLeft() && !input->isRight())
	{
		if ((direction > 0 || velocity.x == 0.0f) && ((int)velocity.y == 0))
		{
			setSpriteState("turnleft",true);
			states["runleft"]->rewind(); // reset running anim to match end of turn
		}
		if (velocity.x > 0.0f)
		{
			acceleration[0].x = -0.75;
		}
		else
		{
			acceleration[0].x = -0.25;
		}
		acceleration[1].x = -4;
		activelyMoving = true;
	}
	else if (input->isRight() && !input->isLeft())
	{
		if ((direction < 0 || velocity.x == 0.0f) && ((int)velocity.y == 0))
		{
			setSpriteState("turnright",true);
			states["runright"]->rewind(); // reset running anim to match end of turn
		}
		if (velocity.x < 0.0f)
		{
			acceleration[0].x = 0.75;
		}
		else
		{
			acceleration[0].x = 0.25;
		}
		acceleration[1].x = 4;
		activelyMoving = true;
	}
	else
	{
		activelyMoving = false;
		if (canJump && currentState != "wave")
		{
			setSpriteState("stand");
		}
		if ((int)collisionInfo.correction.y != 0)
		{
			acceleration[0].x = NumberUtility::sign(velocity.x) * -0.5f;
			acceleration[1].x = 0;
			//velocity.x = 0;
		}
		else
		{
			acceleration[0].x = (float)NumberUtility::sign(velocity.x) * -0.25f;
			acceleration[1].x = 0;
		}
	}
	if ((input->isB() || input->isY()) && canJump && !isJumping)
	{
		acceleration[0].y = -4;
		acceleration[1].y = -9;
		//velocity.y = -10;
		canJump = false;
		isJumping = true;
		if (direction < 0)
			setSpriteState("jumpleft",true);
		else
			setSpriteState("jumpright",true);
		fallCounter = JUMP_TO_FALL_FRAMES;
	}
	if (!(input->isB() || input->isY()))
		canJump = true;
}

#ifdef _DEBUG
string BasePlayer::debugInfo()
{
	string result = BaseUnit::debugInfo();
	result += StringUtility::boolToString(activelyMoving) + "\n";
	result += StringUtility::intToString(fallCounter) + "\n";
	return result;
}
#endif
