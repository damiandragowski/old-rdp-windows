// RDMutex.h: interface for the RDMutex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RDMUTEX_H__283046A0_EFDD_4042_9C97_C649BACABF91__INCLUDED_)
#define AFX_RDMUTEX_H__283046A0_EFDD_4042_9C97_C649BACABF91__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "stdafx.h"

class RDMutex  
{
public:
	void unlock();
	void lock();
	RDMutex();
	virtual ~RDMutex();
private:
	CRITICAL_SECTION cs;

};

#endif // !defined(AFX_RDMUTEX_H__283046A0_EFDD_4042_9C97_C649BACABF91__INCLUDED_)
