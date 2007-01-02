/** 
 * @file llxfer.h
 * @brief definition of LLXfer class for a single xfer
 *
 * Copyright (c) 2001-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#ifndef LL_LLXFER_H
#define LL_LLXFER_H

#include "message.h"
#include "lltimer.h"

const S32 LL_XFER_LARGE_PAYLOAD = 7680;

typedef enum ELLXferStatus {
	e_LL_XFER_UNINITIALIZED,
	e_LL_XFER_REGISTERED,         // a buffer which has been registered as available for a request
	e_LL_XFER_PENDING,            // a transfer which has been requested but is waiting for a free slot
	e_LL_XFER_IN_PROGRESS,
	e_LL_XFER_COMPLETE,
	e_LL_XFER_ABORTED,
	e_LL_XFER_NONE
} ELLXferStatus;

class LLXfer
{
 private:
 protected:
        S32 mChunkSize;
  
 public:
	LLXfer *mNext;
	U64 mID;
	S32 mPacketNum;

	LLHost mRemoteHost;
	S32 mXferSize;

	char *mBuffer;
	U32 mBufferLength;
	U32 mBufferStartOffset;
	BOOL mBufferContainsEOF;

	ELLXferStatus mStatus;

	BOOL mWaitingForACK;

	void (*mCallback)(void **,S32);	
	void **mCallbackDataHandle;
	S32 mCallbackResult;

	LLTimer ACKTimer;
	S32 mRetries;

	static const U32 XFER_FILE;
	static const U32 XFER_VFILE;
	static const U32 XFER_MEM;

 private:
 protected:
 public:
	LLXfer (S32 chunk_size);
	virtual ~LLXfer();

	void init(S32 chunk_size);
	virtual void free();

	virtual S32 startSend (U64 xfer_id, const LLHost &remote_host);
	virtual void sendPacket(S32 packet_num);
	virtual void sendNextPacket();
	virtual void resendLastPacket();
	virtual S32 processEOF();
	virtual S32 startDownload();
	virtual S32 receiveData (char *datap, S32 data_size);
	virtual void abort(S32);

	virtual S32 suck(S32 start_position);
	virtual S32 flush();
	
	virtual S32 encodePacketNum(S32 packet_num, BOOL is_eof);	
	virtual void setXferSize (S32 data_size);
	virtual S32  getMaxBufferSize();

	virtual const char *getName();

	virtual U32 getXferTypeTag();

	friend std::ostream& operator<< (std::ostream& os, LLXfer &hh);

};

#endif




