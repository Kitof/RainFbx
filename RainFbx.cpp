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
#include "FbxAPI.h"


struct FbxMeasureConf;

/* Struct [FbxAPIConf] (parent) */
struct FbxAPIConf
{
	void* skin;
	wstring section;
	FbxMeasureConf* lastConfig;		// Keep trace of last child for Finalize()
	FbxAPIConf() : skin(), section(), lastConfig() {}
};


/* Struct [FbxMeasureConf] (child) */
struct FbxMeasureConf
{
	wstring section;
	wstring db;
	wstring field;
	FbxAPIConf* fbxAPIConf;
	FbxMeasureConf() : section(), db(), field() {}
};

std::vector<FbxAPIConf*> g_FbxAPIConfs;

// Flag for write app_token value in ini file. 
bool writeAppToken = false;

// FbxAPI Singleton
FbxAPI& fbx = FbxAPI::Instance();

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	RmLog(LOG_DEBUG, L"RainFbx.dll: Initialize");

	FbxMeasureConf* measureConfig = new FbxMeasureConf;
	*data = measureConfig;
	void* skin = RmGetSkin(rm);

	LPCWSTR fbxAPIConf = RmReadString(rm, L"FbxAPIConf", L"");

	if (!*fbxAPIConf) {
		/* [FbxAPIConf] (parent) */
		RmLog(LOG_DEBUG, L"RainFbx.dll: Reading [FbxAPIConf] (parent)");
		measureConfig->fbxAPIConf = new FbxAPIConf;
		measureConfig->fbxAPIConf->section = RmGetMeasureName(rm);
		measureConfig->fbxAPIConf->skin = skin;
		measureConfig->fbxAPIConf->lastConfig = measureConfig;
		g_FbxAPIConfs.push_back(measureConfig->fbxAPIConf);
		fbx.app_token = RmReadString(rm, L"AppToken", L"");
		if(fbx.app_token == L"") writeAppToken = true;
		fbx.setHostname(RmReadString(rm, L"Hostname", L""));
	}
	else {
		/* [FbxMeasureConf] (child) */
		RmLog(LOG_DEBUG, L"RainFbx.dll: Reading [FbxMeasureConf] (child)");
		// Find parent using name AND the skin handle to be sure that it's the right one
		std::vector<FbxAPIConf*>::const_iterator iter = g_FbxAPIConfs.begin();
		for (; iter != g_FbxAPIConfs.end(); ++iter)	{
			if ( ( (*iter)->section == fbxAPIConf ) && ( (*iter)->skin == skin ) )	{
				measureConfig->section = RmGetMeasureName(rm);
				measureConfig->fbxAPIConf = (*iter);
				return;
			}
		}
		RmLog(LOG_ERROR, L"RainFbx.dll: Invalid FbxAPIConf=");
	}
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	FbxMeasureConf* measureConfig = (FbxMeasureConf*)data;
	RmLogW(LOG_DEBUG, L"RainFbx.dll: [" + measureConfig->section + L"] Reload");

	// No measure for [FbxAPIConf] (parent), only for [FbxMeasureConf] (child) */
	FbxAPIConf* fbxAPIConf = measureConfig->fbxAPIConf;
	if (!fbxAPIConf) return;

	measureConfig->db = RmReadString(rm, L"MeasureDb", L"");
	measureConfig->field = RmReadString(rm, L"MeasureField", L"");
}

PLUGIN_EXPORT double Update(void* data)
{
	FbxMeasureConf* measureConfig = (FbxMeasureConf*)data;
	RmLogW(LOG_DEBUG, L"RainFbx.dll: [" + measureConfig->section + L"] Update");

	// No measure for [FbxAPIConf] (parent), only for [FbxMeasureConf] (child) */
	FbxAPIConf* fbxAPIConf = measureConfig->fbxAPIConf;
	if (!fbxAPIConf) return 0.0;

	// Write AppToken in ini file if successfully login
	if (writeAppToken && (fbx.global_status == LOGIN)) {
		wstring cmd = L"!WriteKeyValue " + fbxAPIConf->section + L" AppToken \"" + fbx.app_token + L"\"";
		RmLogW(LOG_DEBUG, L"RainFbx.dll: Write app_token value : " + cmd);
		RmExecute(fbxAPIConf->skin, _wcsdup(cmd.c_str()));
		writeAppToken = false;
	}

	// Update and execute next call in Freebox API workflow
	fbx.updateStatus();
	fbx.executeNextCall(measureConfig->db, measureConfig->field);

	// Return numeric value for tuple db::field
	return fbx.getNumericMeasure(measureConfig->db, measureConfig->field);
}

PLUGIN_EXPORT LPCWSTR GetString(void* data)
{
	FbxMeasureConf* measureConfig = (FbxMeasureConf*)data;
	RmLogW(LOG_DEBUG, L"RainFbx.dll: [" + measureConfig->section + L"] GetString");

	// Return formatted value for tuple db::field
	return _wcsdup(fbx.getFormattedMeasure(measureConfig->db, measureConfig->field).c_str());
}

PLUGIN_EXPORT void Finalize(void* data)
{
	FbxMeasureConf* measureConfig = (FbxMeasureConf*)data;
	RmLogW(LOG_DEBUG, L"RainFbx.dll: [" + measureConfig->section + L"] Finalize");
	FbxAPIConf* fbxAPIConf = measureConfig->fbxAPIConf;

	// Delete [FbxAPIConf] (parent), only when delete last [FbxMeasureConf] (child) */
	if (fbxAPIConf && fbxAPIConf->lastConfig == measureConfig) {
		delete fbxAPIConf;

		std::vector<FbxAPIConf*>::iterator iter = std::find(g_FbxAPIConfs.begin(), g_FbxAPIConfs.end(), fbxAPIConf);
		g_FbxAPIConfs.erase(iter);
	}

	// Delete current child
	delete measureConfig;

	// Cancel all current task (TODO: Put in FbxAPI destructor, but seems to failed ?!)
	fbx.cts.cancel();
}
