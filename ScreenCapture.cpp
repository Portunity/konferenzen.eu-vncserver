/*
Konferenzen.eu Pr�sentationsserver. Einfacher vorkonfigurierter VNC Client f�r Windows
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

ScreenCapture::ScreenCapture(HDC deviceToCapture, const POINT & cursorOffset) :
 _hScreenDC(deviceToCapture), _hCaptureDC(0), _hCaptureBitmap(), _backBuffer(0), _cursorOff(cursorOffset) {

	_cxScreen = GetDeviceCaps(_hScreenDC, HORZRES);
	_cyScreen = GetDeviceCaps(_hScreenDC, VERTRES);

	// _hCaptureDC = CreateCompatibleDC(_hScreenDC);

	//Der DC f�r das Zwischenspeichern muss kompatibel mit dem Desktop sein, sonst klappt das kopieren des Cursors nicht.
	HDC dsktpDC = GetDC(GetDesktopWindow());
	_hCaptureDC = CreateCompatibleDC(dsktpDC);
	ReleaseDC(GetDesktopWindow(),dsktpDC);

}

void ScreenCapture::resizeBitmap(rfbScreenInfo* screen) {

	if (screen && (_backBuffer == 0 || 
		_bi.biWidth != screen->width ||
		-_bi.biHeight != screen->height ||
		_bi.biBitCount != screen->bitsPerPixel)) {

		delete[] _backBuffer;
		if (_hCaptureBitmap)
			DeleteObject(_hCaptureBitmap);

		_hCaptureBitmap = CreateCompatibleBitmap(_hScreenDC,
			screen->width, screen->height);

		_bi.biSize = sizeof(BITMAPINFOHEADER);
		_bi.biWidth = screen->width;
		_bi.biHeight = -screen->height;
		_bi.biPlanes = 1;
		_bi.biBitCount = screen->bitsPerPixel;
		_bi.biCompression = BI_RGB;
		_bi.biSizeImage = 0;
		_bi.biXPelsPerMeter = 0;
		_bi.biYPelsPerMeter = 0;
		_bi.biClrUsed = 0;
		_bi.biClrImportant = 0;

		int buffersize = screen->width * screen->height * (screen->bitsPerPixel / 8);
		_backBuffer = new char[buffersize];
	}
}

ScreenCapture::~ScreenCapture() {
	delete[] _backBuffer;
	DeleteDC(_hCaptureDC);
	if (_hCaptureBitmap)
		DeleteObject(_hCaptureBitmap);
	DeleteDC(_hScreenDC);
}

void ScreenCapture::capture(rfbScreenInfo* screen) {
	if (screen) {
		resizeBitmap(screen);
		//gilt: screen->width = _bi.biWidth
		//      screen->height = -_bi.biHeight

		SelectObject(_hCaptureDC, _hCaptureBitmap);

		StretchBlt(_hCaptureDC, 0, 0, _bi.biWidth, -_bi.biHeight,
			_hScreenDC, 0, 0, _cxScreen, _cyScreen, SRCCOPY);

		//Cursor drauf zeichnen, damit der mit �bertragen wird.
		POINT pt;
		GetCursorPos(&pt);
		pt.x = (pt.x - _cursorOff.x) * screen->width / _cxScreen;
		pt.y = (pt.y - _cursorOff.y) * screen->height / _cyScreen;
		DrawIcon(_hCaptureDC, pt.x, pt.y,(HICON) GetCursor());

		GetDIBits(_hCaptureDC, _hCaptureBitmap, 0, screen->height, _backBuffer, (BITMAPINFO *)&_bi, DIB_RGB_COLORS);

		//R und B Kanal wieder richtig rum tauschen
		//@TODO Mal nach dem "richtigen" Weg für das hier suchen. Irgendwo muss es ja Einstellungen in LibVNC und/oder NoVNC geben,
		//mit der man die jeweiligen Farbkodierungen einstellen kann.
		for (int x = 0; x < screen->width * screen->height; x++) {
			std::swap(_backBuffer[x * 4], _backBuffer[x * 4 + 2]);
		}
		//�nderungen ermitteln... 
		//@TODO Das hier ist der wohl primitiveste Algorithmus den es dafür überhaupt gibt... => optimieren.
		int min_x = screen->width - 1;
		int min_y = screen->height - 1;
		int max_x = 0, max_y = 0;

		for (int y = 0; y < screen->height; y++) {
			for (int x = 0; x < screen->width; x++) {
				if (((uint32_t*)_backBuffer)[x + y*screen->width] != ((uint32_t*)screen->frameBuffer)[x + y*screen->width]) {
					if (x < min_x) min_x = x;
					if (y < min_y) min_y = y;
					if (x > max_x) max_x = x;
					if (y > max_y) max_y = y;
				}
			}
		}

		if (min_x <= max_x && min_y <= max_y) { //�nderung erkannt
			std::swap(screen->frameBuffer, _backBuffer);
			rfbMarkRectAsModified(screen, min_x, min_y, max_x + 1, max_y + 1);
		}
	}
}