/*********************************************************************
RainFbx - Rainmeter Plugin for Freebox API
Copyright (C) 2016 - Christophe Lampin

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#include <Windows.h>
#include "RainmeterAPI.h"
#include <WinCrypt.h>
#include <comdef.h>

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/json.h>
#include <string>
#include <map>
#include <iostream>
#include <typeinfo>
#include <intsafe.h>
#include <pplcancellation_token.h>

using namespace std;
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams
using namespace web::json;                  // JSON library
using namespace utility;

#ifndef CALG_HMAC
#define CALG_HMAC (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_HMAC)
#endif

#ifndef CRYPT_IPSEC_HMAC_KEY
#define CRYPT_IPSEC_HMAC_KEY 0x00000100
#endif

void RmLogW(int level, wstring message);

typedef struct _my_blob {
	BLOBHEADER header;
	DWORD len;
	BYTE key[0];
}my_blob;

enum FbxAPIStatus
{
	PENDING,
	REGISTER,
	UNREGISTER,
	LOGOUT,
	LOGIN,
	UNKNOWN,
	REVOKE,
	TIMEOUT,
	DENIED,
	GRANTED,
	INSUFFICIENT_RIGHTS,
	AUTH_REQUIRED
};

enum FbxFormat
{
	byte_s,
	kbits_s,
	celsius,
	decibel,
	rpm,
	percent,
	none
};

static map<wstring, map<wstring, FbxFormat>> FbxMeasures = {
	{ L"net", {
			{ L"bw_up", byte_s },			// upload available bandwidth (in byte/s)
			{ L"bw_down", byte_s },			// download available bandwidth (in byte/
			{ L"rate_up", byte_s },			// upload rate (in byte/s)
			{ L"rate_down", byte_s },		// download rate (in byte/s)
			{ L"vpn_rate_up", byte_s },		// vpn client upload rate (in byte/s)
			{ L"vpn_rate_down", byte_s }	// vpn client download rate (in byte/s)
		}
	},
	{ L"temp", {
			{ L"cpum", celsius },		// temperature cpum (in °C)
			{ L"cpub", celsius },		// temperature cpub (in °C)
			{ L"sw", celsius },			// temperature sw (in °C)
			{ L"hdd", celsius },		// temperature hdd (in °C)
			{ L"fan_speed", rpm}		// fan rpm
		}
	},
	{ L"dsl", {
			{ L"rate_up", kbits_s },		// dsl available upload bandwidth (in byte/s)
			{ L"rate_down", kbits_s },	// dsl available download bandwidth (in byte/s)
			{ L"snr_up", decibel },		// upload signal/noise ratio (in 1/10 dB)
			{ L"snr_down", decibel }	// dsl download signal/noise ratio (in 1/10 dB)
		}
	},
	{ L"net", {
			{ L"rx_1", byte_s },		// receive rate on port 1 (in byte/s)
			{ L"tx_1", byte_s },		// transmit on port 1 (in byte/s)
			{ L"rx_2", byte_s },		// receive rate on port 2 (in byte/s)
			{ L"tx_2", byte_s },		// transmit on port 2 (in byte/s)
			{ L"rx_3", byte_s },		// receive rate on port 3 (in byte/s)
			{ L"tx_3", byte_s },		// transmit on port 3 (in byte/s)
			{ L"rx_4", byte_s },		// receive rate on port 4 (in byte/s)
			{ L"tx_4", byte_s }			// transmit on port 4 (in byte/s)
		}
	},
	{ L"composite", {
			{ L"busy_up", none },		// % of up bandwith occupation (net.rate_up / dsl.rate_up)
			{ L"busy_down", none },		// % of down bandwith occupation (net.down_up / dsl.down_up)
			{ L"status", none }		// % of down bandwith occupation (net.down_up / dsl.down_up)
		}
	}
};

class FbxAPI {

public:
	static FbxAPI& Instance();

	wstring hostname;
	wstring tracking_status;
	FbxAPIStatus global_status;
	wstring challenge;
	wstring password;
	wstring password_salt;
	wstring app_token;
	wstring track_id;
	wstring logged_in;
	wstring session_token;

	bool fbxAPIInUse = false;

	wstring rrd_error = L"";

	pplx::cancellation_token_source cts;
	
	void setHostname(LPCWSTR hostname);

	pplx::task<bool> Register();

	pplx::task<bool> Track();

	pplx::task<bool> Login();

	pplx::task<bool> Session();

	bool updateMeasure(wstring database, wstring measure);

	double getNumericMeasure(wstring database, wstring measure);

	wstring getFormattedMeasure(wstring database, wstring measure);

	void updateStatus();

	static wstring format(wstring input, FbxFormat format);

	wstring getStatusMsg();

	bool executeNextCall(wstring db = L"", wstring field = L"");

private:
	
	map <wstring, map<wstring, double>> numericMeasures = {
		{ L"net",{
			{ L"bw_up", 0 },			// upload available bandwidth (in byte/s)
			{ L"bw_down", 0 },			// download available bandwidth (in byte/
			{ L"rate_up", 0 },			// upload rate (in byte/s)
			{ L"rate_down", 0 },		// download rate (in byte/s)
			{ L"vpn_rate_up", 0 },		// vpn client upload rate (in byte/s)
			{ L"vpn_rate_down", 0 }	// vpn client download rate (in byte/s)
		}
		},
		{ L"temp",{
			{ L"cpum", 0 },		// temperature cpum (in °C)
			{ L"cpub", 0 },		// temperature cpub (in °C)
			{ L"sw", 0 },			// temperature sw (in °C)
			{ L"hdd", 0 },		// temperature hdd (in °C)
			{ L"fan_speed", 0 }		// fan rpm
		}
		},
		{ L"dsl",{
			{ L"rate_up", 0 },		// dsl available upload bandwidth (in byte/s)
			{ L"rate_down", 0 },	// dsl available download bandwidth (in byte/s)
			{ L"snr_up", 0 },		// upload signal/noise ratio (in 1/10 dB)
			{ L"snr_down", 0 }	// dsl download signal/noise ratio (in 1/10 dB)
		}
		},
		{ L"net",{
			{ L"rx_1", 0 },		// receive rate on port 1 (in byte/s)
			{ L"tx_1", 0 },		// transmit on port 1 (in byte/s)
			{ L"rx_2", 0 },		// receive rate on port 2 (in byte/s)
			{ L"tx_2", 0 },		// transmit on port 2 (in byte/s)
			{ L"rx_3", 0 },		// receive rate on port 3 (in byte/s)
			{ L"tx_3", 0 },		// transmit on port 3 (in byte/s)
			{ L"rx_4", 0 },		// receive rate on port 4 (in byte/s)
			{ L"tx_4", 0 }			// transmit on port 4 (in byte/s)
		}
		},
		{ L"composite",{
			{ L"status", 0 },		// status
			{ L"busy_up", 0 },		// % of up bandwith occupation (net.rate_up / dsl.rate_up)
			{ L"busy_down", 0 }		// % of down bandwith occupation (net.down_up / dsl.down_up)
		}
		}
	};

	map <wstring, map<wstring, wstring>> formattedMeasures = {
		{ L"net",{
			{ L"bw_up", L"" },			// upload available bandwidth (in byte/s)
			{ L"bw_down", L"" },			// download available bandwidth (in byte/
			{ L"rate_up", L"" },			// upload rate (in byte/s)
			{ L"rate_down", L"" },		// download rate (in byte/s)
			{ L"vpn_rate_up", L"" },		// vpn client upload rate (in byte/s)
			{ L"vpn_rate_down", L"" }	// vpn client download rate (in byte/s)
		}
		},
		{ L"temp",{
			{ L"cpum", L"" },		// temperature cpum (in °C)
			{ L"cpub", L"" },		// temperature cpub (in °C)
			{ L"sw", L"" },			// temperature sw (in °C)
			{ L"hdd", L"" },		// temperature hdd (in °C)
			{ L"fan_speed", L"" }		// fan rpm
		}
		},
		{ L"dsl",{
			{ L"rate_up", L"" },		// dsl available upload bandwidth (in byte/s)
			{ L"rate_down", L"" },	// dsl available download bandwidth (in byte/s)
			{ L"snr_up", L"" },		// upload signal/noise ratio (in 1/10 dB)
			{ L"snr_down", L"" }	// dsl download signal/noise ratio (in 1/10 dB)
		}
		},
		{ L"net",{
			{ L"rx_1", L"" },		// receive rate on port 1 (in byte/s)
			{ L"tx_1", L"" },		// transmit on port 1 (in byte/s)
			{ L"rx_2", L"" },		// receive rate on port 2 (in byte/s)
			{ L"tx_2", L"" },		// transmit on port 2 (in byte/s)
			{ L"rx_3", L"" },		// receive rate on port 3 (in byte/s)
			{ L"tx_3", L"" },		// transmit on port 3 (in byte/s)
			{ L"rx_4", L"" },		// receive rate on port 4 (in byte/s)
			{ L"tx_4", L"" }			// transmit on port 4 (in byte/s)
		}
		},
		{ L"composite",{
			{ L"status", L"" },		// status
			{ L"busy_up", L"" },		// % of up bandwith occupation (net.rate_up / dsl.rate_up)
			{ L"busy_down", L"" }		// % of down bandwith occupation (net.down_up / dsl.down_up)
		}
		}
	};

	pplx::task<json::value> handleFbxAPIResponse(http_response response);

	wstring getLastRRDData(wstring value, json::value json_array);

	char * HMAC(char * str, char * password, DWORD AlgId = CALG_SHA1);

	pplx::task<bool> RRD(wstring database, wstring measure, FbxFormat format);

	FbxAPI& operator= (const FbxAPI&) {}

	FbxAPI(const FbxAPI&) {}

	static FbxAPI m_instance;

	FbxAPI();

	~FbxAPI();
};