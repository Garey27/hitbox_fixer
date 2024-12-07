//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vgui_controls/subrectimage.h"
#include "tier0/dbg.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSubRectImage::CSubRectImage( const char *filename, bool hardwareFiltered, int subx, int suby, int subw, int subh )
{
	SetSize( subw, subh );
	sub[ 0 ] = subx;
	sub[ 1 ] = suby;
	sub[ 2 ] = subw;
	sub[ 3 ] = subh;

	_filtered = hardwareFiltered;
	// HACKHACK - force VGUI materials to be in the vgui/ directory
	//			 This needs to be revisited once GoldSRC is grandfathered off.
	//!! need to make this work with goldsrc
	int size = strlen(filename) + 1 + strlen("vgui/");
	_filename = (char *)malloc( size );
	Assert( _filename );

	Q_snprintf( _filename, size, "vgui/%s", filename );

	_id = 0;
	_uploaded = false;
	_color = Color(255, 255, 255, 255);
	_pos[0] = _pos[1] = 0;
	_valid = true;
	_wide = subw;
	_tall = subh;
	ForceUpload();
}

CSubRectImage::~CSubRectImage()
{
	if ( _filename )
	{
		free( _filename );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CSubRectImage::GetSize(int &wide, int &tall)
{
	wide = _wide;
	tall = _tall;
}

//-----------------------------------------------------------------------------
// Purpose: size of the bitmap
//-----------------------------------------------------------------------------
void CSubRectImage::GetContentSize(int &wide, int &tall)
{
	wide = 0;
	tall = 0;

	if (!_valid)
		return;

	surface()->DrawGetTextureSize(_id, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: ignored
//-----------------------------------------------------------------------------
void CSubRectImage::SetSize(int x, int y)
{
	_wide = x;
	_tall = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CSubRectImage::SetPos(int x, int y)
{
	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CSubRectImage::SetColor(Color col)
{
	_color = col;
}

//-----------------------------------------------------------------------------
// Purpose: returns the file name of the bitmap
//-----------------------------------------------------------------------------
const char *CSubRectImage::GetName()
{
	return _filename;
}

//-----------------------------------------------------------------------------
// Purpose: Renders the loaded image, uploading it if necessary
//			Assumes a valid image is always returned from uploading
//-----------------------------------------------------------------------------
void CSubRectImage::Paint()
{
	if ( !_valid )
		return;

	// if we don't have an _id then lets make one
	if ( !_id )
	{
		_id = surface()->CreateNewTextureID();
	}

	// if we have not uploaded yet, lets go ahead and do so
	if ( !_uploaded )
	{
		ForceUpload();
	}

	// set the texture current, set the color, and draw the biatch
	surface()->DrawSetColor( _color[0], _color[1], _color[2], _color[3] );
	surface()->DrawSetTexture( _id );

	if ( _wide == 0 || _tall == 0 )
		return;

	int cw, ch;
	GetContentSize( cw, ch );
	if ( cw == 0 || ch == 0 )
		return;

	float s[ 2 ];
	float t[ 2 ];

	s[ 0 ] = (float)sub[ 0 ] / (float)cw;
	s[ 1 ] = (float)(sub[ 0 ]+sub[ 2 ]) / (float)cw;
	t[ 0 ] = (float)sub[ 1 ] / (float)ch;
	t[ 1 ] = (float)(sub[ 1 ]+sub[ 3 ]) / (float)ch;
	surface()->DrawTexturedSubRect(
		_pos[0], 
		_pos[1], 
		_pos[0] + _wide, 
		_pos[1] + _tall,
		s[ 0 ], 
		t[ 0 ], 
		s[ 1 ], 
		t[ 1 ] );
}

//-----------------------------------------------------------------------------
// Purpose: ensures the bitmap has been uploaded
//-----------------------------------------------------------------------------
void CSubRectImage::ForceUpload()
{
	if ( !_valid || _uploaded )
		return;

	if ( !_id )
	{
		_id = surface()->CreateNewTextureID( false );
	}

	surface()->DrawSetTextureFile( _id, _filename, _filtered, false );

	_uploaded = true;

	_valid = surface()->IsTextureIDValid( _id );
}


//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
HTexture CSubRectImage::GetID()
{
	return _id;
}

bool CSubRectImage::IsValid()
{
	return _valid;
}

