#ifndef DSOCKET__H
#define DSOCKET__H

#pragma once
#include <string>
using namespace std;
#include "RDMutex.h"

class DSocket
{
public:
	DSocket();
	virtual ~DSocket();

	bool Create();
	bool Shutdown();
	bool Close();
	bool Bind(const int port, const bool localOnly=false);
	bool Connect(const string address, const int port);
	bool Listen();
	bool SetTimeout(long millisec);
	bool SendExact(const char *buff, const int bufflen);
	bool ReadExact(char *buff, const int bufflen);
	DSocket * Accept();
	string GetPeerName();
	string GetSockName();
	static long Resolve(const string & name);
	int Send(const char *buff, const int bufflen);
	int Read(char *buff, const int bufflen);
	bool SendW(char *buff, const int bufflen);
	bool ReadW(char *buff, const int bufflen);
protected:
	int sock;
	static int SockCounter;
	static int winsockVersion;
	//RDMutex mutex;
};


#endif // DSOCKET_H