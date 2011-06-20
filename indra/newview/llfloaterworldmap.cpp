/** 
 * @file llfloaterworldmap.cpp
 * @author James Cook, Tom Yedwab
 * @brief LLFloaterWorldMap class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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
 * Map of the entire world, with multiple background images,
 * avatar tracking, teleportation by double-click, etc.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterworldmap.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcallingcard.h"
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "lldraghandle.h"
#include "llfirstuse.h"
#include "llfloaterreg.h"		// getTypedInstance()
#include "llfocusmgr.h"
#include "lllandmarklist.h"
#include "lllineeditor.h"
#include "llpreviewlandmark.h"
#include "llregionhandle.h"
#include "llscrolllistctrl.h"
#include "llslurl.h"
#include "lltextbox.h"
#include "lltracker.h"
#include "llviewerinventory.h"	// LLViewerInventoryItem
#include "llviewermenu.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llworldmap.h"
#include "llworldmapview.h"
#include "lluictrlfactory.h"
#include "llappviewer.h"
#include "llmapimagetype.h"
#include "llweb.h"
#include "llsliderctrl.h"

#include "llglheaders.h"
#include "llwindow.h"			// copyTextToClipboard()

//---------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------
static const F32 MAP_ZOOM_TIME = 0.2f;

enum EPanDirection
{
	PAN_UP,
	PAN_DOWN,
	PAN_LEFT,
	PAN_RIGHT
};

// Values in pixels per region
static const F32 ZOOM_MAX = 128.f;

static const F32 SIM_COORD_DEFAULT = 128.f;

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------

LLFloaterWorldMap* gFloaterWorldMap = NULL;

class LLMapInventoryObserver : public LLInventoryObserver
{
public:
	LLMapInventoryObserver() {}
	virtual ~LLMapInventoryObserver() {}
	virtual void changed(U32 mask);
};
  
void LLMapInventoryObserver::changed(U32 mask)
{
	// if there's a change we're interested in.
	if((mask & (LLInventoryObserver::CALLING_CARD | LLInventoryObserver::ADD |
				LLInventoryObserver::REMOVE)) != 0)
	{
		gFloaterWorldMap->inventoryChanged();
	}
}

class LLMapFriendObserver : public LLFriendObserver
{
public:
	LLMapFriendObserver() {}
	virtual ~LLMapFriendObserver() {}
	virtual void changed(U32 mask);
};

void LLMapFriendObserver::changed(U32 mask)
{
	// if there's a change we're interested in.
	if((mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE | LLFriendObserver::POWERS)) != 0)
	{
		gFloaterWorldMap->friendsChanged();
	}
}

//---------------------------------------------------------------------------
// Statics
//---------------------------------------------------------------------------

// Used as a pretend asset and inventory id to mean "landmark at my home location."
const LLUUID LLFloaterWorldMap::sHomeID( "10000000-0000-0000-0000-000000000001" );

//---------------------------------------------------------------------------
// Construction and destruction
//---------------------------------------------------------------------------


LLFloaterWorldMap::LLFloaterWorldMap(const LLSD& key)
:	LLFloater(key),
	mInventory(NULL),
	mInventoryObserver(NULL),
	mFriendObserver(NULL),
	mCompletingRegionName(""),
	mWaitingForTracker(FALSE),
	mExactMatch(FALSE),
	mIsClosing(FALSE),
	mSetToUserPosition(TRUE),
	mTrackedLocation(0,0,0),
	mTrackedStatus(LLTracker::TRACKING_NOTHING)
{
	gFloaterWorldMap = this;
	
	mFactoryMap["objects_mapview"] = LLCallbackMap(createWorldMapView, NULL);
	mFactoryMap["terrain_mapview"] = LLCallbackMap(createWorldMapView, NULL);
	
	//Called from floater reg: LLUICtrlFactory::getInstance()->buildFloater(this, "floater_world_map.xml", FALSE);
	mCommitCallbackRegistrar.add("WMap.Location",		boost::bind(&LLFloaterWorldMap::onLocationCommit, this));
	mCommitCallbackRegistrar.add("WMap.AvatarCombo",	boost::bind(&LLFloaterWorldMap::onAvatarComboCommit, this));
	mCommitCallbackRegistrar.add("WMap.SearchResult",	boost::bind(&LLFloaterWorldMap::onCommitSearchResult, this));
	mCommitCallbackRegistrar.add("WMap.CommitLocation",	boost::bind(&LLFloaterWorldMap::onCommitLocation, this));
	mCommitCallbackRegistrar.add("WMap.GoHome",			boost::bind(&LLFloaterWorldMap::onGoHome, this));	
	mCommitCallbackRegistrar.add("WMap.Teleport",		boost::bind(&LLFloaterWorldMap::onClickTeleportBtn, this));	
	mCommitCallbackRegistrar.add("WMap.ShowTarget",		boost::bind(&LLFloaterWorldMap::onShowTargetBtn, this));	
	mCommitCallbackRegistrar.add("WMap.ShowAgent",		boost::bind(&LLFloaterWorldMap::onShowAgentBtn, this));		
	mCommitCallbackRegistrar.add("WMap.Clear",			boost::bind(&LLFloaterWorldMap::onClearBtn, this));		
	mCommitCallbackRegistrar.add("WMap.CopySLURL",		boost::bind(&LLFloaterWorldMap::onCopySLURL, this));
}

// static
void* LLFloaterWorldMap::createWorldMapView(void* data)
{
	return new LLWorldMapView();
}

BOOL LLFloaterWorldMap::postBuild()
{
	mTabs = getChild<LLTabContainer>("maptab");
	if (!mTabs) return FALSE;

	mTabs->setCommitCallback(boost::bind(&LLFloaterWorldMap::onCommitBackground, this));

	// The following callback syncs the worlmap tabs with the images.
	// Commented out since it was crashing when LLWorldMap became a singleton.
	// We should be fine without it but override the onOpen method and put it 
	// there if it turns out to be needed. -MG
	//
	//onCommitBackground();

	LLComboBox *avatar_combo = getChild<LLComboBox>("friend combo");
	if (avatar_combo)
	{
		avatar_combo->selectFirstItem();
		avatar_combo->setPrearrangeCallback( boost::bind(&LLFloaterWorldMap::onAvatarComboPrearrange, this) );
		avatar_combo->setTextEntryCallback( boost::bind(&LLFloaterWorldMap::onComboTextEntry, this) );
	}

	getChild<LLScrollListCtrl>("location")->setFocusChangedCallback(boost::bind(&LLFloaterWorldMap::onLocationFocusChanged, this, _1));

	LLLineEditor *location_editor = getChild<LLLineEditor>("location");
	if (location_editor)
	{
		location_editor->setKeystrokeCallback( boost::bind(&LLFloaterWorldMap::onSearchTextEntry, this, _1), NULL );
	}
	
	getChild<LLScrollListCtrl>("search_results")->setDoubleClickCallback( boost::bind(&LLFloaterWorldMap::onClickTeleportBtn, this));

	LLComboBox *landmark_combo = getChild<LLComboBox>( "landmark combo");
	if (landmark_combo)
	{
		landmark_combo->selectFirstItem();
		landmark_combo->setPrearrangeCallback( boost::bind(&LLFloaterWorldMap::onLandmarkComboPrearrange, this) );
		landmark_combo->setTextEntryCallback( boost::bind(&LLFloaterWorldMap::onComboTextEntry, this) );
	}

	mCurZoomVal = log(gMapScale)/log(2.f);
	childSetValue("zoom slider", gMapScale);

	setDefaultBtn(NULL);

	mZoomTimer.stop();

	return TRUE;
}

// virtual
LLFloaterWorldMap::~LLFloaterWorldMap()
{
	// All cleaned up by LLView destructor
	mTabs = NULL;

	// Inventory deletes all observers on shutdown
	mInventory = NULL;
	mInventoryObserver = NULL;

	// avatar tracker will delete this for us.
	mFriendObserver = NULL;
	
	gFloaterWorldMap = NULL;
}

//static
LLFloaterWorldMap* LLFloaterWorldMap::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterWorldMap>("world_map");
}

// virtual
void LLFloaterWorldMap::onClose(bool app_quitting)
{
	// While we're not visible, discard the overlay images we're using
	LLWorldMap::getInstance()->clearImageRefs();

	setVisible(FALSE);
}

// virtual
void LLFloaterWorldMap::onOpen(const LLSD& key)
{
	bool center_on_target = (key.asString() == "center");

	mIsClosing = FALSE;

	LLWorldMapView* map_panel;
	map_panel = (LLWorldMapView*)mTabs->getCurrentPanel();
	map_panel->clearLastClick();

	{
		// reset pan on show, so it centers on you again
		if (!center_on_target)
		{
			LLWorldMapView::setPan(0, 0, TRUE);
		}
		map_panel->updateVisibleBlocks();

		// Reload the agent positions when we show the window
		LLWorldMap::getInstance()->eraseItems();

		// Reload any maps that may have changed
		LLWorldMap::getInstance()->clearSimFlags();

		const S32 panel_num = mTabs->getCurrentPanelIndex();
		const bool request_from_sim = true;
		LLWorldMap::getInstance()->setCurrentLayer(panel_num, request_from_sim);

		// We may already have a bounding box for the regions of the world,
		// so use that to adjust the view.
		adjustZoomSliderBounds();

		// Could be first show
		LLFirstUse::useMap();

		// Start speculative download of landmarks
		LLUUID landmark_folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
		gInventory.startBackgroundFetch(landmark_folder_id);

		childSetFocus("location", TRUE);
		gFocusMgr.triggerFocusFlash();

		buildAvatarIDList();
		buildLandmarkIDLists();

		// If nothing is being tracked, set flag so the user position will be found
		mSetToUserPosition = ( LLTracker::getTrackingStatus() == LLTracker::TRACKING_NOTHING );
	}
	
	if (center_on_target)
	{
		centerOnTarget(FALSE);
	}
}



// static
void LLFloaterWorldMap::reloadIcons(void*)
{
	LLWorldMap::getInstance()->eraseItems();

	LLWorldMap::getInstance()->sendMapLayerRequest();
}


// virtual
BOOL LLFloaterWorldMap::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled;
	handled = LLFloater::handleHover(x, y, mask);
	return handled;
}

BOOL LLFloaterWorldMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (!isMinimized() && isFrontmost())
	{
		LLRect area;
		childGetRect("search_results", area);
		if(!area.pointInRect(x, y))
		{
			F32 slider_value = (F32)childGetValue("zoom slider").asReal();
			slider_value += ((F32)clicks * -0.3333f);
			childSetValue("zoom slider", LLSD(slider_value));
			return TRUE;
		}
	}

	return LLFloater::handleScrollWheel(x, y, clicks);
}


// virtual
void LLFloaterWorldMap::reshape( S32 width, S32 height, BOOL called_from_parent )
{
	LLFloater::reshape( width, height, called_from_parent );

	// Might have changed size of world display area
	// JC: Technically, this is correct, but it makes the slider "pop"
	// if you resize the window, then draw the slider.  Just leaving it
	// the way it was when you opened the window seems better.
	// adjustZoomSliderBounds();
}


// virtual
void LLFloaterWorldMap::draw()
{
	static LLUIColor map_track_color = LLUIColorTable::instance().getColor("MapTrackColor", LLColor4::white);
	static LLUIColor map_track_disabled_color = LLUIColorTable::instance().getColor("MapTrackDisabledColor", LLColor4::white);
	
	// Hide/Show Mature Events controls
	childSetVisible("events_mature_icon", gAgent.canAccessMature());
	childSetVisible("events_mature_label", gAgent.canAccessMature());
	childSetVisible("event_mature_chk", gAgent.canAccessMature());

	childSetVisible("events_adult_icon", gAgent.canAccessMature());
	childSetVisible("events_adult_label", gAgent.canAccessMature());
	childSetVisible("event_adult_chk", gAgent.canAccessMature());
	bool adult_enabled = gAgent.canAccessAdult();
	if (!adult_enabled)
	{
		childSetValue("event_adult_chk", FALSE);
	}
	childSetEnabled("event_adult_chk", adult_enabled);

	// On orientation island, users don't have a home location yet, so don't
	// let them teleport "home".  It dumps them in an often-crowed welcome
	// area (infohub) and they get confused. JC
	LLViewerRegion* regionp = gAgent.getRegion();
	bool agent_on_prelude = (regionp && regionp->isPrelude());
	bool enable_go_home = gAgent.isGodlike() || !agent_on_prelude;
	childSetEnabled("Go Home", enable_go_home);

	updateLocation();
	
	LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus(); 
	if (LLTracker::TRACKING_AVATAR == tracking_status)
	{
		childSetColor("avatar_icon", map_track_color);
	}
	else
	{
		childSetColor("avatar_icon", map_track_disabled_color);
	}

	if (LLTracker::TRACKING_LANDMARK == tracking_status)
	{
		childSetColor("landmark_icon", map_track_color);
	}
	else
	{
		childSetColor("landmark_icon", map_track_disabled_color);
	}

	if (LLTracker::TRACKING_LOCATION == tracking_status)
	{
		childSetColor("location_icon", map_track_color);
	}
	else
	{
		if (mCompletingRegionName != "")
		{
			F64 seconds = LLTimer::getElapsedSeconds();
			double value = fmod(seconds, 2);
			value = 0.5 + 0.5*cos(value * 3.14159f);
			LLColor4 loading_color(0.0, F32(value/2), F32(value), 1.0);
			childSetColor("location_icon", loading_color);
		}
		else
		{
			childSetColor("location_icon", map_track_disabled_color);
		}
	}

	// check for completion of tracking data
	if (mWaitingForTracker)
	{
		centerOnTarget(TRUE);
	}

	childSetEnabled("Teleport", (BOOL)tracking_status);
//	childSetEnabled("Clear", (BOOL)tracking_status);
	childSetEnabled("Show Destination", (BOOL)tracking_status || LLWorldMap::getInstance()->mIsTrackingUnknownLocation);
	childSetEnabled("copy_slurl", (mSLURL.size() > 0) );

	setMouseOpaque(TRUE);
	getDragHandle()->setMouseOpaque(TRUE);

	//RN: snaps to zoom value because interpolation caused jitter in the text rendering
	if (!mZoomTimer.getStarted() && mCurZoomVal != (F32)childGetValue("zoom slider").asReal())
	{
		mZoomTimer.start();
	}
	F32 interp = mZoomTimer.getElapsedTimeF32() / MAP_ZOOM_TIME;
	if (interp > 1.f)
	{
		interp = 1.f;
		mZoomTimer.stop();
	}
	mCurZoomVal = lerp(mCurZoomVal, (F32)childGetValue("zoom slider").asReal(), interp);
	F32 map_scale = 256.f*pow(2.f, mCurZoomVal);
	LLWorldMapView::setScale( map_scale );
	
	LLFloater::draw();
}


//-------------------------------------------------------------------------
// Internal utility functions
//-------------------------------------------------------------------------


void LLFloaterWorldMap::trackAvatar( const LLUUID& avatar_id, const std::string& name )
{
	LLCtrlSelectionInterface *iface = childGetSelectionInterface("friend combo");
	if (!iface) return;

	buildAvatarIDList();
	if(iface->setCurrentByID(avatar_id) || gAgent.isGodlike())
	{
		// *HACK: Adjust Z values automatically for liaisons & gods so
		// they swoop down when they click on the map. Requested
		// convenience.
		if(gAgent.isGodlike())
		{
			childSetValue("spin z", LLSD(200.f));
		}
		// Don't re-request info if we already have it or we won't have it in time to teleport
		if (mTrackedStatus != LLTracker::TRACKING_AVATAR || name != mTrackedAvatarName)
		{
			mTrackedStatus = LLTracker::TRACKING_AVATAR;
			mTrackedAvatarName = name;
			LLTracker::trackAvatar(avatar_id, name);
		}
	}
	else
	{
		LLTracker::stopTracking(NULL);
	}
	setDefaultBtn("Teleport");
}

void LLFloaterWorldMap::trackLandmark( const LLUUID& landmark_item_id )
{
	LLCtrlSelectionInterface *iface = childGetSelectionInterface("landmark combo");
	if (!iface) return;

	buildLandmarkIDLists();
	BOOL found = FALSE;
	S32 idx;
	for (idx = 0; idx < mLandmarkItemIDList.count(); idx++)
	{
		if ( mLandmarkItemIDList.get(idx) == landmark_item_id)
		{
			found = TRUE;
			break;
		}
	}

	if (found && iface->setCurrentByID( landmark_item_id ) ) 
	{
		LLUUID asset_id = mLandmarkAssetIDList.get( idx );
		std::string name;
		LLComboBox* combo = getChild<LLComboBox>( "landmark combo");
		if (combo) name = combo->getSimple();
		mTrackedStatus = LLTracker::TRACKING_LANDMARK;
		LLTracker::trackLandmark(mLandmarkAssetIDList.get( idx ),	// assetID
								mLandmarkItemIDList.get( idx ), // itemID
								name);			// name

		if( asset_id != sHomeID )
		{
			// start the download process
			gLandmarkList.getAsset( asset_id);
		}

		// We have to download both region info and landmark data, so set busy. JC
//		getWindow()->incBusyCount();
	}
	else
	{
		LLTracker::stopTracking(NULL);
	}
	setDefaultBtn("Teleport");
}


void LLFloaterWorldMap::trackEvent(const LLItemInfo &event_info)
{
	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	LLTracker::trackLocation(event_info.mPosGlobal, event_info.mName, event_info.mToolTip, LLTracker::LOCATION_EVENT);
	setDefaultBtn("Teleport");
}

void LLFloaterWorldMap::trackGenericItem(const LLItemInfo &item)
{
	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	LLTracker::trackLocation(item.mPosGlobal, item.mName, item.mToolTip, LLTracker::LOCATION_ITEM);
	setDefaultBtn("Teleport");
}

void LLFloaterWorldMap::trackLocation(const LLVector3d& pos_global)
{
	LLSimInfo* sim_info = LLWorldMap::getInstance()->simInfoFromPosGlobal(pos_global);
	if (!sim_info)
	{
		LLWorldMap::getInstance()->mIsTrackingUnknownLocation = TRUE;
		LLWorldMap::getInstance()->mInvalidLocation = FALSE;
		LLWorldMap::getInstance()->mUnknownLocation = pos_global;
		LLTracker::stopTracking(NULL);
		S32 world_x = S32(pos_global.mdV[0] / 256);
		S32 world_y = S32(pos_global.mdV[1] / 256);
		LLWorldMap::getInstance()->sendMapBlockRequest(world_x, world_y, world_x, world_y, true);
		setDefaultBtn("");
		return;
	}
	if (sim_info->mAccess == SIM_ACCESS_DOWN)
	{
		// Down sim. Show the blue circle of death!
		LLWorldMap::getInstance()->mIsTrackingUnknownLocation = TRUE;
		LLWorldMap::getInstance()->mUnknownLocation = pos_global;
		LLWorldMap::getInstance()->mInvalidLocation = TRUE;
		LLTracker::stopTracking(NULL);
		setDefaultBtn("");
		return;
	}

	std::string sim_name;
	LLWorldMap::getInstance()->simNameFromPosGlobal( pos_global, sim_name );
	F32 region_x = (F32)fmod( pos_global.mdV[VX], (F64)REGION_WIDTH_METERS );
	F32 region_y = (F32)fmod( pos_global.mdV[VY], (F64)REGION_WIDTH_METERS );
	std::string full_name = llformat("%s (%d, %d, %d)", 
								  sim_name.c_str(), 
								  llround(region_x), 
								  llround(region_y),
								  llround((F32)pos_global.mdV[VZ]));

	std::string tooltip("");
	mTrackedStatus = LLTracker::TRACKING_LOCATION;
	LLTracker::trackLocation(pos_global, full_name, tooltip);
	LLWorldMap::getInstance()->mIsTrackingUnknownLocation = FALSE;
	LLWorldMap::getInstance()->mIsTrackingDoubleClick = FALSE;
	LLWorldMap::getInstance()->mIsTrackingCommit = FALSE;

	setDefaultBtn("Teleport");
}

void LLFloaterWorldMap::updateLocation()
{
	bool gotSimName;

	LLTracker::ETrackingStatus status = LLTracker::getTrackingStatus();

	// These values may get updated by a message, so need to check them every frame
	// The fields may be changed by the user, so only update them if the data changes
	LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();
	if (pos_global.isExactlyZero())
	{
		LLVector3d agentPos = gAgent.getPositionGlobal();

		// Set to avatar's current postion if nothing is selected
		if ( status == LLTracker::TRACKING_NOTHING && mSetToUserPosition )
		{
			// Make sure we know where we are before setting the current user position
			std::string agent_sim_name;
			gotSimName = LLWorldMap::getInstance()->simNameFromPosGlobal( agentPos, agent_sim_name );
			if ( gotSimName )
			{
				mSetToUserPosition = FALSE;

				// Fill out the location field
				childSetValue("location", agent_sim_name);

				// Figure out where user is
				LLVector3d agentPos = gAgent.getPositionGlobal();

				S32 agent_x = llround( (F32)fmod( agentPos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
				S32 agent_y = llround( (F32)fmod( agentPos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
				S32 agent_z = llround( (F32)agentPos.mdV[VZ] );

				childSetValue("spin x", LLSD(agent_x) );
				childSetValue("spin y", LLSD(agent_y) );
				childSetValue("spin z", LLSD(agent_z) );

				// Set the current SLURL
				mSLURL = LLSLURL::buildSLURL(agent_sim_name, agent_x, agent_y, agent_z);
			}
		}

		return; // invalid location
	}
	std::string sim_name;
	gotSimName = LLWorldMap::getInstance()->simNameFromPosGlobal( pos_global, sim_name );
	if ((status != LLTracker::TRACKING_NOTHING) &&
		(status != mTrackedStatus || pos_global != mTrackedLocation || sim_name != mTrackedSimName))
	{
		mTrackedStatus = status;
		mTrackedLocation = pos_global;
		mTrackedSimName = sim_name;
		
		if (status == LLTracker::TRACKING_AVATAR)
		{
			// *HACK: Adjust Z values automatically for liaisons &
			// gods so they swoop down when they click on the
			// map. Requested convenience.
			if(gAgent.isGodlike())
			{
				pos_global[2] = 200;
			}
		}

		childSetValue("location", sim_name);
		
		F32 region_x = (F32)fmod( pos_global.mdV[VX], (F64)REGION_WIDTH_METERS );
		F32 region_y = (F32)fmod( pos_global.mdV[VY], (F64)REGION_WIDTH_METERS );
		childSetValue("spin x", LLSD(region_x) );
		childSetValue("spin y", LLSD(region_y) );
		childSetValue("spin z", LLSD((F32)pos_global.mdV[VZ]) );

		// simNameFromPosGlobal can fail, so don't give the user an invalid SLURL
		if ( gotSimName )
		{
			mSLURL = LLSLURL::buildSLURL(sim_name, llround(region_x), llround(region_y), llround((F32)pos_global.mdV[VZ]));
		}
		else
		{	// Empty SLURL will disable the "Copy SLURL to clipboard" button
			mSLURL = "";
		}
	}
}

void LLFloaterWorldMap::trackURL(const std::string& region_name, S32 x_coord, S32 y_coord, S32 z_coord)
{
	LLSimInfo* sim_info = LLWorldMap::getInstance()->simInfoFromName(region_name);
	z_coord = llclamp(z_coord, 0, 4096);
	if (sim_info)
	{
		LLVector3 local_pos;
		local_pos.mV[VX] = (F32)x_coord;
		local_pos.mV[VY] = (F32)y_coord;
		local_pos.mV[VZ] = (F32)z_coord;
		LLVector3d global_pos = sim_info->getGlobalPos(local_pos);
		trackLocation(global_pos);
		setDefaultBtn("Teleport");
	}
	else
	{
		// fill in UI based on URL
		gFloaterWorldMap->childSetValue("location", region_name);
		childSetValue("spin x", LLSD((F32)x_coord));
		childSetValue("spin y", LLSD((F32)y_coord));
		childSetValue("spin z", LLSD((F32)z_coord));

		// pass sim name to combo box
		gFloaterWorldMap->mCompletingRegionName = region_name;
		LLWorldMap::getInstance()->sendNamedRegionRequest(region_name);
		LLStringUtil::toLower(gFloaterWorldMap->mCompletingRegionName);
		LLWorldMap::getInstance()->mIsTrackingCommit = TRUE;
	}
}

void LLFloaterWorldMap::observeInventory(LLInventoryModel* model)
{
	if(mInventory)
	{
		mInventory->removeObserver(mInventoryObserver);
		delete mInventoryObserver;
		mInventory = NULL;
		mInventoryObserver = NULL;
	}
	if(model)
	{
		mInventory = model;
		mInventoryObserver = new LLMapInventoryObserver;
		// Inventory deletes all observers on shutdown
		mInventory->addObserver(mInventoryObserver);
		inventoryChanged();
	}
}

void LLFloaterWorldMap::inventoryChanged()
{
	if(!LLTracker::getTrackedLandmarkItemID().isNull())
	{
		LLUUID item_id = LLTracker::getTrackedLandmarkItemID();
		buildLandmarkIDLists();
		trackLandmark(item_id);
	}
}

void LLFloaterWorldMap::observeFriends()
{
	if(!mFriendObserver)
	{
		mFriendObserver = new LLMapFriendObserver;
		LLAvatarTracker::instance().addObserver(mFriendObserver);
		friendsChanged();
	}
}

void LLFloaterWorldMap::friendsChanged()
{
	LLAvatarTracker& t = LLAvatarTracker::instance();
	const LLUUID& avatar_id = t.getAvatarID();
	buildAvatarIDList();
	if(avatar_id.notNull())
	{
		LLCtrlSelectionInterface *iface = childGetSelectionInterface("friend combo");
		if(!iface || !iface->setCurrentByID(avatar_id) || 
			!t.getBuddyInfo(avatar_id)->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION) || gAgent.isGodlike())
		{
			LLTracker::stopTracking(NULL);
		}
	}
}

// No longer really builds a list.  Instead, just updates mAvatarCombo.
void LLFloaterWorldMap::buildAvatarIDList()
{
	LLCtrlListInterface *list = childGetListInterface("friend combo");
	if (!list) return;

    // Delete all but the "None" entry
	S32 list_size = list->getItemCount();
	while (list_size > 1)
	{
		list->selectNthItem(1);
		list->operateOnSelection(LLCtrlListInterface::OP_DELETE);
		--list_size;
	}

	LLSD default_column;
	default_column["name"] = "friend name";
	default_column["label"] = "Friend Name";
	default_column["width"] = 500;
	list->addColumn(default_column);

	// Get all of the calling cards for avatar that are currently online
	LLCollectMappableBuddies collector;
	LLAvatarTracker::instance().applyFunctor(collector);
	LLCollectMappableBuddies::buddy_map_t::iterator it;
	LLCollectMappableBuddies::buddy_map_t::iterator end;
	it = collector.mMappable.begin();
	end = collector.mMappable.end();
	for( ; it != end; ++it)
	{
		list->addSimpleElement((*it).first, ADD_BOTTOM, (*it).second);
	}

	list->setCurrentByID( LLAvatarTracker::instance().getAvatarID() );
	list->selectFirstItem();
}


void LLFloaterWorldMap::buildLandmarkIDLists()
{
	LLCtrlListInterface *list = childGetListInterface("landmark combo");
	if (!list)
	{
		return;
	}

    // Delete all but the "None" entry
	S32 list_size = list->getItemCount();
	if (list_size > 1)
	{
		list->selectItemRange(1, -1);
		list->operateOnSelection(LLCtrlListInterface::OP_DELETE);
	}

	mLandmarkItemIDList.reset();
	mLandmarkAssetIDList.reset();

	// Get all of the current landmarks
	mLandmarkAssetIDList.put( LLUUID::null );
	mLandmarkItemIDList.put( LLUUID::null );

	mLandmarkAssetIDList.put( sHomeID );
	mLandmarkItemIDList.put( sHomeID );

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLIsType is_landmark(LLAssetType::AT_LANDMARK);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_landmark);

	std::sort(items.begin(), items.end(), LLViewerInventoryItem::comparePointers());
	
	S32 count = items.count();
	for(S32 i = 0; i < count; ++i)
	{
		LLInventoryItem* item = items.get(i);

		list->addSimpleElement(item->getName(), ADD_BOTTOM, item->getUUID());

		mLandmarkAssetIDList.put( item->getAssetUUID() );
		mLandmarkItemIDList.put( item->getUUID() );
	}
	list->sortByColumn(std::string("landmark name"), TRUE);

	list->selectFirstItem();
}


F32 LLFloaterWorldMap::getDistanceToDestination(const LLVector3d &destination, 
												F32 z_attenuation) const
{
	LLVector3d delta = destination - gAgent.getPositionGlobal();
	// by attenuating the z-component we effectively 
	// give more weight to the x-y plane
	delta.mdV[VZ] *= z_attenuation;
	F32 distance = (F32)delta.magVec();
	return distance;
}


void LLFloaterWorldMap::clearLocationSelection(BOOL clear_ui)
{
	LLCtrlListInterface *list = childGetListInterface("search_results");
	if (list)
	{
		list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}
	if (!childHasKeyboardFocus("spin x"))
	{
		childSetValue("spin x", SIM_COORD_DEFAULT);
	}
	if (!childHasKeyboardFocus("spin y"))
	{
		childSetValue("spin y", SIM_COORD_DEFAULT);
	}
	if (!childHasKeyboardFocus("spin z"))
	{
		childSetValue("spin z", 0);
	}
	LLWorldMap::getInstance()->mIsTrackingCommit = FALSE;
	mCompletingRegionName = "";
	mExactMatch = FALSE;
}


void LLFloaterWorldMap::clearLandmarkSelection(BOOL clear_ui)
{
	if (clear_ui || !childHasKeyboardFocus("landmark combo"))
	{
		LLCtrlListInterface *list = childGetListInterface("landmark combo");
		if (list)
		{
			list->selectByValue( "None" );
		}
	}
}


void LLFloaterWorldMap::clearAvatarSelection(BOOL clear_ui)
{
	if (clear_ui || !childHasKeyboardFocus("friend combo"))
	{
		mTrackedStatus = LLTracker::TRACKING_NOTHING;
		LLCtrlListInterface *list = childGetListInterface("friend combo");
		if (list)
		{
			list->selectByValue( "None" );
		}
	}
}


// Adjust the maximally zoomed out limit of the zoom slider so you
// can see the whole world, plus a little.
void LLFloaterWorldMap::adjustZoomSliderBounds()
{
	// World size in regions
	S32 world_width_regions	 = LLWorldMap::getInstance()->getWorldWidth() / REGION_WIDTH_UNITS;
	S32 world_height_regions = LLWorldMap::getInstance()->getWorldHeight() / REGION_WIDTH_UNITS;

	// Pad the world size a little bit, so we have a nice border on
	// the edge
	world_width_regions++;
	world_height_regions++;

	// Find how much space we have to display the world
	LLWorldMapView* map_panel;
	map_panel = (LLWorldMapView*)mTabs->getCurrentPanel();
	LLRect view_rect = map_panel->getRect();

	// View size in pixels
	S32 view_width = view_rect.getWidth();
	S32 view_height = view_rect.getHeight();

	// Pixels per region to display entire width/height
	F32 width_pixels_per_region = (F32) view_width / (F32) world_width_regions;
	F32 height_pixels_per_region = (F32) view_height / (F32) world_height_regions;

	F32 pixels_per_region = llmin(width_pixels_per_region,
								  height_pixels_per_region);

	// Round pixels per region to an even number of slider increments
	S32 slider_units = llfloor(pixels_per_region / 0.2f);
	pixels_per_region = slider_units * 0.2f;

	// Make sure the zoom slider can be moved at least a little bit.
	// Likewise, less than the increment pixels per region is just silly.
	pixels_per_region = llclamp(pixels_per_region, 1.f, ZOOM_MAX);

	F32 min_power = log(pixels_per_region/256.f)/log(2.f);
	
	getChild<LLSliderCtrl>("zoom slider")->setMinValue(min_power);
}


//-------------------------------------------------------------------------
// User interface widget callbacks
//-------------------------------------------------------------------------

void LLFloaterWorldMap::onGoHome()
{
	gAgent.teleportHome();
	closeFloater();
}


void LLFloaterWorldMap::onLandmarkComboPrearrange( )
{
	if( mIsClosing )
	{
		return;
	}

	LLCtrlListInterface *list = childGetListInterface("landmark combo");
	if (!list) return;

	LLUUID current_choice = list->getCurrentID();

	buildLandmarkIDLists();

	if( current_choice.isNull() || !list->setCurrentByID( current_choice ) )
	{
		LLTracker::stopTracking(NULL);
	}

}

void LLFloaterWorldMap::onComboTextEntry()
{
	// Reset the tracking whenever we start typing into any of the search fields,
	// so that hitting <enter> does an auto-complete versus teleporting us to the
	// previously selected landmark/friend.
	LLTracker::clearFocus();
}

void LLFloaterWorldMap::onSearchTextEntry( LLLineEditor* ctrl )
{
	onComboTextEntry();
	updateSearchEnabled();
}


void LLFloaterWorldMap::onLandmarkComboCommit()
{
	if( mIsClosing )
	{
		return;
	}

	LLCtrlListInterface *list = childGetListInterface("landmark combo");
	if (!list) return;

	LLUUID asset_id;
	LLUUID item_id = list->getCurrentID();

	LLTracker::stopTracking(NULL);

	//RN: stopTracking() clears current combobox selection, need to reassert it here
	list->setCurrentByID(item_id);

	if( item_id.isNull() )
	{
	}
	else if( item_id == sHomeID )
	{
		asset_id = sHomeID;
	}
	else
	{
		LLInventoryItem* item = gInventory.getItem( item_id );
		if( item )
		{
			asset_id = item->getAssetUUID();
		}
		else
		{
			// Something went wrong, so revert to a safe value.
			item_id.setNull();
		}
	}
	
	trackLandmark( item_id);
	onShowTargetBtn();

	// Reset to user postion if nothing is tracked
	mSetToUserPosition = ( LLTracker::getTrackingStatus() == LLTracker::TRACKING_NOTHING );
}

// static 
void LLFloaterWorldMap::onAvatarComboPrearrange( )
{
	if( mIsClosing )
	{
		return;
	}

	LLCtrlListInterface *list = childGetListInterface("friend combo");
	if (!list) return;

	LLUUID current_choice;

	if( LLAvatarTracker::instance().haveTrackingInfo() )
	{
		current_choice = LLAvatarTracker::instance().getAvatarID();
	}

	buildAvatarIDList();

	if( !list->setCurrentByID( current_choice ) || current_choice.isNull() )
	{
		LLTracker::stopTracking(NULL);
	}
}

void LLFloaterWorldMap::onAvatarComboCommit()
{
	if( mIsClosing )
	{
		return;
	}

	LLCtrlListInterface *list = childGetListInterface("friend combo");
	if (!list) return;

	const LLUUID& new_avatar_id = list->getCurrentID();
	if (new_avatar_id.notNull())
	{
		std::string name;
		LLComboBox* combo = getChild<LLComboBox>("friend combo");
		if (combo) name = combo->getSimple();
		trackAvatar(new_avatar_id, name);
		onShowTargetBtn();
	}
	else
	{	// Reset to user postion if nothing is tracked
		mSetToUserPosition = ( LLTracker::getTrackingStatus() == LLTracker::TRACKING_NOTHING );
	}
}

void LLFloaterWorldMap::onLocationFocusChanged( LLFocusableElement* focus )
{
	updateSearchEnabled();
}

void LLFloaterWorldMap::updateSearchEnabled()
{
	if (childHasKeyboardFocus("location") && 
		childGetValue("location").asString().length() > 0)
	{
		setDefaultBtn("DoSearch");
	}
	else
	{
		setDefaultBtn(NULL);
	}
}

void LLFloaterWorldMap::onLocationCommit()
{
	if( mIsClosing )
	{
		return;
	}

	clearLocationSelection(FALSE);
	mCompletingRegionName = "";
	mLastRegionName = "";

	std::string str = childGetValue("location").asString();

	// Trim any leading and trailing spaces in the search target
	std::string saved_str = str;
	LLStringUtil::trim( str );
	if ( str != saved_str )
	{	// Set the value in the UI if any spaces were removed
		childSetValue("location", str);
	}

	LLStringUtil::toLower(str);
	mCompletingRegionName = str;
	LLWorldMap::getInstance()->mIsTrackingCommit = TRUE;
	mExactMatch = FALSE;
	if (str.length() >= 3)
	{
		LLWorldMap::getInstance()->sendNamedRegionRequest(str);
	}
	else
	{
		str += "#";
		LLWorldMap::getInstance()->sendNamedRegionRequest(str);
	}
}

void LLFloaterWorldMap::onClearBtn()
{
	mTrackedStatus = LLTracker::TRACKING_NOTHING;
	LLTracker::stopTracking((void *)(intptr_t)TRUE);
	LLWorldMap::getInstance()->mIsTrackingUnknownLocation = FALSE;
	mSLURL = "";				// Clear the SLURL since it's invalid
	mSetToUserPosition = TRUE;	// Revert back to the current user position
}

void LLFloaterWorldMap::onShowTargetBtn()
{
	centerOnTarget(TRUE);
}

void LLFloaterWorldMap::onShowAgentBtn()
{
	LLWorldMapView::setPan( 0, 0, FALSE); // FALSE == animate
	// Set flag so user's location will be displayed if not tracking anything else
	mSetToUserPosition = TRUE;	
}

void LLFloaterWorldMap::onClickTeleportBtn()
{
	teleport();
}

void LLFloaterWorldMap::onCopySLURL()
{
	getWindow()->copyTextToClipboard(utf8str_to_wstring(mSLURL));
	
	LLSD args;
	args["SLURL"] = mSLURL;

	LLNotifications::instance().add("CopySLURL", args);
}

// protected
void LLFloaterWorldMap::centerOnTarget(BOOL animate)
{
	LLVector3d pos_global;
	if(LLTracker::getTrackingStatus() != LLTracker::TRACKING_NOTHING)
	{
		LLVector3d tracked_position = LLTracker::getTrackedPositionGlobal();
		//RN: tracker doesn't allow us to query completion, so we check for a tracking position of
		// absolute zero, and keep trying in the draw loop
		if (tracked_position.isExactlyZero())
		{
			mWaitingForTracker = TRUE;
			return;
		}
		else
		{
			// We've got the position finally, so we're no longer busy. JC
//			getWindow()->decBusyCount();
			pos_global = LLTracker::getTrackedPositionGlobal() - gAgent.getCameraPositionGlobal();
		}
	}
	else if(LLWorldMap::getInstance()->mIsTrackingUnknownLocation)
	{
		pos_global = LLWorldMap::getInstance()->mUnknownLocation - gAgent.getCameraPositionGlobal();;
	}
	else
	{
		// default behavior = center on agent
		pos_global.clearVec();
	}

	LLWorldMapView::setPan( -llfloor((F32)(pos_global.mdV[VX] * (F64)LLWorldMapView::sPixelsPerMeter)), 
							-llfloor((F32)(pos_global.mdV[VY] * (F64)LLWorldMapView::sPixelsPerMeter)),
							!animate);
	mWaitingForTracker = FALSE;
}

// protected
void LLFloaterWorldMap::fly()
{
	LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();

	// Start the autopilot and close the floater, 
	// so we can see where we're flying
	if (!pos_global.isExactlyZero())
	{
		gAgent.startAutoPilotGlobal( pos_global );
		closeFloater();
	}
	else
	{
		make_ui_sound("UISndInvalidOp");
	}
}


// protected
void LLFloaterWorldMap::teleport()
{
	BOOL teleport_home = FALSE;
	LLVector3d pos_global;
	LLAvatarTracker& av_tracker = LLAvatarTracker::instance();

	LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
	if (LLTracker::TRACKING_AVATAR == tracking_status
		&& av_tracker.haveTrackingInfo() )
	{
		pos_global = av_tracker.getGlobalPos();
		pos_global.mdV[VZ] = childGetValue("spin z");
	}
	else if ( LLTracker::TRACKING_LANDMARK == tracking_status)
	{
		if( LLTracker::getTrackedLandmarkAssetID() == sHomeID )
		{
			teleport_home = TRUE;
		}
		else
		{
			LLLandmark* landmark = gLandmarkList.getAsset( LLTracker::getTrackedLandmarkAssetID() );
			LLUUID region_id;
			if(landmark
			   && !landmark->getGlobalPos(pos_global)
			   && landmark->getRegionID(region_id))
			{
				LLLandmark::requestRegionHandle(
					gMessageSystem,
					gAgent.getRegionHost(),
					region_id,
					NULL);
			}
		}
	}
	else if ( LLTracker::TRACKING_LOCATION == tracking_status)
	{
		pos_global = LLTracker::getTrackedPositionGlobal();
	}
	else
	{
		make_ui_sound("UISndInvalidOp");
	}

	// Do the teleport, which will also close the floater
	if (teleport_home)
	{
		gAgent.teleportHome();
	}
	else if (!pos_global.isExactlyZero())
	{
		if(LLTracker::TRACKING_LANDMARK == tracking_status)
		{
			gAgent.teleportViaLandmark(LLTracker::getTrackedLandmarkAssetID());
		}
		else
		{
			gAgent.teleportViaLocation( pos_global );
		}
	}
}

void LLFloaterWorldMap::flyToLandmark()
{
	LLVector3d destination_pos_global;
	if( !LLTracker::getTrackedLandmarkAssetID().isNull() )
	{
		if (LLTracker::hasLandmarkPosition())
		{
			gAgent.startAutoPilotGlobal( LLTracker::getTrackedPositionGlobal() );
		}
	}
}

void LLFloaterWorldMap::teleportToLandmark()
{
	BOOL has_destination = FALSE;
	LLUUID destination_id; // Null means "home"

	if( LLTracker::getTrackedLandmarkAssetID() == sHomeID )
	{
		has_destination = TRUE;
	}
	else
	{
		LLLandmark* landmark = gLandmarkList.getAsset( LLTracker::getTrackedLandmarkAssetID() );
		LLVector3d global_pos;
		if(landmark && landmark->getGlobalPos(global_pos))
		{
			destination_id = LLTracker::getTrackedLandmarkAssetID();
			has_destination = TRUE;
		}
		else if(landmark)
		{
			// pop up an anonymous request request.
			LLUUID region_id;
			if(landmark->getRegionID(region_id))
			{
				LLLandmark::requestRegionHandle(
					gMessageSystem,
					gAgent.getRegionHost(),
					region_id,
					NULL);
			}
		}
	}

	if( has_destination )
	{
		gAgent.teleportViaLandmark( destination_id );
	}
}


void LLFloaterWorldMap::teleportToAvatar()
{
	LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	if(av_tracker.haveTrackingInfo())
	{
		LLVector3d pos_global = av_tracker.getGlobalPos();
		gAgent.teleportViaLocation( pos_global );
	}
}


void LLFloaterWorldMap::flyToAvatar()
{
	if( LLAvatarTracker::instance().haveTrackingInfo() )
	{
		gAgent.startAutoPilotGlobal( LLAvatarTracker::instance().getGlobalPos() );
	}
}

void LLFloaterWorldMap::onCommitBackground()
{
	// Find my index
	S32 index = mTabs->getCurrentPanelIndex();
	LLWorldMap::getInstance()->setCurrentLayer(index);
}

void LLFloaterWorldMap::updateSims(bool found_null_sim)
{
	if (mCompletingRegionName == "")
	{
		return;
	}

	LLScrollListCtrl *list = getChild<LLScrollListCtrl>("search_results");
	list->operateOnAll(LLCtrlListInterface::OP_DELETE);

	LLSD selected_value = list->getSelectedValue();

	S32 name_length = mCompletingRegionName.length();

	BOOL match_found = FALSE;
	S32 num_results = 0;
	std::map<U64, LLSimInfo*>::const_iterator it;
	for (it = LLWorldMap::getInstance()->mSimInfoMap.begin(); it != LLWorldMap::getInstance()->mSimInfoMap.end(); ++it)
	{
		LLSimInfo* info = (*it).second;
		std::string sim_name = info->mName;
		std::string sim_name_lower = sim_name;
		LLStringUtil::toLower(sim_name_lower);

		if (sim_name_lower.substr(0, name_length) == mCompletingRegionName)
		{
			if (LLWorldMap::getInstance()->mIsTrackingCommit)
			{
				if (sim_name_lower == mCompletingRegionName)
				{
					selected_value = sim_name;
					match_found = TRUE;
				}
			}

			LLSD value;
			value["id"] = sim_name;
			value["columns"][0]["column"] = "sim_name";
			value["columns"][0]["value"] = sim_name;
			list->addElement(value);
			num_results++;
		}
	}
	
	list->selectByValue(selected_value);

	if (found_null_sim)
	{
		mCompletingRegionName = "";
	}

	if (match_found)
	{
		mExactMatch = TRUE;
		childSetFocus("search_results");
		onCommitSearchResult();
	}
	else if (!mExactMatch && num_results > 0)
	{
		list->selectFirstItem(); // select first item by default
		childSetFocus("search_results");
		onCommitSearchResult();
	}
	else if (num_results == 0)
	{
		list->setCommentText(std::string("None found."));
		list->operateOnAll(LLCtrlListInterface::OP_DESELECT);
	}
}

void LLFloaterWorldMap::onCommitLocation()
{
	LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
	if ( LLTracker::TRACKING_LOCATION == tracking_status)
	{
		LLVector3d pos_global = LLTracker::getTrackedPositionGlobal();
		F64 local_x = childGetValue("spin x");
		F64 local_y = childGetValue("spin y");
		F64 local_z = childGetValue("spin z");
		pos_global.mdV[VX] += -fmod(pos_global.mdV[VX], 256.0) + local_x;
		pos_global.mdV[VY] += -fmod(pos_global.mdV[VY], 256.0) + local_y;
		pos_global.mdV[VZ] = local_z;
		trackLocation(pos_global);
	}
}

void LLFloaterWorldMap::onCommitSearchResult()
{
	LLCtrlListInterface *list = childGetListInterface("search_results");
	if (!list) return;

	LLSD selected_value = list->getSelectedValue();
	std::string sim_name = selected_value.asString();
	if (sim_name.empty())
	{
		return;
	}
	LLStringUtil::toLower(sim_name);

	std::map<U64, LLSimInfo*>::const_iterator it;
	for (it = LLWorldMap::getInstance()->mSimInfoMap.begin(); it != LLWorldMap::getInstance()->mSimInfoMap.end(); ++it)
	{
		LLSimInfo* info = (*it).second;
		std::string info_sim_name = info->mName;
		LLStringUtil::toLower(info_sim_name);

		if (sim_name == info_sim_name)
		{
			LLVector3d pos_global = from_region_handle( info->mHandle );
			F64 local_x = childGetValue("spin x");
			F64 local_y = childGetValue("spin y");
			F64 local_z = childGetValue("spin z");
			pos_global.mdV[VX] += local_x;
			pos_global.mdV[VY] += local_y;
			pos_global.mdV[VZ] = local_z;

			childSetValue("location", sim_name);
			trackLocation(pos_global);
			setDefaultBtn("Teleport");
			break;
		}
	}

	onShowTargetBtn();
}
