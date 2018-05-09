// RDThread.cpp: implementation of the RDThread class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "RDThread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RDThread::RDThread(LPTHREAD_START_ROUTINE function, void * param)
{
	thread = CreateThread(0,0, function, param, 0, &id);
}

int RDThread::Resume()
{
	return ResumeThread(thread);
}

int RDThread::Suspend()
{
	return SuspendThread(thread);
}

int RDThread::Terminate()
{
	return TerminateThread(thread, 0);
}

int RDThread::WaitForEnd(DWORD milisec)
{
	return WaitForSingleObject(thread, milisec);
}

int RDThread::PostThreadMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return ::PostThreadMessage(id, Msg, wParam, lParam);
}

RDThread::~RDThread()
{
	TerminateThread(thread, 0);
}
