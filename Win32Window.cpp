// Win32Window.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <Windows.h>
#include <gdiplus.h>
#include <wingdi.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <vsstyle.h>
#include <dwmapi.h>

#include <combaseapi.h>
#include <shellapi.h>
#include <ShObjIdl.h>
#include <gdiplusbrush.h>
#include <comdef.h>

//#include <ole.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>
//#include <objidl.h>
#include <wmp.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "urlmon.lib")

#define MOUSE_EVENT_INDEX 8
#define WINDOW_EVENT_INDEX 0

using namespace Gdiplus;
using namespace DllExports;

class DummyClass : public CAtlExeModuleT<DummyClass> {} dummy;

struct Position { int x, y; };

struct Route
{
	Position Start, End;
};

class Event
{
public:
	Event()
	{
		m_hEvent = CreateEventA(nullptr, true, false, "Global\\EventName");
	}

	void Trigger() 
	{
		SetEvent(m_hEvent);
	}

	void Wait() 
	{
		WaitForSingleObject(m_hEvent, 0xFFFFFFFF);
	}

	void Reset() 
	{
		ResetEvent(m_hEvent);
	}

	~Event() 
	{
		ResetEvent(m_hEvent);
		CloseHandle(m_hEvent);
	}

private:
	HANDLE m_hEvent;
};

enum GdiObjType 
{
	GDI_PEN,
	GDI_BRUSH
};

struct NativeGdiObject
{
	GdiObjType Type;

	union object 
	{
		GpPen* Pen;
		GpBrush* Brush;
	} Object;
};

class DrawObject 
{
public:
	DrawObject(GdiObjType type) 
	{
		m_GdiDrawObject.Type = type;
		std::cout << "DrawObject BASE object created @ 0x" << this << "\n";
	}

	inline virtual NativeGdiObject operator*() const { return m_GdiDrawObject; }
	inline void operator=(const NativeGdiObject& nativeDrawObj) { m_GdiDrawObject = nativeDrawObj; }
	inline virtual const NativeGdiObject* operator->() { return &m_GdiDrawObject; }

	virtual ~DrawObject() 
	{

	}

protected:
	const GdiObjType& Type = m_GdiDrawObject.Type;
	NativeGdiObject m_GdiDrawObject;
};

class Marker : public DrawObject
{
public:
	Marker(unsigned long RGBAPenColour, unsigned short WidthPxl)
		: DrawObject(GdiObjType::GDI_PEN), m_PenWidthPxl(WidthPxl), 
		m_PenColour(RGBAPenColour)
	{
		GdipCreatePen1(RGBAPenColour, WidthPxl, GpUnit::UnitPixel, &this->m_GdiDrawObject.Object.Pen);
		std::cout << "Marker object created @ 0x" << this << "\n";
	}

	const unsigned long& Width = m_PenWidthPxl;
	const unsigned int& Colour = m_PenColour;

	inline void SetWitdh(unsigned short WidthPxl)
	{
		m_PenWidthPxl = WidthPxl;
		GdipSetPenWidth(this->m_GdiDrawObject.Object.Pen, WidthPxl);
	}

	inline void SetColour(unsigned long ARGBPenColour)
	{
		m_PenColour = ARGBPenColour;
		GdipSetPenColor(this->m_GdiDrawObject.Object.Pen, ARGBPenColour);
	}

	~Marker() { GdipDeletePen(this->m_GdiDrawObject.Object.Pen); }

private:
	unsigned short m_PenWidthPxl;
	unsigned long m_PenColour;

	inline void Reset()
	{
		GdipDeletePen(this->m_GdiDrawObject.Object.Pen);
		GdipCreatePen1(m_PenColour, m_PenWidthPxl, GpUnit::UnitPixel, &this->m_GdiDrawObject.Object.Pen);
	}
};

struct Resolution { unsigned short width, height; };
struct WndPosition { Resolution Res; Position Pos; } ;

namespace Win32WndCtrl
{
	enum ServiceID : byte
	{
		ENABLE_GDI = 1,
		ENABLE_ATL = 1 << 1,
		ENABLE_COMMCTRL = 1 << 2,
		ENABLE_COM = 1 << 3,
		ENABLE_ACTCTX = 1 << 4,
		ENABLE_THEME = 1 << 5,
		ENABLE_ALL = (~0 ^ 192)
	};

	class ServiceManager 
	{
	public:
		ServiceManager(char* const argv0 = nullptr) 
			: m_Argv(argv0), m_EnableServices((ServiceID)NULL)
		{

		}

		inline void Enable(ServiceID sID) 
		{
			switch (sID) 
			{
			case ServiceID::ENABLE_ATL:
				m_ServiceStruct.Atl = m_EnableAtl();
				break;

			case ServiceID::ENABLE_COM:
				m_ServiceStruct.COMApi = m_EnableCOM();
				break;

			case ServiceID::ENABLE_COMMCTRL:
				m_ServiceStruct.CommCtrls = m_EnableCommCtrl();
				break;

			case ServiceID::ENABLE_GDI:
				m_EnableGDI(&m_ServiceStruct.Gdip);
				break;

			case ServiceID::ENABLE_THEME:
				m_ServiceStruct.UxTheme = m_EnableTheme();
				break;

			case ServiceID::ENABLE_ACTCTX:
				m_ServiceStruct.ActCtx = m_EnableActCtx(m_Argv);
				break;

			case ServiceID::ENABLE_ALL:
				m_ServiceStruct.CommCtrls = m_EnableCommCtrl();
				m_EnableGDI(&m_ServiceStruct.Gdip);
				m_ServiceStruct.ActCtx = m_EnableActCtx(m_Argv);
				m_ServiceStruct.UxTheme = m_EnableTheme();
				m_ServiceStruct.COMApi = m_EnableCOM();
				m_ServiceStruct.Atl = m_EnableAtl();
				break;
			}
			
		}

		inline bool Disable(ServiceID sID)
		{

		}

		inline bool IsEnabled(ServiceID serviceID) const { return (bool)((((~serviceID) << 2) >> 2) ^ m_EnableServices); }

		~ServiceManager() { delete[] m_Argv; }

	private:
		char* m_Argv;

		struct GdipStartupInfo
		{
			GdiplusStartupOutput startup;
			unsigned long dwToken;
			GpStatus status;
		};

		struct ServiceStruct
		{
			bool CommCtrls;
			unsigned long ActCtx;
			HRESULT COMApi;
			bool Atl;
			GdipStartupInfo Gdip;
			HRESULT UxTheme;
		} m_ServiceStruct = { 0 };

		ServiceID m_EnableServices;

		inline bool m_EnableAtl() const { return ATL::AtlAxWinInit(); }
		inline HRESULT m_EnableCOM() const 
		{
			return (CoInitializeEx(nullptr, COINIT::COINIT_APARTMENTTHREADED) & OleInitialize(nullptr));
		}
		inline bool m_EnableCommCtrl() const  
		{
			INITCOMMONCONTROLSEX commonCtrl = { 0 };
			commonCtrl.dwICC = ICC_PROGRESS_CLASS;
			commonCtrl.dwSize = sizeof(INITCOMMONCONTROLSEX);
			return InitCommonControlsEx(&commonCtrl);
		}
		inline bool m_EnableGDI(GdipStartupInfo* pGDIStartupInfo) const
		{
			if (!pGDIStartupInfo) { return false; }
		
			GdiplusStartupInput GDIPStartupInput = { 0 };
			GDIPStartupInput.GdiplusVersion = 2;
			pGDIStartupInfo->status = GdiplusStartup(&pGDIStartupInfo->dwToken, &GDIPStartupInput, &pGDIStartupInfo->startup);
				
			return true;
		}
		inline DWORD m_EnableActCtx(const char* const exePath) 
		{
			char Buffer[129] = { 0 };
			
			ACTCTXA activationCtx = { 0 };
			activationCtx.cbSize = sizeof(ACTCTXA);
			activationCtx.dwFlags =
			{
				 ACTCTX_FLAG_LANGID_VALID | ACTCTX_FLAG_APPLICATION_NAME_VALID 
				 | ACTCTX_FLAG_PROCESSOR_ARCHITECTURE_VALID
			};

			//activationCtx.lpResourceName = "Win32Window.exe.manifest";//MAKEINTRESOURCEA(RT_MANIFEST);
			activationCtx.lpApplicationName = "Win32Window.exe";
			activationCtx.lpSource = "U:\\00000000001\\Visual Studio 2017\\Projects\\Win32Window\\Debug\\Win32Window.exe.manifest.txt";
			activationCtx.wLangId = LANG_ENGLISH;
			activationCtx.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;

			if (Buffer[0]) {
				int strl = strlen(Buffer);
				m_Argv = new char[strl + 1];
				ZeroMemory(Buffer, (strl + 1));
				memcpy(m_Argv, Buffer, strl);
			}
						
			unsigned long dwActCtxCookie = 0;

			if (ActivateActCtx(CreateActCtxA(&activationCtx), &dwActCtxCookie)) {
				std::cout << "ActivateActCtx: 0x" << std::hex << dwActCtxCookie << std::dec << "\n";
			}

			return dwActCtxCookie;
		}
		inline HRESULT m_EnableTheme() const { return EnableTheming(IsEnabled(ServiceID::ENABLE_ACTCTX)); }
	};

#define WM_RENDERTOWND 3434

	struct EventArgs 
	{
		WPARAM wParam;
		LPARAM lParam;
	};
};

template<typename T>
class ArrayList
{
public:
	ArrayList() : m_Position(0), m_MaxCapacity(16 * 2)
	{
		m_Data = new T[m_MaxCapacity];
		ZeroMemory(m_Data, sizeof(T) * m_MaxCapacity);
	}

	ArrayList(unsigned int capacity) : m_Position(0), m_MaxCapacity(capacity) 
	{

	}

	bool Add(T& obj) 
	{
		if(m_Position < m_MaxCapacity) { 
			m_Data[m_Position] = obj; 
		} else {
			m_Grow();
			m_Data[m_Position] = obj;
		}

		m_Position++;
		return true;
	}

	inline void Remove(int i) 
	{

	}

	const unsigned int& Position = m_Position;

	inline T& operator[](unsigned int i) { return (i < m_Position) ? m_Data[i] : m_Data[0]; }

	inline ~ArrayList() { delete[] m_Data; }

private:
	T* m_Data;
	unsigned int m_MaxCapacity;
	unsigned int m_Position;

	inline void m_Grow() 
	{
		
	}
};

template<typename T>
class HashTable 
{
public:
	HashTable()
	{

	}

	bool Add(T value) 
	{
		m_ArrayData[m_ResolveHash(value)] = value;
	}

	~HashTable()
	{

	}

private:
	ArrayList<T> m_ArrayData;

	__forceinline unsigned int m_ResolveHash(T value) const
	{
		sizeof(T);
		_asm 
		{
			rdseed eax
		}
	}
};

enum ResultType
{
	GDI,
	WIN_32,
	COM
};

template<class T = IUnknown>
class COMObjectBase
{
public:
	COMObjectBase(const IID& clsid)
	{
		CoGetClassObject(clsid, CLSCTX_INPROC_SERVER, nullptr, __uuidof(IClassFactory), (void**)&m_pClassFactory);
		m_pClassFactory->CreateInstance(nullptr, __uuidof(T), (void**)&m_pUnkObject);
		std::cout << "COMObjectBase object created successfully!\n";
	}

	COMObjectBase(IUnknown* pUnkObj) : m_pUnkObject(pUnkObj), m_pClassFactory(nullptr) { }

	inline T* operator->() { return m_pUnkObject; }
	inline T*& operator*() { return m_pUnkObject; }

	virtual ~COMObjectBase()
	{
		((IUnknown*)m_pUnkObject)->Release();
		if (m_pClassFactory) m_pClassFactory->Release();
	}

protected:
	COMObjectBase()
	{

	}

	virtual bool Initialize() { return false; }

protected:
	T* m_pUnkObject;

private:
	IClassFactory* m_pClassFactory;
};

struct FnStatusResult
{
	union ReturnTypes
	{
		unsigned long dwResult;
		GpStatus gdi;
		NTSTATUS nt;
		HRESULT com;

	}result;
};

class DebugBase
{
public:
	DebugBase(ResultType type) : m_Type(type)
	{

	}

	const ResultType& Type = m_Type;
	
	unsigned long Status(unsigned long statusResult, const char* fnName) 
	{
		StatusResult.result.dwResult = statusResult;
		switch(m_Type)
		{
		case ResultType::COM:
			std::cout << "["<< fnName << "] HRESULT -> 0x" << (void*)StatusResult.result.com << "\n";
			break;

		case ResultType::GDI:
			std::cout << "[" << fnName << "] GpStatus -> " << StatusResult.result.gdi << "\n";
			break;

		case ResultType::WIN_32:
			std::cout << "[" << fnName << "] NTSTATUS -> 0x" << (void*)StatusResult.result.nt << "\n";
			break;
		}
		
		return statusResult;
	}

private:
	//virtual bool Initialize() override { return GetErrorInfo(NULL, &this->m_pUnkObject); }

	ResultType m_Type;
	FnStatusResult StatusResult;
};

struct MouseEventArgs 
{
	Position mousePosition;

	enum MouseButton : unsigned short
	{
		LEFT = WM_LBUTTONDOWN, 
		RIGHT = WM_RBUTTONDOWN,
		MIDDLE = WM_MBUTTONDOWN

	} mouseButton;

	bool IsScrollBar;
	enum MouseButtonState : byte { UP , DOWN } mouseState;
};

struct WindowEventArgs 
{
	RECT windowRect;
	WndPosition windowPosition;
};

struct DragDropEventArgs 
{
	unsigned int fileNumber;
	Position dropPosition;
	char FilePath[256];
};

enum EventObjType : byte 
{ 
	MOUSE = 1 << 1,
	WINDOW = 1 << 2, 
	KEYBOARD = 1 << 3, 
	DRAG_DROP = 1 << 4
};

class EventObject 
{
public:
	const EventObjType& EventType = m_EvtObjType;

protected:
	EventObjType& ObjectType = m_EvtObjType;

private:
	EventObjType m_EvtObjType;
};

enum NativeWndType 
{
	BUTTON,
	PROGRESS_BAR,
	NATIVEWINDOW,
	EMBEDDED_WND,
	SCROLL_BAR,
	TEXTBOX,
	LABEL
};

enum WndStyle
{
	WS_NOSCALING = NULL,
	WS_STANDARD = CS_HREDRAW | CS_VREDRAW,
	WS_NOCLOSE = WS_STANDARD | CS_NOCLOSE,
	WS_NOSCALE_NOCLOSE = WS_NOSCALING | CS_NOCLOSE
};

enum WndBorderType : unsigned long
{
	WBT_WND_STANDARD = WS_OVERLAPPEDWINDOW,
	WBT_WND_EMBEDDED = WS_CHILD | WS_VISIBLE,
	WBT_WND_FIXED = WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
};

enum BtnBorderType : unsigned long
{
	BBT_BTN_3STATE = WBT_WND_EMBEDDED | BS_3STATE,
	BBT_BTN_CHECKBOX = WBT_WND_EMBEDDED | BS_CHECKBOX,
	BBT_BTN_PUSHBUTTON = WBT_WND_EMBEDDED | BS_PUSHBUTTON,
	BBT_BTN_RADIOBUTTON = WBT_WND_EMBEDDED | BS_RADIOBUTTON
};

enum PBBorderType : unsigned long
{
	PBBT_PB_BAR = WBT_WND_EMBEDDED | NULL,
	PBBT_PB_SMOOTH = WBT_WND_EMBEDDED | PBS_SMOOTH,
	PBBT_PB_REVERSE = WBT_WND_EMBEDDED | PBS_SMOOTHREVERSE
};

enum LblBorderType : unsigned long
{
	LBLBT_TXT_STANDARD = WBT_WND_EMBEDDED | SS_SIMPLE,
	LBLBT_TXT_SUNKED = WBT_WND_EMBEDDED | SS_SUNKEN,
	LBLBT_TXT_OUTLINE = WBT_WND_EMBEDDED | SS_BLACKRECT,
	LBLBT_TXT_FRAMED = WBT_WND_EMBEDDED | SS_GRAYFRAME
};

enum TxtBoxBorderType : unsigned long 
{
	TXTBT_TXT_STANDARD = WBT_WND_EMBEDDED | ES_LEFT,
	TXTBT_TXT_CENTERED = WBT_WND_EMBEDDED | ES_CENTER,
	TXTBT_TXT_PASSWORD = WBT_WND_EMBEDDED | ES_PASSWORD,
	TXTBT_TXT_MULTILINE = WBT_WND_EMBEDDED | ES_MULTILINE
};

enum ScrollBorderType : unsigned long
{
	SBBT_SIMPLE = WBT_WND_EMBEDDED | SB_SIMPLE,
	SBBT_VERTICAL = WBT_WND_EMBEDDED | SB_VERT,
	SBBT_HORIZONTAL = WBT_WND_EMBEDDED | SB_HORZ
};

enum borderType 
{
	NativeWindowBorder,
	ButtonBorder,
	ProgressBarBorder,
	TextBoxBorder,
	LabelBorder
};

union NativeWndBorderType 
{
	unsigned long dwStyle;

	union Types 
	{
		WndBorderType wbt;
		BtnBorderType bbt;
		PBBorderType pbt;
		TxtBoxBorderType tbt;
		LblBorderType lbt;
		ScrollBorderType sbt;

	} _Types;
};

struct WndStyleProperties
{
	NativeWndBorderType borderType;
	WndStyle style;
};

struct CreateWindowParams
{
	const char* lpszTitle;
	const char* lpszClassName;
	NativeWndType NativeWindowType;
	NativeWndBorderType BorderType;
	WndStyle WndClassStyle;
	unsigned long BackgroundColour;
	HWND hParentWindow;
	WndPosition WindowPosition;
	WNDPROC lpFnEventHandler;
};

class IControl
{
public:
	virtual bool Move(const WndPosition* pWndPos) = 0;
};

struct EventProcData
{
	HWND hWnd;
	UINT msg;
	WPARAM wParam;
	LPARAM lParam;
};

class EventProxy
{
public:
	EventProxy(NativeWindow* pNativeWnd = nullptr) : m_DebugObj(ResultType::COM)
	{ 
		if (InternalDispatch(this, true) == this) 
		{
			std::cout << "Event Proxy was created successfully!\n";
		}
	}

	//Needs to be called on the *SAME THREAD* the Window was created on...
	virtual bool Poll(NativeWindow* pWindow)
	{
		MSG msg = { 0 };

		if (pWindow) 
		{
			if (pWindow->Type == NATIVEWINDOW)
			{
				GetMessageA(&msg, pWindow->Handle, NULL, NULL);
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
				return true;
			}
		}
		return false;
	}

	inline bool Attach(IEventDispatcher* pEventObj) { return m_EventObjects.Add(pEventObj); }

	inline WNDPROC operator*() const { return EventHandler; }

	inline ~EventProxy() { InternalDispatch(nullptr, true); }

private:
	ArrayList<IEventDispatcher*> m_EventObjects;
	DebugBase m_DebugObj;

private:
	__forceinline static EventProxy* __fastcall InternalDispatch(EventProxy* eventObject, bool setIt)
	{
		__declspec(thread) static decltype(eventObject) pEventBase = nullptr;
		pEventBase = (setIt) ? eventObject : pEventBase;
		return pEventBase;
	}

protected:
	__forceinline static LRESULT __stdcall EventHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		EventProcData eventProcData = { hWnd, msg, wParam, lParam };
		ArrayList<IEventDispatcher*>* pEventObjArray = &InternalDispatch(nullptr, false)->m_EventObjects;

		if (pEventObjArray) 
		{
			for (int i = 0; i < pEventObjArray->Position; ++i) 
			{ 
				((*pEventObjArray)[i])->Dispatch(eventProcData); 
			}
		}

		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
};


class NativeWindow : public IControl
{
public:
	NativeWindow(const char* szWndTitle, const WndPosition* pWndPosition, NativeWndType wndType, const char* szClassName = nullptr)
		: m_NativeWndType(wndType), m_lpSzTitle(szWndTitle), m_lpSzClassName((!szClassName) ? szWndTitle : szClassName), m_DebugObject(ResultType::WIN_32)
	{
		ZeroMemory(&this->WndStyleProps, sizeof(WndStyleProperties));

		WindowPosition = *pWndPosition;
		WndStyleProps.borderType._Types.wbt = (wndType == NATIVEWINDOW) ? WBT_WND_STANDARD : WBT_WND_EMBEDDED;
		WndStyleProps.style = (wndType == EMBEDDED_WND) ? WS_NOSCALING : WS_STANDARD;

		std::cout << "NativeWindow object created successfully!\n";
	}

	virtual bool IsAlive() const { return IsWindowEnabled(this->Handle); }
	virtual bool IsChild() const { return ::IsChild(this->ParentWindow, this->Handle); }

	virtual bool GetRectangle(RECT* pRect) const { return (!pRect) ? false : GetWindowRect(this->NativeWnd, pRect); }
	virtual bool Minimise() const { return ShowWindow(NativeWnd, SW_MINIMIZE); }
	virtual bool Restore() const { return ShowWindow(NativeWnd, SW_RESTORE);}
	virtual bool Maximize() const { return ShowWindow(NativeWnd, SW_MAXIMIZE); }
	virtual bool Update() const { return UpdateWindow(NativeWnd); }

	virtual bool SetText(const char* lpszWndText) 
	{ 
		if (this->Type != NATIVEWINDOW || this->Type != TEXTBOX || this->Type != BUTTON) 
		{ 
			return false;  
		}
		
		if (!lpszWndText) 
		{ 
			SetWindowTextA(NativeWnd, "Default Window Title");
		} 
		else 
		{ 
			m_lpSzTitle = lpszWndText; SetWindowTextA(NativeWnd, m_lpSzTitle);
		}

		return true;
	}
	
	virtual const char* GetText() const { return m_lpSzTitle; }

	inline operator HWND() const { return NativeWnd; }
	
	const HWND& Handle = NativeWnd;
	const WndPosition& Position = WindowPosition;
	const NativeWndType& Type = m_NativeWndType;

	virtual bool Move(const WndPosition* pWndPosition) override
	{
		if (!pWndPosition) { return false; }
		this->WindowPosition = *pWndPosition;

		return MoveWindow(this->NativeWnd, WindowPosition.Pos.x, WindowPosition.Pos.y,
			WindowPosition.Res.width, WindowPosition.Res.height, true);
	}

	virtual void Close() const { CloseWindow(NativeWnd); }
	virtual bool Show(bool show) const { return ShowWindow(NativeWnd, ((show) ? SW_SHOW : SW_HIDE)); }

	virtual ~NativeWindow() 
	{
		this->Show(false);
		this->Close();
		DestroyWindow(NativeWnd);
	}

private:
	HWND NativeWnd;
	const char* m_lpSzTitle;
	ATOM registeredClass;
	const char* m_lpSzClassName;
	NativeWndType m_NativeWndType;
	ArrayList<IEventDispatcher*> m_EventObjects;

protected:
	NativeWindow() : m_DebugObject(ResultType::WIN_32)
	{
		std::cout << "NativeWindow BASE object created @ 0x" << this << "\n";
	}

	inline bool Attach(IEventDispatcher* pEventObject) { m_EventObjects.Add(pEventObject); }

	virtual bool Poll() const
	{
		MSG msg = { 0 };

		if (this->Type == NATIVEWINDOW)
		{
			GetMessageA(&msg, this->Handle, NULL, NULL);
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
			return true;
		}

		return false;
	}

	virtual bool Add(NativeWindow* pNativeWnd)
	{
		pNativeWnd->Create();
	}

	using ArrayEventDispatch = ArrayList<IEventDispatcher*>;

	__forceinline static LRESULT __stdcall EventHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		EventProcData eventProcData = { hWnd, msg, wParam, lParam };
		ArrayList<IEventDispatcher*>* pEventObjArray = InternalDispatch(nullptr);

		if (pEventObjArray)
		{
			for (int i = 0; i < pEventObjArray->Position; ++i)
			{
				((*pEventObjArray)[i])->Dispatch(eventProcData);
			}
		}

		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}

	inline bool Create(const WndStyleProperties* pWndStyleProp)
	{
		NativeWndType nwt = m_NativeWndType;

		if (!pWndStyleProp || !IsCompatibleWndBt(pWndStyleProp->borderType)) 
		{
			return false; 
		}
		
		WndStyleProps = *pWndStyleProp;
		
		CreateWindowParams cwp = { 0 };

		cwp.lpszTitle = m_lpSzTitle;
		cwp.lpszClassName = m_lpSzClassName;
		cwp.hParentWindow = (this->Type == NATIVEWINDOW) ? NULL : this->Handle;
		cwp.NativeWindowType = this->Type;
		cwp.WindowPosition = this->WindowPosition;
		cwp.BorderType =  WndStyleProps.borderType;
		cwp.BackgroundColour = 0x00FFFFFF;
		cwp.WndClassStyle = WndStyleProps.style;
		cwp.lpFnEventHandler = EventHandler;

		NativeWnd = this->CreateWnd(&cwp);
		return this->IsAlive();
	}

	inline bool IsCompatibleWndBt(const NativeWndBorderType& borderData) const
	{	
		switch (this->Type)
		{
		case NATIVEWINDOW:
			if (borderData._Types.wbt != WBT_WND_FIXED) 
			{
				if (borderData._Types.wbt != WBT_WND_STANDARD) 
				{ 
					return false; 
				}
			}
			break;

		case EMBEDDED_WND:
			if (borderData._Types.wbt != WBT_WND_EMBEDDED) 
			{
				return false; 
			}
			break;

		case PROGRESS_BAR:
			if (borderData._Types.pbt != PBBT_PB_BAR) 
			{
				if (borderData._Types.pbt != PBBT_PB_SMOOTH) 
				{ 
					return false; 
				} 
			}
			break;

		case LABEL:
			if (borderData._Types.lbt != LBLBT_TXT_STANDARD) 
			{
				if (borderData._Types.lbt != LBLBT_TXT_OUTLINE) 
				{
					if (borderData._Types.lbt != LBLBT_TXT_SUNKED) 
					{
						if (borderData._Types.lbt != LBLBT_TXT_FRAMED) 
						{ 
							return false; 
						}
					} 
				} 
			}
			break;

		case TEXTBOX:
			if (borderData._Types.tbt != TXTBT_TXT_STANDARD) 
			{
				if (borderData._Types.tbt != TXTBT_TXT_CENTERED) 
				{
					if (borderData._Types.tbt != TXTBT_TXT_MULTILINE) 
					{
						if (borderData._Types.tbt != TXTBT_TXT_PASSWORD) 
						{
							return false;
						}
					}
				}
			}

			break;

		case BUTTON:
			if (borderData._Types.bbt != BBT_BTN_3STATE)
			{
				if (borderData._Types.bbt != BBT_BTN_CHECKBOX) 
				{
					if (borderData._Types.bbt != BBT_BTN_PUSHBUTTON) 
					{
						if (borderData._Types.bbt != BBT_BTN_RADIOBUTTON) 
						{ 
							return false; 
						}
					}
				}
			}
			break;

		case SCROLL_BAR:
			if (borderData._Types.sbt != SBBT_VERTICAL) 
			{
				if (borderData._Types.sbt != SBBT_HORIZONTAL) 
				{
					if (borderData._Types.sbt != SBBT_SIMPLE) 
					{ 
						return false; 
					}
				}
			}	 
			break;
		}
		return true;
	}

private:
	__forceinline static ArrayEventDispatch* __fastcall InternalDispatch(ArrayEventDispatch* pEventObjects)
	{
		__declspec(thread) static decltype(pEventObjects) pEventBase = nullptr;
		pEventBase = (pEventObjects) ? pEventObjects : pEventBase;
		return pEventBase;
	}

protected:
	HWND ParentWindow;
	WndPosition WindowPosition;
	WndStyleProperties WndStyleProps;
	DebugBase m_DebugObject;

private:
	HWND CreateWnd(const CreateWindowParams* pCreateWindowParams)
	{
		if ((pCreateWindowParams->NativeWindowType == NATIVEWINDOW || pCreateWindowParams->NativeWindowType == EMBEDDED_WND)) {
			
			WNDCLASSEXA wndClassExA = { 0 };
			wndClassExA.hInstance = NULL;
            wndClassExA.lpfnWndProc = pCreateWindowParams->lpFnEventHandler;
            wndClassExA.cbSize = sizeof(WNDCLASSEXA);
            wndClassExA.lpszClassName = pCreateWindowParams->lpszClassName;
            wndClassExA.style = pCreateWindowParams->WndClassStyle;
            wndClassExA.hbrBackground = CreateSolidBrush(pCreateWindowParams->BackgroundColour);
            
            RegisterClassExA(&wndClassExA);
		}

		WndPosition WndLocation = pCreateWindowParams->WindowPosition;
		
		return CreateWindowExA(NULL, pCreateWindowParams->lpszClassName, pCreateWindowParams->lpszTitle, 
		pCreateWindowParams->BorderType.dwStyle, WndLocation.Pos.x, WndLocation.Pos.y, WndLocation.Res.width, 
		WndLocation.Res.height, pCreateWindowParams->hParentWindow, NULL, NULL, nullptr);
	}
};

enum FileStreamAccessFlags 
{
	FSAF_READ = STGM_READ,
	FSAF_WRITE = STGM_WRITE
};

enum FileStreamShareFlags
{
	FSSF_NOSHARE = STGM_SHARE_EXCLUSIVE,
	FSSF_NOREAD = STGM_SHARE_DENY_READ,
	FSSF_NOWRITE = STGM_SHARE_DENY_WRITE,
	FSSF_ALLOWALL = STGM_SHARE_DENY_NONE
};

class FileStream : public COMObjectBase<IStream> 
{
public:
	FileStream(const char* lpSzFilePath, FileStreamAccessFlags fsaf, FileStreamShareFlags fssf) 
		: m_lpSzFilePath(lpSzFilePath), m_Fsaf(fsaf), m_Fssf(fssf)
	{
		this->Initialize();
		std::cout << "FileStream object created @ 0x" << this << "\n";
	}

	const unsigned int& Size = m_StreamSize;

	~FileStream() 
	{
		
	}

protected:
	virtual bool Initialize() override
	{
		return SHCreateStreamOnFileA(m_lpSzFilePath, m_Fsaf | STGM_DIRECT | m_Fssf, &this->m_pUnkObject);
		ULARGE_INTEGER newPosition = { 0 };

		m_pUnkObject->Seek({ 0 }, STREAM_SEEK_END, &newPosition);
		m_StreamSize = newPosition.LowPart;
		m_pUnkObject->Seek({ 0 }, STREAM_SEEK_SET, &newPosition);
	}

private:
	const char* m_lpSzFilePath;
	FileStreamAccessFlags m_Fsaf;
	FileStreamShareFlags m_Fssf;
	unsigned int m_StreamSize;
};

class MemoryStream : public COMObjectBase<IStream>
{
public:
	MemoryStream(unsigned int streamSize, bool IsAddrOfMemoryStream = false)
		: m_StreamSize(streamSize), IsAddrOfMemStream(IsAddrOfMemoryStream)
	{
		m_hGlobal = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, m_StreamSize);
		CreateStreamOnHGlobal(m_hGlobal, true, &this->m_pUnkObject);
		std::cout << "MemoryStream object created @ 0x" << this << "\n";
	}

	inline byte operator[](unsigned int idx) const
	{
		if (idx < m_StreamSize) { return *(byte*)m_hGlobal; }
		return NULL;
	}
	
	inline void* operator&() { return (IsAddrOfMemStream) ? m_hGlobal : this; }

	bool IsAddrOfMemStream;
	const unsigned int& Size = m_StreamSize;

	inline ~MemoryStream() { GlobalFree(m_hGlobal); }
	
private:
	unsigned int m_StreamSize;
	HGLOBAL m_hGlobal;
	bool m_IsStreamLocked;
};

class IEventDispatcher
{
public:
	virtual void __fastcall Dispatch(const EventProcData& EventProcData) = 0;
};

class __declspec(novtable) EventBase : public EventObject
{
protected:
	virtual bool OnMove() { return false; }
	virtual bool OnQuit() { return false; }
	virtual bool OnClear() { return false; }
	virtual bool OnClose() { return false; }
	virtual bool OnCreate() { return false; }
	virtual bool OnResize() { return false; }
	virtual bool OnDestroy() { return false; }
	virtual bool OnMaximize() { return false; }
	virtual bool OnMinimize() { return false; }
	virtual bool OnDraw(bool redraw) { return false; }
	virtual bool OnFocus(bool isFocused) { return false; }
	virtual bool OnClick(const MouseEventArgs* pArgs) { return false; }
	virtual bool OnDrop(const DragDropEventArgs* pArgs) { return false; }
	virtual bool OnWndMove(const WindowEventArgs* pArgs) { return false; }
	virtual bool OnMouseMove(const MouseEventArgs* pArgs) { return false; }
	virtual bool OnMouseScroll(const MouseEventArgs* pArgs) { return false; }
};

class MouseEvent : private EventBase, public virtual IEventDispatcher
{
public:
	MouseEvent() { this->ObjectType = MOUSE; }

	virtual bool OnMouseScroll(const MouseEventArgs* pArgs) override = 0;
	virtual bool OnClick(const MouseEventArgs* pArgs) override = 0;
	virtual bool OnMouseMove(const MouseEventArgs* pArgs) override = 0;

private:
	void __fastcall Dispatch(const EventProcData& eventProcData) override final
	{
		MouseEventArgs mouseEventArgs;
		ZeroMemory(&mouseEventArgs, sizeof(MouseEventArgs));

		switch (eventProcData.msg) 
		{
		case WM_MOUSEWHEEL:
			mouseEventArgs.IsScrollBar = false;

			switch (eventProcData.wParam)
			{
			case 0x780000:
				mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::UP;
				break;

			case 0xFF880000:
				mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::DOWN;
				break;
			}

			this->OnMouseScroll(&mouseEventArgs);
			break;

		case WM_MOUSEFIRST:
			mouseEventArgs.mousePosition.x = ((eventProcData.lParam << 16) >> 16);
			mouseEventArgs.mousePosition.y = (eventProcData.lParam >> 16);
			this->OnMouseMove(&mouseEventArgs);
			
			break;

		case WM_RBUTTONUP:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::UP;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_RBUTTONDOWN:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::DOWN;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_LBUTTONUP:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::UP;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_LBUTTONDOWN:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::DOWN;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_MBUTTONUP:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::UP;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_MBUTTONDOWN:
			mouseEventArgs.mouseButton = (MouseEventArgs::MouseButton)eventProcData.msg;
			mouseEventArgs.mouseState = MouseEventArgs::MouseButtonState::DOWN;
			this->OnClick(&mouseEventArgs);
			break;

		case WM_VSCROLL:
			mouseEventArgs.IsScrollBar = true;
			mouseEventArgs.mouseState = (MouseEventArgs::MouseButtonState)eventProcData.wParam;
			this->OnMouseScroll(&mouseEventArgs);
			break;
		}
	}
};

class WindowEvent : private EventBase, public virtual IEventDispatcher
{
public:
	WindowEvent() { this->ObjectType = WINDOW; }

	virtual bool OnMove() override = 0;
	virtual bool OnQuit() override = 0;
	virtual bool OnClear() override = 0;
	virtual bool OnClose() override = 0;
	virtual bool OnCreate() override = 0;
	virtual bool OnResize() override = 0;
	virtual bool OnDestroy() override = 0;
	virtual bool OnMinimize() override = 0;
	virtual bool OnMaximize() override = 0;
	virtual bool OnDraw(bool redraw) override = 0;
	virtual bool OnFocus(bool isFocused) override = 0;
	virtual bool OnWndMove(const WindowEventArgs* pArgs) override = 0;

private:
	void __fastcall Dispatch(const EventProcData& EventProcData) override final
	{
		WindowEventArgs windowEventArgs;
		ZeroMemory(&windowEventArgs, sizeof(WindowEventArgs));
		PAINTSTRUCT ps = { 0 };

		switch (EventProcData.msg)
		{
		case WM_CREATE:	this->OnCreate(); break;
		case WM_DESTROY: this->OnDestroy(); break;
		case WM_MOVE: this->OnMove(); break;
		case WM_SIZE: this->OnResize(); break;
		case WM_SETFOCUS: this->OnFocus(true); break;
		case WM_KILLFOCUS: this->OnFocus(false); break;
		case WM_SETREDRAW: this->OnDraw(true); break;

		case WM_PAINT: 
			BeginPaint(EventProcData.hWnd, &ps);
			this->OnDraw(false); 
			
			EndPaint(EventProcData.hWnd, &ps);
			break;

		case WM_CLOSE: this->OnClose(); break;
		case WM_QUIT: this->OnQuit(); break;

		case WM_WINDOWPOSCHANGED:
			GetWindowRect(EventProcData.hWnd, &windowEventArgs.windowRect);
			this->OnWndMove(&windowEventArgs);
			break;

		case WM_SIZING: this->OnResize(); break;
		case WM_CLEAR: this->OnClear(); break;
		}
	}
};

class DragDrop : private EventBase, public virtual IEventDispatcher
{
public:
	DragDrop() { this->ObjectType = DRAG_DROP; }

	virtual bool OnDrop(const DragDropEventArgs* pArgs) override = 0;

private:
	void __fastcall Dispatch(const EventProcData& EventProcData) override final
	{
		unsigned int draggedFiles = 0;
		DragDropEventArgs dragDropEventArgs;
		ZeroMemory(&dragDropEventArgs, sizeof(DragDropEventArgs));

		switch (EventProcData.msg)
		{
		case WM_DROPFILES:
			DragQueryPoint((HDROP)EventProcData.wParam, (POINT*)&dragDropEventArgs.dropPosition);

			draggedFiles = DragQueryFileA((HDROP)EventProcData.wParam, 0xFFFFFFFF, nullptr, NULL);

			for (int i = WINDOW_EVENT_INDEX; i < draggedFiles; ++i) {
				dragDropEventArgs.fileNumber = i;
				DragQueryFileA((HDROP)EventProcData.wParam, i, &dragDropEventArgs.FilePath[0], 255);

				this->OnDrop(&dragDropEventArgs);

				ZeroMemory(&dragDropEventArgs.FilePath[0], 255);
			}
			break;
		}
	}
};

class KeyEvent : public EventBase, public virtual IEventDispatcher
{
public:
	KeyEvent() 
	{

	}

	~KeyEvent() 
	{

	}
private:

};

class NativeWndAttributes
{
public:
	NativeWndAttributes(const WndStyleProperties* pWndStyleProp, const WndPosition* wndPosition, bool AllowDragDrop)
		: m_WndPosition(*wndPosition), m_DragDrop(AllowDragDrop)
	{
		m_StyleProps.borderType._Types.wbt = WndBorderType::WBT_WND_STANDARD;
		m_StyleProps.style = WndStyle::WS_STANDARD;
	}

	Position& Position = m_WndPosition.Pos;
	Resolution& Resolution = m_WndPosition.Res;

	const WndPosition& WindowPosition = m_WndPosition;
	const WndStyleProperties& StyleProperties = m_StyleProps;
	bool& CanDragDrop = m_DragDrop;

	~NativeWndAttributes()
	{

	}

private:
	WndPosition m_WndPosition;
	WndStyleProperties m_StyleProps;
	bool m_DragDrop;
};

class Theme
{
public:
	Theme(const wchar_t* szThemeName) : m_ThemeHandle(OpenThemeData(NULL, szThemeName))
	{
		EnableTheming(true);
	}

	inline void Draw(const NativeWindow& NativeWndObj) const
	{
		RECT wndRect = { 0 };
		NativeWndObj.GetRectangle(&wndRect);
		//DrawThemeParentBackground(NativeWndObj.GetHandle(), GetDC(NativeWndObj.GetHandle()), &wndRect);
		switch (NativeWndObj.Type) 
		{
		case PROGRESS_BAR:
			DrawThemeBackground(m_ThemeHandle, GetDC(NativeWndObj.Handle), 
				PROGRESSPARTS::PP_FILL, PBST_NORMAL, &wndRect, NULL);
			break;

		case BUTTON:
			DrawThemeBackground(m_ThemeHandle, GetDC(NativeWndObj.Handle),
				BUTTONPARTS::BP_PUSHBUTTON, PBS_NORMAL, &wndRect, nullptr);
			break;
		}
	}

	~Theme()
	{
		EnableTheming(false);
		CloseThemeData(m_ThemeHandle);
	}

private:
	HTHEME m_ThemeHandle;
};

class Mutex
{
public:
	Mutex()
	{
		InitializeCriticalSection(&m_CriticalSection);
	}

	inline void Aquire() { EnterCriticalSection(&m_CriticalSection); }

	inline void Release() { LeaveCriticalSection(&m_CriticalSection); }

	inline ~Mutex() { this->Release(); }

private:
	CRITICAL_SECTION m_CriticalSection;
};

//Unused... Reserved for future use.
enum TranslatedEventCode : unsigned int
{
	WND_MOVE		= (((int)WM_MOVE) | (int)(1 << 16)),
	WND_QUIT		= (((int)WM_QUIT) | (int)(1 << 16)),
	WND_SIZE		= (((int)WM_SIZE) | (int)(1 << 16)),
	WND_CLEAR		= (((int)WM_CLEAR) | (int)(1 << 16)),
	WND_PAINT		= (((int)WM_PAINT) | (int)(1 << 16)),
	WND_CLOSE		= (((int)WM_CLOSE) | (int)(1 << 16)),
	WND_CREATE		= (((int)WM_CREATE) | (int)(1 << 16)),
	WND_SIZING		= (((int)WM_SIZING) | (int)(1 << 16)),
	WND_DESTROY		= (((int)WM_DESTROY) | (int)(1 << 16)),
	WND_SETREDRAW	= (((int)WM_SETREDRAW) | (int)(1 << 16)),
	
	MSE_RBUTTONUP	= (((int)WM_RBUTTONUP) | (int)(2 << 16)),
	MSE_LBUTTONUP	= (((int)WM_LBUTTONUP) | (int)(2 << 16)),
	MSE_MBUTTONUP	= (((int)WM_MBUTTONUP) | (int)(2 << 16)),
	MSE_LBUTTONDOWN = (((int)WM_LBUTTONDOWN) | (int)(2 << 16)),
	MSE_RBUTTONDOWN = (((int)WM_RBUTTONDOWN) | (int)(2 << 16)),
	MSE_MBUTTONDOWN = (((int)WM_RBUTTONDOWN) | (int)(2 << 16)),
	
	KBD_UP			= (((int)WM_KEYUP) | (int)((3 << 16))),
	KBD_DOWN		= (((int)WM_KEYDOWN) | (int)((3 << 16)))
};

class Window : public NativeWindow
{
public:
	Window(const char* szWndTitle, const NativeWndAttributes& WndAttrib, const EventProxy& eventProxy)
		: NativeWindow(szWndTitle, &WndAttrib.WindowPosition, NATIVEWINDOW, "WindowClass1")
	{
		std::cout << "Creating Window...\n";

		WndPosition windowPos =
		{
			WndAttrib.Position.x,
			WndAttrib.Position.y,
			WndAttrib.Resolution.height,
			WndAttrib.Resolution.width
		};

		this->Create(&WndAttrib.StyleProperties);
	
		DragAcceptFiles(this->Handle, WndAttrib.CanDragDrop);

		m_DebugObject.Status(this->Show(true), __FUNCTION__);

		if (!windowPos.Pos.x && !windowPos.Pos.y) { this->WindowPosition = windowPos; }
		m_PrintWndInfo(WndAttrib, &this->WindowPosition);
	}

	virtual bool Poll() const override { return NativeWindow::Poll(); }

	inline bool Add(NativeWindow* pNativeWnd) 
	{
		return pNativeWnd->Show(true);
	}

	inline void Top() const { BringWindowToTop(this->Handle); }

	inline ~Window() { DragAcceptFiles(this->Handle, false); }

private:
	inline void m_PrintWndInfo(const NativeWndAttributes& wndAttributes, const WndPosition* windowPos) const
	{
		std::cout << "---------Window Information---------\n";
		std::cout << "Window Object Address: 0x" << std::hex << (void*)this << std::dec << "\n";
		std::cout << "Window Handle: 0x" << std::hex << this->Handle << std::dec << "\n";
		std::cout << "Window Title: " << this->GetText() << "\n";
		std::cout << "Width: " << windowPos->Res.width << "\n";
		std::cout << "Height: " << windowPos->Res.height << "\n";
		std::cout << "Window Position -> X: " << windowPos->Pos.x << " Y: " << windowPos->Pos.y << "\n";
	}
};

class Surface : public DebugBase
{
public:
	Surface(Window& renderWindow, unsigned int ClearColour) : m_SurfaceWnd(renderWindow), DebugBase(ResultType::GDI)
	{
		this->Status(GdipCreateFromHWND(renderWindow.Handle, &m_GfxCtx), __FUNCTION__);
	}
	
	inline GpGraphics* operator*() const { return m_GfxCtx; }

	inline bool Clear() const { return GdipGraphicsClear(m_GfxCtx, m_ClearColour); }

	const Window& RenderWindow = m_SurfaceWnd;

	inline ~Surface() { GdipDeleteGraphics(m_GfxCtx); }

private:
	unsigned long m_dwGdipToken;
	unsigned long m_ClearColour;
	GpGraphics* m_GfxCtx;
	Window& m_SurfaceWnd;
};

using RenderPosition = WndPosition;

class Renderable : public DebugBase
{
public:
	Renderable(DrawObject* pDrawObject = nullptr) : DebugBase(ResultType::GDI), ObjectPosition({NULL}), DrawingObject(pDrawObject) { }

	virtual bool Render(const Surface& RenderSurface) = 0;

	const Position& position = ObjectPosition.Pos;

	virtual ~Renderable() { }

protected:
	WndPosition ObjectPosition;	
	DrawObject* DrawingObject;
};

class Square : public Renderable 
{
public:
	Square(DrawObject* pDrawObject, const Position* const objPosition, unsigned int perimeterPxl) : Renderable(pDrawObject)
	{
		unsigned int OneSideLen = perimeterPxl / 4;

		m_PointPth[0].Start = *objPosition;
		m_PointPth[0].End.x = (m_PointPth[0].Start.x + OneSideLen);
		m_PointPth[0].End.y = m_PointPth[0].Start.y;
		
		m_PointPth[1].Start = m_PointPth[0].End;
		m_PointPth[1].End.x = m_PointPth[0].End.x;
		m_PointPth[1].End.y = (m_PointPth[1].Start.y + OneSideLen);
		
		m_PointPth[2].Start.x = m_PointPth[0].Start.x;
		m_PointPth[2].Start.y = (m_PointPth[0].Start.y + OneSideLen);
		m_PointPth[2].End.x = m_PointPth[0].End.x;
		m_PointPth[2].End.y = (m_PointPth[0].End.y + OneSideLen);
		
		m_PointPth[3].Start = m_PointPth[0].Start;
		m_PointPth[3].End.x = m_PointPth[3].Start.x;
		m_PointPth[3].End.y = (m_PointPth[0].Start.y + OneSideLen);
	}

	virtual bool Render(const Surface& RenderSurface) override
	{
		if((*DrawingObject)->Type != GdiObjType::GDI_PEN) { return false; }

		for (int i = 0; i < 4; ++i) {
			this->Status(GdipDrawLineI(*RenderSurface, (*DrawingObject)->Object.Pen, m_PointPth[i].Start.x,
				m_PointPth[i].Start.y, m_PointPth[i].End.x, m_PointPth[i].End.y), __FUNCTION__);
		}
		return true;
	}

	~Square() 
	{

	}

private:
	Route m_PointPth[4] = { 0 };
};

class EventObj : public MouseEvent
{
public:
	EventObj()
	{

	}

	~EventObj()
	{

	}

protected:
	virtual bool OnClick(const MouseEventArgs* pArgs) override
	{
		if (pArgs->mouseState == MouseEventArgs::MouseButtonState::DOWN) {
			switch (pArgs->mouseButton) 
			{
			case MouseEventArgs::MouseButton::LEFT: std::cout << "Left"; break;
			case MouseEventArgs::MouseButton::RIGHT: std::cout << "Right"; break;
			case MouseEventArgs::MouseButton::MIDDLE: std::cout << "Middle"; break;
			}
			std::cout << " mouse button clicked!\n";
		}
		return true;
	}

	virtual bool OnMouseScroll(const MouseEventArgs* pArgs) override
	{
		std::cout << "OnMouseScroll!\n";
		return true;
	}

	virtual bool OnMouseMove(const MouseEventArgs* pArgs) override 
	{
		std::cout << "X: " << pArgs->mousePosition.x;
		std::cout << " Y: " << pArgs->mousePosition.y << "\n";
		return true;
	}

private:
	int a = 0;
};

class DragDropObj : public DragDrop
{
public:
	DragDropObj() 
	{
	}

	~DragDropObj()
	{

	}

protected:
	virtual bool OnDrop(const DragDropEventArgs* pArgs) override 
	{
		std::cout << "Position File Dropped-> X: " << pArgs->dropPosition.x << " Y: " << pArgs->dropPosition.y << "\n";
		std::cout << "File Dropped: " <<(char*)&pArgs->FilePath[0] << "\n";
		return true;
	}

private:
};

class Button : public NativeWindow
{
public:
	Button(const Window& parentWindow, const char* buttonText, const WndPosition* pWndPosition, BtnBorderType borderType)
		: NativeWindow(buttonText, pWndPosition, BUTTON, "BUTTON")
	{
		WndStyleProps.borderType.dwStyle |= borderType;
		this->Create(&this->WndStyleProps, nullptr, parentWindow.Handle);
		std::cout << "Button object created @ 0x" << this << "\n";
	}

	~Button()
	{

	}
};

class ProgressBar : public NativeWindow
{
public:
	ProgressBar(const Window& parentWindow, const WndPosition* pWndPosition, PBBorderType borderType)
		: NativeWindow(nullptr, pWndPosition, PROGRESS_BAR, PROGRESS_CLASSA)
	{
		WndStyleProps.borderType.dwStyle |= borderType;
		this->Create(&this->WndStyleProps, nullptr, parentWindow.Handle);

		SendMessageA(this->Handle, PBM_SETRANGE, NULL, ((DWORD)maxRange << 16) | minRange);
		SendMessageA(this->Handle, PBM_SETSTEP, 4, 0);
		std::cout << "ProgressBar object created @ 0x" << this << "\n";
	}

	void Increment() 
	{
		SendMessageA(this->Handle, PBM_STEPIT, NULL, NULL);
	}

	~ProgressBar() 
	{

	}
private:
	unsigned short minRange = 0;
	unsigned short maxRange = 8192;
};

class TextBox : public NativeWindow 
{
public:
	TextBox(const Window& parentWindow, const char* szText, const WndPosition* pWndPosition, TxtBoxBorderType borderType)
		: NativeWindow(szText, pWndPosition, TEXTBOX, "Edit")
	{
		WndStyleProps.borderType.dwStyle = borderType;
		m_DebugObject.Status(this->Create(&this->WndStyleProps, nullptr, parentWindow.Handle), __FUNCTION__);
		std::cout << "TextBox object created @ 0x" << this << "\n";
	}
	
	~TextBox() 
	{

	}

private:

};

class Scrollbar : public NativeWindow, public MouseEvent
{ 
public:
	Scrollbar(const Window& parentWindow, const WndPosition* pWndPos, ScrollBorderType borderType)
		: NativeWindow(nullptr, pWndPos, NativeWndType::SCROLL_BAR, "SCROLLBAR")
	{
		WndStyleProps.borderType.dwStyle = borderType;
		WndStyleProps.style = WndStyle::WS_STANDARD;

		this->Create(&this->WndStyleProps);

		SetScrollRange(this->Handle, 0, 0, 100, false);
	}

	inline bool Add(NativeWindow* pNativeWnd) { return m_AttachedObjs.Add(pNativeWnd); }

	inline bool Add(ArrayList<NativeWindow*>& nativeWndArray)
	{
		for (int i = 0; i < nativeWndArray.Position; ++i) 
		{
			if (!m_AttachedObjs.Add(nativeWndArray[i])) 
			{ 
				return false; 
			}
		}
		return true;
	}

	inline bool Add(NativeWindow** const ppNativeWnds, unsigned int count) 
	{
		for (int i = 0; i < count; ++i) 
		{ 
			m_AttachedObjs.Add(ppNativeWnds[i]); 
		}

		return true;
	}

	inline ~Scrollbar() 
	{

	}

protected:
	virtual bool OnMouseScroll(const MouseEventArgs* pArgs) override 
	{
		if (!pArgs->IsScrollBar) { return false; }

		WndPosition pos = { 0 };

		switch (pArgs->mouseState)
		{
		case MouseEventArgs::MouseButtonState::UP:
			for (int i = 0; i < m_AttachedObjs.Position; ++i) {
				pos = m_AttachedObjs[i]->Position;
				pos.Pos.y += 2;
				m_AttachedObjs[i]->Move(&pos);
			}
			break;

		case MouseEventArgs::MouseButtonState::DOWN:
			for (int i = 0; i < m_AttachedObjs.Position; ++i) {
				pos = m_AttachedObjs[i]->Position;
				pos.Pos.y -= 2;
				m_AttachedObjs[i]->Move(&pos);
			}
			break;
		}
		
		return true;
	}

	virtual bool OnClick(const MouseEventArgs* pArgs) override { return true; }

	virtual bool OnMouseMove(const MouseEventArgs* pArgs) override { return true; }
	
private:
	ArrayList<NativeWindow*> m_AttachedObjs;
};

class VideoPlayer : public NativeWindow, public COMObjectBase<IWMPControls>
{
public:
	VideoPlayer(const Window& parentWindow, const WndPosition* pWndPosition, const EventProxy& evtProxy)
		: NativeWindow(nullptr, pWndPosition, EMBEDDED_WND, "VideoPlayerWnd1"),
		m_WmpPlayer(__uuidof(WindowsMediaPlayer))
	{
		this->Create(&this->WndStyleProps, *evtProxy, parentWindow.Handle);
		this->Show(false);

		IAxWinHostWindow* pAxWinHostWindow = nullptr;

		IUnknown* pUnk = (IUnknown*)SendMessageA(this->Handle, WM_ATLGETHOST, NULL, NULL);
		pUnk->QueryInterface(__uuidof(IAxWinHostWindow), (void**)&pAxWinHostWindow);
		CWindow win;
		
		

		pAxWinHostWindow->AttachControl(*m_WmpPlayer, this->Handle);
		
		axWindow.Attach(this->Handle);
		m_DebugObject.Status(axWindow.AttachControl(*m_WmpPlayer, nullptr), __FUNCTION__);
		m_DebugObject.Status(m_WmpPlayer->get_controls(&m_WmpCtrls), __FUNCTION__);
		m_DebugObject.Status(m_WmpPlayer->get_settings(&m_WmpSettings), __FUNCTION__);
		m_DebugObject.Status(m_WmpPlayer->put_uiMode((BSTR)L"none"), __FUNCTION__);
		m_DebugObject.Status(m_WmpPlayer->put_enableContextMenu(false), __FUNCTION__);

		this->Initialize();
		this->Show(true);
		std::cout << "VideoPlayer object created @ 0x" << this << "\n";
	}

	inline HRESULT SetFilePath(const wchar_t* lpszFilePath) {  return m_WmpPlayer->put_URL((BSTR)lpszFilePath);  }

	~VideoPlayer()
	{
		m_WmpSettings->Release();
		m_WmpCtrls->Release();
		CloseWindow(axWindow.m_hWnd);
		DestroyWindow(axWindow.m_hWnd);
	}

protected:
	virtual bool Initialize() override 
	{
		m_pUnkObject = m_WmpCtrls; 
		return true;
	}

private:
	CAxWindow2 axWindow;
	COMObjectBase<IWMPPlayer4> m_WmpPlayer;
	IWMPControls* m_WmpCtrls;
	IWMPSettings* m_WmpSettings;
};

class AudioPlayer : public COMObjectBase<IWMPControls>
{
public:
	AudioPlayer() : m_WmpPlayer(__uuidof(WindowsMediaPlayer))
	{
		m_WmpPlayer->get_controls(&m_WmpCtrls);
		this->Initialize();
		std::cout << "AudioPlayer object created @ 0x" << this << "\n";
	}

	inline HRESULT SetFilePath(const wchar_t* szfilePath) { return m_WmpPlayer->put_URL((BSTR)szfilePath); }

	~AudioPlayer() 
	{
		m_WmpCtrls->Release();
		m_WmpPlayer->Release();
	}

private:
	virtual bool Initialize() override 
	{
		m_pUnkObject = m_WmpCtrls; 
		return true; 
	}

	COMObjectBase<IWMPPlayer4> m_WmpPlayer;
	IWMPControls* m_WmpCtrls;
};

class NativePicture : public NativeWindow, public WindowEvent
{
public:
	NativePicture(const Window& parentWindow, const wchar_t* const szFilePath, const WndPosition* pWndPos, const EventProxy& eventProxy)
		: NativeWindow(nullptr, pWndPos, EMBEDDED_WND, "PictureWnd")
	{
		this->Create(&this->WndStyleProps, *eventProxy, parentWindow.Handle);
		m_DebugObject.Status(GdipLoadImageFromFile(szFilePath, &m_Image), __FUNCTION__);
		this->GetImageSize(&m_ImageSize);

		m_DebugObject.Status(GdipCreateFromHWND(parentWindow.Handle, &m_GfxCtx), __FUNCDNAME__);
		this->Draw();

		std::cout << "Picture object created @ 0x" << this << "\n";
	}

	NativePicture(const Window& parentWindow, COMObjectBase<IStream>& ComObjectStream, const WndPosition* pWndPos, const EventProxy& eventProxy)
		: NativeWindow(nullptr, pWndPos, EMBEDDED_WND, "PictureWnd")
	{
		LARGE_INTEGER BackupStreamPos = { 0 };
		ComObjectStream->Seek(BackupStreamPos, STREAM_SEEK_CUR, (ULARGE_INTEGER*)&BackupStreamPos);
		
		ComObjectStream->Commit(STGC_OVERWRITE);
		LARGE_INTEGER seekPos = { 0 };
		ComObjectStream->Seek(seekPos, STREAM_SEEK_SET, (ULARGE_INTEGER*)&seekPos);

		m_DebugObject.Status(GdipLoadImageFromStream(*ComObjectStream, &m_Image), __FUNCTION__);
		this->GetImageSize(&m_ImageSize);

		this->Create(&this->WndStyleProps, *eventProxy, parentWindow.Handle);
		m_DebugObject.Status(GdipCreateFromHWND(parentWindow.Handle, &m_GfxCtx), __FUNCTION__);

		this->Draw();

		ULARGE_INTEGER newSeekPosition = { 0 };
		ComObjectStream->Seek(BackupStreamPos, STREAM_SEEK_SET, &newSeekPosition);

		std::cout << "Picture object created @ 0x" << this << "\n";
	}

	const WndPosition& ImageSize = m_ImageSize;

	inline ~NativePicture()
	{ 
		GdipDisposeImage(m_Image);
		GdipDeleteGraphics(m_GfxCtx);
	}

protected:
	virtual bool OnMove() override { return this->Draw(); }
	virtual bool OnQuit() override { return true; }
	virtual bool OnClear() override { return true; }
	virtual bool OnClose() override { return true; }
	virtual bool OnCreate() override { return true; }
	virtual bool OnResize() override { return true; }
	virtual bool OnDestroy() override { return true; }
	virtual bool OnMinimize() override { return true; }
	virtual bool OnMaximize() override { return true; }
	virtual bool OnDraw(bool redraw) override { return this->Draw(); }
	virtual bool OnFocus(bool isFocused) override { return isFocused; }
	virtual bool OnWndMove(const WindowEventArgs* pArgs) override { return true; }

private:
	inline bool Draw() const 
	{
		return (!GdipDrawImageRectI(m_GfxCtx, m_Image, this->WindowPosition.Pos.x,
			this->WindowPosition.Pos.y, this->WindowPosition.Res.width,
			this->WindowPosition.Res.height)) ? true : false;
	}

	inline bool GetImageSize(WndPosition* pWndPosition) const
	{
		if (!pWndPosition) { return false; }
		GdipGetImageHeight(m_Image, (unsigned int*)&pWndPosition->Res.height);
		GdipGetImageWidth(m_Image, (unsigned int*)&pWndPosition->Res.width);
		pWndPosition->Pos = this->Position.Pos;
		return true;
	}

private:
	GpImage* m_Image;
	GpGraphics* m_GfxCtx;
	WndPosition m_ImageSize;
};

//FOR DEBUGGING PURPOSES ONLY!!!
struct StackInfo { const void *reserved1, *reserved2; };

//FOR DEBUGGING PURPOSES ONLY!!!
 __forceinline __declspec(naked) void __fastcall GetStackInfo(StackInfo* stackInfo)
{
	_asm 
	{
		mov DWORD ptr [ecx], ebp
		add ecx, 4
		mov DWORD ptr [ecx], esp
		sub ecx, 4
		ret
	}
}

//DEBUG PURPOSES ONLY!!!
class StackTracer 
{
public:
	inline StackTracer(const StackInfo* si) : pStackBase(si->reserved1), pStackPtr(si->reserved2)
	{
		m_StackSize = (((unsigned int)pStackBase) - ((unsigned int)pStackPtr));
	}

	inline unsigned int GetStackSize() const { return m_StackSize; }
	inline const void* GetStackData() const  { return pStackBase; }

	inline ~StackTracer() 
	{

	}

private:
	const void* const pStackBase;
	const void* const pStackPtr;
	unsigned int m_StackSize;
};

class NativeTheme
{
public:
	NativeTheme(const NativeWindow& parentWindow)
	{
		const wchar_t* lpWThemeType = nullptr;
		
		switch (parentWindow.Type)
		{
		case NativeWndType::EMBEDDED_WND:
			lpWThemeType = L"Window";
			break;

		case NativeWndType::BUTTON:
			lpWThemeType = L"Button";
			break;

		case NativeWndType::LABEL:
			lpWThemeType = L"Label";
			break;

		case NativeWndType::NATIVEWINDOW:
			lpWThemeType = L"Window";
			break;

		case NativeWndType::PROGRESS_BAR:
			lpWThemeType = L"ProgressBar";
			break;

		case NativeWndType::SCROLL_BAR:
			lpWThemeType = L"ScrollBar";
			break;

		case NativeWndType::TEXTBOX:
			lpWThemeType = L"TextBox";
			break;
		}

		OpenThemeData(*parentWindow, lpWThemeType);
	}

	~NativeTheme() 
	{
		CloseThemeData(m_hTheme);
	}

private:
	HTHEME m_hTheme;
};

class NativeFont : public DebugBase
{
public:
	NativeFont(const wchar_t* lpSzFontFamily, float Size, FontStyle fStyle, const Surface& RenderSurface) 
		: Style(fStyle), m_FontSize(Size), DebugBase(ResultType::GDI)
	{
		this->Status(GdipCreateFontFamilyFromName(lpSzFontFamily, nullptr, &m_FontFamily), __FUNCTION__);

		this->Status(GdipCreateFont(m_FontFamily, Size, fStyle, GpUnit::UnitPixel, &m_NativeFont), __FUNCTION__);
		std::cout << "NativeFont object created @ 0x" << this << "\n";
	}

	inline GpFont* operator*() const { return m_NativeFont; }

	const float& Size = m_FontSize;
	FontStyle Style;

	~NativeFont()
	{
		GdipDeleteFont(m_NativeFont);
		GdipDeleteFontFamily(m_FontFamily);
	}

private:
	GpFontFamily* m_FontFamily;
	GpFont* m_NativeFont;

	float m_FontSize;
};

class NativeBrush : public DrawObject
{
public:
	NativeBrush(unsigned int argb) 
		: DrawObject(GDI_BRUSH), Colour(argb)
	{
		GdipCreateSolidFill(Colour, (GpSolidFill**)&this->m_GdiDrawObject.Object.Brush);
		std::cout << "NativeBrush object created @ 0x" << this << "\n";
	}

	NativeBrush(const Marker& markerObject) 
		: DrawObject(GDI_BRUSH), Colour(markerObject.Colour)
	{
		GdipGetPenBrushFill((*markerObject).Object.Pen, &this->m_GdiDrawObject.Object.Brush);
		std::cout << "NativeBrush object created @ 0x" << this << "\n";
	}

	unsigned int Colour;

	inline ~NativeBrush()
	{
		GdipDeleteBrush(this->m_GdiDrawObject.Object.Brush);
	}
};

class NativeStrFormat
{
public:
	NativeStrFormat()
	{
		if (GdipCreateStringFormat(StringFormatFlags::StringFormatFlagsDisplayFormatControl, 0, &m_StringFormat))  {
			std::cout << "Failed to create string format...\n";
			return;
		}

		std::cout << "NativeStrFormat object created @ 0x" << this << "\n";
	}

	inline GpStringFormat* operator*() const { return m_StringFormat; }

	inline ~NativeStrFormat() { GdipDeleteStringFormat(m_StringFormat); }

private:
	GpStringFormat* m_StringFormat;
};

struct TextParams
{
	NativeFont* pStringFont;
	NativeStrFormat* pStringFormat;
	NativeBrush* pNativeBrush;
};

class Text : public Renderable
{
public:
	Text(const wchar_t* lpszString, const Position* pPosition, const TextParams& textParams)
		: m_StringLength(lstrlenW(lpszString)), m_Font(textParams.pStringFont), m_NativeBrush(textParams.pNativeBrush),
		m_TextPosition(*pPosition), m_NativeStringFormat(textParams.pStringFormat), m_lpSzString(lpszString)
	{
		m_TextRectangle.Height = (float)m_Font->Size * 2;
		m_TextRectangle.Width = pow((float)m_StringLength, 2.0f);
		m_TextRectangle.X = (float)pPosition->x;
		m_TextRectangle.Y = (float)pPosition->y;

		std::cout << "Text object created @ 0x" << this << "\n";
	}

	virtual bool Render(const Surface& renderSurface) override 
	{
		if ((**m_NativeBrush).Type != GDI_BRUSH) { return false; }
		this->Status(GdipDrawString(*renderSurface, m_lpSzString, m_StringLength, *(*m_Font),
			&m_TextRectangle, *(*m_NativeStringFormat), (*(*m_NativeBrush)).Object.Brush), __FUNCTION__);
		return true;
	}

	const unsigned int& TextLength = m_StringLength;
	
	~Text()
	{
		
	}

private:
	const wchar_t* m_lpSzString;
	unsigned int m_StringLength;
	NativeFont* m_Font;
	Position m_TextPosition;
	RectF m_TextRectangle;
	NativeStrFormat* m_NativeStringFormat;
	NativeBrush* m_NativeBrush;
};

class Label : public NativeWindow
{
public:
	Label(const Window& parentWindow, const char* lpszText, const WndPosition* pWndPos, LblBorderType borderType)
		: NativeWindow(lpszText, pWndPos, NativeWndType::LABEL, "STATIC")
	{
		WndStyleProperties wndProps;
		ZeroMemory(&wndProps, sizeof(WndStyleProperties));

		wndProps.borderType._Types.lbt = borderType;
		this->Create(&wndProps, nullptr, parentWindow.Handle);
	}

	~Label()
	{

	}

private:

};

class MsgBox
{
public:
	MsgBox(const Window* pParentWindow) : m_ParentWindow(pParentWindow)
	{

	}

	~MsgBox() 
	{

	}

private:
	const Window* m_ParentWindow;
};

int main(int argc, const char** argv)
{
	Win32WndCtrl::ServiceManager sm;
	//sm.Enable(Win32WndCtrl::ServiceID::ENABLE_COM);
	sm.Enable(Win32WndCtrl::ServiceID::ENABLE_ALL);

	EventProxy eventProxy;

	WndPosition windowPos = { 0 };
	windowPos.Res.width = 1600;
	windowPos.Res.height = 900;
	
	WndStyleProperties wndStyleProp;
	wndStyleProp.borderType._Types.wbt = WBT_WND_STANDARD;
	wndStyleProp.style = WS_STANDARD;

	NativeWndAttributes wndAttrib(&wndStyleProp, &windowPos, true);
	Window window("New Application", wndAttrib, eventProxy);

	EventObj eObj;
	eventProxy.Attach(&eObj);

	DragDropObj ddObj;
	eventProxy.Attach(&ddObj);

	Marker pen(0xFF000000, 3);
	Surface WndSurface(window, 0xFFFFFFF0);
	WndSurface.Clear();
	
	FileStream fs("U:\\00000000003\\0000.png", FileStreamAccessFlags::FSAF_READ, FileStreamShareFlags::FSSF_NOSHARE);

	
	
	//HANDLE hFile = CreateFileA("U:\\00000000003\\_1_d3a799ff06a3589bb477145338106692.jpg", GENERIC_READ, NULL, nullptr, OPEN_EXISTING, NULL, NULL);
	//unsigned long FileSize = GetFileSize(hFile, nullptr);

	MemoryStream ms(4096);
	ms.IsAddrOfMemStream = true;
	
	unsigned long cbRead = 0;

	byte ReadBuffer[256] = { 0 };
	ms->Read(ReadBuffer, 256, &cbRead);
	ms->Seek({ 0 }, STREAM_SEEK_SET, nullptr);
	
	//byte* pStreamData = (byte*)&ms;
	//unsigned long dwBytesRead = 0;
	//ReadFile(hFile, &ms, FileSize, &dwBytesRead, nullptr);
	//CloseHandle(hFile);

	WndPosition picPos = { 0 };
	picPos.Pos.x = 850;
	picPos.Pos.y = 400;
	picPos.Res.width = 480;
	picPos.Res.height = 272;

	NativePicture pic(window, fs, &picPos, eventProxy);
	eventProxy.Attach(&pic);

	Position position = { 300, 400 };
	Square square(&pen, &position, 400);

	WndPosition buttonPos = { 0 };
	buttonPos.Res.height = 75;
	buttonPos.Res.width = 250;
	buttonPos.Pos = position;

	Theme buttonTheme(L"Button");

	Button button(window, "OK", &buttonPos, BtnBorderType::BBT_BTN_PUSHBUTTON);
	buttonPos.Pos.x += button.Position.Res.width;
	Button button2(window, "Cancel", &buttonPos, BtnBorderType::BBT_BTN_PUSHBUTTON);
	
	position.x = 500;
	//SetWindowTheme(window.Handle, L"Explorer", nullptr);

	WndPosition textBoxPos = { 0 };
	textBoxPos.Res.width = 500;
	textBoxPos.Res.height = 50;
	textBoxPos.Pos.x = 200;
	textBoxPos.Pos.y = 350;
	
	TextBox textBox(window, "my g.", &textBoxPos, TxtBoxBorderType::TXTBT_TXT_STANDARD);

	//Win32WndCtrl::WndEventHndlr::Add(&window);

	position.y -= 150;

	WndPosition progBarPosition = { 0 };
	progBarPosition.Res.width = 400;
	progBarPosition.Res.height = 35;
	progBarPosition.Pos = position;

	square.Render(WndSurface);
	ProgressBar progBar(window, &progBarPosition, PBBorderType::PBBT_PB_SMOOTH);

	Position TextPosition = { 0 };
	TextPosition.x = 350;
	TextPosition.y = 500;

	NativeStrFormat stringFormatting;

	NativeBrush textBrush(0xFF0000FF);
	NativeFont textFont(L"Arial", 20.0f, FontStyle::FontStyleRegular, WndSurface);

	TextParams textParams = { &textFont, &stringFormatting, &textBrush };
	Text textWithFontSupport(L"my g with the fancy fonts...", &TextPosition, textParams);

	WndPosition vpPosition = { 0 };
	vpPosition.Res.width = 426;
	vpPosition.Res.height = 240;

	COMObjectBase<IFileOpenDialog> openFileWnd(__uuidof(FileOpenDialog));
	openFileWnd->SetTitle(L"Open a file...");
	openFileWnd->Show(window.Handle);

	VideoPlayer vp(window, &vpPosition, eventProxy);
	vp.SetFilePath(L"U:\\00000000005\\YouTube\\Trailers\\Forza Horizon 3 Official Launch Trailer.mp4");
	vp->play();

	AudioPlayer ap;
	ap.SetFilePath(L"U:\\00000000008\\Officialmunroe x Childsplaybeats  BANDOE   UK DRILL TYPE BEAT 60 Lease.mp3");
	ap->play();

	WndPosition scrollPos = { 0 };
	scrollPos.Res.height = 150;
	scrollPos.Res.width = 20;
	scrollPos.Pos.x = 820;
	scrollPos.Pos.y = 400;

	Scrollbar scrollBar(window, &scrollPos, ScrollBorderType::SBBT_VERTICAL);
	eventProxy.Attach(&scrollBar);

	NativeWindow* objectArray[2] = { nullptr };
	objectArray[0] = &button;
	objectArray[1] = &button2;
	
	scrollBar.Add(objectArray, 2);

	StackInfo stackInfo = { 0 };
	GetStackInfo(&stackInfo);
	StackTracer st(&stackInfo);

	const void* stackBase = st.GetStackData();
	std::cout << "Stack Base: 0x" << stackBase << "\n";
	std::cout << "Stack Size: " << st.GetStackSize() << " bytes...\n";

	textWithFontSupport.Render(WndSurface);

	while (true) 
	{	
		progBar.Increment();
		eventProxy.Poll(&window);
		window.Poll();
	}

	getchar();
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file


class String
{
public:
	String() : m_IsUsingSBO(true), m_StringBuffer(nullptr), m_Length(32)
	{
		RtlSecureZeroMemory(m_SBO, 32);
	}

	const unsigned int& Length = m_Length;

	String(const char* lpszArg) : m_StringBuffer(nullptr)
	{
		m_IsStringDataOwned = false;
	}

	String(String&& string)
	{
		m_IsStringDataOwned = true;
	}

	inline const char* operator*() const
	{
		return (this->m_IsUsingSBO) ? &this->m_SBO[0] : &this->m_StringBuffer[0];
	}

	void operator+=(String& concatString)
	{
		char* pSrcBuff = nullptr;
		char* pDestBuff = nullptr;

		if (concatString.m_IsStringDataOwned && this->m_IsStringDataOwned)
		{
			if (!this->m_IsUsingSBO)
			{
				this->DeterminePtrDest(concatString, pSrcBuff, pDestBuff);

				memcpy(this->m_StringBuffer, pSrcBuff, concatString.m_Length);
			}
		}

		memcpy(pDestBuff, pSrcBuff, concatString.Length);
	}

	bool operator==(const String& val)
	{

	}

	bool operator==(const char* copyAssign)
	{
		
	}

	void operator=(const String& copyAssign)
	{
		this->Clear((this->m_Length > copyAssign.Length) ? false : true);

		this->m_IsUsingSBO = copyAssign.m_IsUsingSBO;
		this->m_IsStringDataOwned = copyAssign.m_IsStringDataOwned;
		this->m_Length = copyAssign.m_Length;

		if (this->m_IsStringDataOwned)
		{
			if (this->m_IsUsingSBO)
			{
				memcpy(m_SBO, copyAssign.m_SBO, 32);
			}
			else
			{
				if (m_StringBuffer)
				{
					memcpy(m_StringBuffer, copyAssign.m_StringBuffer, this->Length);
				}
				else
				{
					this->AllocStrBuffer(this->Length);
					memcpy(m_StringBuffer, copyAssign.m_StringBuffer, this->Length);
				}
			}

		}
		else
		{
			this->m_StringBuffer = copyAssign.m_StringBuffer;
		}

	}

	~String()
	{
		this->Reset(true);
	}

private:
	bool m_IsStringDataOwned;
	bool m_IsUsingSBO;
	unsigned int m_Length;
	char* m_StringBuffer;
	char m_SBO[33];

	void Reset(bool delStrBuff)
	{

	}

	inline void Clear(bool clearHeapBuff)
	{
		RtlSecureZeroMemory((clearHeapBuff) ? this->m_StringBuffer : this->m_SBO, this->Length);
	}

	inline bool Copy(String& str)
	{
		if (this->m_IsUsingSBO)
		{
			if (this->m_IsStringDataOwned)
			{
				if (this->m_StringBuffer)
				{

				}
				else
				{
					this->AllocStrBuffer(strlen(this->m_SBO) + str.Length);
					memcpy(this->m_StringBuffer, (str.m_IsUsingSBO) ? str.m_SBO : str.m_StringBuffer, str.Length);
					this->Clear(false);
				}
			}
		}
	}

	inline bool GetRequiredBufferSize(String& str, bool copy)
	{
		if (copy)
		{
			if (this->m_IsStringDataOwned)
			{

			}
			else
			{

			}
		}
		else
		{

		}
	}

	inline bool AllocStrBuffer(unsigned int buffSize)
	{
		if (m_IsStringDataOwned && m_StringBuffer)
		{
			delete m_StringBuffer;
		}

		m_StringBuffer = new char[buffSize + 1];
		RtlSecureZeroMemory(m_StringBuffer, (buffSize + 1));

		return true;
	}

	inline bool DeterminePtrDest(String& rhs, char*& rpSrcBuff, char*& rpDestBuff)
	{
		char* srcBuffer = (rhs.m_IsUsingSBO) ? &rhs.m_SBO[0] : &rhs.m_StringBuffer[0];
		char* destBuffer = (this->m_IsUsingSBO) ? &this->m_SBO[0] : &this->m_StringBuffer[0];

		rpSrcBuff = srcBuffer;
		rpDestBuff = destBuffer;

		return true;
	}
};
