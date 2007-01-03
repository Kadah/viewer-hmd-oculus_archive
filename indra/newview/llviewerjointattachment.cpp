/** 
 * @file llviewerjointattachment.cpp
 * @brief Implementation of LLViewerJointAttachment class
 *
 * Copyright (c) 2002-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewerjointattachment.h"

#include "llagentconstants.h"

#include "llviewercontrol.h"
#include "lldrawable.h"
#include "llgl.h"
#include "llvoavatar.h"
#include "llvolume.h"
#include "pipeline.h"
#include "llinventorymodel.h"
#include "llviewerobjectlist.h"
#include "llface.h"
#include "llvoavatar.h"

#include "llglheaders.h"

extern LLPipeline gPipeline;

//-----------------------------------------------------------------------------
// LLViewerJointAttachment()
//-----------------------------------------------------------------------------
LLViewerJointAttachment::LLViewerJointAttachment()
{
	mJoint = NULL;
	mAttachedObject = NULL;
	mAttachmentDirty = FALSE;
	mGroup = 0;
	mUpdateXform = FALSE;
	mIsHUDAttachment = FALSE;
	mValid = FALSE;
	mPieSlice = -1;
}

//-----------------------------------------------------------------------------
// ~LLViewerJointAttachment()
//-----------------------------------------------------------------------------
LLViewerJointAttachment::~LLViewerJointAttachment()
{
}

//-----------------------------------------------------------------------------
// isTransparent()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::isTransparent()
{
	return FALSE;
}

//-----------------------------------------------------------------------------
// drawShape()
//-----------------------------------------------------------------------------
U32 LLViewerJointAttachment::drawShape( F32 pixelArea )
{
	if (LLVOAvatar::sShowAttachmentPoints)
	{
		LLGLDisable cull_face(GL_CULL_FACE);
		
		glColor4f(1.f, 1.f, 1.f, 1.f);
		glBegin(GL_QUADS);
		{
			glVertex3f(-0.1f, 0.1f, 0.f);
			glVertex3f(-0.1f, -0.1f, 0.f);
			glVertex3f(0.1f, -0.1f, 0.f);
			glVertex3f(0.1f, 0.1f, 0.f);
		}glEnd();
	}
	return 0;
}

//-----------------------------------------------------------------------------
// lazyAttach()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::lazyAttach()
{
	if (!mAttachedObject)
	{
		return;
	}
	LLDrawable *drawablep = mAttachedObject->mDrawable;

	if (mAttachmentDirty && drawablep)
	{
		setupDrawable(drawablep);
		mAttachmentDirty = FALSE;
	}
}

void LLViewerJointAttachment::setupDrawable(LLDrawable* drawablep)
{
	drawablep->mXform.setParent(&mXform); // LLViewerJointAttachment::lazyAttach
	drawablep->makeActive();
	LLVector3 current_pos = mAttachedObject->getRenderPosition();
	LLQuaternion current_rot = mAttachedObject->getRenderRotation();
	LLQuaternion attachment_pt_inv_rot = ~getWorldRotation();

	current_pos -= getWorldPosition();
	current_pos.rotVec(attachment_pt_inv_rot);

	current_rot = current_rot * attachment_pt_inv_rot;

	drawablep->mXform.setPosition(current_pos);
	drawablep->mXform.setRotation(current_rot);
	gPipeline.markMoved(drawablep);
	gPipeline.markTextured(drawablep); // face may need to change draw pool to/from POOL_HUD
	drawablep->setState(LLDrawable::USE_BACKLIGHT);
	
	if(mIsHUDAttachment)
	{
		for (S32 face_num = 0; face_num < drawablep->getNumFaces(); face_num++)
		{
			drawablep->getFace(face_num)->setState(LLFace::HUD_RENDER);
		}
	}

	for (LLViewerObject::child_list_t::iterator iter = mAttachedObject->mChildList.begin();
		 iter != mAttachedObject->mChildList.end(); ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			childp->mDrawable->setState(LLDrawable::USE_BACKLIGHT);
			gPipeline.markTextured(childp->mDrawable); // face may need to change draw pool to/from POOL_HUD
			if(mIsHUDAttachment)
			{
				for (S32 face_num = 0; face_num < childp->mDrawable->getNumFaces(); face_num++)
				{
					childp->mDrawable->getFace(face_num)->setState(LLFace::HUD_RENDER);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// addObject()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::addObject(LLViewerObject* object)
{
	if (mAttachedObject)
	{
		llwarns << "Attempted to attach object where an attachment already exists!" << llendl;
		return FALSE;
	}
	mAttachedObject = object;
	
	LLUUID item_id;

	// Find the inventory item ID of the attached object
	LLNameValue* item_id_nv = object->getNVPair("AttachItemID");
	if( item_id_nv )
	{
		const char* s = item_id_nv->getString();
		if( s )
		{
			item_id.set( s );
			lldebugs << "getNVPair( AttachItemID ) = " << item_id << llendl;
		}
	}

	mItemID = item_id;

	LLDrawable* drawablep = object->mDrawable;

	if (drawablep)
	{
		setupDrawable(drawablep);
	}
	else
	{
		// do lazy update once we have a drawable for this object
		mAttachmentDirty = TRUE;
	}

	if (mIsHUDAttachment)
	{
		if (object->mText.notNull())
		{
			object->mText->setOnHUDAttachment(TRUE);
		}
		for (LLViewerObject::child_list_t::iterator iter = object->mChildList.begin();
			iter != object->mChildList.end(); ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp && childp->mText.notNull())
			{
				childp->mText->setOnHUDAttachment(TRUE);
			}
		}
	}
	calcLOD();
	mUpdateXform = TRUE;

	return TRUE;
}

//-----------------------------------------------------------------------------
// removeObject()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::removeObject(LLViewerObject *object)
{
	// force object visibile
	setAttachmentVisibility(TRUE);

	if (object->mDrawable.notNull())
	{
		LLVector3 cur_position = object->getRenderPosition();
		LLQuaternion cur_rotation = object->getRenderRotation();

		object->mDrawable->mXform.setPosition(cur_position);
		object->mDrawable->mXform.setRotation(cur_rotation);
		gPipeline.markMoved(object->mDrawable, TRUE);
		gPipeline.markTextured(object->mDrawable); // face may need to change draw pool to/from POOL_HUD
		object->mDrawable->clearState(LLDrawable::USE_BACKLIGHT);

		if (mIsHUDAttachment)
		{
			for (S32 face_num = 0; face_num < object->mDrawable->getNumFaces(); face_num++)
			{
				object->mDrawable->getFace(face_num)->clearState(LLFace::HUD_RENDER);
			}
		}
	}

	for (LLViewerObject::child_list_t::iterator iter = object->mChildList.begin();
		 iter != object->mChildList.end(); ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			childp->mDrawable->clearState(LLDrawable::USE_BACKLIGHT);
			gPipeline.markTextured(childp->mDrawable); // face may need to change draw pool to/from POOL_HUD
			if (mIsHUDAttachment)
			{
				for (S32 face_num = 0; face_num < childp->mDrawable->getNumFaces(); face_num++)
				{
					childp->mDrawable->getFace(face_num)->clearState(LLFace::HUD_RENDER);
				}
			}
		}
	} 

	if (mIsHUDAttachment)
	{
		if (object->mText.notNull())
		{
			object->mText->setOnHUDAttachment(FALSE);
		}
		for (LLViewerObject::child_list_t::iterator iter = object->mChildList.begin();
			iter != object->mChildList.end(); ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp->mText.notNull())
			{
				childp->mText->setOnHUDAttachment(FALSE);
			}
		}
	}

	mAttachedObject = NULL;
	mUpdateXform = FALSE;
	mItemID.setNull();
}

//-----------------------------------------------------------------------------
// setAttachmentVisibility()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::setAttachmentVisibility(BOOL visible)
{
	if (!mAttachedObject || mAttachedObject->mDrawable.isNull())
		return;

	if (visible)
	{
		// Hack to make attachments not visible by disabling their type mask!
		// This will break if you can ever attach non-volumes! - djs 02/14/03
		mAttachedObject->mDrawable->setRenderType(LLPipeline::RENDER_TYPE_VOLUME);
		for (LLViewerObject::child_list_t::iterator iter = mAttachedObject->mChildList.begin();
			 iter != mAttachedObject->mChildList.end(); ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp && childp->mDrawable.notNull())
			{
				childp->mDrawable->setRenderType(LLPipeline::RENDER_TYPE_VOLUME);
			}
		}
	}
	else
	{
		//RN: sometimes we call this on objects that are already invisible, so check for that case
		if (!mAttachedObject->mDrawable->isRenderType(LLPipeline::RENDER_TYPE_VOLUME) && 
			!mAttachedObject->mDrawable->isRenderType(0))
		{
			llerrs << "Tried to attach non-volume to a joint, visibility hack dangerous!" << llendl;
		}
		mAttachedObject->mDrawable->setRenderType(0);
		for (LLViewerObject::child_list_t::iterator iter = mAttachedObject->mChildList.begin();
			 iter != mAttachedObject->mChildList.end(); ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp && childp->mDrawable.notNull())
			{
				if (!childp->mDrawable->isRenderType(LLPipeline::RENDER_TYPE_VOLUME) && 
					!mAttachedObject->mDrawable->isRenderType(0))
				{
					llerrs << "Tried to attach non-volume to a joint, visibility hack dangerous!" << llendl;
				}
				childp->mDrawable->setRenderType(0);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// setOriginalPosition()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::setOriginalPosition(LLVector3& position)
{
	mOriginalPos = position;
	setPosition(position);
}

//-----------------------------------------------------------------------------
// clampObjectPosition()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::clampObjectPosition()
{
	if (mAttachedObject)
	{
		// *NOTE: object can drift when hitting maximum radius
		LLVector3 attachmentPos = mAttachedObject->getPosition();
		F32 dist = attachmentPos.normVec();
		dist = llmin(dist, MAX_ATTACHMENT_DIST);
		attachmentPos *= dist;
		mAttachedObject->setPosition(attachmentPos);
	}
}

//-----------------------------------------------------------------------------
// calcLOD()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::calcLOD()
{
	F32 maxarea = mAttachedObject->getMaxScale() * mAttachedObject->getMidScale();
	for (LLViewerObject::child_list_t::iterator iter = mAttachedObject->mChildList.begin();
		 iter != mAttachedObject->mChildList.end(); ++iter)
	{
		LLViewerObject* childp = *iter;
		F32 area = childp->getMaxScale() * childp->getMidScale();
		maxarea = llmax(maxarea, area);
	}
	maxarea = llclamp(maxarea, .01f*.01f, 1.f);
	F32 avatar_area = (4.f * 4.f); // pixels for an avatar sized attachment
	F32 min_pixel_area = avatar_area / maxarea;
	setLOD(min_pixel_area);
}

//-----------------------------------------------------------------------------
// updateLOD()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::updateLOD(F32 pixel_area, BOOL activate)
{
	BOOL res = FALSE;
	if (!mValid)
	{
		setValid(TRUE, TRUE);
		res = TRUE;
	}
	return res;
}

