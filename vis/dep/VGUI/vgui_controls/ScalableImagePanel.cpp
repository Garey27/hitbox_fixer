//========= Copyright ?1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>

#include <vgui/IBorder.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IBorder.h>
#include <KeyValues.h>

#include <vgui_controls/ScalableImagePanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( ScalableImagePanel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ScalableImagePanel::ScalableImagePanel(Panel *parent, const char *name) : Panel(parent, name)
{
	m_iSrcCornerHeight = 0;
	m_iSrcCornerWidth = 0;

	m_iCornerHeight = 0;
	m_iCornerWidth = 0;

	m_pszImageName = NULL;

	m_iTextureID = surface()->CreateNewTextureID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImagePanel::SetImage(const char *imageName)
{
	delete [] m_pszImageName;
	m_pszImageName = NULL;

	if (*imageName)
	{
		int len = Q_strlen(imageName) + 1 + 9;	// 9 for "gfx/vgui/"
		delete [] m_pszImageName;
		m_pszImageName = new char[ len ];
		Q_snprintf( m_pszImageName, len, "gfx/vgui/%s", imageName );
		InvalidateLayout();
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScalableImagePanel::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);

	surface()->DrawSetColor( 255, 255, 255, GetAlpha() );
	surface()->DrawSetTexture( m_iTextureID );

	int x = 0;
	int y = 0;

	float uvx = 0;
	float uvy = 0;
	float uvw, uvh;

	float drawW, drawH;

	int row, col;
	for ( row=0;row<3;row++ )
	{
		x = 0;
		uvx = 0;

		if ( row == 0 || row == 2 )
		{
			//uvh - row 0 or 2, is src_corner_height
			uvh = m_flCornerHeightPercent;
			drawH = m_iCornerHeight;
		}
		else
		{
			//uvh - row 1, is tall - ( 2 * src_corner_height ) ( min 0 )
			uvh = max( 1.0 - 2 * m_flCornerHeightPercent, 0.0f );
			drawH = max( 0, ( tall - 2 * m_iCornerHeight ) );
		}

		for ( col=0;col<3;col++ )
		{
			if ( col == 0 || col == 2 )
			{
				//uvw - col 0 or 2, is src_corner_width
				uvw = m_flCornerWidthPercent;
				drawW = m_iCornerWidth;
			}
			else
			{
				//uvw - col 1, is wide - ( 2 * src_corner_width ) ( min 0 )
				uvw = max( 1.0 - 2 * m_flCornerWidthPercent, 0.0f );
				drawW = max( 0, ( wide - 2 * m_iCornerWidth ) );
			}

			float verts[4][4];

			verts[0][0] = x;
			verts[0][1] = y;
			verts[0][2] = uvx;
			verts[0][3] = uvy;

			verts[1][0] = x + drawW;
			verts[1][1] = y;
			verts[1][2] = uvx + uvw;
			verts[1][3] = uvy;

			verts[2][0] = x + drawW;
			verts[2][1] = y + drawH;
			verts[2][2] = uvx + uvw;
			verts[2][3] = uvy + uvh;

			verts[3][0] = x;
			verts[3][1] = y + drawH;
			verts[3][2] = uvx;
			verts[3][3] = uvy + uvh;

			vgui::surface()->DrawTexturedPolygon( 4, (float *)verts );	

			x += drawW;
			uvx += uvw;
		}

		y += drawH;
		uvy += uvh;
	}

	vgui::surface()->DrawSetTexture(0);
}

//-----------------------------------------------------------------------------
// Purpose: Gets control settings for editing
//-----------------------------------------------------------------------------
void ScalableImagePanel::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);

	outResourceData->SetInt("src_corner_height", m_iSrcCornerHeight);
	outResourceData->SetInt("src_corner_width", m_iSrcCornerWidth);

	outResourceData->SetInt("draw_corner_height", m_iCornerHeight);
	outResourceData->SetInt("draw_corner_width", m_iCornerWidth);

	if (m_pszImageName)
	{
		outResourceData->SetString("image", m_pszImageName);
	}	
}

//-----------------------------------------------------------------------------
// Purpose: Applies designer settings from res file
//-----------------------------------------------------------------------------
void ScalableImagePanel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	m_iSrcCornerHeight = inResourceData->GetInt( "src_corner_height" );
	m_iSrcCornerWidth = inResourceData->GetInt( "src_corner_width" );

	m_iCornerHeight = inResourceData->GetInt( "draw_corner_height" );
	m_iCornerWidth = inResourceData->GetInt( "draw_corner_width" );

	if ( IsProportional() )
	{
		// scale the x and y up to our screen co-ords
		m_iCornerHeight = scheme()->GetProportionalScaledValueEx(GetScheme(), m_iCornerHeight);
		m_iCornerWidth = scheme()->GetProportionalScaledValueEx(GetScheme(), m_iCornerWidth);
	}

	const char *imageName = inResourceData->GetString("image", "");
	SetImage( imageName );

	InvalidateLayout();
}

void ScalableImagePanel::PerformLayout( void )
{
	if ( m_pszImageName )
	{
		surface()->DrawSetTextureFile( m_iTextureID, m_pszImageName, true, false);
	}

	// get image dimensions, compare to m_iSrcCornerHeight, m_iSrcCornerWidth
	int wide,tall;
	surface()->DrawGetTextureSize( m_iTextureID, wide, tall );

	m_flCornerWidthPercent = ( wide > 0 ) ? ( (float)m_iSrcCornerWidth / (float)wide ) : 0;
	m_flCornerHeightPercent = ( tall > 0 ) ? ( (float)m_iSrcCornerHeight / (float)tall ) : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Describes editing details
//-----------------------------------------------------------------------------
const char *ScalableImagePanel::GetDescription()
{
	static char buf[1024];
	_snprintf(buf, sizeof(buf), "%s string image, int src_corner_height, int src_corner_width, int draw_corner_height, int draw_corner_width", BaseClass::GetDescription());
	return buf;
}