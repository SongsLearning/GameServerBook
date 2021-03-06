// tcp_many_client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <memory>
#include <iostream>

#include "../ImaysNet/ImaysNet.h"

using namespace std;

const int ClientNum = 10;

int main()
{
	// 이 프로그램은 여러 스레드가 CPU 자원을 잔뜩 먹을 것이다. 이 때문에 컴퓨터가 먹통이 될 수도 있다.
	// 따라서 프로세스 우선순위를 낮추어서 실험을 불편하지 않게 해준다.
	// 실무 개발에서는 이런 것을 하지는 않는다.
	SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

	using namespace std::chrono_literals;

	// 많은 스레드를 둡니다. 각 스레드는 TCP 연결 하나를 맡습니다.
	// TCP 연결 하나는 서버에 접속 후 데이터를 지속적으로 보냅니다.
	// 서버는 받은 데이터를 그대로 클라이언트에게 보냅니다. 클라이언트는 그것을 수신합니다.
	// 이 프로그램은 모든 클라이언트가 수신한 총량을 출력합니다.

	recursive_mutex mutex; // 아래 변수들을 보호한다.
	vector<shared_ptr<thread>> threads;
	int64_t totalReceivedBytes = 0;
	int connectedClientCount = 0;

	for (int i = 0; i < ClientNum; i++)
	{
		// TCP 연결을 하고 송수신을 하는 스레드를 생성한다. 무한 루프를 돈다.
		shared_ptr<thread> th = make_shared<thread>([&connectedClientCount, &mutex, &totalReceivedBytes]()
		{
			try
			{
				Socket tcpSocket(SocketType::Tcp);
				tcpSocket.Bind(Endpoint::Any);
				tcpSocket.Connect(Endpoint("127.0.0.1", 5555));				
				
				{
					lock_guard<recursive_mutex> lock(mutex);
					connectedClientCount++;
				}

				string receivedData;

				while (true)
				{
					const char* dataToSend = "hello world.";
					// 원칙대로라면 TCP 스트림 특성상 일부만 송신하고 리턴하는 경우도 고려해야 하나,
					// 지금은 독자의 이해가 우선이므로, 생략하도록 한다.
					tcpSocket.Send(dataToSend, strlen(dataToSend) + 1);
					int receiveLength = tcpSocket.Receive();
					if (receiveLength <= 0)
					{
						// 소켓 연결에 문제가 생겼습니다. 루프를 끝냅니다.
						break;
					}
					lock_guard<recursive_mutex> lock(mutex);
					totalReceivedBytes += receiveLength;
				}
			}
			catch (Exception& e)
			{
				lock_guard<recursive_mutex> lock(mutex);
				cout << "A TCP socket work failed: " << e.what() << endl;
			}			

			lock_guard<recursive_mutex> lock(mutex);
			connectedClientCount--;
		});

		lock_guard<recursive_mutex> lock(mutex);
		threads.push_back(th);
	}

	// 메인 스레드는 매 초마다 총 송수신량을 출력한다.
	while (true)
	{
		{
			lock_guard<recursive_mutex> lock(mutex);
			cout << "Total echoed bytes: " << (uint64_t)totalReceivedBytes << ", thread count: " << connectedClientCount << endl;
		}
		this_thread::sleep_for(2s);
	}


    return 0;
}

