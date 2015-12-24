// EchoServer.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "NetworkService.h"

static int bExit = 0;

int AppExit()
{
	return bExit;
}

int main()
{
	NetworkService network;
	network.CreateServer("127.0.0.1", 9999);

	while (true)
		Sleep(100);

	bExit = 1;

	return 0;
}

