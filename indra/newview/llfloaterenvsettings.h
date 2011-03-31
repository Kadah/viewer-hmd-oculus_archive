/** 
 * @file llfloaterskysettings.h
 * @brief LLFloaterEnvSettings class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

/*
 * Simple menu for adjusting the atmospheric settings of the world
 */

#ifndef LL_LLFLOATERENVSETTINGS_H
#define LL_LLFLOATERENVSETTINGS_H

#include "llfloater.h"


/// Menuing system for all of windlight's functionality
class LLFloaterEnvSettings : public LLFloater
{
	LOG_CLASS(LLFloaterEnvSettings);
public:

	LLFloaterEnvSettings(const LLSD &key);
	/*virtual*/ ~LLFloaterEnvSettings();
	/*virtual*/	BOOL	postBuild();	

	/// initialize all the callbacks for the menu
	void initCallbacks(void);

	/// one and one instance only
	LLFloaterEnvSettings* instance();
		
	/// handle if time of day is changed
	static void onChangeDayTime(LLUICtrl* ctrl, void* userData);

	/// handle if cloud coverage is changed
	static void onChangeCloudCoverage(LLUICtrl* ctrl, void* userData);

	/// handle change in water fog density
	static void onChangeWaterFogDensity(LLUICtrl* ctrl, void* expFloatControl);

	/// handle change in water fog color
	static void onChangeWaterColor(LLUICtrl* ctrl, void* colorControl);

	/// open the advanced sky settings menu
	static void onOpenAdvancedSky(void* userData1, void* userData2);

	/// open the advanced water settings menu
	static void onOpenAdvancedWater(void* userData1, void* userData2);

	/// sync time with the server
	static void onUseEstateTime(void* userData1, void* userData2);

	// opt-in for region Windlight settings
	//static void onUseRegionEnvironment(LLUICtrl* ctrl, void* userData);
	static void onUseRegionEnvironment(LLUICtrl*, void*);

	//// menu management

	/// sync up sliders with parameters
	void syncMenu();

private:
	// one instance on the inside

	static void setOptIn(bool opt_in);
};


#endif
