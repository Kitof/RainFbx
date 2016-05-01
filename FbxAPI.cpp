#include "fbxAPI.h"

/* Utilities Functions */

/* RmLog with wstring */
void RmLogW(int level, wstring message) {
	RmLog(level, _wcsdup(message.c_str()));
}

/* Crop first and last char of a wstring, useful to remove " */
wstring crop(wstring tocrop) {
	if (tocrop.size() < 2) return tocrop;
	tocrop.erase(0, 1);
	tocrop.erase(tocrop.size() - 1);
	return tocrop;
}

/* Convert wchar to integer */
int wctoi(const wchar_t * input_wc) {
	size_t outSize;
	char * input_c = new char[wcslen(input_wc)];
	wcstombs_s(&outSize, input_c, wcslen(input_wc), input_wc, wcslen(input_wc) - 1);
	return atoi(input_c);
}

/* Return length of a double */
unsigned int dbllen(double n) {
	string input;
	cout << n;
	cin >> input;
	return (int)input.size();
}

/* Return hostname of current computer */
void getHostName(wchar_t * infoBuf) {
	DWORD  bufCharCount = 1014;
	if (!GetComputerName(infoBuf, &bufCharCount))
		infoBuf = L"Unknown";
}

/* Convert char to wchar (shorcut for mbstowcs_s) */
void charToWchar(const char * input, wchar_t * output) {
	size_t nbcopy, size = strlen(input) + 1;
	mbstowcs_s(&nbcopy, output, size, input, size);
}

/* Convert wchar to char (shorcut for wcstombs_s)  */
void wcharToChar(const wchar_t * input, char * output) {
	size_t nbcopy, size = wcslen(input) + 1;
	wcstombs_s(&nbcopy, output, size, input, size);
}

/* FbxAPI Implementation */

// Singleton initialisation
FbxAPI FbxAPI::m_instance = FbxAPI();

// Constructor
FbxAPI::FbxAPI() {}

// Destructor
FbxAPI::~FbxAPI() {
	this->cts.cancel();
}

// Return singleton instance
FbxAPI& FbxAPI::Instance() {
	return m_instance;
}

// Set Hostname of Freebox OS
void FbxAPI::setHostname(LPCWSTR hostname) {
	if ( _wcsicmp(hostname, L"") == 0)
	{
		this->hostname = L"http://mafreebox.freebox.fr/";
	} else {
		this->hostname = hostname;
	}
}

// Generic function to handle Freebox API response
pplx::task<json::value> FbxAPI::handleFbxAPIResponse(http_response response) {
	this->fbxAPIInUse = false;
	try {
		if ((response.status_code() == status_codes::OK) || (response.status_code() == status_codes::Forbidden) )
		{
			return response.extract_json();
		}
		RmLogW(LOG_DEBUG, L"RainFbx.dll : Wrong Status_Code : " + response.status_code());
		return pplx::task_from_result(json::value());
	}
	catch (const exception &) {
		RmLog(LOG_DEBUG, L"RainFbx.dll : Empty Response");
		return pplx::task_from_result(json::value());
	}
}

// Register to Freebox API
pplx::task<bool> FbxAPI::Register()
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: FBXRegister");
	this->fbxAPIInUse = true;
	json::value query;
	query[L"app_id"] = json::value::string(L"net.rainmeter.plugin.rainfbx");
	query[L"app_name"] = json::value::string(L"Rainmeter Freebox Plugin");
	query[L"app_version"] = json::value::string(L"0.0.1");	// TODO : Find a proper way

	// Set requester hostname
	wchar_t local_hostname[1024];
	getHostName(local_hostname);
	query[L"device_name"] = json::value::string(local_hostname);

	http_client client(this->hostname);
	return client.request(methods::POST, L"/api/v3/login/authorize/", conversions::to_utf16string(query.serialize()), L"application/json").then([this](http_response response) -> pplx::task<json::value>
	{
		return this->handleFbxAPIResponse(response);
	}, this->cts.get_token())
		.then([this](pplx::task<json::value> previousTask)
	{
		try {
			const json::value& v = previousTask.get();
			if (v.is_null()) return false;
			RmLogW(LOG_DEBUG, L"DisplayJSONValue: " + v.serialize());
			if (v.at(L"success").serialize() == L"true") {
				this->track_id = v.at(L"result").at(L"track_id").serialize();
				this->app_token = crop(v.at(L"result").at(L"app_token").serialize());
				return true;
			}
			return false;
		}
		catch (const http_exception &) {
			return false;
		}
	}, this->cts.get_token());
}

// Track previous authorization request
pplx::task<bool> FbxAPI::Track()
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: FBXTrack");
	this->fbxAPIInUse = true;

	http_client client(this->hostname);
	return client.request(methods::GET, L"/api/v3/login/authorize/" + track_id, L"", L"application/json").then([this](http_response response) -> pplx::task<json::value>
	{
		return this->handleFbxAPIResponse(response);
	}, this->cts.get_token())
		.then([this](pplx::task<json::value> previousTask)
	{
		try  {
			const json::value& v = previousTask.get();
			if (v.is_null()) return false;
			RmLogW(LOG_DEBUG, L"DisplayJSONValue: " + v.serialize());
			if (v.at(L"success").serialize() == L"true") {
				this->tracking_status = v.at(L"result").at(L"status").serialize();
				this->password_salt = v.at(L"result").at(L"password_salt").serialize();
				this->challenge = crop(v.at(L"result").at(L"challenge").serialize());
				return true;
			}
			return false;
		}
		catch (const http_exception &) {
			return false;
		}
	}, this->cts.get_token());
}

// Login on Freebox API
pplx::task<bool> FbxAPI::Login()
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: FBXLogin");
	this->fbxAPIInUse = true;

	http_client client(this->hostname);
	return client.request(methods::GET, L"/api/v3/login/", L"", L"application/json").then([this](http_response response) -> pplx::task<json::value>
	{
		return this->handleFbxAPIResponse(response);
	}, this->cts.get_token())
		.then([this](pplx::task<json::value> previousTask)
	{
		try {
			const json::value& v = previousTask.get();
			if (v.is_null()) return false;
			RmLogW(LOG_DEBUG, L"DisplayJSONValue: " + v.serialize());
			if (v.at(L"success").serialize() == L"true") {
				this->logged_in = v.at(L"result").at(L"logged_in").serialize();
				this->challenge = crop(v.at(L"result").at(L"challenge").serialize());
				return true;
			}
			return false;
		}
		catch (const http_exception &) {
			return false;
		}
	}, this->cts.get_token());
}

// Open a session on Freebox API
pplx::task<bool> FbxAPI::Session()
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: FBXSession");
	this->fbxAPIInUse = true;
	char * app_token_c = new char[this->app_token.size() + 1];
	wcharToChar(app_token.c_str(),app_token_c);
	char * challenge_c = new char[this->challenge.size() + 1];
	wcharToChar(this->challenge.c_str(), challenge_c);
	wchar_t password_wc[41];
	charToWchar(HMAC(challenge_c, app_token_c), password_wc);
	this->password = password_wc;

	RmLogW(LOG_DEBUG, L"RainFbx.dll: App_Token:" + this->app_token + L" Challenge:" + this->challenge + L" Password:" + password_wc);

	json::value query;
	query[L"app_id"] = json::value::string(L"net.rainmeter.plugin.rainfbx");
	query[L"password"] = json::value::string(password);

	http_client client(this->hostname);
	return client.request(methods::POST, L"/api/v3/login/session/", conversions::to_utf16string(query.serialize()), L"application/json").then([this](http_response response) -> pplx::task<json::value>
	{
		return this->handleFbxAPIResponse(response);
	}, this->cts.get_token())
		.then([this](pplx::task<json::value> previousTask)
	{
		try {
			const json::value& v = previousTask.get();
			if (v.is_null()) return false;
			RmLogW(LOG_DEBUG, L"DisplayJSONValue: " + v.serialize());
			if (v.at(L"success").serialize() == L"true") {
				this->session_token = crop(v.at(L"result").at(L"session_token").serialize());
				return true;
			}
			else {
				if (v.at(L"error_code").serialize() == L"\"invalid_token\"") this->challenge = crop(v.at(L"result").at(L"challenge").serialize());
			}
			return false;
		}
		catch (const http_exception &) {
			return false;
		}
	}, this->cts.get_token());
}

// Request a RRD value on Freebox API
pplx::task<bool> FbxAPI::RRD(wstring database, wstring measure, FbxFormat _format)
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: FBXRRD");
	this->fbxAPIInUse = true;
	json::value query;
	query[L"db"] = json::value::string(database);

	// TODO : There is probably a way to limit response to the last value to improve performance

	json::value field = json::value::string(measure);
	query[L"fields"] = json::value::array();
	query[L"fields"][0] = field;

	http_client client(this->hostname);
	http_request request(methods::POST);
	request.set_request_uri(L"/api/v3/rrd/");
	request.set_body(conversions::to_utf16string(query.serialize()));
	request.headers().add(L"X-Fbx-App-Auth", session_token);

	return client.request(request).then([this](http_response response) -> pplx::task<json::value>
	{
		return this->handleFbxAPIResponse(response);
	}, this->cts.get_token())
		.then([this, database, measure, _format](pplx::task<json::value> previousTask)
	{
		try {
			const json::value& v = previousTask.get();
			if (v.is_null()) return false;
			RmLogW(LOG_DEBUG, L"DisplayJSONValue: " + v.serialize());
			if (v.at(L"success").serialize() == L"true") {
				this->rrd_error = L"";
				wstring data = getLastRRDData(measure, v.at(L"result").at(L"data")).c_str();
				this->formattedMeasures[database][measure] = this->format(data, _format);
				this->numericMeasures[database][measure] = wctoi(data.c_str());
			}
			else {
				this->rrd_error = v.at(L"error_code").serialize();
				if (this->rrd_error == L"\"auth_required\"") this->challenge = crop(v.at(L"result").at(L"challenge").serialize());
			}
			return false;
		}
		catch (const http_exception &) {
			return false;
		}
	}, this->cts.get_token());
}

// Request a RRD value on Freebox API
bool FbxAPI::updateMeasure(wstring database, wstring measure) {
	RmLog(LOG_DEBUG, L"RainFbx.dll: updateMeasure");
	if (FbxMeasures.find(database) != FbxMeasures.end()) {
		if (FbxMeasures[database].find(measure) != FbxMeasures[database].end()) {
			if (database == L"composite") {
				if (measure == L"busy_down") {
					if (this->numericMeasures[L"dsl"][L"rate_down"] < 0.1  ) {
						RRD(L"dsl", L"rate_down", FbxMeasures[L"dsl"][L"rate_down"]);
					}
					RRD(L"net", L"rate_down", FbxMeasures[L"net"][L"rate_down"]);
				}
				if (measure == L"busy_up") {
					if (this->numericMeasures[L"dsl"][L"rate_up"] < 0.1) {
						RRD(L"dsl", L"rate_up", FbxMeasures[L"dsl"][L"rate_up"]);
					}
					RRD(L"net", L"rate_up", FbxMeasures[L"net"][L"rate_up"]);
				}
			} else RRD(database, measure, FbxMeasures[database][measure]);
			return true;
		}
	}
	RmLog(LOG_ERROR, L"RainFbx.dll: Unknow database/measure name.");
	return false;
}

// Get a numeric RRD Measure
double FbxAPI::getNumericMeasure(wstring database, wstring measure) {
	RmLog(LOG_DEBUG, L"RainFbx.dll: getNumericMeasure");
	if (database == L"composite") {
		if ((measure == L"busy_down") && (numericMeasures[L"dsl"][L"rate_down"] > 0.1)) {
			this->numericMeasures[L"composite"][L"busy_down"] = this->numericMeasures[L"net"][L"rate_down"] / (this->numericMeasures[L"dsl"][L"rate_down"] / 8 * 1000);
			this->formattedMeasures[L"composite"][L"busy_down"] = format(to_wstring((int)(numericMeasures[L"composite"][L"busy_down"] * 100)), none);
		}
		if ((measure == L"busy_up") && (numericMeasures[L"dsl"][L"rate_up"] > 0.1)) {
			this->numericMeasures[L"composite"][L"busy_up"] = this->numericMeasures[L"net"][L"rate_up"] / (this->numericMeasures[L"dsl"][L"rate_up"] / 8 * 1000);
			this->formattedMeasures[L"composite"][L"busy_up"] = format(to_wstring((int)(this->numericMeasures[L"composite"][L"busy_up"] * 100)), none);
		}
	}
	return this->numericMeasures[database][measure];
}

// Get a string formatted RRD Measure
wstring FbxAPI::getFormattedMeasure(wstring database, wstring measure) {
	RmLog(LOG_DEBUG, L"RainFbx.dll: getFormattedMeasure");
	if (database == L"composite") {
		if (measure == L"status") {
			this->formattedMeasures[L"composite"][L"status"] = this->getStatusMsg();
		}
		if ((measure == L"busy_down") && (this->numericMeasures[L"dsl"][L"rate_down"] > 0.1)) {
			this->numericMeasures[L"composite"][L"busy_down"] = this->numericMeasures[L"net"][L"rate_down"] / (this->numericMeasures[L"dsl"][L"rate_down"] / 8 * 1000);
			this->formattedMeasures[L"composite"][L"busy_down"] = format(to_wstring((int)(this->numericMeasures[L"composite"][L"busy_down"] * 100)), none);
		}
		if ((measure == L"busy_up") && (numericMeasures[L"dsl"][L"rate_up"] > 0.1)) {
			this->numericMeasures[L"composite"][L"busy_up"] = this->numericMeasures[L"net"][L"rate_up"] / (this->numericMeasures[L"dsl"][L"rate_up"] / 8 * 1000);
			this->formattedMeasures[L"composite"][L"busy_up"] = format(to_wstring((int)(this->numericMeasures[L"composite"][L"busy_up"] * 100)), none);
		}
	}
	return this->formattedMeasures[database][measure];
}

// Update Freebox API status according current state
void FbxAPI::updateStatus() {
	RmLog(LOG_DEBUG, L"RainFbx.dll: updateStatus");
	
	if (this->app_token == L"") {
		this->global_status = UNREGISTER;
		return;
	}

	if (this->track_id != L"") {
		if ((this->tracking_status == L"\"pending\"") || (this->tracking_status == L"")) {
			this->global_status = PENDING;
			return;
		}
		if (this->tracking_status == L"\"unknown\"") {
			this->global_status = REVOKE;
			return;
		}
		if (this->tracking_status == L"\"timeout\"") {
			this->global_status = TIMEOUT;
			return;
		}
		if (this->tracking_status == L"\"denied\"") {
			this->global_status = DENIED;
			return;
		}
		// if tracking_status == "Granted" continue
	}

	if ((this->session_token == L"") && (this->challenge == L"")) {
		this->global_status = LOGOUT;
		return;
	}

	if ((this->session_token == L"") && (this->challenge != L"")) {
		this->global_status = REGISTER;
		return;
	}

	if (this->session_token != L"") {
		if (this->rrd_error != L"") {
			if (this->rrd_error == L"\"insufficient_rights\"") {
				this->rrd_error = L"";
				this->global_status = INSUFFICIENT_RIGHTS;
				return;
			}
			if (this->rrd_error == L"\"auth_required\"") {
				this->session_token = L"";
				this->rrd_error = L"";
				this->global_status = REGISTER;
				return;
			}
			this->global_status = UNKNOWN;
			return;
		}
		this->global_status = LOGIN;
		return;
	}

	this->global_status = UNKNOWN;
	return;
}

// Get current status message
wstring FbxAPI::getStatusMsg() {
	RmLog(LOG_DEBUG, L"RainFbx.dll: getStatusMsg");
/*
	if (this->fbxAPIInUse) {
		return L"Interrogation de la Freebox...";
	}
*/
	switch (this->global_status) {
	case UNREGISTER:
		return L"Application non enregistrée.\nDemande d'autorisation.";

	case PENDING:
		return L"En attente d'autorisation.\nVérifiez l'affichage de votre Freebox.";

	case REVOKE:
		return L"Accès révoqué. Actualisez le skin.";

	case TIMEOUT:
		return L"Accès expiré. Actualisez le skin.";

	case DENIED:
		return L"Accès refusé. Actualisez le skin.";

	case LOGOUT:
		return L"Plugin autorisé.\nTentative d'authentification.";

	case REGISTER:
		return L"Authentifié.\nTentative d'ouverture de session.";

	case INSUFFICIENT_RIGHTS:
		return L"Droits insuffisants. Autorisez \"Modification des réglages de la Freebox\" sous FreeboxOS";

	case LOGIN:
		return L"Loggué.";

	default:
		return L"Erreur inconnue.";
	}
}

// Execute next call in Freebox API workflow
bool FbxAPI::executeNextCall(wstring db/* =L"" */, wstring field/* =L"" */) {
	RmLog(LOG_DEBUG, L"RainFbx.dll: executeNextCall");
	switch (this->global_status) {
	case UNREGISTER:
		if (!this->fbxAPIInUse) this->Register();
		return false;

	case PENDING:
		if (!this->fbxAPIInUse) this->Track();
		return false;

	case LOGOUT:
		if (!this->fbxAPIInUse) this->Login();
		return false;

	case REGISTER:
		if (!this->fbxAPIInUse) this->Session();
		return false;

	case INSUFFICIENT_RIGHTS:
		this->updateMeasure(db, field);
		return false;

	case LOGIN:
		this->updateMeasure(db, field);
		return true;

	default:
		return false;
	}
}

// Return HMAC of two strings
char * FbxAPI::HMAC(char * str, char * password, DWORD AlgId) {

	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	HCRYPTKEY hKey = 0;
	HCRYPTHASH hHmacHash = 0;
	BYTE * pbHash = 0;
	DWORD dwDataLen = 0;
	HMAC_INFO HmacInfo;

	int err = 0;

	ZeroMemory(&HmacInfo, sizeof(HmacInfo));

	if (AlgId == CALG_MD5) {
		HmacInfo.HashAlgid = CALG_MD5;
		pbHash = new BYTE[16];
		dwDataLen = 16;
	}
	else if (AlgId == CALG_SHA1) {
		HmacInfo.HashAlgid = CALG_SHA1;
		pbHash = new BYTE[20];
		dwDataLen = 20;
	}
	else {
		return 0;
	}

	ZeroMemory(pbHash, sizeof(dwDataLen));
	char * res = new char[(dwDataLen * 2) + 1];

	my_blob * kb = NULL;
	DWORD kbSize;
	SIZETToDWord(sizeof(my_blob) + strlen(password), &kbSize);
	kb = (my_blob*)malloc(kbSize);
	kb->header.bType = PLAINTEXTKEYBLOB;
	kb->header.bVersion = CUR_BLOB_VERSION;
	kb->header.reserved = 0;
	kb->header.aiKeyAlg = CALG_RC2;
	SIZETToDWord(strlen(password), &(kb->len));
	memcpy(&kb->key, password, strlen(password));

	if (!CryptAcquireContext(&hProv, NULL, MS_ENHANCED_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_NEWKEYSET)) {
		err = 1;
		goto Exit;
	}


	if (!CryptImportKey(hProv, (BYTE*)kb, kbSize, 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) {
		err = 1;
		goto Exit;
	}

	if (!CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHmacHash)) {
		err = 1;
		goto Exit;
	}


	if (!CryptSetHashParam(hHmacHash, HP_HMAC_INFO, (BYTE*)&HmacInfo, 0)) {
		err = 1;
		goto Exit;
	}

	DWORD str_len;
	SIZETToDWord(strlen(str), &str_len);
	if (!CryptHashData(hHmacHash, (BYTE*)str, str_len, 0)) {
		err = 1;
		goto Exit;
	}

	if (!CryptGetHashParam(hHmacHash, HP_HASHVAL, pbHash, &dwDataLen, 0)) {
		err = 1;
		goto Exit;
	}

	ZeroMemory(res, dwDataLen * 2 + 1);
	char * temp;
	temp = new char[3];
	ZeroMemory(temp, 3);
	for (unsigned int m = 0; m < dwDataLen; m++) {
		sprintf_s(temp, 3, "%2x", pbHash[m]);
		if (temp[1] == ' ') temp[1] = '0'; // note these two: they are two CORRECTIONS to the conversion in HEX, sometimes the Zeros are
		if (temp[0] == ' ') temp[0] = '0'; // printed with a space, so we replace spaces with zeros; ( error occurs mainly in HMAC-SHA1)
		sprintf_s(res, (dwDataLen * 2) + 1, "%s%s", res, temp);
	}

	return res;

Exit:
	free(kb);
	if (hHmacHash)
		CryptDestroyHash(hHmacHash);
	if (hKey)
		CryptDestroyKey(hKey);
	if (hHash)
		CryptDestroyHash(hHash);
	if (hProv)
		CryptReleaseContext(hProv, 0);


	if (err == 1) {
		delete[] res;
		return "";
	}

	return res;
}

// Format a string according FbxFormat
wstring FbxAPI::format(wstring input, FbxFormat format) {
	switch (format) {
	case byte_s:
		if (input.length() > 9) {
			return input.substr(0, input.length() - 9) + L"." + input.substr(input.length() - 9, 2) + L" G";
		}
		else if (input.length() > 6) {
			return input.substr(0, input.length() - 6) + L"." + input.substr(input.length() - 6, 2) + L" M";
		}
		else if (input.length() > 3) {
			return input.substr(0, input.length() - 3) + L"." + input.substr(input.length() - 3, 2) + L" K";
		}
		else {
			return input;
		}
	case kbits_s:
		if (input.length() > 3) {
			return input.substr(0, input.length() - 3) + L"." + input.substr(input.length() - 3, 2) + L" M";
		}
		else {
			return input + L" K";;
		}
	case celsius:
		return input + L" °C";
	case decibel:
		if (input.length() > 2) {
			return input.substr(0, input.length() - 2) + L"." + input.substr(input.length() - 2, 2) + L" dB";
		}
		else {
			return L"0." + input + L" dB";
		}
	case rpm:
		return input + L" rpm";
	case percent:
		return input + L" %";
	default:
	case none:
		return input;
	}
}

// Return last RDD data of a json
wstring FbxAPI::getLastRRDData(wstring value, json::value json_array) {

	return json_array[(json_array.size() - 1)].at(value).serialize();

}