#include "stdafx.h"
#include ".\dsocket.h"
#include "logger.h"

int DSocket::SockCounter = 0;
int DSocket::winsockVersion = 0;


DSocket::DSocket()
{
	if ( SockCounter == 0 )
	{
		int wVersionRequested = MAKEWORD(2, 0);
		WSADATA wsaData;
		if (WSAStartup(wVersionRequested, &wsaData) != 0)
		{
			logger.AddString(TEXT("Socket init Error\n"));
			return;
		}
		winsockVersion = wsaData.wVersion;
	}
	++SockCounter;
	sock = -1;
}

DSocket::~DSocket()
{
	Close();
	--SockCounter;
	if ( SockCounter == 0 )
		WSACleanup();
}

bool DSocket::Create()
{
	const int one = 1;
	if (sock >= 0)
		Close();

	if ((sock = (int)socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		logger.AddString(TEXT("socket: Create Error\n"));
		return false;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)))
		return false;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)))
		return false;
	return true;
}

bool DSocket::Close()
{
	if (sock >= 0)
    {
		shutdown(sock, SD_BOTH);
		closesocket(sock);
		sock = -1;
    }
	return true;
}

bool DSocket::Shutdown()
{
	if (sock >= 0)
		shutdown(sock, SD_BOTH);
	return true;
}

bool DSocket::Bind(const int port, const bool localOnly)
{
	struct sockaddr_in addr;
	if (sock < 0)
		return false;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (localOnly)
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	else
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return false;
	return true;
}

bool DSocket::Connect(const string address, const int port)
{
	if (sock < 0)
		return false;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(address.c_str());

	if (addr.sin_addr.s_addr == -1)
    {
		struct hostent *pHost;
		pHost = gethostbyname(address.c_str());
		if (pHost != NULL)
		{
			if (pHost->h_addr == NULL)
				return false;
			addr.sin_addr.s_addr = ((struct in_addr *)pHost->h_addr)->s_addr;
		}
		else
			return false;
    }
	addr.sin_port = htons(port);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
		return true;

	return false;
}

bool DSocket::Listen()
{
	if (sock < 0)
		return false;
	if (listen(sock, 5) < 0)
		return false;
	return true;
}

DSocket * DSocket::Accept()
{
	const int one = 1;
	int new_socket_id;
	DSocket * new_socket;

	if (sock < 0)
		return NULL;
	if ((new_socket_id = (int)accept(sock, NULL, 0)) < 0)
		return NULL;
	new_socket = new DSocket;
	if (new_socket != NULL)
	{
		new_socket->sock = new_socket_id;
	}
	else
	{
		shutdown(new_socket_id, SD_BOTH);
		closesocket(new_socket_id);
	}
	setsockopt(new_socket->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));
	return new_socket;
}

string DSocket::GetPeerName()
{
	struct sockaddr_in	sockinfo;
	struct in_addr		address;
	int					sockinfosize = sizeof(sockinfo);
	string				name;

	getpeername(sock, (struct sockaddr *)&sockinfo, &sockinfosize);
	memcpy(&address, &sockinfo.sin_addr, sizeof(address));

	name = inet_ntoa(address);
	if (name == "")
		return "<unavailable>";
	else
		return name;
}

string DSocket::GetSockName()
{
	struct sockaddr_in	sockinfo;
	struct in_addr		address;
	int					sockinfosize = sizeof(sockinfo);
	string				name;

	getsockname(sock, (struct sockaddr *)&sockinfo, &sockinfosize);
	memcpy(&address, &sockinfo.sin_addr, sizeof(address));
	name = inet_ntoa(address);
	if (name == "")
		return "<unavailable>";
	else
		return name;
}

long DSocket::Resolve(const string & address)
{
	long addr;
	addr = inet_addr(address.c_str());
	if (addr == 0xffffffff)
    {
		struct hostent *pHost;
		pHost = gethostbyname(address.c_str());
		if (pHost != NULL)
		{
			if (pHost->h_addr == NULL)
				return 0;
			addr = ((struct in_addr *)pHost->h_addr)->s_addr;
		}
		else
			return 0;
	}
	return addr;
}

bool DSocket::SetTimeout(long millisec)
{
	if (LOBYTE(winsockVersion) < 2)
		return false;
	int timeout=millisec;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		return false;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		return false;
	}
	return true;
}

int DSocket::Send(const char *buff, const int bufflen)
{
	int ret=0;
	//mutex.lock();
	ret = send(sock, buff, bufflen, 0);
	//mutex.unlock();
	return ret;
}

bool DSocket::SendExact(const char *buff, const int bufflen)
{
	int n;
	int currlen = bufflen;

	while (currlen > 0)
	{
		n = Send(buff, currlen);
		if (n > 0)
		{
			buff += n;
			currlen -= n;
		} else if (n == 0) {
			return false;
		} else {
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return false;
			}
		}
    }
	return true;
}

int DSocket::Read(char *buff, const int bufflen)
{
	int ret=0;
	//mutex.lock();
	ret = recv(sock, buff, bufflen, 0);
	//mutex.unlock();
	return ret;
}

bool DSocket::ReadExact(char *buff, const int bufflen)
{
	int n;
	int currlen = bufflen;

	while (currlen > 0)
	{
		n = Read(buff, currlen);
		if (n > 0)
		{
			buff += n;
			currlen -= n;
		} else if (n == 0) {
			return false;
		} else {
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return false;
			}
		}
    }
	return true;
}

bool DSocket::SendW(char *buff, const int bufflen)
{
	bool ret = false;
	/*
	fd_set fd[1];
	FD_ZERO(fd);
	FD_SET(sock, fd);
	select(1, 0, fd, 0, 0);
	if ( FD_ISSET(sock, fd) )
	*/
		ret = SendExact(buff, bufflen);
	return ret;
}

bool DSocket::ReadW(char *buff, const int bufflen)
{
	bool ret = false;
	/*
	fd_set fd[1];
	FD_ZERO(fd);
	FD_SET(sock, fd);
	select(1, fd, 0, 0, 0);
	if ( FD_ISSET(sock, fd) )
	*/
		ret = ReadExact(buff, bufflen);
	return ret;
}