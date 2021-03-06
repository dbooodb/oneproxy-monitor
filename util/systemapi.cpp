/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*
* @FileName: systemapi.cpp
* @Description: TODO
* All rights Reserved, Designed By huih
* @Company: onexsoft
* @Author: hui
* @Version: V1.0
* @Date: 2016年8月17日
*
*/

#include "memmanager.h"
#include "systemapi.h"
#include "logger.h"
#include "mutexlock.h"

#include <errno.h>
#include <string.h>
#include <map>
#include <string>
#include <iostream>

#ifdef linux
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#else
#include "winsock2.h"
#include "iphlpapi.h"
#endif

int SystemApi::getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result)
{
#ifdef linux
	return ::getaddrinfo(hostname, service, hints, result);
#else

	union _addr {
		struct sockaddr sa;
		struct sockaddr_in sin;
	#ifdef linux
		struct sockaddr_in6 sin6;
	#endif
	} addr;

	struct addrinfo* ai = NULL;
	unsigned long ld = INADDR_NONE;
	if((ld = inet_addr(hostname)) != INADDR_NONE) {
		memcpy(&addr.sin.sin_addr, &ld, sizeof(ld));
		addr.sin.sin_family = AF_INET;
		addr.sin.sin_port = htons(atoi(service));
		ai = (struct addrinfo*)MemManager::malloc(sizeof(struct addrinfo));
		ai->ai_addr = (struct sockaddr*)MemManager::malloc(sizeof(struct sockaddr));
		memcpy(ai->ai_addr, &addr, sizeof(addr));
		ai->ai_family = addr.sin.sin_family;
		ai->ai_next = NULL;
	} else {
		struct hostent* hostent = gethostbyname(hostname);
		if (hostent == NULL) {
			return -1;
		}

		char** h_addr;
		struct addrinfo* currAddr = NULL;
		for (h_addr = hostent->h_addr_list; h_addr != NULL; ++h_addr) {
			memcpy(&addr.sin.sin_addr, *h_addr, sizeof(addr.sin.sin_addr));
			addr.sin.sin_family = AF_INET;
			addr.sin.sin_port = htons(atoi(service));

			if (ai == NULL) {
				ai = (struct addrinfo*)MemManager::malloc(sizeof(struct addrinfo));
				ai->ai_addr = (struct sockaddr*)MemManager::malloc(sizeof(struct sockaddr));
				memcpy(ai->ai_addr, &addr, sizeof(addr));
				ai->ai_family = addr.sin.sin_family;
				ai->ai_next = NULL;
				currAddr = ai;
			} else {
				struct addrinfo *tai = (struct addrinfo*)MemManager::malloc(sizeof(struct addrinfo));
				tai->ai_addr = (struct sockaddr*)MemManager::malloc(sizeof(struct sockaddr));
				memcpy(tai->ai_addr, &addr, sizeof(addr));
				tai->ai_family = addr.sin.sin_family;
				tai->ai_next = NULL;
				currAddr->ai_next = tai;
				currAddr = currAddr->ai_next;
			}
		}
	}
	*result = ai;
	return 0;
#endif
}

void SystemApi::freeaddrinfo(struct addrinfo* ai)
{
#ifdef linux
	::freeaddrinfo(ai);
#else
	for (;;) {
		struct addrinfo* tai = ai;
		if (tai == NULL) {
			break;
		}
		ai = ai->ai_next;
		MemManager::free(tai->ai_addr);
		MemManager::free(tai);
	}
#endif
}

void SystemApi::close(int fd)
{
#ifdef linux
	::close(fd);
#else
	::closesocket(fd);
#endif
}

int SystemApi::system_errno()
{
#ifdef WIN32
	errno = WSAGetLastError();
#endif
	return errno;
}

char* SystemApi::system_strerror(int errorno)
{
#ifdef WIN32
	static char buf[1024];
	LPVOID lpMsgBuf;
	::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			errorno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
	::_snprintf(buf, 1024, "%s", lpMsgBuf);
	::LocalFree(lpMsgBuf);
	return (char*)buf;

#else
	return strerror(errorno);
#endif
}

char* SystemApi::system_strerror()
{
	return SystemApi::system_strerror(SystemApi::system_errno());
}

time_t SystemApi::system_time()
{
	time_t ttime;
	ttime = time(NULL);
	return ttime;
}

std::string SystemApi::system_timeStr()
{
	char buf[32];

#ifdef linux
	time_t ttime = time(NULL);
	struct tm tm = {0};
	localtime_r(&ttime, &tm);
	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tm);
#else
	SYSTEMTIME sys;
	GetLocalTime(&sys);
	snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d", sys.wYear, sys.wMonth,sys.wDay,sys.wHour,sys.wMinute, sys.wSecond);
#endif

	return std::string(buf, strlen(buf));
}

std::string SystemApi::system_time2Str(time_t t) {
	char buf[32];
	struct tm tm = {0};
#ifdef linux
	localtime_r(&t, &tm);
#else
	tm = *localtime(&t);
#endif
	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tm);

	return std::string(buf, strlen(buf));
}

u_uint64 SystemApi::system_millisecond()
{
	u_uint64 result = 0;
#ifdef linux
	struct timeval t_start;
	gettimeofday(&t_start, NULL);//when use gperftool, maybe core.
	result = ((u_uint64)t_start.tv_sec) * 1000 + ((u_uint64)t_start.tv_usec) / 1000;
#else
	result = (u_uint64)GetTickCount();
#endif
	return result;
}

u_uint64 SystemApi::system_second()
{
	return SystemApi::system_millisecond()/1000;
}

struct tm* SystemApi::system_timeTM()
{
	struct tm *cur_p;
	time_t t = time(NULL);
#ifdef WIN32
	cur_p = gmtime(&t);
#else
	static struct tm cur;
	gmtime_r(&t, &cur);
	cur_p = &cur;
#endif
	return cur_p;
}

int SystemApi::system_limitFileNum(unsigned int& fileNum)
{
	fileNum = 65536;
#ifdef linux
	fileNum = 0;
	struct rlimit limit;
	if (getrlimit(RLIMIT_NOFILE, &limit)) {
		logs(Logger::ERR, "get limit error(%s)", SystemApi::system_strerror());
		return -1;
	}
	fileNum = limit.rlim_cur;
#endif
	return 0;
}

unsigned int SystemApi::system_fdSetSize()
{
	return (unsigned int)FD_SETSIZE;
}

void SystemApi::system_sleep(int ms)
{
#ifdef linux
	sleep((ms*1.0)/1000.0);
#else
	::Sleep(ms);
#endif
}

int SystemApi::init_networkEnv()
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		logs(Logger::ERR, "WSAStartup failed witherror: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 ||HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not finda usable */
		/* WinSock DLL.                                  */
		logs(Logger::ERR, "Could not find a usableversion of Winsock.dll\n");
		WSACleanup();
		return -1;
	}
#endif
	return 0;
}

void SystemApi::clear_networkEnv()
{
#ifdef _WIN32
	WSACleanup();
#endif
}

int SystemApi::get_pid()
{
#ifdef _WIN32
	return (int)::GetCurrentProcessId();
#else
	return (int)getpid();
#endif
}

unsigned int SystemApi::system_cpus()
{
	unsigned int count = 1; // 至少一个

#ifdef linux
	count = sysconf(_SC_NPROCESSORS_CONF);
#else
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	count = si.dwNumberOfProcessors;
#endif
	return count;
}

int SystemApi::system_setSocketRcvTimeo(SystemSocket& sockRaw, int second, int microsecond)
{
#ifdef _WIN32
	int timeout = second * 1000 + microsecond / 1000;
	if(setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout))) {
		logs(Logger::ERR,"failed to set recv timeout: %s\n", SystemApi::system_strerror());
		return -1;
	}
#else
	socklen_t optlen = sizeof(struct timeval);
	struct timeval tv;
	tv.tv_sec = second;
	tv.tv_usec = microsecond;
	if (setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, &tv, optlen)) {
		logs(Logger::ERR,"failed to set recv timeout: %s\n", SystemApi::system_strerror());
		return -1;
	}
#endif
	return 0;
}

int SystemApi::system_setSocketSndTimeo(SystemSocket& sockRaw, int second, int microsecond)
{
#ifdef _WIN32
	int timeout = second * 1000 + microsecond / 1000;
	if(setsockopt(sockRaw,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout, sizeof(timeout))) {
		logs(Logger::ERR,"failed to set send timeout: %s\n", SystemApi::system_strerror());
		return -1;
	}
#else
	socklen_t optlen = sizeof(struct timeval);
	struct timeval tv;
	tv.tv_sec = second;
	tv.tv_usec = microsecond;
	if(getsockopt(sockRaw, SOL_SOCKET,SO_SNDTIMEO, &tv, &optlen)) {
		logs(Logger::ERR,"failed to set send timeout: %s\n", SystemApi::system_strerror());
		return -1;
	}
#endif
	return 0;
}

int SystemApi::system_setSockNonblocking(unsigned int fd, bool nonBlocking)
{
#ifdef WIN32
	unsigned long int blockValue = 0;
	if (nonBlocking) {
		blockValue = 1;
	} else {
		blockValue = 0;
	}
	if (system_ioctl(fd, FIONBIO, (unsigned long int *)&blockValue)) {
		logs(Logger::ERR, "ioctlsocket error, non_block(%d), errmsg: %s", nonBlocking, SystemApi::system_strerror());
		return -1;
	}
#else
	int flags;

	/* get old flags */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		logs(Logger::ERR, "get sock(%d) flag error, errormsg: %s", fd, SystemApi::system_strerror());
		return -1;
	}

	/* flip O_NONBLOCK */
	if (nonBlocking)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	/* set new flags */
	if (fcntl(fd, F_SETFL, flags) < 0) {
		logs(Logger::ERR, "set sock(%d) flags(%d) error(%s)", fd, flags, SystemApi::system_strerror());
		return -1;
	}

#endif
	return 0;
}

int SystemApi::system_inetPton(int af, const char *src, void *dst)
{
	switch(af) {
	case AF_INET6:
	case AF_INET:
#ifndef __WIN32
		if (inet_pton(af, src, dst) < 0) {
			logs(Logger::ERR, "inet_pton error, address: %s", src);
			return -1;
		}
#else
		if (af == AF_INET) {
			unsigned long laddress = inet_addr(src);
			memcpy(dst, &laddress, sizeof(laddress));
		} else {
			logs(Logger::WARNING, "unsupport AF_INET6");
		}
#endif
		break;
	default:
		logs(Logger::ERR, "unkown protocol family(%d)", af);
		return -1;
	}
	return 0;
}

int SystemApi::system_ntop(int af, const void *src, char *dst, socklen_t cnt)
{

	switch(af) {
		case AF_INET6:
		case AF_INET:
		{
#ifndef __WIN32
			if (inet_ntop(af, src, dst, cnt) < 0) {
				logs(Logger::ERR, "inet_ntop error");
				return -1;
			}
#else
			if (af == AF_INET) {
				char* tbuf = inet_ntoa((*((struct in_addr *)src)));
				memcpy((void*)dst, tbuf, cnt);
			} else {
				logs(Logger::WARNING, "unsupport AF_INET6");
				return -1;
			}
#endif
			break;
		}
		default:
		{
			logs(Logger::ERR, "unkown family type(%d)", af);
			return -1;
		}
	}
	return 0;
}

void SystemApi::system_setThreadName(std::string name) {
#ifdef linux
	prctl(PR_SET_NAME, name.c_str());
#endif
}

int SystemApi::system_setFDNum(unsigned int max_files_number) {
#ifdef _WIN32
	int max_files_number_set;

	max_files_number_set = _setmaxstdio(max_files_number);

	if (-1 == max_files_number_set) {
		logs(Logger::ERR, "set max files number error");
		return -1;
	} else if ((unsigned int)max_files_number_set != max_files_number) {
		logs(Logger::ERR, "set max files number error");
		return -1;
	}
	return 0;
#else
	struct rlimit max_files_rlimit;
	rlim_t soft_limit;
	rlim_t hard_limit;

	if (-1 == getrlimit(RLIMIT_NOFILE, &max_files_rlimit)) {
		logs(Logger::ERR, "getrlimit error(%s)", SystemApi::system_strerror());
		return -1;
	}

	soft_limit = max_files_rlimit.rlim_cur;
	hard_limit = max_files_rlimit.rlim_max;

	max_files_rlimit.rlim_cur = max_files_number;
	if (hard_limit < max_files_number) { /* raise the hard-limit too in case it is smaller than the soft-limit, otherwise we get a EINVAL */
		max_files_rlimit.rlim_max = max_files_number;
	}

	if (-1 == setrlimit(RLIMIT_NOFILE, &max_files_rlimit)) {
		logs(Logger::ERR, "setrlimit error(%s)", SystemApi::system_strerror());
		return -1;
	}

	return 0;
#endif
}

int SystemApi::system_socketpair(int family, int type, int protocol, int fd[2]) {
#ifdef linux
	return socketpair(family, type, protocol, fd);
#else
	if (protocol || family != AF_INET || family != AF_LOCAL) {
		logs(Logger::ERR, "protocol or family error");
		return -1;
	}
	int connector = -1;
	int acceptor = -1;
	int linster = -1;

	do{
		linster = socket(AF_INET, type, 0);
		if (linster < 0) {
			logs(Logger::ERR, "create socket error(%s)", SystemApi::system_strerror());
			break;
		}

		struct sockaddr_in linsten_addr;
		memset(&linsten_addr, 0, sizeof(linsten_addr));
		linsten_addr.sin_family = AF_INET;
		linsten_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		linsten_addr.sin_port = 0;

		if (bind(linster, (struct sockaddr*)&linsten_addr, sizeof(linsten_addr)) == -1) {
			logs(Logger::ERR, "bind address error(%s)", SystemApi::system_strerror());
			break;
		}

		if (listen(linster, 1) == -1) {
			logs(Logger::ERR, "listen error(%s)", SystemApi::system_strerror());
			break;
		}

		connector = socket(AF_INET, type, 0);
		if (connector < 0) {
			logs(Logger::ERR, "create connector socket error(%s)", SystemApi::system_strerror());
			break;
		}

		struct sockaddr_in connect_addr;
		int size = sizeof(connect_addr);
		if (getsockname(linster, (struct sockaddr*)&connect_addr, &size) == -1) {
			logs(Logger::ERR, "get socket name error(%s)", SystemApi::system_strerror());
			break;
		}
		if (size != sizeof(connect_addr)) {
			logs(Logger::ERR, "size error");
			break;
		}

		if (connect(connector, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) == -1) {
			logs(Logger::ERR, "connect error(%s)", SystemApi::system_strerror());
			break;
		}

		size = sizeof(linsten_addr);
		acceptor = accept(linster, (struct sockaddr*)&linsten_addr, &size);
		if (acceptor < 0) {
			logs(Logger::ERR, "accept error(%s)", SystemApi::system_strerror());
			break;
		}

		if (size != sizeof(linsten_addr)) {
			logs(Logger::ERR, "size error");
			break;
		}

		if (getsockname(connector, (struct sockaddr*)&connect_addr, &size) == -1) {
			logs(Logger::ERR, "get socket name error(%s)", SystemApi::system_strerror());
			break;
		}
		if (size != sizeof(connect_addr)
				|| linsten_addr.sin_family != connect_addr.sin_family
				|| linsten_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
				|| linsten_addr.sin_port != connect_addr.sin_port) {
			logs(Logger::ERR, "addr error");
			break;
		}
		close(linster);

		fd[0] = connector;
		fd[1] = acceptor;
		return 0;
	}while(0);

	if (linster != -1) {
		close(linster);
	}
	if (connector != -1) {
		close(connector);
	}
	if (acceptor != -1) {
		close(acceptor);
	}
	return -1;
#endif
}

void SystemApi::system_showNetworkInterfInfo() {
#ifdef _WIN32
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	int netCardNum = 0;
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel=GetAdaptersInfo(pIpAdapterInfo,&stSize);
	}
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{
			std::cout << "No:" << ++netCardNum << std::endl;
			std::cout << "device name:" << pIpAdapterInfo->AdapterName << std::endl;
			IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
			do
			{
				std::cout << "ip address: " << pIpAddrString->IpAddress.String << std::endl;
				pIpAddrString = pIpAddrString->Next;
			} while (pIpAddrString);
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	if (pIpAdapterInfo)
	{
		delete pIpAdapterInfo;
	}
#else
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[2048];
	struct  sockaddr_in my_addr;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		logs(Logger::ERR, "create socket error(%s)", SystemApi::system_strerror());
		return;
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
		close(sock);
		return;
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
	int count = 0;

	for(; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (!(ifr.ifr_flags & IFF_LOOPBACK)) {
			if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
			{
				logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
				close(sock);
				return;
			}
			char ipaddr[32] = {0};
			memcpy(&my_addr, &ifr.ifr_addr, sizeof(my_addr));
			strcpy(ipaddr, inet_ntoa(my_addr.sin_addr));
			std::cout << "No: " << (++count) << " deviceName: " << it->ifr_name
					<< " ipAddress:" << ipaddr << std::endl;
		}
	}
	close(sock);
#endif
}

std::string SystemApi::system_getIp(std::string device) {
#ifdef linux
	return SystemApi::system_getIpOnLinux(device);
#else
	return SystemApi::system_getIpOnWindows(device);
#endif
}

std::string SystemApi::system_getIpOnLinux(std::string device)
{
	std::string retIp;
#ifdef linux
	int sock_fd;
	struct  sockaddr_in my_addr;
	struct ifreq ifr;

	/**//* Get socket file descriptor */
	if ((sock_fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
	  logs(Logger::ERR, "create socket error(%s)", SystemApi::system_strerror());
	  return std::string();
	}

	/* Get IP Address */
	strncpy(ifr.ifr_name, device.c_str(), IF_NAMESIZE);
	ifr.ifr_name[IFNAMSIZ-1] = '\0';

	if (ioctl(sock_fd, SIOCGIFADDR, &ifr) < 0)
	{
		logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
		return std::string();
	}

	char ipaddr[32] = {0};
	memcpy(&my_addr, &ifr.ifr_addr, sizeof(my_addr));
	strcpy(ipaddr, inet_ntoa(my_addr.sin_addr));
	close(sock_fd);
	retIp = std::string(ipaddr);
#endif
	return retIp;
}

std::string SystemApi::system_getIpOnWindows(std::string device)
{
	std::string retIp;
#ifdef _WIN32
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);

	if (ERROR_BUFFER_OVERFLOW == nRel)//memory is too small.
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel=GetAdaptersInfo(pIpAdapterInfo,&stSize);
	}

	//get network interface info success.
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{
			std::string name = std::string(pIpAdapterInfo->AdapterName, strlen(pIpAdapterInfo->AdapterName));
			if (name == device) {
				IP_ADDR_STRING *pIpAddrString =&(pIpAdapterInfo->IpAddressList);
				if (pIpAddrString) {//use first ip
					retIp = std::string(pIpAddrString->IpAddress.String, strlen(pIpAddrString->IpAddress.String));
					break;//end search next ip.
				}
			}
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	if (pIpAdapterInfo)
		delete pIpAdapterInfo;
#endif
	return retIp;
}

std::string SystemApi::system_getDeivceName(std::string ip)
{
	std::string deviceName;
#ifdef _WIN32
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel=GetAdaptersInfo(pIpAdapterInfo,&stSize);
	}
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{

			IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
			do
			{
				std::string tip = std::string(pIpAddrString->IpAddress.String, strlen(pIpAddrString->IpAddress.String));
				if (tip == ip) {
					deviceName = std::string(pIpAdapterInfo->AdapterName, strlen(pIpAdapterInfo->AdapterName));
					break;
				}
				pIpAddrString = pIpAddrString->Next;
			} while (pIpAddrString);
			if (!deviceName.empty())
				break;
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	if (pIpAdapterInfo)
	{
		delete pIpAdapterInfo;
	}
#else
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[2048];
	struct  sockaddr_in my_addr;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		logs(Logger::ERR, "create socket error(%s)", SystemApi::system_strerror());
		return deviceName;
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
		close(sock);
		return deviceName;
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for(; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (!(ifr.ifr_flags & IFF_LOOPBACK)) {
			if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
			{
				logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
				close(sock);
				return deviceName;
			}
			char ipaddr[32] = {0};
			memcpy(&my_addr, &ifr.ifr_addr, sizeof(my_addr));
			strcpy(ipaddr, inet_ntoa(my_addr.sin_addr));
			std::string tip = std::string(ipaddr, strlen(ipaddr));
			if (tip == ip) {
				deviceName = std::string(it->ifr_name, strlen(it->ifr_name));
				break;
			}
		}
	}
	close(sock);
#endif
	return deviceName;
}

int SystemApi::system_getIpList(std::string device, std::list<std::string>& ipList)
{
#ifdef _WIN32
	PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
	unsigned long stSize = sizeof(IP_ADAPTER_INFO);
	int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
	if (ERROR_BUFFER_OVERFLOW == nRel)
	{
		delete pIpAdapterInfo;
		pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
		nRel=GetAdaptersInfo(pIpAdapterInfo,&stSize);
	}
	if (ERROR_SUCCESS == nRel)
	{
		while (pIpAdapterInfo)
		{
			std::string tdn = std::string(pIpAdapterInfo->AdapterName, strlen(pIpAdapterInfo->AdapterName));
			if (tdn == device) {
				IP_ADDR_STRING *pIpAddrString = &(pIpAdapterInfo->IpAddressList);
				do
				{
					ipList.push_back(std::string(pIpAddrString->IpAddress.String,
							strlen(pIpAddrString->IpAddress.String)));
					pIpAddrString = pIpAddrString->Next;
				} while (pIpAddrString);
				break;
			}
			pIpAdapterInfo = pIpAdapterInfo->Next;
		}
	}
	if (pIpAdapterInfo)
	{
		delete pIpAdapterInfo;
	}
	if (ERROR_SUCCESS != nRel) {
		logs(Logger::ERR, "get device name(%s) ip error(%s)", device.c_str(), SystemApi::system_strerror());
		return -1;
	}
#else
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[2048];
	struct  sockaddr_in my_addr;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		logs(Logger::ERR, "create socket error(%s)", SystemApi::system_strerror());
		return -1;
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
		close(sock);
		return -1;
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for(; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		std::string tdn = std::string(it->ifr_name, strlen(it->ifr_name));
		if (tdn == device) {
			if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
			{
				logs(Logger::ERR, "ioctl error(%s)", SystemApi::system_strerror());
				close(sock);
				return -1;
			}
			char ipaddr[32] = {0};
			memcpy(&my_addr, &ifr.ifr_addr, sizeof(my_addr));
			strcpy(ipaddr, inet_ntoa(my_addr.sin_addr));
			ipList.push_back(std::string(ipaddr, strlen(ipaddr)));
		}
	}
	close(sock);
#endif
	return 0;
}
