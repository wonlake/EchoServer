//
// Copyright (C) Mei Jun 2011
//
#include "stdafx.h"
#include "NetworkService.h"

#pragma comment( lib, "MSWSock.lib" )
#pragma comment( lib, "WS2_32.lib" )

NetworkService::NetworkService( BYTE minorVer, BYTE majorVer )
{
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(minorVer, majorVer);
	if( WSAStartup(sockVersion,&wsaData) != 0 )
	{
		ExitProcess( -1 );
	}

	m_hIOCP		   = NULL;
	m_bServer	   = TRUE;
	m_bEstablished = FALSE;
	m_sServer	   = NULL;
}


NetworkService::~NetworkService(void)
{
	WSACleanup();
}

BOOL NetworkService::Start( const char* addr, short port, BOOL bServer  )
{ 
	if( bServer )
		return CreateServer( addr, port );
	else
		return CreateClient( addr, port );
}

BOOL NetworkService::CreateServer( const char* addr, short port )
{
	m_hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, NULL, 0 );
	if( m_hIOCP == NULL )
	{
		_tprintf( _T("完成端口创建失败 %s %d\n"), __FILE__, __LINE__ );
		return FALSE;
	}

	SOCKET sListen;
	sListen = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );

	SOCKADDR_IN si;
	si.sin_family	   = AF_INET;
	si.sin_port		   = htons(port);
	si.sin_addr.s_addr = inet_addr( addr );
	if( bind(sListen, (sockaddr*)&si, sizeof(si) ) != 0 )
	{
		_tprintf( _T("端口绑定失败 %s %d\n"), __FILE__, __LINE__ );
		closesocket( sListen );
		return FALSE;
	}
	listen( sListen, 2 );

	HANDLE_DATA* pListen = new HANDLE_DATA; 
	ZeroMemory( pListen, sizeof(*pListen) );
	pListen->s = sListen;
	CreateIoCompletionPort( (HANDLE)pListen->s, m_hIOCP,
		(ULONG_PTR)pListen, 0 );

	PostAccept( sListen, GetIOData() );

	DWORD dwThreadId = 0;
	CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)ThreadServerWorker,
		(LPVOID)this, 0, &dwThreadId );
	return TRUE;
}

BOOL NetworkService::CreateClient( const char* addr, short port )
{
	m_hIOCP = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, NULL, 0 );
	if( m_hIOCP == NULL )
	{
		_tprintf( _T("完成端口创建失败 %s %d\n"), __FILE__, __LINE__ );
		return FALSE;
	}

	SOCKET sServer;
	sServer = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );

	SOCKADDR_IN si;
	si.sin_family	   = AF_INET;
	si.sin_port		   = htons( port );
	si.sin_addr.s_addr = inet_addr( addr );

	if( connect(sServer, (sockaddr*)&si, sizeof(si) ) != 0 )
	{
		_tprintf( _T("连接服务器失败 %s %d\n"), __FILE__, __LINE__ );
		closesocket( sServer );
		return FALSE;
	}

	m_bEstablished = TRUE;
	m_sServer	   = sServer;

	HANDLE_DATA* pServer = new HANDLE_DATA; 
	ZeroMemory( pServer, sizeof(*pServer) );
	pServer->s = sServer;
	CreateIoCompletionPort( (HANDLE)pServer->s, m_hIOCP,
		(ULONG_PTR)pServer, 0 );

	PostRecv( pServer->s, GetIOData() );
	DWORD dwThreadId = 0;
	CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)ThreadClientWorker,
		(LPVOID)this, 0, &dwThreadId );

	return TRUE;
}

VOID NetworkService::PostAccept( SOCKET s, IO_DATA* pData )
{
	pData->s = socket( AF_INET, SOCK_STREAM, 0 );
	pData->op = OP_ACCEPT;
	AcceptEx( s, pData->s, pData->BufPackage, 0, 
		sizeof(sockaddr) + 16, sizeof(sockaddr) + 16, 
		&pData->dwBufSize, &pData->ol );
}

VOID NetworkService::PostSend( SOCKET s, IO_DATA* pData, 
	CHAR* pDataBuf, DWORD dwDataSize )
{
	WSABUF WSABuf;
	WSABuf.buf	= pDataBuf;
	WSABuf.len	= dwDataSize;
	pData->op	= OP_WRITE;
	WSASend( s, &WSABuf, 1, &pData->dwBufSize, pData->dwFlags, &pData->ol, NULL );
}

VOID NetworkService::PostRecv( SOCKET s, IO_DATA* pData )
{
	WSABUF WSABuf;
	WSABuf.buf	= (CHAR*)pData->BufPackage;
	WSABuf.len	= sizeof(pData->BufPackage);
	pData->op	= OP_READ;
	WSARecv( s, &WSABuf, 1, &pData->dwBufSize, &pData->dwFlags, &pData->ol, NULL );
}

IO_DATA* NetworkService::GetIOData()
{
	IO_DATA* pData = new IO_DATA;
	ZeroMemory( pData, sizeof(*pData) );
	return pData;
}

VOID NetworkService::ReleaseIOData( IO_DATA* pData )
{
	delete pData;
}

extern BOOL AppExit();

VOID WINAPI NetworkService::ThreadServerWorker( LPVOID lParam )
{
	NetworkService* pNetworkService = (NetworkService*)lParam;
	while( !AppExit() )
	{
		HANDLE_DATA* pHandle = NULL;
		IO_DATA*	 pData	 = NULL;
		DWORD dwSizeTransfer = 0;

		int ret = GetQueuedCompletionStatus( pNetworkService->m_hIOCP, &dwSizeTransfer,
			(PULONG_PTR)&pHandle, (LPOVERLAPPED*)&pData, INFINITE );

		if( ret == 0 )
		{
			closesocket( pHandle->s );
			delete pHandle;
			ReleaseIOData( pData );
			continue;
		}
		else if( dwSizeTransfer == 0 && 
			(pData->op == OP_READ ||
			 pData->op == OP_WRITE) )
		{
			closesocket( pHandle->s );
			delete pHandle;
			continue;
		}
		switch( pData->op )
		{
		case OP_ACCEPT:
			{
				HANDLE_DATA* pNewHandle = new HANDLE_DATA;
				ZeroMemory( pNewHandle, sizeof(*pNewHandle) );
				pNewHandle->s = pData->s;
				CreateIoCompletionPort( (HANDLE)pNewHandle->s, pNetworkService->m_hIOCP, 
					(ULONG_PTR)pNewHandle, 0 );
				ReleaseIOData( pData );
				pNetworkService->PostRecv( pNewHandle->s, GetIOData() );
				++pNewHandle->dwIORecv;
				pNetworkService->PostAccept( pHandle->s, GetIOData() );
				break;
			}
		case OP_READ:
			{
				--pHandle->dwIORecv;
				memcpy( pHandle->BufRecv, 
					pData->BufPackage, dwSizeTransfer );
				pHandle->dwSizeRecv = dwSizeTransfer;
				pHandle->BufRecv[pHandle->dwSizeRecv] = 0;
				ReleaseIOData( pData );
				printf( "收到客户程序信息:\n%s", (LPCSTR)pHandle->BufRecv );
				pNetworkService->PostRecv( pHandle->s, GetIOData() );
				pNetworkService->PostSend( pHandle->s, GetIOData(), (LPSTR)pHandle->BufRecv, pHandle->dwSizeRecv );

				break;
			}
		case OP_WRITE:
			{
				ReleaseIOData( pData );
				break;
			}
		case OP_CLOSE:
			{
				break;
			}
		}
	}
}

VOID WINAPI NetworkService::ThreadClientWorker( LPVOID lParam )
{
	NetworkService* pNetworkService = (NetworkService*)lParam;
	while( !AppExit() )
	{
		HANDLE_DATA* pHandle = NULL;
		IO_DATA*	 pData	 = NULL;
		DWORD dwSizeTransfer = 0;

		int ret = GetQueuedCompletionStatus( pNetworkService->m_hIOCP, &dwSizeTransfer,
			(PULONG_PTR)&pHandle, (LPOVERLAPPED*)&pData, INFINITE );

		if( ret == 0 )
		{
			closesocket( pHandle->s );
			delete pHandle;
			ReleaseIOData( pData );
			continue;
		}
		else if( dwSizeTransfer == 0 && 
			(pData->op == OP_READ ||
			pData->op == OP_WRITE) )
		{
			closesocket( pHandle->s );
			delete pHandle;
			continue;
		}
		switch( pData->op )
		{
		case OP_READ:
			{
				break;
			}
		case OP_WRITE:
			{
				break;
			}
		case OP_CLOSE:
			{
				break;
			}
		}
	}
	pNetworkService->m_bEstablished = FALSE;
}