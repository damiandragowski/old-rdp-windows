// RDThread.h: interface for the RDThread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RDTHREAD_H__16F3FCA5_DAFC_4345_84F9_826C716D2E86__INCLUDED_)
#define AFX_RDTHREAD_H__16F3FCA5_DAFC_4345_84F9_826C716D2E86__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class RDThread  
{
private:
	HANDLE thread;
	DWORD  id;
public:
	RDThread(LPTHREAD_START_ROUTINE function, void * param);
	virtual ~RDThread();
	virtual int Suspend();
	virtual int Resume();
	virtual int Terminate();
	int PostThreadMessage(UINT Msg, WPARAM wParam, LPARAM lParam);
	int WaitForEnd(DWORD milisec);
	DWORD getID() const { return id; }
};

#endif // !defined(AFX_RDTHREAD_H__16F3FCA5_DAFC_4345_84F9_826C716D2E86__INCLUDED_)
