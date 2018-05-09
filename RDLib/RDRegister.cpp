#include ".\rdregister.h"

RDRegister::RDRegister(void):hKey(0)
{

}

RDRegister::~RDRegister(void)
{

}

void RDRegister::setBaseKey(HKEY Key)
{
	base = Key;
}
