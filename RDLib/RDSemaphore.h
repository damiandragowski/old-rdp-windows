#ifndef DSEMAPHORE__H
#define DSEMAPHORE__H

#include "stdafx.h"

class RDSemaphore
{
public:
	RDSemaphore();
	~RDSemaphore();
	int  Init(unsigned int ui);
	void Wait();
	void Release();
private:
	HANDLE	* semaphore;
};

#endif