// RDMutex.cpp: implementation of the RDMutex class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "RDMutex.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RDMutex::RDMutex()
{
	InitializeCriticalSection(&cs);
}

RDMutex::~RDMutex()
{
	DeleteCriticalSection(&cs);
}

void RDMutex::lock()
{
	EnterCriticalSection(&cs);
}

void RDMutex::unlock()
{
	LeaveCriticalSection(&cs);
}
