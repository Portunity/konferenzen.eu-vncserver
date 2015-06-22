/*
	Konferenzen.eu Präsentationsserver. Einfacher vorkonfigurierter VNC Client für Windows
	Copyright(C) 2015 Portunity GmbH, author: Benjamin Dürholt

	This program is free software : you can redistribute it and / or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.If not, see <http://www.gnu.org/licenses/>

	to contact us see our website at <http://www.portunity.de>
*/

#include <rfb/rfb.h>
#include <rfb/rfbclient.h>

#include <windows.h>
#include <gdiplus.h>
using namespace Gdiplus;
#include <Shlwapi.h>
#include <Richedit.h>

#include <math.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <fstream>
#include <ctime>

#include "resources.h"
#include "PresentationServer.h"

// Global Variables:
std::unique_ptr<PresentationServer> g_server; //VNC Server mit ScreenCapturing
std::wstring g_lastErrorString; //Ausgabe der Verbindungsfehler
std::shared_ptr<ScreenCapture> g_capture;
bool g_devMode = false;
std::string g_cert; //Zertifikat wird aus den Ressourcen geladen

HINSTANCE g_instance = 0;// current instance
// Window Handles
HWND g_windowHandle = 0; //Handle des Hauptfenster
HWND g_buttonHandle = 0; //Handle des Buttonchilds
HWND g_textInputHandle = 0;
HWND g_logoHandle = 0;

bool g_mouseHoversOverButton = false;

//Timer wird verwendet um den Countdown für Demopräsentationen zu aktualisieren
const UINT_PTR TIMER_ID = 2000; //beliebiger eindeutiger Wert

int g_fontSize = 17; //Die Schriftgröße wird als Bezugsgröße für alle Größen verwendet. Hier also Schriftgröße bei 96DPI
//Grafikressourcen
HFONT g_font = 0;

TextureBrush* g_brushBackground1 = 0;
LinearGradientBrush* g_brushBackground2 = 0;
SolidBrush* g_brushWhite = 0;
SolidBrush* g_brushBlack = 0;
SolidBrush* g_brushCyan = 0; //konferenzen.eu blau
SolidBrush* g_brushRed = 0; //konferenzen.eu blau
SolidBrush* g_brushLightGray = 0; //für Demo Schrift
Bitmap* g_imageLogo = 0;
Bitmap* g_imageButtonStart = 0;
Bitmap* g_imageButtonEnd = 0;
Pen* g_penWhite = 0;

//konstanten
const wchar_t* STR_MAIN_WINDOW_CLASS = L"PresiCtrl";			// the main window class name
const wchar_t* STR_BUTTON_WINDOW_CLASS = L"PresiButton";			// the main window class name
const wchar_t* STR_LOGO_WINDOW_CLASS = L"PresiLogo";			// the main window class name
const int WID_START_BUTTON = 9003;
const int WID_URL_EDIT = 9004;

/**
	Lädt die angegebene Resource in einen neu allokierten buffer.
	Nutzt g_instance als Modulhandle
*/
bool loadDataFromResource(int resId, int resType, char** buffer, size_t* size) {
	HRSRC hFindRes = FindResource(g_instance, MAKEINTRESOURCE(resId), MAKEINTRESOURCE(resType));
	if (!hFindRes) {
		std::cerr << "failed to load (FindResource)" << std::endl;
		return false;
	}
	HGLOBAL hLoadRes = LoadResource(g_instance, hFindRes);
	if (!hLoadRes) {
		std::cerr << "failed to load (LoadResource)" << std::endl;
		return false;
	}

	DWORD sizeRes = SizeofResource(g_instance, hFindRes);
	if (!sizeRes) {
		std::cerr << "failed to load (SizeofResource)" << std::endl;
		return false;
	}
	char* memRes = static_cast<char*>(LockResource(hLoadRes));
	if (!memRes) {
		std::cerr << "failed to load (LockResource)" << std::endl;
		return false;
	}

	*size = sizeRes;
	*buffer = new char[*size];
	memcpy(*buffer, memRes, *size);

	std::cout << "Loaded resource " << resId << " " << *size << std::endl;
	return true;
}

typedef IStream*(*SHCreateMemStreamProc) (const BYTE*, UINT);
SHCreateMemStreamProc SHCreateMemStream = 0;
HINSTANCE hshlwapidll = 0;
/*
Initialisiert Schriften, Pinsel und dergleichen
*/
bool initGraphicResources() {

	//Für den nächsten Schritt wird wieder eine nicht in der Headerdatei aufgeführte Funktion benötigt
	if (SHCreateMemStream == 0) {
		/*
		MinGW hat unvollständige bibliotheken also hierüber an die funktion gelangen
		*/
		HINSTANCE hshlwapidll = LoadLibrary("Shlwapi.dll");
		if (hshlwapidll)
			SHCreateMemStream = (SHCreateMemStreamProc)GetProcAddress(hshlwapidll, "SHCreateMemStream");
	}

	if (SHCreateMemStream == 0) {
		std::cout << "Could not load SHCreateMemStream()" << std::endl;
		return false;
	}

	g_font = CreateFont(g_fontSize, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_MODERN,
		"Arial");

	Color white(255, 255, 255);
	//Pinsel für Hintergründe setzen
	Bitmap *backgroundBitmap = Bitmap::FromResource(g_instance, MAKEINTRESOURCEW(RID_BACKGROUND));
	g_brushBackground1 = new TextureBrush(backgroundBitmap);
	delete backgroundBitmap;

	g_brushBackground2 = new LinearGradientBrush(Rect(0, 0, g_fontSize, g_fontSize*2),
		Color(245, 245, 245), white, LinearGradientModeVertical);
	g_brushWhite = new SolidBrush(white);
	g_brushBlack = new SolidBrush(Color(0, 0, 0));
	g_brushCyan = new SolidBrush(Color(0x11,0xd5,0xe7));
	g_brushRed = new SolidBrush(Color(255,0,0));
	g_brushLightGray = new SolidBrush(Color(0xcc, 0xcc, 0xcc));
	g_penWhite = new Pen(white);

	char* buffer;
	size_t bufSize;
	loadDataFromResource(RID_LOGO, RID_PNG_TYPE, &buffer, &bufSize);
	IStream* stream = SHCreateMemStream(reinterpret_cast<unsigned char*>(buffer), bufSize);
	g_imageLogo = Bitmap::FromStream(stream);
	stream->Release();
	delete[] buffer;

	loadDataFromResource(RID_BUTTON_START, RID_PNG_TYPE, &buffer, &bufSize);
	stream = SHCreateMemStream(reinterpret_cast<unsigned char*>(buffer), bufSize);
	g_imageButtonStart = Bitmap::FromStream(stream);
	stream->Release();
	delete[] buffer;

	loadDataFromResource(RID_BUTTON_END, RID_PNG_TYPE, &buffer, &bufSize);
	stream = SHCreateMemStream(reinterpret_cast<unsigned char*>(buffer), bufSize);
	g_imageButtonEnd = Bitmap::FromStream(stream);
	stream->Release();
	delete[] buffer;

	return true;
}

void cleanupGraphicResources() {
	DeleteObject(g_font);

	delete g_brushBackground1;
	delete g_brushBackground2;
	delete g_brushWhite;
	delete g_brushBlack;
	delete g_brushCyan;
	delete g_brushRed;
	delete g_brushLightGray;
	delete g_penWhite;
	delete g_imageLogo;
	delete g_imageButtonStart;
	delete g_imageButtonEnd;

	FreeLibrary(hshlwapidll);
	hshlwapidll = 0;
	SHCreateMemStream = 0;
}

/**
	Lädt die zuletzt verwendete Konferenzraumurl aus der im Userordner abgelegten Textdatei
*/
bool loadLastUsedConferenceUrl(std::string & url) {

	char* appdataPath = getenv("APPDATA");
	if (!appdataPath)
		return false;
	
	std::string filepath(appdataPath);
	filepath += "\\Konferenzen.eu\\last_used.txt";
	
	if (!PathFileExists(filepath.c_str()))
		return false;

	std::ifstream fin(filepath);
	fin>>url;

	std::cout << "loaded last used conference url from " << filepath << std::endl;
	std::cout << url << std::endl;

	return false;
}

/**
	Speichert die zuletzt verwendete Konferenzraumurl in APPDATA
*/
bool saveLastUsedConferenceUrl(const std::string & url) {
	char* appdataPath = getenv("APPDATA");
	if (!appdataPath)
		return false;

	std::string filepath(appdataPath);
	filepath += "\\Konferenzen.eu";

	if (!PathFileExists(filepath.c_str())) {
		CreateDirectory(filepath.c_str(), 0);
	}

	filepath += "\\last_used.txt";
	
	std::ofstream fout(filepath, std::ofstream::trunc);
	fout << url;

	std::cout << "saved last used conference url to " << filepath << std::endl;
	std::cout << url << std::endl;

	return false;
}
//Wieder einkommentieren, wenn mal irgendwann ssl support implementiert wurde
/*
bool loadCertificateFromResource() {
	char* tmp;
	size_t s;
	loadDataFromResource(MAKEINTRESOURCE(RID_CERT), MAKEINTRESOURCE(RID_CERT_TYPE), &tmp, &s);

	g_cert = std::string(tmp, s);
	std::cout << "Loaded certificate (" << g_cert.size() << std::endl;
	delete[] tmp;
	return true;
}
*/

/**
	Markiert den Bereich mit Timer als ungültig
*/
void redrawTimerArea() {
	RECT rc;
	GetClientRect(g_windowHandle, &rc);
	rc.left = 16 * g_fontSize;
	rc.right -= 1 * g_fontSize;
	rc.bottom -= 1 * g_fontSize;
	rc.top = rc.bottom - 4 * g_fontSize;
	InvalidateRect(g_windowHandle, &rc, false);
}

/**
	Markiert den Bereich mit Timer und Button als ungültig
*/
void redrawStatusArea() {
	RECT rc;
	GetClientRect(g_windowHandle, &rc);
	rc.left += 1 * g_fontSize;
	rc.right -= 1 * g_fontSize;
	rc.bottom -= 1 * g_fontSize;
	rc.top = rc.bottom - 4 * g_fontSize;
	InvalidateRect(g_windowHandle, &rc, false);
}

/**
	Starten der Präsentation
*/
bool startPresentation() {

	//if (g_cert.size() > 0 || loadCertificateFromResource()) {
		char buffer[512];
		GetWindowText(g_textInputHandle, buffer, 512);
		if (strlen(buffer) == 0) {
			g_lastErrorString = STR_ERR_NO_URL;
			std::cerr << "No url given" << std::endl;
			//Neuzeichnen des Buttons und des Statusbereichs nötig
			redrawStatusArea();
			return false;
		}
		else {
			HMONITOR hmon = MonitorFromWindow(g_windowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX moninfo;
			moninfo.cbSize = sizeof(MONITORINFOEX);
			GetMonitorInfo(hmon, &moninfo);

			std::cout << "Monitor: " << moninfo.szDevice << std::endl;
			HDC dcmon = CreateDC("DISPLAY", moninfo.szDevice, NULL, NULL);
			std::cout << GetDeviceCaps(dcmon, HORZRES) << "x" << GetDeviceCaps(dcmon, VERTRES) << std::endl;
			std::cout << "@" << GetDeviceCaps(dcmon, LOGPIXELSX) << " " << GetDeviceCaps(dcmon, LOGPIXELSY) << std::endl;
			std::cout << "off: " << moninfo.rcMonitor.left << " " << moninfo.rcMonitor.top << std::endl;

			POINT cursorOff;
			cursorOff.x = moninfo.rcMonitor.left;
			cursorOff.y = moninfo.rcMonitor.top;

			g_capture.reset(new ScreenCapture(dcmon, cursorOff));

			if (g_devMode) {
				g_server.reset(new PresentationServer(buffer, STR_MANAGER_HOST_DEV, STR_MANAGER_PORT_DEV, GetDeviceCaps(dcmon, HORZRES), GetDeviceCaps(dcmon, VERTRES), g_cert));
			}
			else
				g_server.reset(new PresentationServer(buffer, STR_MANAGER_HOST, STR_MANAGER_PORT, GetDeviceCaps(dcmon, HORZRES), GetDeviceCaps(dcmon, VERTRES), g_cert));

			g_server->setCapture(g_capture);

			//Wenn demo dann timer mit Countdown
			if (g_server->isDemo() && g_server->useTimeOfDeath())
				SetTimer(g_windowHandle, TIMER_ID, 1000, 0);


			saveLastUsedConferenceUrl(std::string(buffer));

			//Neuzeichnen des Buttons und des Statusbereichs nötig
			redrawStatusArea();
			return true;
		}
	/*}
	else {
		MessageBox(g_windowHandle, "Laden des Zertifikats schlug fehl.", "Schwerwiegender Fehler", MB_OK);
		return false;
	}*/
}

/**
	Stoppt Server und damit verbundene Timer
*/
void stopPresentation() {
	g_server.reset(nullptr);
	KillTimer(g_windowHandle, TIMER_ID);
	//Neuzeichnen des Buttons und des Statusbereichs nötig
	redrawStatusArea();
}

/**
	Um einen Link zu simulieren verwende ich für das Logo auch wieder ein Fensterchen
*/
LRESULT CALLBACK logolinkProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_ERASEBKGND: {
			HDC hdc = (HDC)wParam;
			RECT rc;
			std::unique_ptr<Graphics> graphics(new Graphics(hdc));
			GetClientRect(hWnd, &rc);

			//Logo: Text Bildschirmübertragung und konferenzen.eu logo rechts
			graphics->DrawImage(g_imageLogo, (int)rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		}
		break;
	case WM_PAINT:
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_LBUTTONDOWN:
		ShellExecuteW(0, L"open", STR_OPEN_LINK_FROM_LOGO, 0, 0, SW_SHOWNORMAL);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}


/**
	Für das Zeichen eines eigenen Buttons mit anderem Hintergrund muss doch tatsächlich ein komplett neues Kind gebastelt werden.
*/
LRESULT CALLBACK buttonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_ERASEBKGND:
		break;
	case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc;
			RECT rc;
			hdc = BeginPaint(hWnd, &ps);
			std::unique_ptr<Graphics> graphics;
			//Hier gibts keinen Text, der ist leider schon im Bild mit drin
			graphics.reset(new Graphics(hdc));
			GetClientRect(hWnd, &rc);
			//richtiges Bild auswählen
			Image* img = g_server ? g_imageButtonEnd : g_imageButtonStart;
			//und um einen Hovereffekt zu erzielen wird der Farbraum des Bildes bei Bedarf
			//mit folgender Transformation versehen. (aufhellen)
			ImageAttributes attr;
			if (g_mouseHoversOverButton) {
				ColorMatrix transMat = {
					1.2f, 0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.2f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.2f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f, 1.0f
				};

				attr.SetColorMatrix(
					&transMat,
					ColorMatrixFlagsDefault,
					ColorAdjustTypeBitmap);
			}
			graphics->DrawImage(img,
				Rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top),
				0,0,img->GetWidth(), img->GetHeight(), UnitPixel, &attr);
			
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_LBUTTONDOWN:
		SendMessage(g_windowHandle, WM_COMMAND, WID_START_BUTTON, 0);
		break;
	case WM_MOUSEMOVE:
		//Ich hoffe das das Verhalten, dass das Kind die Mousevents schluckt wenn es drunter liegt auch immer so ist
		if (!g_mouseHoversOverButton) {
			g_mouseHoversOverButton = true;
			redrawStatusArea();
		}
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

//
//  FUNCTION: windowProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_TIMER:
		//Wird wenn gestartet einmal pro sekunde ausgefüht und dann wird auch nichts anderes gemacht als das neuzeichnen des Countdowns zu beginnen
		if (g_server)
			redrawTimerArea();
		break;
	case WM_ERASEBKGND: {
			HDC hdc;
			RECT clientRect;
			RECT rc;
			std::unique_ptr<Graphics> graphics;
			std::unique_ptr<Font> fontArial;

			//Hintergrund zeichnen
			hdc = (HDC)wParam;
			graphics.reset(new Graphics(hdc));
			GetClientRect(g_windowHandle, &clientRect);
			rc = clientRect;
			//Gepunkteter Hintergrund
			graphics->FillRectangle(g_brushBackground1, (int)rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			//Weiße Box mit leichtem Farbverlauf
			rc.left += g_fontSize; rc.right -= g_fontSize;
			rc.top += 4 * g_fontSize; rc.bottom -= g_fontSize;
			graphics->FillRectangle(g_brushWhite, (int)rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			graphics->FillRectangle(g_brushBackground2, (int)rc.left, rc.top, rc.right - rc.left, g_fontSize * 2);
			graphics->DrawRectangle(g_penWhite, (int)rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		}
		break;
	case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc;
			RECT clientRect;
			RECT rc;
			hdc = BeginPaint(hWnd, &ps);
			std::unique_ptr<Graphics> graphics;
			std::unique_ptr<Font> fontArial;
			StringFormat txtFormatRightAlign;
			txtFormatRightAlign.SetAlignment(StringAlignmentFar);
			
			graphics.reset(new Graphics(hdc));
			//Infotext
			fontArial.reset(new Font(hdc, g_font));
			GetClientRect(g_windowHandle, &clientRect);
			rc = clientRect;
			graphics->DrawString(STR_INFO_TEXT, -1, fontArial.get(), PointF(rc.left + 2 * g_fontSize, rc.top + 5 * g_fontSize), g_brushBlack);

			//Statusausgabe, weiß überschreiben, da sich der ja ändern könnte.
			graphics->FillRectangle(g_brushWhite, rc.left + 5 * g_fontSize / 2, rc.bottom - 5 * g_fontSize, rc.right - rc.left - 4 * g_fontSize, 5 * g_fontSize / 2);
			if (g_server) {
				if (g_server->isDemo() && g_server->useTimeOfDeath()) {
					//Demo mit Ablaufzeit
					int time = std::chrono::duration_cast<std::chrono::seconds>(g_server->getTimeOfDeath() - std::chrono::system_clock::now()).count();
					std::wstring txt = STR_DEMO;
					txt.append(L" ");
					if (time >= 3600)
						txt.append(std::to_wstring(time / 3600) + L":");
					wchar_t tmp[16];
					swprintf(tmp, 16, L"%02d", (time % 3600) / 60);
					txt.append(tmp); txt.append(L":");
					swprintf(tmp, 16, L"%02d", time % 60);
					txt.append(tmp);
					graphics->DrawString(txt.c_str(), txt.length(), fontArial.get(), 
						RectF(rc.left, rc.bottom - 7 * g_fontSize / 2, rc.right - rc.left - 2 * g_fontSize, 2 * g_fontSize),
						&txtFormatRightAlign, g_brushLightGray);
				}
			}
			else {
				if (!g_lastErrorString.empty()) {
					//Fehlerausgabe
					graphics->DrawString(g_lastErrorString.c_str(), g_lastErrorString.length(), fontArial.get(),
						RectF(rc.left, rc.bottom - 7 * g_fontSize / 2, rc.right - rc.left - 2 * g_fontSize, 2 * g_fontSize),
						&txtFormatRightAlign, g_brushRed);
				}
			}
			//Eingabelabel
			FontFamily ffamily;
			fontArial->GetFamily(&ffamily);
			fontArial.reset(new Font(&ffamily, fontArial->GetSize(), FontStyleBold, fontArial->GetUnit()));
			graphics->DrawString(STR_INPUT_LABEL, -1, fontArial.get(), PointF(rc.left + 2 * g_fontSize, rc.top + 17 * g_fontSize / 2), g_brushCyan);
			EndPaint(hWnd, &ps);
		}
		
		break;
	case WM_MOUSEMOVE:
		//Ich hoffe das das Verhalten, dass das Kind die Mousevents schluckt wenn es drunter liegt auch immer so ist
		if (g_mouseHoversOverButton) {
			g_mouseHoversOverButton = false;
			redrawStatusArea();
		}
		break;
	case WM_COMMAND:

		switch (LOWORD(wParam))	{
		case WID_START_BUTTON:
			if (g_server) {
				stopPresentation();
			}
			else {
				try {
					if (startPresentation()) {
						g_lastErrorString.clear();
					}
				}
				catch (runtime_error_with_extra_msg & e) {
					
					std::wstring mbmsg(e.getPublicMessage().begin(), e.getPublicMessage().end());
					MessageBoxW(g_windowHandle, mbmsg.c_str(), L"Achtung", MB_OK);

					g_lastErrorString = std::wstring(e.what(), e.what() + strlen(e.what()));
					std::cerr << "Error: " << e.what() << std::endl;
					stopPresentation();
				}
				catch (std::exception & e) {
					g_lastErrorString = std::wstring(e.what(),e.what()+strlen(e.what()));
					std::cerr << "Error: " << e.what() << std::endl;
					stopPresentation();
				}
			}
			break;
		default:
			break;
		}
		break;
	case WM_QUIT:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

//
//  FUNCTION: registerWindowClass()
//
//  PURPOSE: Registers the window class.
//
void registerWindowClass()
{
	WNDCLASSEXW wcex;
	//Registerieren der Klasse für das Hauptfenster
	wcex.cbSize = sizeof(WNDCLASSEXW);

	wcex.style = 0;
	wcex.lpfnWndProc = windowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = g_instance;
	wcex.hIcon = LoadIcon(g_instance, MAKEINTRESOURCE(RID_FAVICON));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = STR_MAIN_WINDOW_CLASS;
	wcex.hIconSm = 0; 

	RegisterClassExW(&wcex);

	//Registrieren der Klasse für das Kindfenster (Button)
	wcex.lpfnWndProc = buttonProc;
	wcex.hIcon = NULL;
	wcex.lpszClassName = STR_BUTTON_WINDOW_CLASS;
	wcex.hCursor = LoadCursor(NULL, IDC_HAND);

	RegisterClassExW(&wcex);

	//Registrieren der Klasse für Logo
	wcex.lpfnWndProc = logolinkProc;
	wcex.lpszClassName = STR_LOGO_WINDOW_CLASS;

	RegisterClassExW(&wcex);
}

//
//   Erstellt das Fenster, die Kinder des Fensters und alle fürs Zeichnen nötigen Ressourcen
//
HWND initWindowInstance(int nCmdShow)
{
	initGraphicResources();
	registerWindowClass();

	g_windowHandle = CreateWindowW(STR_MAIN_WINDOW_CLASS, STR_WINDOW_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, g_fontSize * 32, g_fontSize * 19, NULL, NULL, g_instance, NULL);

	if (!g_windowHandle) {
		return 0;
	}

	std::string confurl;
	loadLastUsedConferenceUrl(confurl);

	if (LoadLibrary("Riched32.dll")) {
		g_textInputHandle = CreateWindowEx(WS_EX_CLIENTEDGE, RICHEDIT_CLASS, confurl.c_str(), WS_CHILD | WS_VISIBLE |
			ES_MULTILINE, g_fontSize*2, g_fontSize * 10, g_fontSize * 28, g_fontSize * 5/2, g_windowHandle,
			(HMENU)WID_URL_EDIT, g_instance, 0);
	}
	else {
		g_textInputHandle = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", confurl.c_str(), WS_CHILD | WS_VISIBLE |
			ES_MULTILINE, g_fontSize * 2, g_fontSize * 10, g_fontSize * 28, g_fontSize * 5 / 2, g_windowHandle,
			(HMENU)WID_URL_EDIT, g_instance, 0);
	}
	//Einen Button für Start und Stop :)
	//g_buttonHandle = CreateWindowW(L"BUTTON", STR_START_PRESENTATION, WS_CHILD | WS_VISIBLE, g_fontSize * 2, g_fontSize * 27 / 2, g_fontSize * 10, g_fontSize * 2, g_windowHandle, (HMENU)WID_START_BUTTON, g_instance, 0);
	g_buttonHandle = CreateWindowExW(0, STR_BUTTON_WINDOW_CLASS, L"", WS_CHILD | WS_VISIBLE,
		g_fontSize * 2, g_fontSize * 27 / 2, g_fontSize * 13, g_fontSize * 2, g_windowHandle, 0, g_instance, 0);
	if (g_font) {
		SendMessage(g_windowHandle, WM_SETFONT, (WPARAM)g_font, TRUE);
		SendMessage(g_textInputHandle, WM_SETFONT, (WPARAM)g_font, TRUE);
	}
	//Kind für Logo mit Link
	g_buttonHandle = CreateWindowExW(0, STR_LOGO_WINDOW_CLASS, L"", WS_CHILD | WS_VISIBLE,
		g_fontSize * 2, g_fontSize / 2, g_imageLogo->GetWidth()*g_fontSize * 13 / 4 / g_imageLogo->GetHeight(),
		g_fontSize * 13 / 4, g_windowHandle, 0, g_instance, 0);
	
	ShowWindow(g_windowHandle, nCmdShow);
	UpdateWindow(g_windowHandle);

	return g_windowHandle;
}

typedef BOOL(*SetProcessDPIAwareProc) ();
SetProcessDPIAwareProc SetProcessDPIAware = 0;

/**
	Setzt die Anwendung als DPI Aware und skaliert den Wert der Schriftgröße in Abh. von der tatsächlichen Auflösung
*/
void setDPIAware() {
	/*
		MinGW hat unvollständige bibliotheken also hierüber an die funktion gelangen
	*/
	HINSTANCE huser32dll = LoadLibrary("User32.dll");
	if (huser32dll) {
		SetProcessDPIAware = (SetProcessDPIAwareProc)GetProcAddress(huser32dll, "SetProcessDPIAware");
		if (SetProcessDPIAware) {
			SetProcessDPIAware();
			
			HMONITOR hmon = MonitorFromWindow(g_windowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX moninfo;
			moninfo.cbSize = sizeof(MONITORINFOEX);
			GetMonitorInfo(hmon, &moninfo);
			
			std::cout << "Monitor: " << moninfo.szDevice << std::endl;
			HDC dcmon = CreateDC("DISPLAY", moninfo.szDevice, NULL, NULL);
			std::cout << GetDeviceCaps(dcmon, HORZRES) << "x" << GetDeviceCaps(dcmon, VERTRES) << std::endl;
			std::cout << "@" << GetDeviceCaps(dcmon, LOGPIXELSX) << " " << GetDeviceCaps(dcmon, LOGPIXELSY) << std::endl;
			g_fontSize = g_fontSize * GetDeviceCaps(dcmon, LOGPIXELSX) / 96;
			std::cout << " => Fontsize = " << g_fontSize << std::endl;

			DeleteDC(dcmon);
		}

		FreeLibrary(huser32dll);
	}
}

int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
	bool openedConsole = false;
	//Parameter parsen, mit Konsole starten?
	std::string cmdline(lpCmdLine);
	if (cmdline.find("-console") != std::string::npos) {
		openedConsole = AllocConsole();
		if (openedConsole) {
			//stdout und stderr auf konsole umleiten
			freopen("CONOUT$", "wb", stdout);
			freopen("CONOUT$", "wb", stderr);
		}
	}

	if (cmdline.find("-dev") != std::string::npos) {
		g_devMode = true;
	}

	setDPIAware();

	//Gdiplus initialisieren
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	//Windows Netzwerkkram initialisieren
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData)) {
		std::cerr << "WSAStartup failed" << std::endl;
		return -3;
	}

	g_instance = hInstance;
	
	if (!(g_windowHandle = initWindowInstance(nCmdShow)))	{
		std::cerr << "Creating Window failed" << std::endl;
		return FALSE;
	}

	bool run = true;
	MSG msg;
	while (run) {

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				run = false;
		}
		
		if (g_server) {
			if (!g_server->run()) {
				stopPresentation();
			}
		}
		else {
			//Wenn der Server läuft sorgen die bei den Netzwerkfunktionen nötigen Timeouts und
			//Verzögerungen schon dafür, dass die CPU nicht so stark in Anspruch genommen wird.
			//Im anderen Fall, also hier würde das alleinige Ausführen der MEssage Loop mit Peek aber den
			//Prozessor schon sehr belasten
			usleep(1000);
		}
	}

	cleanupGraphicResources();
	//Aufräumen
	WSACleanup();
	GdiplusShutdown(gdiplusToken);

	if (openedConsole) {
		fclose(stdout);
		fclose(stderr);
		FreeConsole();
	}

	return 0;
}