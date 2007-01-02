/** 
 * @dir lldirpicker.h
 * @brief OS-specific dir picker
 *
 * Copyright (c) 2001-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

// OS specific dir selection dialog. This is implemented as a
// singleton class, so call the instance() method to get the working
// instance. 

#ifndef LL_LLDIRPICKER_H
#define LL_LLDIRPICKER_H

#include "stdtypes.h"

#if LL_DARWIN
#include <Carbon/Carbon.h>

// AssertMacros.h does bad things.
#undef verify
#undef check
#undef require

#include <vector>
#include "llstring.h"

#endif

// Need commdlg.h for OPENDIRNAMEA
#ifdef LL_WINDOWS
#include <commdlg.h>
#endif

class LLDirPicker
{
public:
	// calling this before main() is undefined
	static LLDirPicker& instance( void ) { return sInstance; }

	BOOL getDir(LLString* filename);
	LLString getDirName();

	// clear any lists of buffers or whatever, and make sure the dir
	// picker isn't locked.
	void reset();

private:
	enum
	{
		SINGLE_DIRNAME_BUFFER_SIZE = 1024,
		//DIRNAME_BUFFER_SIZE = 65536
		DIRNAME_BUFFER_SIZE = 65000 
	};
	
	void buildDirname( void );

#if LL_WINDOWS
//FIXME
#endif

#if LL_DARWIN
	NavDialogCreationOptions mNavOptions;
	static pascal void doNavCallbackEvent(NavEventCallbackMessage callBackSelector,
										 NavCBRecPtr callBackParms, void* callBackUD);
	OSStatus	doNavChooseDialog();
	
#endif

	char mDirs[DIRNAME_BUFFER_SIZE];
	LLString* mFileName;
	LLString  mDir;
	BOOL mLocked;

	static LLDirPicker sInstance;
	
public:
	// don't call these directly please.
	LLDirPicker();
	~LLDirPicker();
};

#endif
