//
// Copyright (C) Mei Jun 2011
//

#pragma once

#define MAX_BUFFER_SIZE	 4096
#define MAX_PACKAGE_SIZE 1024

struct HANDLE_DATA
{
	SOCKET s;
	DWORD  dwIOSend;
	DWORD  dwIORecv;
	DWORD  dwSizeSend;
	DWORD  dwSizeRecv;
	BYTE   BufSend[MAX_BUFFER_SIZE];
	BYTE   BufRecv[MAX_BUFFER_SIZE];

};

struct IO_DATA
{
#define OP_ACCEPT	1
#define	OP_READ		2
#define OP_WRITE	3
#define OP_CLOSE	4
	OVERLAPPED ol;
	SOCKET	   s;
	DWORD	   dwFlags;
	DWORD	   op;
	DWORD	   dwBufSize;
	BYTE	   BufPackage[MAX_PACKAGE_SIZE];
};

class NetworkService
{
public:
	HANDLE		m_hIOCP;
	BOOL		m_bServer;
	BOOL		m_bEstablished;
	SOCKET		m_sServer;
public:
	NetworkService( BYTE minorVer = 2, BYTE majorVer = 2 );
	~NetworkService(void);

public:

	BOOL Start( const char* addr, short port, BOOL bServer = TRUE );

	static VOID WINAPI ThreadServerWorker( LPVOID lParam );

	static VOID WINAPI ThreadClientWorker( LPVOID lParam );

	static IO_DATA* GetIOData();

	static VOID ReleaseIOData( IO_DATA* pData );
 
	BOOL CreateServer( const char* addr, short port );

	BOOL CreateClient( const char* addr, short port );

	VOID PostAccept( SOCKET s, IO_DATA* pData );

	VOID PostSend( SOCKET s, IO_DATA* pData, 
		CHAR* pDataBuf, DWORD dwDataSize );

	VOID PostRecv( SOCKET s, IO_DATA* pData );
};

