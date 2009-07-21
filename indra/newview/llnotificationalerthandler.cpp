/** 
 * @file llnotificationalerthandler.cpp
 * @brief Notification Handler Class for Alert Notifications
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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


#include "llviewerprecompiledheaders.h" // must be first include

#include "llnotificationhandler.h"
#include "lltoastnotifypanel.h"
#include "llbottomtray.h"
#include "llviewercontrol.h"

#include "lltoastalertpanel.h"

using namespace LLNotificationsUI;

//--------------------------------------------------------------------------
LLAlertHandler::LLAlertHandler(e_notification_type type, const LLSD& id) : mIsModal(false)
{
	mType = type;

	LLBottomTray* tray = LLBottomTray::getInstance();
	LLChannelManager::Params p;
	p.id = LLUUID("F3E07BC8-A973-476D-8C7F-F3B7293975D1");
	p.channel_right_bound = tray->getRect().getWidth() / 2;
	p.channel_width = 0;
	p.align = NA_CENTRE;

	// Getting a Channel for our notifications
	mChannel = LLChannelManager::getInstance()->createChannel(p);
	mChannel->setFollows(FOLLOWS_BOTTOM | FOLLOWS_TOP); 
}

//--------------------------------------------------------------------------
LLAlertHandler::~LLAlertHandler()
{
}

//--------------------------------------------------------------------------
void LLAlertHandler::processNotification(const LLSD& notify)
{
	LLToast* toast = NULL;
	
	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());

	if (notify["sigtype"].asString() == "add" || notify["sigtype"].asString() == "load")
	{
		LLToastAlertPanel* alert_dialog = new LLToastAlertPanel(notification, mIsModal);
		
		toast = mChannel->addToast(notification->getID(), (LLToastPanel*)alert_dialog);
		if(!toast)
			return;
		toast->setHideButtonEnabled(false);
		toast->setOnToastDestroyCallback((boost::bind(&LLAlertHandler::onToastDestroy, this, toast)));
		toast->setCanFade(false);
		toast->setModal(mIsModal);
	}
	else if (notify["sigtype"].asString() == "change")
	{
		LLToastAlertPanel* alert_dialog = new LLToastAlertPanel(notification, mIsModal);
		mChannel->modifyToastByNotificationID(notification->getID(), (LLToastPanel*)alert_dialog);
	}
	else
	{
		mChannel->killToastByNotificationID(notification->getID());
	}
}

//--------------------------------------------------------------------------

void LLAlertHandler::onToastDestroy(LLToast* toast)
{
	toast->close();
}

//--------------------------------------------------------------------------
void LLAlertHandler::onChicletClick(void)
{
}

//--------------------------------------------------------------------------
void LLAlertHandler::onChicletClose(void)
{
}

//--------------------------------------------------------------------------


