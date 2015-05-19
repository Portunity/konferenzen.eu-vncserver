/*
	Konferenzen.eu Präsentationsserver. Einfacher vorkonfigurierter VNC Client für Windows
	Einfache Klasse zur Aufnahme des Desktop

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
#pragma once

#include <rfb/rfb.h>
#include <windows.h>

/**
	Nutzt BitBlt und ein Bitmap um den Bildschirminhalt zu kriegen und
	diesen an den rfbScreen zu übergeben.
*/
class ScreenCapture {
private:
	//DC of screen to capture
	HDC _hScreenDC;
	// Resolution of screen DC
	long _cxScreen;
	long _cyScreen;

	//Bitmap DC used for copying
	HDC _hCaptureDC;
	//bitmap used for capturing
	HBITMAP _hCaptureBitmap;
	//Bitmapinfo
	BITMAPINFOHEADER   _bi;

	//Buffer for copying from bitmap
	char* _backBuffer;

	POINT _cursorOff;

	/*
		Passt Bitmaps und Zwischenspeicher an den Screen an
	*/
	void resizeBitmap(rfbScreenInfo* screen);
public:
	/**
		Initialisiert das Objekt.
		@param deviceToCapture ist der zu spiegelnde DC, wird beim
		zerstören des Objekts mit DeleteDC wieder freigegebn
	*/
	ScreenCapture(HDC deviceToCapture, const POINT & cursorOffset );
	~ScreenCapture();

	/**
		Erfasst den Bildschirminhalt und schreibt dies in den Puffer des Servers und markiert die
		erkannten Bereiche als verändert.
	*/
	void capture(rfbScreenInfo* screen);

};