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
#pragma once

#include <rfb/rfb.h>
#include <chrono>
#include <memory>
#include <string>
#include "ScreenCapture.h"

/**
	Kapselt den VNC Server und den Authentifizierungsaufruf an die Managerroutine der node Anwendung
*/
class PresentationServer {
private:
	rfbScreenInfo* _server;
	std::shared_ptr<ScreenCapture> _capture;

	//Z�hler f�r die Zeit um Bilder nur ein paar mal pro Sekunde aufzunehmen
	std::chrono::high_resolution_clock::time_point _tick;

	//L�uft als Demo. �ndert nichts an dem Verhalten sondern ist nur dazu da um �ber isDemo
	//Abgefragt werden zu k�nnen um dann in der Ansicht eine Information auszugeben
	bool _demo;
	//Zeitpunkt zu dem die Pr�sentation vorraussichtlich von der Gegenstelle beendet wird.
	std::chrono::system_clock::time_point _timeOfDeath;
	//Wenn == false, dann wurde ein Endzeitpunkt nicht �bermittelt.
	bool _useTimeOfDeath;
public:
	/**
		@param conferenceUrl Die Konferenzraumurl die an den Manager gesendet wird um diesen Server zu authentifizieren.
		@param managerHost Addresse des Pr�sentationsmanagers der nach Authentifizierung den VNC Proxy f�r diese Anwendung startet.
						   Die Kommunikation mit dem Pr�sentationsmanager l�uft �ber HTTP
		@param managerPort Port f�r die Anfrage an den Manager
		@param screenWidth Breite des zu �bertragenden Screens
		@param screenHeight H�he des zu �bertragenden Screens
		@param certificate F�r SSL Verbindungen zu benutzendes Zertifikat... (nicht benutzt)
		@param managerPath Pfad, der bei der Anfrage genutzt werden soll
		@param managerParam Name des POST Parameters in dem die conferenceUrl �bermittelt werden soll.
	*/
	PresentationServer(const std::string & conferenceUrl, const std::string & managerHost,
		unsigned short managerPort, int screenWidth, int screenHeight, const std::string & certificate = std::string() , const std::string & managerPath = std::string("/startPresentation"),
		const std::string & managerParam = std::string("url"));
	~PresentationServer();

	bool run();

	rfbScreenInfo* getScreen() const {
		return _server;
	}

	/**
		Setzt das Objekt welches zum Capturen des Des Screens verwendet wird.
	*/
	void setCapture(std::shared_ptr<ScreenCapture> & cap) {
		_capture = cap;
	}

	/*
		Demo Pr�sentation
	*/
	bool isDemo() const { return _demo; }
	
	/*
		Wert TimeOfDeath ist g�ltig?
	*/
	bool useTimeOfDeath() const { return _useTimeOfDeath;  }

	/*
		Liefert den Zeitpunkt zur�ck zu welchem die Pr�sentation vorraussichtlich von der Gegenstelle beendet wird.
	*/
	const std::chrono::system_clock::time_point & getTimeOfDeath() const { return _timeOfDeath; }
};