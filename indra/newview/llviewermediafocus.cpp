/** 
 * @file llviewermediafocus.cpp
 * @brief Governs focus on Media prims
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2007, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlife.com/developers/opensource/flossexception
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

#include "llviewerprecompiledheaders.h"

#include "llviewermediafocus.h"

//LLViewerMediaFocus
#include "llviewerobjectlist.h"
#include "llpanelprimmediacontrols.h"
#include "llpluginclassmedia.h"
#include "llagent.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewermedia.h"
#include "llhudview.h"
#include "lluictrlfactory.h"
#include "lldrawable.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "llweb.h"
#include "llmediaentry.h"
#include "llkeyboard.h"
#include "lltoolmgr.h"
#include "llvovolume.h"

//
// LLViewerMediaFocus
//

LLViewerMediaFocus::LLViewerMediaFocus()
:	mFocusedObjectFace(0),
	mHoverObjectFace(0)
{
}

LLViewerMediaFocus::~LLViewerMediaFocus()
{
	// The destructor for LLSingletons happens at atexit() time, which is too late to do much.
	// Clean up in cleanupClass() instead.
}

void LLViewerMediaFocus::setFocusFace(LLPointer<LLViewerObject> objectp, S32 face, viewer_media_t media_impl, LLVector3 pick_normal)
{	
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	
	LLViewerMediaImpl *old_media_impl = getFocusedMediaImpl();
	if(old_media_impl)
	{
		old_media_impl->focus(false);
	}
	
	// Always clear the current selection.  If we're setting focus on a face, we'll reselect the correct object below.
	LLSelectMgr::getInstance()->deselectAll();
	mSelection = NULL;

	if (media_impl.notNull() && objectp.notNull())
	{
		bool face_auto_zoom = false;

		mFocusedImplID = media_impl->getMediaTextureID();
		mFocusedObjectID = objectp->getID();
		mFocusedObjectFace = face;
		mFocusedObjectNormal = pick_normal;
		
		// Set the selection in the selection manager so we can draw the focus ring.
		mSelection = LLSelectMgr::getInstance()->selectObjectOnly(objectp, face);

		// Focusing on a media face clears its disable flag.
		media_impl->setDisabled(false);

		LLTextureEntry* tep = objectp->getTE(face);
		if(tep->hasMedia())
		{
			LLMediaEntry* mep = tep->getMediaData();
			face_auto_zoom = mep->getAutoZoom();
			if(!media_impl->hasMedia())
			{
				std::string url = mep->getCurrentURL().empty() ? mep->getHomeURL() : mep->getCurrentURL();
				media_impl->navigateTo(url, "", true);
			}
		}
		else
		{
			// This should never happen.
			llwarns << "Can't find media entry for focused face" << llendl;
		}

		media_impl->focus(true);
		gFocusMgr.setKeyboardFocus(this);
		
		// We must do this before  processing the media HUD zoom, or it may zoom to the wrong face. 
		update();

		if(mMediaControls.get())
		{
			if(face_auto_zoom && ! parcel->getMediaPreventCameraZoom())
			{
				// Zoom in on this face
				mMediaControls.get()->resetZoomLevel();
				mMediaControls.get()->nextZoomLevel();
			}
			else
			{
				// Reset the controls' zoom level without moving the camera.
				// This fixes the case where clicking focus between two non-autozoom faces doesn't change the zoom-out button back to a zoom-in button.
				mMediaControls.get()->resetZoomLevel(false);
			}
		}
	}
	else
	{
		if(hasFocus())
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}
		
		mFocusedImplID = LLUUID::null;
		if (objectp.notNull())
		{
			// Still record the focused object...it may mean we need to load media data.
			// This will aid us in determining this object is "important enough"
			mFocusedObjectID = objectp->getID();
			mFocusedObjectFace = face;
		}
		else {
			mFocusedObjectID = LLUUID::null;
			mFocusedObjectFace = 0;
		}
	}
	
}

void LLViewerMediaFocus::clearFocus()
{
	setFocusFace(NULL, 0, NULL);
}

void LLViewerMediaFocus::setHoverFace(LLPointer<LLViewerObject> objectp, S32 face, viewer_media_t media_impl, LLVector3 pick_normal)
{
	if (media_impl.notNull())
	{
		mHoverImplID = media_impl->getMediaTextureID();
		mHoverObjectID = objectp->getID();
		mHoverObjectFace = face;
		mHoverObjectNormal = pick_normal;
	}
	else
	{
		mHoverObjectID = LLUUID::null;
		mHoverObjectFace = 0;
		mHoverImplID = LLUUID::null;
	}
}

void LLViewerMediaFocus::clearHover()
{
	setHoverFace(NULL, 0, NULL);
}


bool LLViewerMediaFocus::getFocus()
{
	if (gFocusMgr.getKeyboardFocus() == this)
	{
		return true;
	}
	return false;
}

// This function selects an ideal viewing distance based on the focused object, pick normal, and padding value
void LLViewerMediaFocus::setCameraZoom(LLViewerObject* object, LLVector3 normal, F32 padding_factor)
{
	if (object)
	{
		gAgent.setFocusOnAvatar(FALSE, ANIMATE);

		LLBBox bbox = object->getBoundingBoxAgent();
		LLVector3d center = gAgent.getPosGlobalFromAgent(bbox.getCenterAgent());
		F32 height;
		F32 width;
		F32 depth;
		F32 angle_of_view;
		F32 distance;

		// We need the aspect ratio, and the 3 components of the bbox as height, width, and depth.
		F32 aspect_ratio = getBBoxAspectRatio(bbox, normal, &height, &width, &depth);
		F32 camera_aspect = LLViewerCamera::getInstance()->getAspect();

		// We will normally use the side of the volume aligned with the short side of the screen (i.e. the height for 
		// a screen in a landscape aspect ratio), however there is an edge case where the aspect ratio of the object is 
		// more extreme than the screen.  In this case we invert the logic, using the longer component of both the object
		// and the screen.  
		bool invert = (camera_aspect > 1.0f && aspect_ratio > camera_aspect) ||
			(camera_aspect < 1.0f && aspect_ratio < camera_aspect);

		// To calculate the optimum viewing distance we will need the angle of the shorter side of the view rectangle.
		// In portrait mode this is the width, and in landscape it is the height.
		// We then calculate the distance based on the corresponding side of the object bbox (width for portrait, height for landscape)
		// We will add half the depth of the bounding box, as the distance projection uses the center point of the bbox.
		if(camera_aspect < 1.0f || invert)
		{
			angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect());
			distance = width * 0.5 * padding_factor / tan(angle_of_view * 0.5f );
		}
		else
		{
			angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView());
			distance = height * 0.5 * padding_factor / tan(angle_of_view * 0.5f );
		}

		distance += depth * 0.5;

		// Finally animate the camera to this new position and focal point
		LLVector3d camera_pos, target_pos;
		// The target lookat position is the center of the selection (in global coords)
		target_pos = center;
		// Target look-from (camera) position is "distance" away from the target along the normal 
		LLVector3d pickNormal = LLVector3d(normal);
		pickNormal.normalize();
        camera_pos = target_pos + pickNormal * distance;
        if (pickNormal == LLVector3d::z_axis || pickNormal == LLVector3d::z_axis_neg)
        {
			// If the normal points directly up, the camera will "flip" around.
			// We try to avoid this by adjusting the target camera position a 
			// smidge towards current camera position
			// *NOTE: this solution is not perfect.  All it attempts to solve is the
			// "looking down" problem where the camera flips around when it animates
			// to that position.  You still are not guaranteed to be looking at the
			// media in the correct orientation.  What this solution does is it will
			// put the camera into position keeping as best it can the current 
			// orientation with respect to the face.  In other words, if before zoom
			// the media appears "upside down" from the camera, after zooming it will
			// still be upside down, but at least it will not flip.
            LLVector3d cur_camera_pos = LLVector3d(gAgent.getCameraPositionGlobal());
            LLVector3d delta = (cur_camera_pos - camera_pos);
            F64 len = delta.length();
            delta.normalize();
            // Move 1% of the distance towards original camera location
            camera_pos += 0.01 * len * delta;
        }

		gAgent.setCameraPosAndFocusGlobal(camera_pos, target_pos, object->getID() );
	}
	else
	{
		// If we have no object, focus back on the avatar.
		gAgent.setFocusOnAvatar(TRUE, ANIMATE);
	}
}
void LLViewerMediaFocus::onFocusReceived()
{
	LLViewerMediaImpl* media_impl = getFocusedMediaImpl();
	if(media_impl)
		media_impl->focus(true);

	LLFocusableElement::onFocusReceived();
}

void LLViewerMediaFocus::onFocusLost()
{
	LLViewerMediaImpl* media_impl = getFocusedMediaImpl();
	if(media_impl)
		media_impl->focus(false);

	gViewerWindow->focusClient();
	LLFocusableElement::onFocusLost();
}

BOOL LLViewerMediaFocus::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	LLViewerMediaImpl* media_impl = getFocusedMediaImpl();
	if(media_impl)
	{
		media_impl->handleKeyHere(key, mask);

		if (key == KEY_ESCAPE)
		{
			// Reset camera zoom in this case.
			if(mFocusedImplID.notNull())
			{
				if(mMediaControls.get())
				{
					mMediaControls.get()->resetZoomLevel(true);
				}
			}
			
			clearFocus();
		}
	}
	
	return true;
}

BOOL LLViewerMediaFocus::handleUnicodeChar(llwchar uni_char, BOOL called_from_parent)
{
	LLViewerMediaImpl* media_impl = getFocusedMediaImpl();
	if(media_impl)
		media_impl->handleUnicodeCharHere(uni_char);
	return true;
}
BOOL LLViewerMediaFocus::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	BOOL retval = FALSE;
	LLViewerMediaImpl* media_impl = getFocusedMediaImpl();
	if(media_impl && media_impl->hasMedia())
	{
        // the scrollEvent() API's x and y are not the same as handleScrollWheel's x and y.
        // The latter is the position of the mouse at the time of the event
        // The former is the 'scroll amount' in x and y, respectively.
        // All we have for 'scroll amount' here is 'clicks'.
		// We're also not passed the keyboard modifier mask, but we can get that from gKeyboard.
		media_impl->getMediaPlugin()->scrollEvent(0, clicks, gKeyboard->currentMask(TRUE));
		retval = TRUE;
	}
	return retval;
}

void LLViewerMediaFocus::update()
{
	if(mFocusedImplID.notNull())
	{
		// We have a focused impl/face.
		if(!getFocus())
		{
			// We've lost keyboard focus -- check to see whether the media controls have it
			if(mMediaControls.get() && mMediaControls.get()->hasFocus())
			{
				// the media controls have focus -- don't clear.
			}
			else
			{
				// Someone else has focus -- back off.
				clearFocus();
			}
		}
		else if(LLToolMgr::getInstance()->inBuildMode())
		{
			// Build tools are selected -- clear focus.
			clearFocus();
		}
	}
	
	
	LLViewerMediaImpl *media_impl = getFocusedMediaImpl();
	LLViewerObject *viewer_object = getFocusedObject();
	S32 face = mFocusedObjectFace;
	LLVector3 normal = mFocusedObjectNormal;
	bool focus = true;
	
	if(!media_impl || !viewer_object)
	{
		focus = false;
		media_impl = getHoverMediaImpl();
		viewer_object = getHoverObject();
		face = mHoverObjectFace;
		normal = mHoverObjectNormal;
	}
	
	if(media_impl && viewer_object)
	{
		// We have an object and impl to point at.
		
		// Make sure the media HUD object exists.
		if(! mMediaControls.get())
		{
			LLPanelPrimMediaControls* media_controls = new LLPanelPrimMediaControls();
			mMediaControls = media_controls->getHandle();
			gHUDView->addChild(media_controls);	
		}
		mMediaControls.get()->setMediaFace(viewer_object, face, media_impl, normal);
	}
	else
	{
		// The media HUD is no longer needed.
		if(mMediaControls.get())
		{
			mMediaControls.get()->setMediaFace(NULL, 0, NULL);
		}
	}
}


// This function calculates the aspect ratio and the world aligned components of a selection bounding box.
F32 LLViewerMediaFocus::getBBoxAspectRatio(const LLBBox& bbox, const LLVector3& normal, F32* height, F32* width, F32* depth)
{
	// Convert the selection normal and an up vector to local coordinate space of the bbox
	LLVector3 local_normal = bbox.agentToLocalBasis(normal);
	LLVector3 z_vec = bbox.agentToLocalBasis(LLVector3(0.0f, 0.0f, 1.0f));
	
	LLVector3 comp1(0.f,0.f,0.f);
	LLVector3 comp2(0.f,0.f,0.f);
	LLVector3 bbox_max = bbox.getExtentLocal();
	F32 dot1 = 0.f;
	F32 dot2 = 0.f;

	// The largest component of the localized normal vector is the depth component
	// meaning that the other two are the legs of the rectangle.
	local_normal.abs();
	if(local_normal.mV[VX] > local_normal.mV[VY])
	{
		if(local_normal.mV[VX] > local_normal.mV[VZ])
		{
			// Use the y and z comps
			comp1.mV[VY] = bbox_max.mV[VY];
			comp2.mV[VZ] = bbox_max.mV[VZ];
			*depth = bbox_max.mV[VX];
		}
		else
		{
			// Use the x and y comps
			comp1.mV[VY] = bbox_max.mV[VY];
			comp2.mV[VZ] = bbox_max.mV[VZ];
			*depth = bbox_max.mV[VZ];
		}
	}
	else if(local_normal.mV[VY] > local_normal.mV[VZ])
	{
		// Use the x and z comps
		comp1.mV[VX] = bbox_max.mV[VX];
		comp2.mV[VZ] = bbox_max.mV[VZ];
		*depth = bbox_max.mV[VY];
	}
	else
	{
		// Use the x and y comps
		comp1.mV[VY] = bbox_max.mV[VY];
		comp2.mV[VZ] = bbox_max.mV[VZ];
		*depth = bbox_max.mV[VX];
	}
	
	// The height is the vector closest to vertical in the bbox coordinate space (highest dot product value)
	dot1 = comp1 * z_vec;
	dot2 = comp2 * z_vec;
	if(fabs(dot1) > fabs(dot2))
	{
		*height = comp1.length();
		*width = comp2.length();
	}
	else
	{
		*height = comp2.length();
		*width = comp1.length();
	}

	// Return the aspect ratio.
	return *width / *height;
}

bool LLViewerMediaFocus::isFocusedOnFace(LLPointer<LLViewerObject> objectp, S32 face)
{
	return objectp->getID() == mFocusedObjectID && face == mFocusedObjectFace;
}

bool LLViewerMediaFocus::isHoveringOverFace(LLPointer<LLViewerObject> objectp, S32 face)
{
	return objectp->getID() == mHoverObjectID && face == mHoverObjectFace;
}


LLViewerMediaImpl* LLViewerMediaFocus::getFocusedMediaImpl()
{
	return LLViewerMedia::getMediaImplFromTextureID(mFocusedImplID);
}

LLViewerObject* LLViewerMediaFocus::getFocusedObject()
{
	return gObjectList.findObject(mFocusedObjectID);
}

LLViewerMediaImpl* LLViewerMediaFocus::getHoverMediaImpl()
{
	return LLViewerMedia::getMediaImplFromTextureID(mHoverImplID);
}

LLViewerObject* LLViewerMediaFocus::getHoverObject()
{
	return gObjectList.findObject(mHoverObjectID);
}

void LLViewerMediaFocus::focusZoomOnMedia(LLUUID media_id)
{
	LLViewerMediaImpl* impl = LLViewerMedia::getMediaImplFromTextureID(media_id);
	
	if(impl)
	{	
		// Get the first object from the media impl's object list.  This is completely arbitrary, but should suffice.
		LLVOVolume *obj = impl->getSomeObject();
		if(obj)
		{
			// This media is attached to at least one object.  Figure out which face it's on.
			S32 face = obj->getFaceIndexWithMediaImpl(impl, -1);
			
			// We don't have a proper pick normal here, and finding a face's real normal is... complicated.
			LLVector3 normal = obj->getApproximateFaceNormal(face);
			if(normal.isNull())
			{
				// If that didn't work, use the inverse of the camera "look at" axis, which should keep the camera pointed in the same direction.
//				llinfos << "approximate face normal invalid, using camera direction." << llendl;
				normal = LLViewerCamera::getInstance()->getAtAxis();
				normal *= (F32)-1.0f;
			}
			
			// Attempt to focus/zoom on that face.
			setFocusFace(obj, face, impl, normal);
			
			if(mMediaControls.get())
			{
				mMediaControls.get()->resetZoomLevel();
				mMediaControls.get()->nextZoomLevel();
			}
		}
	}
}

LLUUID LLViewerMediaFocus::getControlsMediaID()
{
	if(getFocusedMediaImpl())
	{
		return mFocusedImplID;
	}
	else if(getHoverMediaImpl())
	{
		return mHoverImplID;
	}
	
	return LLUUID::null;
}
