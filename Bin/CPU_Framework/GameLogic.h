#ifndef _GAME_LOGIC_H_
#define _GAME_LOGIC_H_

#include "../Common/d3dUtil.h"
#include "../Bin/GPU_Framework/MeshManager.h"
#include "../Bin/GPU_Framework/Camera.h"
#include "../Bin/GPU_Framework/GPUCollision.h"
#include "Car.h"
#include "Track.h"
#include <winsock2.h>
#include <mswsock.h>
#include <io.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_RECV_BUFFER_SZ 1024
#define MAX_CAR_STATE_BUFFER_SZ 1024
#define MAX_ERROR_CODE_BUFFER_SZ 64

const int PORT_ADDR = 1234;
const char INET_ADDR[] = { "192.168.1.130" };//83.217.12.37

const char on_update_buf[] = { "ON_UPDATE\n" };
const char init_request_buf[] = { "INIT_INFO_REQUEST\n" };
const char update_request_buf[] = { "UPDATE_INFO_REQUEST\n" };
const char CarStateFormatString[] = {"{{%f,%f,%f,%f},{%f,%f,%f,%f},{%f,%f,%f,%f},{%f,%f,%f,%f}},{%f},{%d},{%f},{%f,%f,%f},{%f,%f,%f},{%f,%f,%f},{%d}"};
const char StopSymbol[] = "*";
const char FirstSymbol[] = "s"; 

//Initial cars' states
static const int MaxPlayersNumber = 1;
static const std::string CarsFilenames[] = {"Models\\nissan_silvia_s15.carcfg", "Models\\toyota_soarer.carcfg" };
static const XMMATRIX AdditionalCarOffset = XMMatrixTranslation(-40.0f, 0.0f, 0.0f);

DWORD WINAPI NetworkingThreadProc(LPVOID lpParam);
void StartSocket();
void StopSocket();
void WaitSocket(LPVOID lpParam);
void CreateSocket(SOCKET&);
void CloseSocket(SOCKET);
void ConnectServer(SOCKET);
void GetInitInfo(SOCKET, LPVOID lpParam);
void StartClientSession(SOCKET, LPVOID lpParam);
std::string CarStateToString(CarState);
CarState StringToCarState(std::string);

class GameLogic
{
public:
	std::vector<Car*>ppCars;
	Track* pTrack;

	BOOL mCameraCarChaseFlag;
	BOOL mNetworkStarted;
	BOOL mUpdating;
	UINT mPlayerCarIdx;
public:
	GameLogic::GameLogic(MeshManager* pmeshmanager, std::map<BodyIndex, std::vector<MeshInstanceIndex>>&bodies, std::vector<MacroParams>&bodiesParams, ID3DX11EffectTechnique* activetech);
	//User interaction
	void GameLogic::Update(FLOAT dt, FLOAT aspectratio);
	void GameLogic::OnKeyPress();
	void GameLogic::SetCameraCarChaseFlag(BOOL csflag) { InterlockedExchange(reinterpret_cast<LONG*>(&mCameraCarChaseFlag), csflag); };
	BOOL GameLogic::GetCameraCarChaseFlag() { return mCameraCarChaseFlag; };
	Camera GameLogic::GetBumberChaseCamera();
};

#endif
