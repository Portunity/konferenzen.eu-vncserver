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
#include <cstdlib>
#include <cstdint>

/**
	Nutzt BitBlt und ein Bitmap um den Bildschirminhalt zu kriegen und
	diesen an den rfbScreen zu übergeben.
*/
class ScreenCapture {
private:
	
    MONITORINFOEX _monitorInfo;//< Monitor dessen Inhalt übertragen werden soll.
	const unsigned short _bitsPerPixel = 32; //< Unterstützt wird nur eine Farbtiefe von 32 Bit
    std::uint32_t* _backBuffer;//< Buffer for copying from bitmap
	std::size_t _backBufferSize; //< Size of Backbuffer
	
public:
    /**
     * Intialisiert das Objekt für die Aufnahme des Inhalts des angegebenen Bildschirms
     * @param monInfo Struktur, welche den aufzunehmenden Monitor beschreibt
     */
    ScreenCapture(const MONITORINFOEX & monInfo);
	
	~ScreenCapture();

	/**
		Erfasst den Bildschirminhalt, schreibt dies in den Puffer des Servers und markiert die
		erkannten Bereiche als verändert.
	*/
	void capture(rfbScreenInfo* screen);
    
    /**
     * Setzt den Monitor dessen Inhalt aufzunehmen ist.
     * @param monInfo
     */
    void setMonitorInfo(const MONITORINFOEX & monInfo); 
};