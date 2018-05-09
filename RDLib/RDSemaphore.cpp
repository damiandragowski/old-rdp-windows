#include "stdafx.h"
#include "RDSemaphore.h"

RDSemaphore::RDSemaphore()
{
	semaphore = 0;
		
}

int RDSemaphore::Init(unsigned int ui)
{
	CreateSemaphore(0, ui, MAXLONG, 0);
	if ( semaphore == 0 )
		return -1;
	return 0;
}

RDSemaphore::~RDSemaphore()
{
	if ( semaphore != 0 ) {
		if (!CloseHandle(semaphore))
			return;	
	}

}

void RDSemaphore::Wait()
{
	if (WaitForSingleObject(semaphore, INFINITE) != WAIT_OBJECT_0)
		return;	
}

void RDSemaphore::Release()
{
    if (!ReleaseSemaphore(semaphore, 1, 0))
		return;
}