/** 
 * @file llcombobox.cpp
 * @brief LLComboBox base class
 *
 * Copyright (c) 2001-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

// A control that displays the name of the chosen item, which when
// clicked shows a scrolling box of options.

#include "linden_common.h"

// file includes
#include "llcombobox.h"

// common includes
#include "llstring.h"

// newview includes
#include "llbutton.h"
#include "llkeyboard.h"
#include "llscrolllistctrl.h"
#include "llwindow.h"
#include "llfloater.h"
#include "llscrollbar.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "v2math.h"

// Globals
S32 LLCOMBOBOX_HEIGHT = 0;
S32 LLCOMBOBOX_WIDTH = 0;
 
LLComboBox::LLComboBox(	const LLString& name, const LLRect &rect, const LLString& label,
	void (*commit_callback)(LLUICtrl*,void*),
	void *callback_userdata,
	S32 list_width
	)
:	LLUICtrl(name, rect, TRUE, commit_callback, callback_userdata, 
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mDrawButton(TRUE),
	mTextEntry(NULL),
	mArrowImage(NULL),
	mAllowTextEntry(FALSE),
	mMaxChars(20),
	mTextEntryTentative(TRUE),
	mPrearrangeCallback( NULL ),
	mTextEntryCallback( NULL ),
	mListWidth(list_width)
{
	// For now, all comboboxes don't take keyboard focus when clicked.
	// This might change if it is part of a modal dialog.
	// mKeyboardFocusOnClick = FALSE;

	// Revert to standard behavior.  When this control's parent is hidden, it needs to
	// hide this ctrl--which won't just happen automatically since when LLComboBox is 
	// showing its list, it's also set to TopView.  When keyboard focus is cleared all
	// controls (including this one) know that they are no longer editing.
	mKeyboardFocusOnClick = TRUE;

	LLRect r;
	r.setOriginAndSize(0, 0, rect.getWidth(), rect.getHeight());

	// Always use text box 
	// Text label button
	mButton = new LLSquareButton("comboxbox button",
								 r, label, NULL, LLString::null,
								 &LLComboBox::onButtonClick, this);
	mButton->setFont(LLFontGL::sSansSerifSmall);
	mButton->setFollows(FOLLOWS_LEFT | FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mButton->setHAlign( LLFontGL::LEFT );

	const S32 ARROW_WIDTH = 16;
	mButton->setRightHPad( ARROW_WIDTH );
	addChild(mButton);

	// Default size, will be set by arrange() call in button callback. 
	if (list_width == 0)
	{
		list_width = mRect.getWidth() + SCROLLBAR_SIZE;
	}
	r.setOriginAndSize(0, 16, list_width, 220);

	// disallow multiple selection
	mList = new LLScrollListCtrl(
		"ComboBox", r, 
		&LLComboBox::onItemSelected, this, FALSE);
	mList->setVisible(FALSE);
	mList->setBgWriteableColor( LLColor4(1,1,1,1) );
	mList->setCommitOnKeyboardMovement(FALSE);
	addChild(mList);

	LLRect border_rect(0, mRect.getHeight(), mRect.getWidth(), 0);
	mBorder = new LLViewBorder( "combo border", border_rect );
	addChild( mBorder );
	mBorder->setFollows(FOLLOWS_LEFT|FOLLOWS_RIGHT|FOLLOWS_TOP|FOLLOWS_BOTTOM);

	LLUUID arrow_image_id( LLUI::sAssetsGroup->getString("combobox_arrow.tga") );
	mArrowImage = LLUI::sImageProvider->getUIImageByID(arrow_image_id);
}


LLComboBox::~LLComboBox()
{
	// children automatically deleted, including mMenu, mButton
}

// virtual
LLXMLNodePtr LLComboBox::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	// Attributes

	node->createChild("allow_text_entry", TRUE)->setBoolValue(mAllowTextEntry);

	node->createChild("max_chars", TRUE)->setIntValue(mMaxChars);

	// Contents

	std::vector<LLScrollListItem*> data_list = mList->getAllData();
	std::vector<LLScrollListItem*>::iterator data_itor;
	for (data_itor = data_list.begin(); data_itor != data_list.end(); ++data_itor)
	{
		LLScrollListItem* item = *data_itor;
		LLScrollListCell* cell = item->getColumn(0);
		if (cell)
		{
			LLXMLNodePtr item_node = node->createChild("combo_item", FALSE);
			LLSD value = item->getValue();
			item_node->createChild("value", TRUE)->setStringValue(value.asString());
			item_node->createChild("enabled", TRUE)->setBoolValue(item->getEnabled());
			item_node->setStringValue(cell->getText());
		}
	}

	return node;
}

// static
LLView* LLComboBox::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLString name("combo_box");
	node->getAttributeString("name", name);

	LLString label("");
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	BOOL allow_text_entry = FALSE;
	node->getAttributeBOOL("allow_text_entry", allow_text_entry);

	S32 max_chars = 20;
	node->getAttributeS32("max_chars", max_chars);

	LLUICtrlCallback callback = NULL;

	LLComboBox* combo_box = new LLComboBox(name,
							rect, 
							label,
							callback,
							NULL);
	combo_box->setAllowTextEntry(allow_text_entry, max_chars);

	combo_box->initFromXML(node, parent);

	const LLString& contents = node->getValue();

	if (contents.find_first_not_of(" \n\t") != contents.npos)
	{
		llerrs << "Legacy combo box item format used! Please convert to <combo_item> tags!" << llendl;
	}
	else
	{
		LLXMLNodePtr child;
		for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
		{
			if (child->hasName("combo_item"))
			{
				LLString label = child->getTextContents();

				LLString value = label;
				child->getAttributeString("value", value);

				combo_box->add(label, LLSD(value) );
			}
		}
	}

	combo_box->selectFirstItem();

	return combo_box;
}

void LLComboBox::setEnabled(BOOL enabled)
{
	LLUICtrl::setEnabled(enabled);
	mButton->setEnabled(enabled);
}

// *HACK: these are all hacks to support the fact that the combobox
// has mouse capture so we can hide the list when we don't handle the
// mouse up event
BOOL LLComboBox::handleHover(S32 x, S32 y, MASK mask)
{
	if (mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			return mList->handleHover(local_x, local_y, mask);
		}
	}
	return LLUICtrl::handleHover(x, y, mask);
}

BOOL LLComboBox::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			return mList->handleMouseDown(local_x, local_y, mask);
		}
	}
	BOOL has_focus_now = hasFocus();
	BOOL handled = LLUICtrl::handleMouseDown(x, y, mask);
	if (handled && !has_focus_now)
	{
		onFocusReceived();
	}

	return handled;
}	

BOOL LLComboBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			return mList->handleRightMouseDown(local_x, local_y, mask);
		}
	}
	return LLUICtrl::handleRightMouseDown(x, y, mask);
}

BOOL LLComboBox::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	if (mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			return mList->handleRightMouseUp(local_x, local_y, mask);
		}
	}
	return LLUICtrl::handleRightMouseUp(x, y, mask);
}

BOOL LLComboBox::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			return mList->handleDoubleClick(local_x, local_y, mask);
		}
	}
	return LLUICtrl::handleDoubleClick(x, y, mask);
}

BOOL LLComboBox::handleMouseUp(S32 x, S32 y, MASK mask)
{
	BOOL handled = childrenHandleMouseUp(x, y, mask) != NULL;

	if (!handled && mList->getVisible())
	{
		S32 local_x, local_y;
		LLView::localPointToOtherView(x, y, &local_x, &local_y, mList);
		if (mList->pointInView(local_x, local_y))
		{
			handled = mList->handleMouseUp(local_x, local_y, mask);
		}
	}

	if( !handled && gFocusMgr.getMouseCapture() == this )
	{
		// Mouse events that we didn't handle cause the list to be hidden.
		// Eat mouse event, regardless of where on the screen it happens.
		hideList();
		handled = TRUE;
	}

	return handled;
}

void LLComboBox::clear()
{ 
	if (mTextEntry)
	{
		mTextEntry->setText("");
	}
	mButton->setLabelSelected("");
	mButton->setLabelUnselected("");
	mButton->setDisabledLabel("");
	mButton->setDisabledSelectedLabel("");
	mList->deselectAllItems();
}

void LLComboBox::onCommit()
{
	if (mAllowTextEntry && getCurrentIndex() != -1)
	{
		// we have selected an existing item, blitz the manual text entry with
		// the properly capitalized item
		mTextEntry->setValue(getSimple());
		mTextEntry->setTentative(FALSE);
	}
	LLUICtrl::onCommit();
}

// add item "name" to menu
void LLComboBox::add(const LLString& name, EAddPosition pos, BOOL enabled)
{
	mList->addSimpleItem(name, pos, enabled);
	mList->selectFirstItem();
}

// add item "name" with a unique id to menu
void LLComboBox::add(const LLString& name, const LLUUID& id, EAddPosition pos, BOOL enabled )
{
	mList->addSimpleItem(name, LLSD(id), pos, enabled);
	mList->selectFirstItem();
}

// add item "name" with attached userdata
void LLComboBox::add(const LLString& name, void* userdata, EAddPosition pos, BOOL enabled )
{
	LLScrollListItem* item = mList->addSimpleItem(name, pos, enabled);
	item->setUserdata( userdata );
	mList->selectFirstItem();
}

// add item "name" with attached generic data
void LLComboBox::add(const LLString& name, LLSD value, EAddPosition pos, BOOL enabled )
{
	mList->addSimpleItem(name, value, pos, enabled);
	mList->selectFirstItem();
}


void LLComboBox::sortByName()
{
	mList->sortByColumn(0, TRUE);
}


// Choose an item with a given name in the menu.
// Returns TRUE if the item was found.
BOOL LLComboBox::setSimple(const LLString& name)
{
	BOOL found = mList->selectSimpleItem(name, FALSE);

	if (found)
	{
		setLabel(name);
	}

	return found;
}

// virtual
void LLComboBox::setValue(const LLSD& value)
{
	BOOL found = mList->selectByValue(value);
	if (found)
	{
		LLScrollListItem* item = mList->getFirstSelected();
		if (item)
		{
			setLabel( mList->getSimpleSelectedItem() );
		}
	}
}

const LLString& LLComboBox::getSimple() const
{
	const LLString& res = mList->getSimpleSelectedItem();
	if (res.empty() && mAllowTextEntry)
	{
		return mTextEntry->getText();
	}
	else
	{
		return res;
	}
}

const LLString& LLComboBox::getSimpleSelectedItem(S32 column) const
{
	return mList->getSimpleSelectedItem(column);
}

// virtual
LLSD LLComboBox::getValue() const
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return item->getValue();
	}
	else if (mAllowTextEntry)
	{
		return mTextEntry->getValue();
	}
	else
	{
		return LLSD();
	}
}

void LLComboBox::setLabel(const LLString& name)
{
	if ( mAllowTextEntry )
	{
		mTextEntry->setText(name);
		if (mList->selectSimpleItem(name, FALSE))
		{
			mTextEntry->setTentative(FALSE);
		}
		else
		{
			mTextEntry->setTentative(mTextEntryTentative);
		}
	}
	else
	{
		mButton->setLabelUnselected(name);
		mButton->setLabelSelected(name);
		mButton->setDisabledLabel(name);
		mButton->setDisabledSelectedLabel(name);
	}
}


BOOL LLComboBox::remove(const LLString& name)
{
	BOOL found = mList->selectSimpleItem(name);

	if (found)
	{
		LLScrollListItem* item = mList->getFirstSelected();
		if (item)
		{
			mList->deleteSingleItem(mList->getItemIndex(item));
		}
	}

	return found;
}

BOOL LLComboBox::remove(S32 index)
{
	if (index < mList->getItemCount())
	{
		mList->deleteSingleItem(index);
		return TRUE;
	}
	return FALSE;
}

// Keyboard focus lost.
void LLComboBox::onFocusLost()
{
	hideList();
	// if valid selection
	if (mAllowTextEntry && getCurrentIndex() != -1)
	{
		mTextEntry->selectAll();
	}
}

void LLComboBox::setButtonVisible(BOOL visible)
{
	mButton->setVisible(visible);
	mDrawButton = visible;
	if (mTextEntry)
	{
		LLRect text_entry_rect(0, mRect.getHeight(), mRect.getWidth(), 0);
		if (visible)
		{
			text_entry_rect.mRight -= mArrowImage->getWidth() + 2 * LLUI::sConfigGroup->getS32("DropShadowButton");
		}
		//mTextEntry->setRect(text_entry_rect);
		mTextEntry->reshape(text_entry_rect.getWidth(), text_entry_rect.getHeight(), TRUE);
	}
}

void LLComboBox::draw()
{
	if( getVisible() )
	{
		mBorder->setKeyboardFocusHighlight(hasFocus());

		mButton->setEnabled(mEnabled /*&& !mList->isEmpty()*/);

		// Draw children
		LLUICtrl::draw();

		if (mDrawButton)
		{
			// Paste the graphic on the right edge
			if (!mArrowImage.isNull())
			{
				S32 left = mRect.getWidth() - mArrowImage->getWidth() - LLUI::sConfigGroup->getS32("DropShadowButton");

				gl_draw_image( left, 0, mArrowImage,
					LLColor4::white);
			}
		}
	}
}

BOOL LLComboBox::setCurrentByIndex( S32 index )
{
	BOOL found = mList->selectNthItem( index );
	if (found)
	{
		setLabel(mList->getSimpleSelectedItem());
	}
	return found;
}

S32 LLComboBox::getCurrentIndex() const
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return mList->getItemIndex( item );
	}
	return -1;
}


void* LLComboBox::getCurrentUserdata()
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return item->getUserdata();
	}
	return NULL;
}


void LLComboBox::showList()
{
	// Make sure we don't go off top of screen.
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	//HACK: shouldn't have to know about scale here
	mList->arrange( 192, llfloor((F32)window_size.mY / LLUI::sGLScaleFactor.mV[VY]) - 50 );

	// Move rect so it hangs off the bottom of this view
	LLRect rect = mList->getRect();

	rect.setLeftTopAndSize(0, 0, rect.getWidth(), rect.getHeight() );
	mList->setRect(rect);

	// Make sure that we can see the whole list
	LLRect floater_area_screen;
	LLRect floater_area_local;
	gFloaterView->getParent()->localRectToScreen( gFloaterView->getRect(), &floater_area_screen );
	screenRectToLocal( floater_area_screen, &floater_area_local );
	mList->translateIntoRect( floater_area_local, FALSE );

	// Make sure we didn't go off bottom of screen
	S32 x, y;
	mList->localPointToScreen(0, 0, &x, &y);

	if (y < 0)
	{
		mList->translate(0, -y);
	}

	gFocusMgr.setMouseCapture( this, LLComboBox::onMouseCaptureLost );
	// NB: this call will trigger the focuslost callback which will hide the list, so do it first
	// before finally showing the list

	if (!mList->getFirstSelected())
	{
		// if nothing is selected, select the first item
		// so that the callback is not immediately triggered on setFocus()
		mList->selectFirstItem();
	}
	gFocusMgr.setKeyboardFocus(mList, onListFocusLost);

	// Show the list and push the button down
	mButton->setToggleState(TRUE);
	mList->setVisible(TRUE);
	
	gFocusMgr.setTopView(mList, LLComboBox::onTopViewLost );

}

void LLComboBox::hideList()
{
	mButton->setToggleState(FALSE);
	mList->setVisible(FALSE);
	mList->highlightNthItem(-1);

	if( gFocusMgr.getTopView() == mList )
	{
		gFocusMgr.setTopView(NULL, NULL);
	}

	if( gFocusMgr.getMouseCapture() == this )
	{
		gFocusMgr.setMouseCapture( NULL, NULL );
	}

	if( gFocusMgr.getKeyboardFocus() == mList )
	{
		if (mAllowTextEntry)
		{
			mTextEntry->setFocus(TRUE);
		}
		else
		{
			setFocus(TRUE);
		}
	}
}



//------------------------------------------------------------------
// static functions
//------------------------------------------------------------------

// static
void LLComboBox::onButtonClick(void *userdata)
{
	LLComboBox *self = (LLComboBox *)userdata;

	if (!self->mList->getVisible())
	{
		LLScrollListItem* last_selected_item = self->mList->getLastSelectedItem();
		if (last_selected_item)
		{
			// highlight the original selection before potentially selecting a new item
			self->mList->highlightNthItem(self->mList->getItemIndex(last_selected_item));
		}

		if( self->mPrearrangeCallback )
		{
			self->mPrearrangeCallback( self, self->mCallbackUserData );
		}

		if (self->mList->getItemCount() != 0)
		{
			self->showList();
		}

		if (self->mKeyboardFocusOnClick && !self->hasFocus())
		{
			self->setFocus( TRUE );
		}
	}
	else
	{
		// hide and release keyboard focus
		self->hideList();

		self->onCommit();
	}
}



// static
void LLComboBox::onItemSelected(LLUICtrl* item, void *userdata)
{
	// Note: item is the LLScrollListCtrl
	LLComboBox *self = (LLComboBox *) userdata;

	const LLString& name = self->mList->getSimpleSelectedItem();

	self->hideList();

	S32 cur_id = self->getCurrentIndex();
	if (cur_id != -1)
	{
		self->setLabel(self->mList->getSimpleSelectedItem());

		if (self->mAllowTextEntry)
		{
			self->mTextEntry->setText(name);
			self->mTextEntry->setTentative(FALSE);
			gFocusMgr.setKeyboardFocus(self->mTextEntry, NULL);
			self->mTextEntry->selectAll();
		}
		else
		{
			self->mButton->setLabelUnselected( name );
			self->mButton->setLabelSelected( name );
			self->mButton->setDisabledLabel( name );
			self->mButton->setDisabledSelectedLabel( name );
		}
	}
	self->onCommit();
}

// static
void LLComboBox::onTopViewLost(LLView* old_focus)
{
	LLComboBox *self = (LLComboBox *) old_focus->getParent();
	self->hideList();
}


// static
void LLComboBox::onMouseCaptureLost(LLMouseHandler*)
{
	// Can't hide the list here.  If the list scrolls off the screen,
	// and you click in the arrow buttons of the scroll bar, they must capture
	// the mouse to handle scrolling-while-mouse-down.
}

BOOL LLComboBox::handleToolTip(S32 x, S32 y, LLString& msg, LLRect* sticky_rect_screen)
{
	
    LLString tool_tip;

	if (LLUI::sShowXUINames)
	{
		tool_tip = mName;
	}
	else
	{
		tool_tip = mToolTipMsg;
	}

	if( getVisible() && pointInView( x, y ) ) 
	{
		if( !tool_tip.empty() )
		{
			msg = tool_tip;

			// Convert rect local to screen coordinates
			localPointToScreen( 
				0, 0, 
				&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
			localPointToScreen(
				mRect.getWidth(), mRect.getHeight(),
				&(sticky_rect_screen->mRight), &(sticky_rect_screen->mTop) );
		}
		return TRUE;
	}
	return FALSE;
}

BOOL LLComboBox::handleKeyHere(KEY key, MASK mask, BOOL called_from_parent)
{
	BOOL result = FALSE;
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		//give list a chance to pop up and handle key
		LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
		if (last_selected_item)
		{
			// highlight the original selection before potentially selecting a new item
			mList->highlightNthItem(mList->getItemIndex(last_selected_item));
		}
		result = mList->handleKeyHere(key, mask, FALSE);
		// if selection has changed, pop open list
		if (mList->getLastSelectedItem() != last_selected_item)
		{
			showList();
		}
	}
	return result;
}

BOOL LLComboBox::handleUnicodeCharHere(llwchar uni_char, BOOL called_from_parent)
{
	BOOL result = FALSE;
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		// space bar just shows the list
		if (' ' != uni_char )
		{
			LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
			if (last_selected_item)
			{
				// highlight the original selection before potentially selecting a new item
				mList->highlightNthItem(mList->getItemIndex(last_selected_item));
			}
			result = mList->handleUnicodeCharHere(uni_char, called_from_parent);
			if (mList->getLastSelectedItem() != last_selected_item)
			{
				showList();
			}
		}
	}
	return result;
}

void LLComboBox::setAllowTextEntry(BOOL allow, S32 max_chars, BOOL set_tentative)
{
	LLRect rect( 0, mRect.getHeight(), mRect.getWidth(), 0);
	if (allow && !mAllowTextEntry)
	{
		S32 shadow_size = LLUI::sConfigGroup->getS32("DropShadowButton");
		mButton->setRect(LLRect( mRect.getWidth() - mArrowImage->getWidth() - 2 * shadow_size,
								rect.mTop, rect.mRight, rect.mBottom));
		mButton->setTabStop(FALSE);

		// clear label on button
		LLString cur_label = mButton->getLabelSelected();
		setLabel("");
		if (!mTextEntry)
		{
			LLRect text_entry_rect(0, mRect.getHeight(), mRect.getWidth(), 0);
			text_entry_rect.mRight -= mArrowImage->getWidth() + 2 * LLUI::sConfigGroup->getS32("DropShadowButton");
			mTextEntry = new LLLineEditor("combo_text_entry",
										text_entry_rect,
										"",
										LLFontGL::sSansSerifSmall,
										max_chars,
										onTextCommit,
										onTextEntry,
										NULL,
										this,
										NULL,		// prevalidate func
										LLViewBorder::BEVEL_NONE,
										LLViewBorder::STYLE_LINE,
										0);	// no border
			mTextEntry->setSelectAllonFocusReceived(TRUE);
			mTextEntry->setHandleEditKeysDirectly(TRUE);
			mTextEntry->setCommitOnFocusLost(FALSE);
			mTextEntry->setText(cur_label);
			mTextEntry->setIgnoreTab(TRUE);
			addChild(mTextEntry);
			mMaxChars = max_chars;
		}
		else
		{
			mTextEntry->setVisible(TRUE);
		}
	}
	else if (!allow && mAllowTextEntry)
	{
		mButton->setRect(rect);
		mButton->setTabStop(TRUE);

		if (mTextEntry)
		{
			mTextEntry->setVisible(FALSE);
		}
	}
	mAllowTextEntry = allow;
	mTextEntryTentative = set_tentative;	
}

void LLComboBox::setTextEntry(const LLString& text)
{
	if (mTextEntry)
	{
		mTextEntry->setText(text);
		updateSelection();
	}
}

//static 
void LLComboBox::onTextEntry(LLLineEditor* line_editor, void* user_data)
{
	LLComboBox* self = (LLComboBox*)user_data;

	if (self->mTextEntryCallback)
	{
		(*self->mTextEntryCallback)(line_editor, self->mCallbackUserData);
	}

	KEY key = gKeyboard->currentKey();
	if (key == KEY_BACKSPACE || 
		key == KEY_DELETE)
	{
		if (self->mList->selectSimpleItem(line_editor->getText(), FALSE))
		{
			line_editor->setTentative(FALSE);
		}
		else
		{
			line_editor->setTentative(self->mTextEntryTentative);
		}
		return;
	}

	if (key == KEY_LEFT || 
		key == KEY_RIGHT)
	{
		return;
	}

	if (key == KEY_DOWN)
	{
		self->setCurrentByIndex(llmin(self->getItemCount() - 1, self->getCurrentIndex() + 1));
		if (!self->mList->getVisible())
		{
			if( self->mPrearrangeCallback )
			{
				self->mPrearrangeCallback( self, self->mCallbackUserData );
			}

			if (self->mList->getItemCount() != 0)
			{
				self->showList();
			}
		}
		line_editor->selectAll();
		line_editor->setTentative(FALSE);
	}
	else if (key == KEY_UP)
	{
		self->setCurrentByIndex(llmax(0, self->getCurrentIndex() - 1));
		if (!self->mList->getVisible())
		{
			if( self->mPrearrangeCallback )
			{
				self->mPrearrangeCallback( self, self->mCallbackUserData );
			}

			if (self->mList->getItemCount() != 0)
			{
				self->showList();
			}
		}
		line_editor->selectAll();
		line_editor->setTentative(FALSE);
	}
	else
	{
		// RN: presumably text entry
		self->updateSelection();
	}
}

void LLComboBox::updateSelection()
{
	LLWString left_wstring = mTextEntry->getWText().substr(0, mTextEntry->getCursor());
	// user-entered portion of string, based on assumption that any selected
    // text was a result of auto-completion
	LLWString user_wstring = mTextEntry->hasSelection() ? left_wstring : mTextEntry->getWText();
	LLString full_string = mTextEntry->getText();

	// go ahead and arrange drop down list on first typed character, even
	// though we aren't showing it... some code relies on prearrange
	// callback to populate content
	if( mTextEntry->getWText().size() == 1 )
	{
		if (mPrearrangeCallback)
		{
			mPrearrangeCallback( this, mCallbackUserData );
		}
	}

	if (mList->selectSimpleItem(full_string, FALSE))
	{
		mTextEntry->setTentative(FALSE);
	}
	else if (!mList->selectSimpleItemByPrefix(left_wstring, FALSE))
	{
		mList->deselectAllItems();
		mTextEntry->setText(wstring_to_utf8str(user_wstring));
		mTextEntry->setTentative(mTextEntryTentative);
	}
	else
	{
		LLWString selected_item = utf8str_to_wstring(mList->getSimpleSelectedItem());
		LLWString wtext = left_wstring + selected_item.substr(left_wstring.size(), selected_item.size());
		mTextEntry->setText(wstring_to_utf8str(wtext));
		mTextEntry->setSelection(left_wstring.size(), mTextEntry->getWText().size());
		mTextEntry->endSelection();
		mTextEntry->setTentative(FALSE);
	}
}

//static 
void LLComboBox::onTextCommit(LLUICtrl* caller, void* user_data)
{
	LLComboBox* self = (LLComboBox*)user_data;
	LLString text = self->mTextEntry->getText();
	self->setSimple(text);
	self->onCommit();
	self->mTextEntry->selectAll();
}

void LLComboBox::setFocus(BOOL b)
{
	LLUICtrl::setFocus(b);

	if (b)
	{
		mList->clearSearchString();
	}
}

//============================================================================
// LLCtrlListInterface functions

S32 LLComboBox::getItemCount() const
{
	return mList->getItemCount();
}

void LLComboBox::addColumn(const LLSD& column, EAddPosition pos)
{
	mList->clearColumns();
	mList->addColumn(column, pos);
}

void LLComboBox::clearColumns()
{
	mList->clearColumns();
}

void LLComboBox::setColumnLabel(const LLString& column, const LLString& label)
{
	mList->setColumnLabel(column, label);
}

LLScrollListItem* LLComboBox::addElement(const LLSD& value, EAddPosition pos, void* userdata)
{
	return mList->addElement(value, pos, userdata);
}

LLScrollListItem* LLComboBox::addSimpleElement(const LLString& value, EAddPosition pos, const LLSD& id)
{
	return mList->addSimpleElement(value, pos, id);
}

void LLComboBox::clearRows()
{
	mList->clearRows();
}

void LLComboBox::sortByColumn(LLString name, BOOL ascending)
{
}

//============================================================================
//LLCtrlSelectionInterface functions

BOOL LLComboBox::setCurrentByID(const LLUUID& id)
{
	BOOL found = mList->selectByID( id );

	if (found)
	{
		setLabel(mList->getSimpleSelectedItem());
	}

	return found;
}

LLUUID LLComboBox::getCurrentID()
{
	return mList->getStringUUIDSelectedItem();
}
BOOL LLComboBox::setSelectedByValue(LLSD value, BOOL selected)
{
	BOOL found = mList->setSelectedByValue(value, selected);
	if (found)
	{
		setLabel(mList->getSimpleSelectedItem());
	}
	return found;
}

LLSD LLComboBox::getSimpleSelectedValue()
{
	return mList->getSimpleSelectedValue();
}

BOOL LLComboBox::isSelected(LLSD value)
{
	return mList->isSelected(value);
}

BOOL LLComboBox::operateOnSelection(EOperation op)
{
	if (op == OP_DELETE)
	{
		mList->deleteSelectedItems();
		return TRUE;
	}
	return FALSE;
}

BOOL LLComboBox::operateOnAll(EOperation op)
{
	if (op == OP_DELETE)
	{
		clearRows();
		return TRUE;
	}
	return FALSE;
}

//static 
void LLComboBox::onListFocusLost(LLUICtrl* old_focus)
{
	// if focus is going to nothing (user hit ESC), take it back
	LLComboBox* combo = (LLComboBox*)old_focus->getParent();
	combo->hideList();
	if (gFocusMgr.getKeyboardFocus() == NULL)
	{
		combo->focusFirstItem();
	}
}
