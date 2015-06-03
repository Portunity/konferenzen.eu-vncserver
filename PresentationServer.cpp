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
#include "PresentationServer.h"
#include <string>
#include <rfb/rfbclient.h>
#include <stdlib.h>
#include <iostream>

const char* STR_ERR_WEBHOST_UNREACHABLE = "konferenzen.eu nicht erreichbar";
const char* STR_ERR_RFBHOST_UNREACHABLE = "Server nicht erreichbar";
const char* STR_ERR_WRONG_ANSWER = "Fehlerhafte Antwort von Server";

#ifdef HAS_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/gnutlsxx.h>
#endif

//Version des Tools, die an den Webserver übermittelt wird,
//immer dann erhöhen wenn Änderungen zu Inkompatibilität führen könnten
const int TOOL_VERSION = 1;

std::string urlencode(const std::string & param) {
	std::string r;
	char tmp[4];
	for (auto & c : param) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			r += c;
		}
		else {
			snprintf(tmp, 4, "%%%02X", c);
			r += tmp;
		}
	}

	return r;
}

std::wstring utf8_to_ucs2(const std::string & data) {
	//from utf8 to ucs2
	int tmpbsize = MultiByteToWideChar(CP_UTF8, 0, data.data(), data.size(), 0, 0);
	if (tmpbsize > 0) {
		std::unique_ptr<wchar_t[]> tmpb(new wchar_t[tmpbsize + 1]);
		MultiByteToWideChar(CP_UTF8, 0, data.data(), data.size(), tmpb.get(), tmpbsize);
		return std::wstring(tmpb.get(), tmpb.get() + tmpbsize);
	}
	return std::wstring();
}

//Parst den Parameter $name aus der antwort
std::string getParameter(const std::string & str, const std::string & name) {
	std::string s = name + ": ";
	std::size_t pos = str.find(s);
	if (pos != std::string::npos) {
		pos += s.length();
		std::size_t pos2 = str.find_first_of("\r\n", pos);
		return str.substr(pos, pos2==std::string::npos?pos2:pos2 - pos);
	}

	std::cerr << "Parameter " << name << " not found in answer" << std::endl;
	throw std::runtime_error(STR_ERR_WRONG_ANSWER);
}

//Parst den Parameter aus der Antwort. Anders als die erste Variante wird, wenn dieser nicht gefunden wurde keine Exception geworfen
//sondern der übergebene Defaultwert zurückgegeben
std::string getParameter(const std::string & str, const std::string & name, const std::string & defaultValue) {
	std::string s = name + ": ";
	std::size_t pos = str.find(s);
	if (pos != std::string::npos) {
		pos += s.length();
		std::size_t pos2 = str.find_first_of("\r\n", pos);
		return str.substr(pos, pos2 == std::string::npos ? pos2 : pos2 - pos);
	}

	std::cout << "Parameter " << name << " not found in answer" << std::endl;
	std::cout << "Return default value " << defaultValue << std::endl;
	return defaultValue;
}

PresentationServer::PresentationServer(const std::string & conferenceUrl,
	const std::string & managerHost, unsigned short managerPort,
	int screenWidth, int screenHeight,
	const std::string & certificate,
	const std::string & managerPath, const std::string & managerParam):
_server(0) {
	//Erstmal dürfte jetzt die Authorisierung und die Anfrage an den Manager geschehen
	//Dazu einfach über nen Socket ne primitive http anfrage senden und die Antwort auswerten
	//Achtung momentan ist BUF_SIZE auch die maximale Nachrichtengröße die Empfangen werden kann!!
	const int BUF_SIZE = 2048;
	char tmpBuffer[BUF_SIZE];
	SOCKET httpSocket = rfbConnectToTcpAddr(const_cast<char*>(managerHost.c_str()), managerPort);
	if (httpSocket == INVALID_SOCKET) {
		std::cerr << "Failed to connect to " << managerHost << ":" << managerPort << std::endl;
		throw std::runtime_error(STR_ERR_WEBHOST_UNREACHABLE);
	}

	//gnutls aufbauen
#ifdef HAS_GNUTLS
	gnutls::client_session session;
	gnutls::certificate_credentials credentials;

	gnutls_datum_t data;
	data.size = certificate.size();
	data.data = (unsigned char*)certificate.data();
	credentials.set_x509_trust(data, GNUTLS_X509_FMT_PEM);
	session.set_credentials(credentials);
	session.set_priority("NORMAL", 0);
	session.set_transport_ptr((gnutls_transport_ptr_t)(ptrdiff_t)httpSocket);
	
	if ( session.handshake() < 0) {
		std::cerr << "TLS failed." << std::endl;
		throw std::runtime_error("TLS failed.");
	}
	else {
		std::cout<<session.get_cipher()<<std::endl;
	}
#endif

	std::string httpRequest;
	std::string httpRequestBody = managerParam + "=" + urlencode(conferenceUrl) + "&version=" + std::to_string(TOOL_VERSION);
	httpRequest += "POST ";
	httpRequest += managerPath;
	httpRequest += " HTTP/1.1\r\n";
	httpRequest += "Host: ";
	httpRequest += managerHost + "\r\n";
	httpRequest += "Content-Type: application/x-www-form-urlencoded\r\n";
	httpRequest += "Connection: close\r\n";
	sprintf(tmpBuffer, "%d", httpRequestBody.length());
	httpRequest += "Content-Length: " + std::string(tmpBuffer) + "\r\n";
	httpRequest += "\r\n";
	httpRequest += httpRequestBody;

	std::cout << "SEND >>" << std::endl << httpRequest << std::endl << "<<" << std::endl;
#ifdef HAS_GNUTLS
	session.send(httpRequest.data(), httpRequest.length());
	int c = session.recv(tmpBuffer, BUF_SIZE);
#else
	send(httpSocket, httpRequest.data(), httpRequest.length(), 0);

	//Alles lesen und hoffen dass sich der Webserver dran hält und die Verbindung schließt
	//wenn er fertig mit Senden ist
	int c = 0, r = 0;
	do {
		r = recv(httpSocket, tmpBuffer + c, BUF_SIZE - c, 0);
		if (r > 0) c += r;
	}  while (r > 0 && c < BUF_SIZE);
#endif
	
	if (c > 1024 || r < 0) {
		std::cerr << "Couldn't receive answer." << std::endl;
		throw std::runtime_error(STR_ERR_WRONG_ANSWER);
	}
	std::string httpResponse(tmpBuffer, c);
	std::cout << "RECV >>" << std::endl << httpResponse << std::endl << "<<" << std::endl;
	/**
		Antwort sollte jetzt der typische HTTP Antwortquark sein und als Inhalt
		sollte ein Text der folgenden Form sein:

		PresentationServerUseHost: <host>\n
		PresentationServerUsePort: <port>\n
	*/
	unsigned short port;
	std::string host;
	int lifetime;
	
	try {
		_messageBox = utf8_to_ucs2(getParameter(httpResponse, "MessageBox", ""));
		port = atoi(getParameter(httpResponse, "PresentationServerUsePort").c_str());
		host = getParameter(httpResponse, "PresentationServerUseHost");
		_demo = atoi(getParameter(httpResponse, "Demo", "0").c_str())?true:false;
		lifetime = atoi(getParameter(httpResponse, "RuntimeSec", "0").c_str());
	}
	catch (std::runtime_error e) {
		if (!_messageBox.empty())
			throw runtime_error_with_extra_msg(_messageBox, getParameter(httpResponse, "Message"));
		throw std::runtime_error(getParameter(httpResponse,"Message"));
	}
	//@TOOD vlt unterstützung von http weiterleitungen einbauen.

	//Fertig schließen
#ifdef HAS_GNUTLS
	session.bye(GNUTLS_SHUT_RDWR);
#endif
	closesocket(httpSocket);

	//Wenn die erfolgreich war dann den Server erstellen, Größe = Desktopgröße
	_server = rfbGetScreen(0, 0, screenWidth, screenHeight, 8, 3, 4);
	int buffersize = _server->width * _server->height * (_server->bitsPerPixel / 8);
	_server->frameBuffer = new char[buffersize];

	_server->alwaysShared = false;
	_server->neverShared = true;

	//Kein horchen auf neue Verbindungen
	_server->autoPort = false;
	_server->port = -1;
	_server->ipv6port = -1;
	_server->httpPort = -1;
	_server->http6Port = -1;

	/* Initialize the server */
	rfbInitServer(_server);

	if (!rfbReverseConnection(_server, const_cast<char*>(host.c_str()), port)) {
		std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
		throw std::runtime_error(STR_ERR_RFBHOST_UNREACHABLE);
	}
	
	_tick = std::chrono::high_resolution_clock::now();
	if (lifetime > 0) {
		_timeOfDeath = std::chrono::system_clock::now() + std::chrono::seconds(lifetime);
		_useTimeOfDeath = true;
	}
	else
		_useTimeOfDeath = false;

}

PresentationServer::~PresentationServer() {
	delete[] _server->frameBuffer;
	_server->frameBuffer = 0;
	rfbScreenCleanup(_server);
}

bool PresentationServer::run() {
	//Kein Client mehr da?
	if (_server->clientHead == 0)
		return false;

	//Server runtergefahren?
	if (!rfbIsActive(_server))
		return false;

	if (_capture) {
		//5 Bilder / Sekunde machen
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _tick).count() > 200) {
			_capture->capture(_server);
			_tick = std::chrono::high_resolution_clock::now();
		}
	}

	rfbProcessEvents(_server, 1000);

	// Wird die Präsentation irgendwann mit Gewalt beendet, dann kommen wir dem zuvor
	if (_useTimeOfDeath) {
		if (std::chrono::system_clock::now() > _timeOfDeath)
			return false;
	}

	return true; //Weitermachen
}