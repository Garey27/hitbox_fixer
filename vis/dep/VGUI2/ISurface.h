#ifndef ISURFACE_H
#define ISURFACE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CreateFont
#undef CreateFont
#endif

#ifdef PlaySound
#undef PlaySound
#endif

#include "IPanel.h"
#include "IHTML.h"

class Color;

template< typename Function > Function call_func( PVOID Base , DWORD Index )
{
	PDWORD* VTablePointer = (PDWORD*)Base;
	PDWORD VTableFunctionBase = *VTablePointer;
	DWORD dwAddress = VTableFunctionBase[Index];
	return (Function)( dwAddress );
}

namespace vgui
{
	class IImage;
	class Image;
	class Point;
	class IHTML;
	class IHTMLEvents;

	typedef unsigned long HCursor;
	typedef unsigned long HTexture;
	typedef unsigned long HFont;

	class ISurface : public IBaseInterface
	{
	public:
		enum EFontFlags
		{
			FONTFLAG_NONE ,
			FONTFLAG_ITALIC = 0x001 ,
			FONTFLAG_UNDERLINE = 0x002 ,
			FONTFLAG_STRIKEOUT = 0x004 ,
			FONTFLAG_SYMBOL = 0x008 ,
			FONTFLAG_ANTIALIAS = 0x010 ,
			FONTFLAG_GAUSSIANBLUR = 0x020 ,
			FONTFLAG_ROTARY = 0x040 ,
			FONTFLAG_DROPSHADOW = 0x080 ,
			FONTFLAG_ADDITIVE = 0x100 ,
			FONTFLAG_OUTLINE = 0x200 ,
		};

		enum SurfaceFeature_e
		{
			ANTIALIASED_FONTS = 1 ,
			DROPSHADOW_FONTS = 2 ,
			ESCAPE_KEY = 3 ,
			OPENING_NEW_HTML_WINDOWS = 4 ,
			FRAME_MINIMIZE_MAXIMIZE = 5 ,
			DIRECT_HWND_RENDER = 6 ,
		};

	public:
		virtual void Shutdown( void ) = 0;
		virtual void RunFrame( void ) = 0;
		virtual IPanel* GetEmbeddedPanel( void ) = 0;
		virtual void SetEmbeddedPanel( IPanel* pPanel ) = 0;
		virtual void PushMakeCurrent( IPanel* panel , bool useInsets ) = 0;
		virtual void PopMakeCurrent( IPanel* panel ) = 0;
		virtual void DrawSetColor( int r , int g , int b , int a ) = 0;
		virtual void DrawSetColor( Color col ) = 0;
		virtual void DrawFilledRect( int x0 , int y0 , int x1 , int y1 ) = 0;
		virtual void DrawOutlinedRect( int x0 , int y0 , int x1 , int y1 ) = 0;
		virtual void DrawLine( int x0 , int y0 , int x1 , int y1 ) = 0;
		virtual void DrawPolyLine( int *px , int *py , int numPoints ) = 0;
		virtual void DrawSetTextFont( HFont font ) = 0;
		virtual void DrawSetTextColor( int r , int g , int b , int a ) = 0;
		virtual void DrawSetTextColor( Color col ) = 0;
		virtual void DrawSetTextPos( int x , int y ) = 0;
		virtual void DrawGetTextPos( int &x , int &y ) = 0;
		virtual void DrawPrintText( const wchar_t *text , int textLen ) = 0;
		virtual void DrawUnicodeChar( wchar_t wch ) = 0;
		virtual void DrawUnicodeCharAdd( wchar_t wch ) = 0;
		virtual void DrawFlushText( void ) = 0;
		virtual IHTML* CreateHTMLWindow( IHTMLEvents *events , IPanel* context ) = 0;
		virtual void PaintHTMLWindow( IHTML *htmlwin ) = 0;
		virtual void DeleteHTMLWindow( IHTML *htmlwin ) = 0;
		virtual void DrawSetTextureFile( int id , const char *filename , int hardwareFilter , bool forceReload ) = 0;
		virtual void DrawSetTextureRGBA( int id , const unsigned char *rgba , int wide , int tall , int hardwareFilter , bool forceReload ) = 0;
		virtual void DrawSetTexture( int id ) = 0;
		virtual void DrawGetTextureSize( int id , int &wide , int &tall ) = 0;
		virtual void DrawTexturedRect( int x0 , int y0 , int x1 , int y1 ) = 0;
		virtual bool IsTextureIDValid( int id ) = 0;
		virtual int CreateNewTextureID( bool procedural = false ) = 0;
		virtual void GetScreenSize( int &wide , int &tall ) = 0;
		virtual void SetAsTopMost( IPanel* panel , bool state ) = 0;
		virtual void BringToFront( IPanel* panel ) = 0;
		virtual void SetForegroundWindow( IPanel* panel ) = 0;
		virtual void SetPanelVisible( IPanel* panel , bool state ) = 0;
		virtual void SetMinimized( IPanel* panel , bool state ) = 0;
		virtual bool IsMinimized( IPanel* panel ) = 0;
		virtual void FlashWindow( IPanel* panel , bool state ) = 0;
		virtual void SetTitle( IPanel* panel , const wchar_t *title ) = 0;
		virtual void SetAsToolBar( IPanel* panel , bool state ) = 0;
		virtual void CreatePopup( IPanel* panel , bool minimised , bool showTaskbarIcon = true , bool disabled = false , bool mouseInput = true , bool kbInput = true ) = 0;
		virtual void SwapBuffers( IPanel* panel ) = 0;
		virtual void Invalidate( IPanel* panel ) = 0;
		virtual void SetCursor( HCursor cursor ) = 0;
		virtual bool IsCursorVisible( void ) = 0;
		virtual void ApplyChanges( void ) = 0;
		virtual bool IsWithin( int x , int y ) = 0;
		virtual bool HasFocus( void ) = 0;
		virtual bool SupportsFeature( SurfaceFeature_e feature ) = 0;
		virtual void RestrictPaintToSinglePanel( IPanel* panel ) = 0;
		virtual void SetModalPanel( IPanel* panel ) = 0;
		virtual IPanel* GetModalPanel( void ) = 0;
		virtual void UnlockCursor( void ) = 0;
		virtual void LockCursor( void ) = 0;
		virtual void SetTranslateExtendedKeys( bool state ) = 0;
		virtual IPanel* GetTopmostPopup( void ) = 0;
		virtual void SetTopLevelFocus( IPanel* panel ) = 0;
		virtual HFont CreateFont( void ) = 0;
		virtual bool AddGlyphSetToFont( HFont font , const char *windowsFontName , int tall , int weight , int blur , int scanlines , int flags , int lowRange , int highRange ) = 0;
		virtual bool AddCustomFontFile( const char *fontFileName ) = 0;
		virtual int GetFontTall( HFont font ) = 0;
		virtual void GetCharABCwide( HFont font , int ch , int &a , int &b , int &c ) = 0;
		virtual int GetCharacterWidth( HFont font , int ch ) = 0;
		virtual void GetTextSize( HFont font , const wchar_t *text , int &wide , int &tall ) = 0;
		virtual IPanel* GetNotifyPanel( void ) = 0;
		virtual void SetNotifyIcon( IPanel* context , HTexture icon , IPanel* panelToReceiveMessages , const char *text ) = 0;
		virtual void PlaySound( const char *fileName ) = 0;
		virtual int GetPopupCount( void ) = 0;
		virtual IPanel* GetPopup( int index ) = 0;
		virtual bool ShouldPaintChildPanel( IPanel* childPanel ) = 0;
		virtual bool RecreateContext( IPanel* panel ) = 0;
		virtual void AddPanel( IPanel* panel ) = 0;
		virtual void ReleasePanel( IPanel* panel ) = 0;
		virtual void MovePopupToFront( IPanel* panel ) = 0;
		virtual void MovePopupToBack( IPanel* panel ) = 0;
		virtual void SolveTraverse( IPanel* panel , bool forceApplySchemeSettings = false ) = 0;
		virtual void PaintTraverse( IPanel* panel ) = 0;
		virtual void EnableMouseCapture( IPanel* panel , bool state ) = 0;
		virtual void GetWorkspaceBounds( int &x , int &y , int &wide , int &tall ) = 0;
		virtual void GetAbsoluteWindowBounds( int &x , int &y , int &wide , int &tall ) = 0;
		virtual void GetProportionalBase( int &width , int &height ) = 0;
		virtual void CalculateMouseVisible( void ) = 0;
		virtual bool NeedKBInput( void ) = 0;
		virtual bool HasCursorPosFunctions( void ) = 0;
		virtual void SurfaceGetCursorPos( int &x , int &y ) = 0;
		virtual void SurfaceSetCursorPos( int x , int y ) = 0;
		virtual void DrawTexturedPolygon( int *p , int n ) = 0;
		virtual int GetFontAscent( HFont font , wchar_t wch ) = 0;
		virtual void SetAllowHTMLJavaScript( bool state ) = 0;
	};
}

extern vgui::ISurface* g_pISurface;

#define VGUI_SURFACE_INTERFACE_VERSION "VGUI_Surface026"
#endif