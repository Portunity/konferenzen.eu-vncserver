﻿/*
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

//Konstanten
const wchar_t* STR_WINDOW_TITLE = L"Konferenzen.eu Bildschirmübertragung";	// The title bar text

const char* STR_MANAGER_HOST = "www.konferenzen.eu";
const unsigned short STR_MANAGER_PORT = 80;

const char* STR_MANAGER_HOST_DEV = "www.dev.konferenzen.eu";
const unsigned short STR_MANAGER_PORT_DEV = 3062;

const wchar_t* STR_INFO_TEXT = L"Dieses Programm überträgt den Bildschirminhalt Ihres Monitors an\nIhren Konferenzraum bei konferenzen.eu für Ihre Konferenzteilnehmer:";
const wchar_t* STR_INPUT_LABEL = L"URL Ihres Konferenzraumes";
const wchar_t* STR_DEMO = L"Demo-Modus:";
const wchar_t* STR_ERR_NO_URL = L"Bitte Url angeben.";

const wchar_t* STR_OPEN_LINK_FROM_LOGO = L"http://www.konferenzen.eu";

//IDs der eingebetteten Ressourcen
#define RID_FAVICON 9001

#define RID_CERT 9002
#define RID_CERT_TYPE 10002

#define RID_BACKGROUND 9010
#define RID_LOGO 9020
#define RID_BUTTON_START 9021
#define RID_BUTTON_END 9022
#define RID_PNG_TYPE 10003