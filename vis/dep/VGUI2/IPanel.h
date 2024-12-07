#ifndef IPANEL_H
#define IPANEL_H
#ifdef _WIN32
#pragma once
#endif

#ifdef SendMessage
#undef GetClassName
#endif
#ifdef SendMessage
#undef SendMessage
#endif

class KeyValues;

namespace vgui
{
	class SurfacePlat;
	class IClientPanel;
	
	class IPanel : public IBaseInterface
	{
	public:
		virtual void Init( IPanel* vguiPanel , IClientPanel *panel ) = 0;
		virtual void SetPos( IPanel* vguiPanel , int x , int y ) = 0;
		virtual void GetPos( IPanel* vguiPanel , int &x , int &y ) = 0;
		virtual void SetSize( IPanel* vguiPanel , int wide , int tall ) = 0;
		virtual void GetSize( IPanel* vguiPanel , int &wide , int &tall ) = 0;
		virtual void SetMinimumSize( IPanel* vguiPanel , int wide , int tall ) = 0;
		virtual void GetMinimumSize( IPanel* vguiPanel , int &wide , int &tall ) = 0;
		virtual void SetZPos( IPanel* vguiPanel , int z ) = 0;
		virtual int GetZPos( IPanel* vguiPanel ) = 0;
		virtual void GetAbsPos( IPanel* vguiPanel , int &x , int &y ) = 0;
		virtual void GetClipRect( IPanel* vguiPanel , int &x0 , int &y0 , int &x1 , int &y1 ) = 0;
		virtual void SetInset( IPanel* vguiPanel , int left , int top , int right , int bottom ) = 0;
		virtual void GetInset( IPanel* vguiPanel , int &left , int &top , int &right , int &bottom ) = 0;
		virtual void SetVisible( IPanel* vguiPanel , bool state ) = 0;
		virtual bool IsVisible( IPanel* vguiPanel ) = 0;
		virtual void SetParent( IPanel* vguiPanel , IPanel* newParent ) = 0;
		virtual int GetChildCount( IPanel* vguiPanel ) = 0;
		virtual IPanel* GetChild( IPanel* vguiPanel , int index ) = 0;
		virtual IPanel* GetParent( IPanel* vguiPanel ) = 0;
		virtual void MoveToFront( IPanel* vguiPanel ) = 0;
		virtual void MoveToBack( IPanel* vguiPanel ) = 0;
		virtual bool HasParent( IPanel* vguiPanel , IPanel* potentialParent ) = 0;
		virtual bool IsPopup( IPanel* vguiPanel ) = 0;
		virtual void SetPopup( IPanel* vguiPanel , bool state ) = 0;
		virtual bool Render_GetPopupVisible( IPanel* vguiPanel ) = 0;
		virtual void Render_SetPopupVisible( IPanel* vguiPanel , bool state ) = 0;
		virtual vgui::Scheme GetScheme( IPanel* vguiPanel ) = 0;
		virtual bool IsProportional( IPanel* vguiPanel ) = 0;
		virtual bool IsAutoDeleteSet( IPanel* vguiPanel ) = 0;
		virtual void DeletePanel( IPanel* vguiPanel ) = 0;
		virtual void SetKeyBoardInputEnabled( IPanel* vguiPanel , bool state ) = 0;
		virtual void SetMouseInputEnabled( IPanel* vguiPanel , bool state ) = 0;
		virtual bool IsKeyBoardInputEnabled( IPanel* vguiPanel ) = 0;
		virtual bool IsMouseInputEnabled( IPanel* vguiPanel ) = 0;
		virtual void Solve( IPanel* vguiPanel ) = 0;
		virtual const char *GetName( IPanel* vguiPanel ) = 0;
		virtual const char *GetClassName( IPanel* vguiPanel ) = 0;
		virtual void SendMessage( IPanel* vguiPanel , KeyValues *params , IPanel* ifromPanel ) = 0;
		virtual void Think( IPanel* vguiPanel ) = 0;
		virtual void PerformApplySchemeSettings( IPanel* vguiPanel ) = 0;
		virtual void PaintTraverse( IPanel* vguiPanel , bool forceRepaint , bool allowForce = true ) = 0;
		virtual void Repaint( IPanel* vguiPanel ) = 0;
		virtual IPanel* IsWithinTraverse( IPanel* vguiPanel , int x , int y , bool traversePopups ) = 0;
		virtual void OnChildAdded( IPanel* vguiPanel , IPanel* child ) = 0;
		virtual void OnSizeChanged( IPanel* vguiPanel , int newWide , int newTall ) = 0;
		virtual void InternalFocusChanged( IPanel* vguiPanel , bool lost ) = 0;
		virtual bool RequestInfo( IPanel* vguiPanel , KeyValues *outputData ) = 0;
		virtual void RequestFocus( IPanel* vguiPanel , int direction = 0 ) = 0;
		virtual bool RequestFocusPrev( IPanel* vguiPanel , IPanel* existingPanel ) = 0;
		virtual bool RequestFocusNext( IPanel* vguiPanel , IPanel* existingPanel ) = 0;
		virtual IPanel* GetCurrentKeyFocus( IPanel* vguiPanel ) = 0;
		virtual int GetTabPosition( IPanel* vguiPanel ) = 0;
		virtual SurfacePlat *Plat( IPanel* vguiPanel ) = 0;
		virtual void SetPlat( IPanel* vguiPanel , SurfacePlat *Plat ) = 0;
		virtual IPanel *GetPanel( IPanel* vguiPanel , const char *destinationModule ) = 0;
		virtual bool IsEnabled( IPanel* vguiPanel ) = 0;
		virtual void SetEnabled( IPanel* vguiPanel , bool state ) = 0;
		virtual void *Client( IPanel* vguiPanel ) = 0;
		virtual const char *GetModuleName( IPanel* vguiPanel ) = 0;
	};
}

extern vgui::IPanel* g_pIPanel;

#define VGUI_PANEL_INTERFACE_VERSION "VGUI_Panel007"
#endif