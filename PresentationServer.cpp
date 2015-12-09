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
#include "PresentationServer.h"
#include <string>
#include <stdlib.h>
#include <iostream>
extern "C" {
	#include <gnutls/gnutls.h>
	#include <rfb/rfb.h>
	struct rfbssl_ctx {
		char peekbuf[2048];
		int peeklen;
		int peekstart;
		gnutls_session_t session;
		gnutls_certificate_credentials_t x509_cred;
		gnutls_dh_params_t dh_params;
	#ifdef I_LIKE_RSA_PARAMS_THAT_MUCH
		gnutls_rsa_params_t rsa_params;
	#endif
	};
}

const char* const STR_ERR_WEBHOST_UNREACHABLE = "Konferenzen.eu nicht erreichbar";
const char* const STR_ERR_RFBHOST_UNREACHABLE = "VNC Proxy nicht erreichbar";
const char* const STR_ERR_WRONG_ANSWER = "Fehlerhafte Antwort von Server";
const char* const STR_ERR_TLS_FAILED = "Sichere Verbindung fehlgeschlagen";

const char* const CIPHERSUITE_PRIORITIES = "SECURE128:SUITEB128:-VERS-SSL3.0:-VERS-TLS1.0:-VERS-TLS1.1:-RSA:-DHE-RSA:-SHA1";

//Version des Tools, die an den Webserver �bermittelt wird,
//immer dann erh�hen wenn �nderungen zu Inkompatibilit�t f�hren k�nnten
const int TOOL_VERSION = 3;

std::string urlencode(const std::string & param) {
	std::string r;
	char tmp[4];
	for (auto & c : param) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			r += c;
		} else {
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
		std::unique_ptr<wchar_t[] > tmpb(new wchar_t[tmpbsize + 1]);
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
		return str.substr(pos, pos2 == std::string::npos ? pos2 : pos2 - pos);
	}

	std::cerr << "Parameter " << name << " not found in answer" << std::endl;
	throw std::runtime_error(STR_ERR_WRONG_ANSWER);
}

//Parst den Parameter aus der Antwort. Anders als die erste Variante wird, wenn dieser nicht gefunden wurde keine Exception geworfen
//sondern der �bergebene Defaultwert zur�ckgegeben

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

namespace {
	//Um Daten an den ClientCreation Hook zu reichen müssen jetzt einfach mal diese globalen Variablen herhalten...
	std::string g_peerHostName;
	std::string g_caCertificate;
	std::string g_serverPassword;
}

rfbNewClientAction upgradeNewClientToTls (_rfbClientRec* cl) {
	//Eine SSL Session beginnen
	gnutls_session_t session = 0;
	gnutls_certificate_credentials_t credentials = 0;
	gnutls_datum_t data;
	int gtlsRet = GNUTLS_E_SUCCESS;
	std::cout<<"New Client Connection, upgrade protocol to Tls"<<std::endl;
	try {
		//Zertifikat
		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_certificate_allocate_credentials(&credentials))) {
			std::cerr << "failed to allocate credentials." << std::endl;
			throw std::runtime_error("failed to allocate credentials.");
		}

		data.size = g_caCertificate.size();
		data.data = (unsigned char*) g_caCertificate.data();
		gnutls_certificate_set_x509_trust_mem(credentials, &data, GNUTLS_X509_FMT_PEM);

		// Verifizierung des Zertifikats in der übergebenen Callback Funktion, Ausführung erfolgt als Teil des Handshakes
		gnutls_certificate_set_verify_function(credentials, [] (gnutls_session_t session) throw () -> int {
			std::cout << "verifying certificate...";
			//Server Zertifikat prüfen:
			unsigned int verify = 0;
			if (GNUTLS_E_SUCCESS != gnutls_certificate_verify_peers3(session, (const char*) gnutls_session_get_ptr(session), &verify)) {
				std::cerr << "certficate verification failed." << std::endl;
				return -1;
			}
			if (verify != 0) {
				gnutls_datum_t pr;
				std::cout << "no" << std::endl;
				gnutls_certificate_verification_status_print(verify, GNUTLS_CRT_X509, &pr, 0);
				std::cerr << pr.data << std::endl;
				free(pr.data);
				return -2;
			}
			std::cout << "yes" << std::endl;
			return 0;
		});
		//Session
		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_init(&session, GNUTLS_CLIENT))) {
			std::cerr << "failed to init session." << std::endl;
			throw std::runtime_error("failed to init session.");
		}

		gnutls_server_name_set(session, GNUTLS_NAME_DNS, g_peerHostName.data(), g_peerHostName.length());
		gnutls_session_set_ptr(session, (void*) g_peerHostName.c_str());

		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_priority_set_direct(session, CIPHERSUITE_PRIORITIES, 0))) {
			std::cerr << "failed to set priority." << std::endl;
			throw std::runtime_error("failed to set priority.");
		}

		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
		gnutls_transport_set_int(session, cl->sock);
		gnutls_handshake_set_timeout(session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
		if (GNUTLS_E_SUCCESS != ((gtlsRet = gnutls_handshake(session)))) {
			std::cerr << "handshake failed." << std::endl;
			throw std::runtime_error("handshake failed.");
		}

		//Ciphersuite 
		std::cout << "ciphersuite: " << gnutls_cipher_suite_get_name(gnutls_kx_get(session), gnutls_cipher_get(session), gnutls_mac_get(session)) << std::endl;

		//Send password, which was received in answer of webserver
		if ((gtlsRet = gnutls_record_send(session, (void*)g_serverPassword.data(), g_serverPassword.size())) <= 0) {
			std::cerr << "Sending Password failed." << std::endl;
			throw std::runtime_error("Sending Password failed.");
		}

		rfbssl_ctx* ctx = (rfbssl_ctx*) malloc(sizeof(struct rfbssl_ctx));
		ctx->peeklen = 0;
		ctx->peekstart = 0;
		ctx->session = session;
		ctx->x509_cred = credentials;
		ctx->dh_params = 0;
		
		cl->sslctx = (rfbSslCtx*)ctx;
	} catch (...) {
		//Irgendein Fehler trat auf, dann schließen.
		std::cerr << gtlsRet << ' ' << gnutls_error_is_fatal(gtlsRet) << std::endl;
		std::cerr << gnutls_strerror(gtlsRet) << std::endl;
		std::cerr << gnutls_alert_get_name(gnutls_alert_get(session)) << std::endl;
		if (session) gnutls_deinit(session);
		if (credentials) gnutls_certificate_free_credentials(credentials);
		
		return RFB_CLIENT_REFUSE;
	}
	
	return RFB_CLIENT_ACCEPT;
}

PresentationServer::PresentationServer(const std::string & conferenceUrl,
		const std::string & managerHost, unsigned short managerPort,
		int screenWidth, int screenHeight,
		const std::string & caCertificate,
		const std::string & managerPath, const std::string & managerParam) :
_server(0), _fps(5) {

	if (caCertificate.empty()) {
		throw std::invalid_argument("no CA Certificate provided!");
	}
	//Erstmal d�rfte jetzt die Authorisierung und die Anfrage an den Manager geschehen
	//Dazu einfach �ber nen Socket ne primitive http anfrage senden und die Antwort auswerten
	//Achtung momentan ist BUF_SIZE auch die maximale Nachrichtengr��e die Empfangen werden kann!!
	const int BUF_SIZE = 2048;
	char tmpBuffer[BUF_SIZE];
	SOCKET httpSocket = rfbConnectToTcpAddr(const_cast<char*> (managerHost.c_str()), managerPort);
	std::string httpResponse;
	if (httpSocket == INVALID_SOCKET) {
		std::cerr << "Failed to connect to " << managerHost << ":" << managerPort << std::endl;
		throw std::runtime_error(STR_ERR_WEBHOST_UNREACHABLE);
		return;
	}

	//HTTPS Verbindung mit GnuTLS und handkodierter HTTP Nachricht :)
	gnutls_session_t session = 0;
	gnutls_certificate_credentials_t credentials = 0;
	gnutls_datum_t data;
	int gtlsRet = GNUTLS_E_SUCCESS;
	try {
		//Zertifikat
		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_certificate_allocate_credentials(&credentials))) {
			std::cerr << "failed to allocate credentials." << std::endl;
			throw std::runtime_error("failed to allocate credentials.");
		}

		data.size = caCertificate.size();
		data.data = (unsigned char*) caCertificate.data();
		gnutls_certificate_set_x509_trust_mem(credentials, &data, GNUTLS_X509_FMT_PEM);
		
		// Verifizierung des Zertifikats in der übergebenen Callback Funktion, Ausführung erfolgt als Teil des Handshakes
		gnutls_certificate_set_verify_function(credentials, [] (gnutls_session_t session) throw () -> int {
			std::cout << "verifying certificate...";
			//Server Zertifikat prüfen:
			unsigned int verify = 0;
			if (GNUTLS_E_SUCCESS != gnutls_certificate_verify_peers3(session, (const char*) gnutls_session_get_ptr(session), &verify)) {
				std::cerr << "certficate verification failed." << std::endl;
				return -1;
			}
			if (verify != 0) {
				gnutls_datum_t pr;
				std::cout << "no" << std::endl;
				gnutls_certificate_verification_status_print(verify, GNUTLS_CRT_X509, &pr, 0);
				std::cerr << pr.data << std::endl;
				free(pr.data);
				return -2;
			}
			std::cout << "yes" << std::endl;
			return 0;
		});
		//Session
		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_init(&session, GNUTLS_CLIENT))) {
			std::cerr << "failed to init session." << std::endl;
			throw std::runtime_error("failed to init session.");
		}

		gnutls_server_name_set(session, GNUTLS_NAME_DNS, managerHost.data(), managerHost.length());
		gnutls_session_set_ptr(session, (void*) managerHost.c_str());

		if (GNUTLS_E_SUCCESS != (gtlsRet = gnutls_priority_set_direct(session, CIPHERSUITE_PRIORITIES, 0))) {
			std::cerr << "failed to set priority." << std::endl;
			throw std::runtime_error("failed to set priority.");
		}

		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, credentials);
		gnutls_transport_set_int(session, httpSocket);
		gnutls_handshake_set_timeout(session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
		if (GNUTLS_E_SUCCESS != ((gtlsRet = gnutls_handshake(session)))) {
			std::cerr << "handshake failed." << std::endl;
			throw std::runtime_error("handshake failed.");
		}

		//Ciphersuite 
		std::cout << "ciphersuite: " << gnutls_cipher_suite_get_name(gnutls_kx_get(session), gnutls_cipher_get(session), gnutls_mac_get(session)) << std::endl;
		//Prepare HTTP Request
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

		gnutls_record_send(session, httpRequest.data(), httpRequest.length());

		std::cout << "WAITING TO RECEIVE.." << std::endl;
		//Alles lesen und hoffen dass sich der Webserver dran h�lt und die Verbindung schlie�t
		//wenn er fertig mit Senden ist
		int c = 0, r = 0;

		do {
			r = gnutls_record_recv(session, tmpBuffer + c, BUF_SIZE - c);
			if (r > 0) c += r;
		} while (r > 0 && c < BUF_SIZE);

		if (c > 1024 || c <= 0) {
			std::cout << "received " << c << " bytes" << std::endl;
			std::cout << std::string(tmpBuffer, c) << std::endl;
			std::cerr << "Couldn't receive answer." << std::endl;
			throw std::runtime_error(STR_ERR_WRONG_ANSWER);
		}

		httpResponse = std::string(tmpBuffer, c);

		//Und fertig Verbindung beenden
		gnutls_bye(session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(session);
		gnutls_certificate_free_credentials(credentials);
		closesocket(httpSocket);
	} catch (...) {
		//Irgendein Fehler trat auf, dann schließen.
		std::cerr << gtlsRet << ' ' << gnutls_error_is_fatal(gtlsRet) << std::endl;
		std::cerr << gnutls_strerror(gtlsRet) << std::endl;
		std::cerr << gnutls_alert_get_name(gnutls_alert_get(session)) << std::endl;
		if (session) gnutls_deinit(session);
		if (credentials) gnutls_certificate_free_credentials(credentials);
		closesocket(httpSocket);
		throw std::runtime_error(STR_ERR_TLS_FAILED); //weiterschmeißen
	}
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
		_demo = atoi(getParameter(httpResponse, "Demo", "0").c_str()) ? true : false;
		lifetime = atoi(getParameter(httpResponse, "RuntimeSec", "0").c_str());
		_serverPassword = getParameter(httpResponse, "PresentationServerPassword");
	} catch (std::runtime_error e) {
		if (!_messageBox.empty())
			throw runtime_error_with_extra_msg(_messageBox, getParameter(httpResponse, "Message"));
		throw std::runtime_error(getParameter(httpResponse, "Message"));
	}

	//Wenn die erfolgreich war dann den Server erstellen, Gr��e = Desktopgr��e
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
	
	g_serverPassword = _serverPassword;
	g_peerHostName = managerHost;
	g_caCertificate = caCertificate;
	_server->newClientCreationHook = upgradeNewClientToTls;

	/* Initialize the server */
	rfbInitServer(_server);

	if (!rfbReverseConnection(_server, const_cast<char*> (host.c_str()), port)) {
		std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
		throw std::runtime_error(STR_ERR_RFBHOST_UNREACHABLE);
	}

	_tick = std::chrono::high_resolution_clock::now();
	if (lifetime > 0) {
		_timeOfDeath = std::chrono::system_clock::now() + std::chrono::seconds(lifetime);
		_useTimeOfDeath = true;
	} else {
		_useTimeOfDeath = false;
	}

}

PresentationServer::~PresentationServer() {
	if (_server) {
		delete[] _server->frameBuffer;
		_server->frameBuffer = 0;
		rfbScreenCleanup(_server);
	}
}

bool PresentationServer::run() {
	//Server tot oder kein Client mehr da?
	if (!_server || _server->clientHead == 0)
		return false;

	//Server runtergefahren?
	if (!rfbIsActive(_server))
		return false;

	if (_capture) {
		//5 Bilder / Sekunde machen
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _tick).count() > (1000 / _fps)) {
			_capture->capture(_server);
			_tick = std::chrono::high_resolution_clock::now();
		}
	}

	rfbProcessEvents(_server, 1000);

	// Soll die Präsentation 
	if (_useTimeOfDeath) {
		if (std::chrono::system_clock::now() > _timeOfDeath)
			return false;
	}

	return true; //Weitermachen
}