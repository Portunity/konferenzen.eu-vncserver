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
#include <chrono>
#include <memory>
#include <string>
#include <stdexcept>
#include "ScreenCapture.h"

/**
* exception um eine zusätzliche nachricht mit schicken zu können.
*/
class runtime_error_with_extra_msg : public std::runtime_error {
	private:
		std::wstring _publicmsg;
	public:
		runtime_error_with_extra_msg(const std::wstring & publicmsg, const std::string & msg1) :_publicmsg(publicmsg), runtime_error(msg1) {}
		const std::wstring & getPublicMessage() const { return _publicmsg; }
};


/**
	Kapselt den VNC Server und den Authentifizierungsaufruf an die Managerroutine der node Anwendung
*/
class PresentationServer {
private:
	rfbScreenInfo* _server;
	std::shared_ptr<ScreenCapture> _capture;

	//Zähler für die Zeit um Bilder nur ein paar mal pro Sekunde aufzunehmen
	std::chrono::high_resolution_clock::time_point _tick;

	//Läuft als Demo. Ändert nichts an dem Verhalten sondern ist nur dazu da um über isDemo
	//Abgefragt werden zu können um dann in der Ansicht eine Information auszugeben
	bool _demo;
	//Zeitpunkt zu dem die Präsentation vorraussichtlich von der Gegenstelle beendet wird.
	std::chrono::system_clock::time_point _timeOfDeath;
	//Wenn == false, dann wurde ein Endzeitpunkt nicht übermittelt.
	bool _useTimeOfDeath;
	//Wert der vom Webserver im Parameter MEssageBox übermittelt wurde
	std::wstring _messageBox;
public:
	/**
		@param conferenceUrl Die Konferenzraumurl die an den Manager gesendet wird um diesen Server zu authentifizieren.
		@param managerHost Addresse des Präsentationsmanagers der nach Authentifizierung den VNC Proxy für diese Anwendung startet.
						   Die Kommunikation mit dem Präsentationsmanager läuft über HTTP
		@param managerPort Port für die Anfrage an den Manager
		@param screenWidth Breite des zu übertragenden Screens
		@param screenHeight Höhe des zu übertragenden Screens
		@param certificate Für SSL Verbindungen zu benutzendes Zertifikat... (nicht benutzt)
		@param managerPath Pfad, der bei der Anfrage genutzt werden soll
		@param managerParam Name des POST Parameters in dem die conferenceUrl übermittelt werden soll.
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
		Demo Präsentation
	*/
	bool isDemo() const { return _demo; }
	
	/*
		Wert TimeOfDeath ist gültig?
	*/
	bool useTimeOfDeath() const { return _useTimeOfDeath;  }

	/*
		Liefert den Zeitpunkt zurück zu welchem die Präsentation vorraussichtlich von der Gegenstelle beendet wird.
	*/
	const std::chrono::system_clock::time_point & getTimeOfDeath() const { return _timeOfDeath; }

	/*
		Der Webserver hat bei der Anfrage die Möglichkeit den Parameter MessageBox zu setzen und damit einen
		string anzugeben, der dem Benutzer angezeit werden soll. Hier kann er ausgelesen werden.
	*/
	const std::wstring & getMessageToShow() const {
		return _messageBox;
	}
};