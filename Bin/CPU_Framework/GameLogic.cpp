#include "GameLogic.h"

DWORD WINAPI NetworkingThreadProc(LPVOID lpParam)
{
	StartSocket();
	WaitSocket(lpParam);
	StopSocket();
	return 0;
}

void StartSocket()
{
	WSADATA wsaData;
	if (WSAStartup(WINSOCK_VERSION, &wsaData)) {
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "StartSocket(): error %d occured!", error);
		MessageBoxA(NULL, "StartSocket() error!", NULL, MB_OK);
		ExitThread(-1);
	}
}

void WaitSocket(LPVOID lpParam)
{
	SOCKET serverSocket;
	CreateSocket(serverSocket);
	ConnectServer(serverSocket);
	GetInitInfo(serverSocket, lpParam);
	StartClientSession(serverSocket, lpParam);
	CloseSocket(serverSocket);
}

void StopSocket()
{
	if (WSACleanup())
	{
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "StopSocket(): error %d occured!", error);
		MessageBoxA(NULL, "StopSocket(): error occured!", NULL, MB_OK);
		ExitThread(-1);
	}
}

void CreateSocket(SOCKET& serverSocket)
{
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "CreateSocket(): error %d occured!", error);
		MessageBoxA(NULL, "CreateSocket(): error occured!", NULL, MB_OK);
		ExitThread(-1);
	}
}

void CloseSocket(SOCKET socket)
{
	closesocket(socket);
}

void ConnectServer(SOCKET socket)
{
	sockaddr_in serverSocketAddress;
	serverSocketAddress.sin_family = AF_INET;
	serverSocketAddress.sin_addr.S_un.S_addr = inet_addr(INET_ADDR);
	serverSocketAddress.sin_port = htons(PORT_ADDR);
	if (connect(socket, (SOCKADDR*)&serverSocketAddress, sizeof(serverSocketAddress)) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "ConnectServer(): error %d occured!", error);
		MessageBoxA(NULL, error_code, NULL, MB_OK);
		ExitThread(-1);
	}
}

void GetInitInfo(SOCKET socket, LPVOID lpParam)
{
	if (send(socket, init_request_buf, sizeof(init_request_buf), 0) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "GetInitInfo(): error %d occured!", error);
		MessageBoxA(NULL, error_code, NULL, MB_OK);
		ExitThread(-1);
	}

	char recvbuffer[MAX_RECV_BUFFER_SZ];
	int rbytes;
	if ((rbytes = recv(socket, recvbuffer, sizeof(recvbuffer), 0)) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		char error_code[MAX_ERROR_CODE_BUFFER_SZ];
		sprintf(error_code, "GetInitInfo(): error %d occured!", error);
		MessageBoxA(NULL, error_code, NULL, MB_OK);
		((GameLogic*)lpParam)->mPlayerCarIdx = 0;
		ExitThread(-1);
	}
	recvbuffer[rbytes] = '\0';
	sscanf_s(recvbuffer, "%d", &((GameLogic*)lpParam)->mPlayerCarIdx);
}

void StartClientSession(SOCKET socket, LPVOID lpParam)
{
	char recvbuffer[MAX_RECV_BUFFER_SZ];
	for (;;)
	{
		if (send(socket, on_update_buf, sizeof(on_update_buf), 0) == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			char error_code[MAX_ERROR_CODE_BUFFER_SZ];
			sprintf(error_code, "StartClientSession(): error %d occured!", error);
			MessageBoxA(NULL, error_code, NULL, MB_OK);
			ExitThread(-1);
		};
		///<Send other players cars' states>
		char carStateBuffer[MAX_CAR_STATE_BUFFER_SZ];
		sprintf(carStateBuffer, "%s", CarStateToString(((GameLogic*)lpParam)->ppCars[((GameLogic*)lpParam)->mPlayerCarIdx]->GetCarState()).c_str());
		if (send(socket, carStateBuffer, strlen(carStateBuffer), 0) == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			char error_code[MAX_ERROR_CODE_BUFFER_SZ];
			sprintf(error_code, "StartClientSession(): error %d occured!", error);
			MessageBoxA(NULL, error_code, NULL, MB_OK);
			ExitThread(-1);
		}
		///</Send other players cars' states>
		if (send(socket, update_request_buf, sizeof(update_request_buf), 0) == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			char error_code[MAX_ERROR_CODE_BUFFER_SZ];
			sprintf(error_code, "StartClientSession(): error %d occured!", error);
			MessageBoxA(NULL, error_code, NULL, MB_OK);
			ExitThread(-1);
		}
		///<Update other players cars' states>
		int rbytes;
		if ((rbytes = recv(socket, recvbuffer, sizeof(recvbuffer), 0)) == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			char error_code[MAX_ERROR_CODE_BUFFER_SZ];
			sprintf(error_code, "StartClientSession(): error %d occured!", error);
			MessageBoxA(NULL, error_code, NULL, MB_OK);
			ExitThread(-1);
		}
		recvbuffer[rbytes] = '\0';
		int CarStateStartIdx = 0;
		for (int i = 0; i < MaxPlayersNumber; ++i)
		{
			recvbuffer[CarStateStartIdx] = FirstSymbol[0];
			int j = CarStateStartIdx;
			for (; recvbuffer[j] != StopSymbol[0]; ++j)
				carStateBuffer[j - CarStateStartIdx] = recvbuffer[j];
			CarStateStartIdx = j + 1;
			carStateBuffer[j] = '\0';
			CarState loadedCarState = StringToCarState(carStateBuffer);
			if (i != ((GameLogic*)lpParam)->mPlayerCarIdx)
			{
				if (!((GameLogic*)lpParam)->mUpdating)
					((GameLogic*)lpParam)->ppCars[i]->SetCarState(loadedCarState);
			}
		}
		///</Update other players cars' states>
	}
}

std::string CarStateToString(CarState cs)
{
	XMMATRIX CarWorld = cs.mCarWorld;
	XMFLOAT4X4 CarWorldF4X4;
	XMStoreFloat4x4(&CarWorldF4X4, CarWorld);

	FLOAT SteeringAngle = cs.mSteeringAngle;

	INT SteeringDirection = cs.mSteeringDirection;

	FLOAT SpinAngle = cs.mSpinAngle;

	XMVECTOR Velocity = cs.mVelocity;
	XMFLOAT3 VelocityF3;
	XMStoreFloat3(&VelocityF3, Velocity);

	XMVECTOR CarOrientation = cs.mCarOrientation;
	XMFLOAT3 CarOrientationF3;
	XMStoreFloat3(&CarOrientationF3, CarOrientation);

	XMVECTOR Acceleration = cs.mAcceleration;
	XMFLOAT3 AccelerationF3;
	XMStoreFloat3(&AccelerationF3, Acceleration);

	INT AccelerationDirection = cs.mAccelerationDirection;

	std::string format = CarStateFormatString;
	format += "\n";
	char tmpbuf[MAX_CAR_STATE_BUFFER_SZ];
	sprintf(tmpbuf, format.c_str(),
		CarWorldF4X4._11, CarWorldF4X4._12, CarWorldF4X4._13, CarWorldF4X4._14,
		CarWorldF4X4._21, CarWorldF4X4._22, CarWorldF4X4._23, CarWorldF4X4._24,
		CarWorldF4X4._31, CarWorldF4X4._32, CarWorldF4X4._33, CarWorldF4X4._34,
		CarWorldF4X4._41, CarWorldF4X4._42, CarWorldF4X4._43, CarWorldF4X4._44,
		SteeringAngle,
		SteeringDirection,
		SpinAngle,
		VelocityF3.x, VelocityF3.y, VelocityF3.z,
		CarOrientationF3.x, CarOrientationF3.y, CarOrientationF3.z,
		AccelerationF3.x, AccelerationF3.y, AccelerationF3.z,
		AccelerationDirection);
	return tmpbuf;
}

CarState StringToCarState(std::string in)
{
	XMFLOAT4X4 CarWorldF4X4;
	FLOAT SteeringAngle;
	INT SteeringDirection;
	FLOAT SpinAngle;
	XMFLOAT3 VelocityF3;
	XMFLOAT3 CarOrientationF3;
	XMFLOAT3 AccelerationF3;
	INT AccelerationDirection;
	std::string format = FirstSymbol;
	format += CarStateFormatString;
	sscanf(in.data(), format.c_str(),
		&CarWorldF4X4._11, &CarWorldF4X4._12, &CarWorldF4X4._13, &CarWorldF4X4._14,
		&CarWorldF4X4._21, &CarWorldF4X4._22, &CarWorldF4X4._23, &CarWorldF4X4._24,
		&CarWorldF4X4._31, &CarWorldF4X4._32, &CarWorldF4X4._33, &CarWorldF4X4._34,
		&CarWorldF4X4._41, &CarWorldF4X4._42, &CarWorldF4X4._43, &CarWorldF4X4._44,
		&SteeringAngle,
		&SteeringDirection,
		&SpinAngle,
		&VelocityF3.x, &VelocityF3.y, &VelocityF3.z,
		&CarOrientationF3.x, &CarOrientationF3.y, &CarOrientationF3.z,
		&AccelerationF3.x, &AccelerationF3.y, &AccelerationF3.z,
		&AccelerationDirection);
	CarState cs;
	cs.mCarWorld = XMLoadFloat4x4(&CarWorldF4X4);
	cs.mSteeringAngle = SteeringAngle;
	cs.mSteeringDirection = SteeringDirection;
	cs.mSpinAngle = SpinAngle;
	cs.mCarOrientation = XMLoadFloat3(&CarOrientationF3);
	cs.mVelocity = XMLoadFloat3(&VelocityF3);
	cs.mAcceleration = XMLoadFloat3(&AccelerationF3);
	cs.mAccelerationDirection = AccelerationDirection;
	return cs;
}

GameLogic::GameLogic(MeshManager* pmeshmanager, std::map<BodyIndex, std::vector<MeshInstanceIndex>>&bodies, std::vector<MacroParams>&bodiesParams, ID3DX11EffectTechnique* activetech) : mCameraCarChaseFlag(false), mNetworkStarted(false), mUpdating(false), mPlayerCarIdx(0)
{
	XMMATRIX offset = XMMatrixIdentity();
	for (int i = 0; i < MaxPlayersNumber; ++i)
	{
		if (i % 2 == 0 && i != 0)
			offset *= AdditionalCarOffset;
		ppCars.push_back(new Car(pmeshmanager, bodies, bodiesParams, activetech, CarsFilenames[i % 2]));
		int carEndIdx = pmeshmanager->GetMeshCount() - 1;
		CarState lastCarState = ppCars[ppCars.size() - 1]->GetCarState();
		lastCarState.mCarWorld *= offset;
		ppCars[ppCars.size() - 1]->SetCarState(lastCarState);
	}
}

void GameLogic::Update(FLOAT dt, FLOAT aspectratio)
{
	mUpdating = true;
	if (!mNetworkStarted)
	{
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)(NetworkingThreadProc), this, NULL, NULL);
		mNetworkStarted = true;
	}
	for (auto car : ppCars)
		car->Update(dt, aspectratio);
	mUpdating = false;
}

Camera GameLogic::GetBumberChaseCamera()
{
	return ppCars[mPlayerCarIdx]->GetBumperChaseCamera();
}

void GameLogic::OnKeyPress()
{
	if (GetAsyncKeyState('Q') & 0x8000)
	{
		if (ppCars.size() >= mPlayerCarIdx + 1 && ppCars[mPlayerCarIdx])
		{
			ppCars[mPlayerCarIdx]->Steer(STEER_LEFT);
		}
	}
	if (GetAsyncKeyState('E') & 0x8000)
	{
		if (ppCars.size() >= mPlayerCarIdx + 1 && ppCars[mPlayerCarIdx])
		{
			ppCars[mPlayerCarIdx]->Steer(STEER_RIGHT);
		}
	}

	if (GetAsyncKeyState('G') & 0x8000)
	{
		if (ppCars.size() >= mPlayerCarIdx + 1 && ppCars[mPlayerCarIdx])
		{
			ppCars[mPlayerCarIdx]->Accelerate(ACCELERATE_FRONT);
		}
	}
	if (GetAsyncKeyState('B') & 0x8000)
	{
		if (ppCars.size() >= mPlayerCarIdx + 1 && ppCars[mPlayerCarIdx])
		{
			ppCars[mPlayerCarIdx]->Accelerate(ACCELERATE_BACK);
		}
	}
}