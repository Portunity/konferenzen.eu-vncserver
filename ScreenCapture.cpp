/*
Konferenzen.eu Präsentationsserver. Einfacher vorkonfigurierter VNC Client f�r Windows
Einfache Klasse zur Aufnahme des Desktop

Copyright(C) 2015 Portunity GmbH, author: Benjamin D�rholt

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
#include "ScreenCapture.h"
#include "PresentationServer.h"
#include <algorithm>
#include <iostream>

extern HINSTANCE g_hInstance;

void bswap (std::uint32_t & p) {
	__asm__(
		"bswap %0;\n\t"
		: "+r"(p));
}

ScreenCapture::ScreenCapture(const MONITORINFOEX & monInfo) :
_monitorInfo(monInfo), _backBuffer(0), _backBufferSize(0) {
}

ScreenCapture::~ScreenCapture() {
	delete[] _backBuffer;
}

void ScreenCapture::capture(rfbScreenInfo* screen) {
	if (screen) {
		if (_bitsPerPixel != screen->bitsPerPixel) {
			std::cerr<<"Only 32 bit pixel depth is supported"<<std::endl;
			throw std::runtime_error("Only 32 bit pixel depth is supported");
		}
		const long width = screen->width;
		const long height = screen->height;
		
		//Erstelle einen Devicekontext für den aufzunehmenden Monitor
		HDC monitorDC = CreateDC("DISPLAY", _monitorInfo.szDevice, NULL, NULL);
		HDC dsktpDC = GetDC(GetDesktopWindow()); //< Leihe den Desktop DC
		HDC captureDC = CreateCompatibleDC(dsktpDC); //< Erstelle einen kompatiblen DC für das Kopieren
		
		ReleaseDC(GetDesktopWindow(), dsktpDC); //< Desktop wieder loslassen
		
		//Erstelle ein Bitmap für die Aufnahme des Bildschirminhalts mit der für die Übertragung vorgesehenen Größe
		HBITMAP screenBitmap = CreateCompatibleBitmap(monitorDC, width, height);
		BITMAPINFOHEADER screenBitmapInfo{
			sizeof (BITMAPINFOHEADER),
			width,
			-height,
			1,
			(WORD)_bitsPerPixel,
			BI_RGB
		};
		std::size_t requiredBackBufferSize = width * height;
		//Muss der Puffer reallokiert werden?
		if (requiredBackBufferSize != _backBufferSize || _backBuffer == 0) {
			std::cout<<"Reallocate intermediate Pixelbuffer. ("<<requiredBackBufferSize<<")"<<std::endl;
			delete[] _backBuffer;
			_backBuffer = new std::uint32_t[requiredBackBufferSize];
			_backBufferSize = requiredBackBufferSize;
		}

		SelectObject(captureDC, screenBitmap); //< Bitmap in den Aufnahme DC einsetzen
		StretchBlt(captureDC, 0, 0, screenBitmapInfo.biWidth, -screenBitmapInfo.biHeight,
			monitorDC, 0, 0, GetDeviceCaps(monitorDC, HORZRES), GetDeviceCaps(monitorDC, VERTRES), SRCCOPY); //< Kopiere vom Monitor DC in das Bitmap

		//Draw Cursor
		CURSORINFO cursorInfo{
			sizeof(CURSORINFO)
		};
		GetCursorInfo(&cursorInfo);
		
		DrawIconEx(captureDC, 
			//Korrekte Position wird einfach in das Koordinatensystem des Monitors und dann 
			//durch Skalieren in das Koordinatensystems des Bitmaps transformiert.
			(cursorInfo.ptScreenPos.x - _monitorInfo.rcMonitor.left) * width / GetDeviceCaps(monitorDC,HORZRES), 
			(cursorInfo.ptScreenPos.y - _monitorInfo.rcMonitor.top) * height / GetDeviceCaps(monitorDC,VERTRES),
			cursorInfo.hCursor,
			0,0,0,NULL,DI_NORMAL);
		
		//Kopieren aus dem Bitmap in in den eigenen Puffer
		GetDIBits(captureDC, screenBitmap, 0, height, _backBuffer, (BITMAPINFO *) & screenBitmapInfo, DIB_RGB_COLORS);

		//Aufräumen:
		DeleteObject(screenBitmap);
		DeleteDC(captureDC);
		DeleteDC(monitorDC);
		
		std::uint32_t pixel;
		//Pixelformat gemäß screen->serverFormat anpassen
		for (int x = 0; x < width * height; x++) {
			pixel = _backBuffer[x];
			_backBuffer[x] = ((pixel & 0x000000ff) << screen->serverFormat.blueShift) | //blau
							 ((pixel >> 8 & 0x000000ff) << screen->serverFormat.greenShift) | //grün
							 ((pixel >> 16 & 0x000000ff) << screen->serverFormat.redShift); //rot
			if (screen->serverFormat.bigEndian) {
				bswap(_backBuffer[x]);
			}
		}

		//Änderungen ermitteln... 
		//@TODO Das hier ist der wohl primitiveste Algorithmus den es dafür überhaupt gibt... => optimieren.
		int min_x = width - 1;
		int min_y = height - 1;
		int max_x = 0, max_y = 0;

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				if (_backBuffer[x + y * width] != ((uint32_t*) screen->frameBuffer)[x + y * width]) {
					if (x < min_x) min_x = x;
					if (y < min_y) min_y = y;
					if (x > max_x) max_x = x;
					if (y > max_y) max_y = y;
				}
			}
		}

		if (min_x <= max_x && min_y <= max_y) { //�nderung erkannt			
			std::cout<<"send area ("<<min_x<<'|'<<min_y<<") ("<<max_x<<'|'<<max_y<<')'<<std::endl;
			//swap pointers to buffers
			std::uint32_t* swap = (std::uint32_t*)screen->frameBuffer;
			screen->frameBuffer = (char*)_backBuffer;
			_backBuffer = swap;
			
			rfbMarkRectAsModified(screen, min_x, min_y, max_x + 1, max_y + 1);
		}
	}
}

void ScreenCapture::setMonitorInfo(const MONITORINFOEX & monInfo) {
	_monitorInfo = monInfo;
}
