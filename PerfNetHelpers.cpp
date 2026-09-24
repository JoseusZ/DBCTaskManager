// PerfNetHelpers.cpp
#include "stdafx.h"
#include "PerfNetHelpers.h"

#include <winsock2.h>
#include <ws2tcpip.h>

BOOL PerfNet_IsIPv6LinkLocal(const SOCKADDR* sa)
{
	if(sa == NULL || sa->sa_family != AF_INET6) return FALSE;
	const SOCKADDR_IN6* sa6 = (const SOCKADDR_IN6*)sa;
	return (sa6->sin6_addr.u.Byte[0] == 0xFE) &&
		   ((sa6->sin6_addr.u.Byte[1] & 0xC0) == 0x80);
}
