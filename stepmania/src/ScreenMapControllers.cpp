#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: ScreenMapControllers

 Desc: Where the player maps device input to pad input.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "ScreenMapControllers.h"
#include "PrefsManager.h"
#include "ScreenManager.h"
#include "GameConstantsAndTypes.h"
#include "PrefsManager.h"
#include "RageLog.h"
#include "InputMapper.h"
#include "GameManager.h"
#include "GameState.h"
#include "RageSounds.h"
#include "ThemeManager.h"
#include "RageDisplay.h"


#define EVEN_LINE_IN		THEME->GetMetric("ScreenMapControllers","EvenLineIn")
#define EVEN_LINE_OUT		THEME->GetMetric("ScreenMapControllers","EvenLineOut")
#define ODD_LINE_IN			THEME->GetMetric("ScreenMapControllers","OddLineIn")
#define ODD_LINE_OUT		THEME->GetMetric("ScreenMapControllers","OddLineOut")

const int FramesToWaitForInput = 2;

// reserve the 3rd slot for hard-coded keys
const int NUM_CHANGABLE_SLOTS = NUM_GAME_TO_DEVICE_SLOTS-1;


const float LINE_START_Y	=	64;
const float LINE_GAP_Y		=	28;
const float BUTTON_COLUMN_X[NUM_GAME_TO_DEVICE_SLOTS*MAX_GAME_CONTROLLERS] =
{
	50, 125, 200, 440, 515, 590 
};


ScreenMapControllers::ScreenMapControllers( CString sClassName ) : Screen( sClassName )
{
	LOG->Trace( "ScreenMapControllers::ScreenMapControllers()" );
	
	for( int b=0; b<GAMESTATE->GetCurrentGameDef()->m_iButtonsPerController; b++ )
	{
		CString sName = GAMESTATE->GetCurrentGameDef()->m_szButtonNames[b];
		CString sSecondary = GAMESTATE->GetCurrentGameDef()->m_szSecondaryFunction[b];

		m_textName[b].LoadFromFont( THEME->GetPathToF("Common title") );
		m_textName[b].SetXY( CENTER_X, -6 );
		m_textName[b].SetText( sName );
		m_textName[b].SetZoom( 0.7f );
		m_textName[b].SetShadowLength( 2 );
		m_Line[b].AddChild( &m_textName[b] );

		m_textName2[b].LoadFromFont( THEME->GetPathToF("Common title") );
		m_textName2[b].SetXY( CENTER_X, +6 );
		m_textName2[b].SetText( sSecondary );
		m_textName2[b].SetZoom( 0.5f );
		m_textName2[b].SetShadowLength( 2 );
		m_Line[b].AddChild( &m_textName2[b] );

		for( int p=0; p<MAX_GAME_CONTROLLERS; p++ ) 
		{			
			for( int s=0; s<NUM_GAME_TO_DEVICE_SLOTS; s++ ) 
			{
				m_textMappedTo[p][b][s].LoadFromFont( THEME->GetPathToF("ScreenMapControllers entry") );
				m_textMappedTo[p][b][s].SetXY( BUTTON_COLUMN_X[p*NUM_GAME_TO_DEVICE_SLOTS+s], 0 );
				m_textMappedTo[p][b][s].SetZoom( 0.5f );
				m_textMappedTo[p][b][s].SetShadowLength( 0 );
				m_Line[b].AddChild( &m_textMappedTo[p][b][s] );
			}
		}
		m_Line[b].SetY( LINE_START_Y + b*LINE_GAP_Y );
		this->AddChild( &m_Line[b] );

		m_Line[b].Command( (b%2)? ODD_LINE_IN:EVEN_LINE_IN );
	}	

	m_textError.LoadFromFont( THEME->GetPathToF("Common normal") );
	m_textError.SetText( "" );
	m_textError.SetXY( CENTER_X, CENTER_Y );
	m_textError.SetDiffuse( RageColor(0,1,0,0) );
	m_textError.SetZoom( 0.8f );
	this->AddChild( &m_textError );


	m_iCurController = 0;
	m_iCurButton = 0;
	m_iCurSlot = 0;

	m_iWaitingForPress = 0;

	m_Menu.Load( "ScreenMapControllers" );
	this->AddChild( &m_Menu );

	SOUND->PlayMusic( THEME->GetPathToS("ScreenMapControllers music") );

	Refresh();
}



ScreenMapControllers::~ScreenMapControllers()
{
	LOG->Trace( "ScreenMapControllers::~ScreenMapControllers()" );
}


void ScreenMapControllers::Update( float fDeltaTime )
{
	Screen::Update( fDeltaTime );

	
	if( m_iWaitingForPress  &&  m_DeviceIToMap.IsValid() )	// we're going to map an input
	{	
		--m_iWaitingForPress;
		if( m_iWaitingForPress )
			return; /* keep waiting */

		GameInput curGameI( (GameController)m_iCurController,
							(GameButton)m_iCurButton );

		INPUTMAPPER->SetInputMap( m_DeviceIToMap, curGameI, m_iCurSlot );
		INPUTMAPPER->AddDefaultMappingsForCurrentGameIfUnmapped();
		// commit to disk so we don't lose the changes!
		INPUTMAPPER->SaveMappingsToDisk();

		Refresh();
	}
}


void ScreenMapControllers::DrawPrimitives()
{
	m_Menu.DrawBottomLayer();
	Screen::DrawPrimitives();
	m_Menu.DrawTopLayer();
}

static bool IsAxis( const DeviceInput& DeviceI )
{
	if( !DeviceI.IsJoystick() )
		return false;

	static int axes[] = 
	{
		JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN,
		JOY_Z_UP, JOY_Z_DOWN,
		JOY_ROT_UP, JOY_ROT_DOWN, JOY_ROT_LEFT, JOY_ROT_RIGHT, JOY_ROT_Z_UP, JOY_ROT_Z_DOWN,
		JOY_HAT_LEFT, JOY_HAT_RIGHT, JOY_HAT_UP, JOY_HAT_DOWN, 
		JOY_AUX_1, JOY_AUX_2, JOY_AUX_3, JOY_AUX_4,
		-1
	};

	for( int ax = 0; axes[ax] != -1; ++ax )
		if( DeviceI.button == axes[ax] )
			return true;

	return false;
}

void ScreenMapControllers::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	if( type != IET_FIRST_PRESS && type != IET_SLOW_REPEAT )
		return;	// ignore

	LOG->Trace( "ScreenMapControllers::Input():  device: %d, button: %d", 
		DeviceI.device, DeviceI.button );

	//
	// TRICKY:  This eliminates the need for a separate "ignore joy axes"
	// preference.  Some adapters map the PlayStation digital d-pad to
	// both axes and buttons.  We want buttons to be used for 
	// any mappings where possible because presses of buttons aren't mutually 
	// exclusive and presses of axes are (e.g. can't read presses of both Left and 
	// Right simultaneously).  So, when the user presses a button, we'll wait
	// until the next Update before adding a mapping so that we get a chance 
	// to see all input events the user's press of a panel.  This screen will be
	// receive input events for joystick axes presses first, then the input events 
	// for button presses.  We'll use the last input event received in the same 
	// Update so that a button presses are favored for mapping over axis presses.
	//

	/* We can't do that: it assumes that button presses are always received after
	 * corresponding axis events.  We need to check and explicitly prefer non-axis events
	 * over axis events. */
	if( m_iWaitingForPress )
	{
		/* Don't allow function keys to be mapped. */
		if ( DeviceI.device == DEVICE_KEYBOARD && (DeviceI.button >= SDLK_F1 && DeviceI.button <= SDLK_F12) )
		{
			m_textError.SetText( "That key can not be mapped." );
			SOUND->PlayOnce( THEME->GetPathToS("Common invalid" ) );
			m_textError.StopTweening();
			m_textError.SetDiffuse( RageColor(0,1,0,1) );
			m_textError.BeginTweening( 3 );
			m_textError.BeginTweening( 1 );
			m_textError.SetDiffuse( RageColor(0,1,0,0) );
		}
		else
		{
			if( m_DeviceIToMap.IsValid() &&
				!IsAxis(m_DeviceIToMap) &&
				IsAxis(DeviceI) )
			{
				LOG->Trace("Ignored input; non-axis event already received");
				return;	// ignore this press
			}

			m_DeviceIToMap = DeviceI;
		}
	}
	else if( DeviceI.device == DEVICE_KEYBOARD )
	{
		switch( DeviceI.button )
		{
		/* We only advertise space as doing this, but most games
		 * use either backspace or delete, and I find them more
		 * intuitive, so allow them, too. -gm */
		case SDLK_SPACE:
		case SDLK_DELETE:
		case SDLK_BACKSPACE: /* Clear the selected input mapping. */
			{
				GameInput curGameI( (GameController)m_iCurController, (GameButton)m_iCurButton );
				INPUTMAPPER->ClearFromInputMap( curGameI, m_iCurSlot );
				INPUTMAPPER->AddDefaultMappingsForCurrentGameIfUnmapped();
				// commit to disk so we don't lose the changes!
				INPUTMAPPER->SaveMappingsToDisk();
			}
			break;
		case SDLK_LEFT: /* Move the selection left, wrapping up. */
			if( m_iCurSlot == 0 && m_iCurController == 0 )
				break;	// can't go left any more
			m_iCurSlot--;
			if( m_iCurSlot < 0 )
			{
				m_iCurSlot = NUM_CHANGABLE_SLOTS-1;
				m_iCurController--;
			}
			break;
		case SDLK_RIGHT:	/* Move the selection right, wrapping down. */
			if( m_iCurSlot == NUM_CHANGABLE_SLOTS-1 && m_iCurController == MAX_GAME_CONTROLLERS-1 )
				break;	// can't go right any more
			m_iCurSlot++;
			if( m_iCurSlot > NUM_CHANGABLE_SLOTS-1 )
			{
				m_iCurSlot = 0;
				m_iCurController++;
			}
			break;
		case SDLK_UP: /* Move the selection up. */
			if( m_iCurButton == 0 )
				break;	// can't go up any more
			m_iCurButton--;
			break;
		case SDLK_DOWN: /* Move the selection down. */
			if( m_iCurButton == GAMESTATE->GetCurrentGameDef()->m_iButtonsPerController-1 )
				break;	// can't go down any more
			m_iCurButton++;
			break;
		case SDLK_ESCAPE: /* Quit the screen. */
			if(!m_Menu.IsTransitioning())
			{
				SOUND->PlayOnce( THEME->GetPathToS("Common start") );
				m_Menu.StartTransitioning( SM_GoToNextScreen );		
				for( int b=0; b<GAMESTATE->GetCurrentGameDef()->m_iButtonsPerController; b++ )
					m_Line[b].Command( (b%2)? ODD_LINE_OUT:EVEN_LINE_OUT );
			}
			break;
		case SDLK_RETURN: /* Change the selection. */
		case SDLK_KP_ENTER:
			m_iWaitingForPress = FramesToWaitForInput;
			m_DeviceIToMap.MakeInvalid();
			break;
		}
	}

//	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );	// default handler

	LOG->Trace( "m_iCurSlot: %d m_iCurController: %d m_iCurButton: %d", m_iCurSlot, m_iCurController, m_iCurButton );

	Refresh();
}

void ScreenMapControllers::HandleScreenMessage( const ScreenMessage SM )
{
	switch( SM )
	{
	case SM_GoToNextScreen:
		SCREENMAN->SetNewScreen( "ScreenOptionsMenu" );
		break;
	}
}

void ScreenMapControllers::Refresh()
{
	for( int p=0; p<MAX_GAME_CONTROLLERS; p++ ) 
	{			
		for( int b=0; b<GAMESTATE->GetCurrentGameDef()->m_iButtonsPerController; b++ ) 
		{
			for( int s=0; s<NUM_GAME_TO_DEVICE_SLOTS; s++ ) 
			{
				bool bSelected = p == m_iCurController  &&  b == m_iCurButton  &&  s == m_iCurSlot; 

				GameInput cur_gi( (GameController)p, (GameButton)b );
				DeviceInput di;
				if( INPUTMAPPER->GameToDevice( cur_gi, s, di ) )
					m_textMappedTo[p][b][s].SetText( di.GetDescription() );
				else
					m_textMappedTo[p][b][s].SetText( "-----------" );
				
				// highlight the currently selected pad button
				RageColor color;
				bool bPulse;
				if( bSelected ) 
				{
					if( m_iWaitingForPress )
					{
						color = RageColor(1,0.5,0.5,1);	// red
						bPulse = true;
					}
					else
					{
						color = RageColor(1,1,1,1);		// white
						bPulse = false;
					}
				} 
				else 
				{
					color = RageColor(0.5,0.5,0.5,1);	// gray
					bPulse = false;
				}
				m_textMappedTo[p][b][s].SetDiffuse( color );
				if( bPulse )
					m_textMappedTo[p][b][s].SetEffectPulse( .5f, .5f, .6f );
				else
					m_textMappedTo[p][b][s].SetEffectNone();
			}
		}
	}
}
