#ifndef _CAR_H_
#define _CAR_H_

#include "../Common/d3dUtil.h"
#include "../Bin/GPU_Framework/MeshManager.h"
#include "../Bin/GPU_Framework/Camera.h"
#include "..\Bin\GPU_Framework\GPUCollision.h"

#define EPSILON 0.0000001f

#define STEER_LEFT 1
#define STEER_RIGHT -1

#define ACCELERATE_FRONT -1
#define ACCELERATE_BACK 1

#define FRONT_LEFT 0
#define FRONT_RIGHT 1
#define REAR_LEFT 2
#define REAR_RIGHT 3

static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

struct CarState
{
	//Position
	XMMATRIX mCarWorld;
	//Front wheels steering angle
	FLOAT mSteeringAngle;
	INT mSteeringDirection;
	//Wheels spin angle
	FLOAT mSpinAngle;
	//Velocity
	XMVECTOR mVelocity;
	XMVECTOR mCarOrientation;
	//Acceleration
	XMVECTOR mAcceleration;
	INT mAccelerationDirection;
};

class Car
{
private:
	//Car parameters
	FLOAT mSteeringDelta;
	FLOAT mMaxSteeringAngle;
	FLOAT mWheelsRadius;
	FLOAT mCarLength;
	FLOAT mAccFrontMagnitude;
	FLOAT mAccBackMagnitude;
	XMFLOAT3 mScale;
	XMFLOAT3 mTranslation;
	XMFLOAT3 mRotation;
	XMFLOAT3 mFrontLeftWheelOffset;
	XMFLOAT3 mRearLeftWheelOffset;
	XMFLOAT3 mBumperChaseCameraPosition;
	//Car meshes names
	std::map<std::string, FLOAT>mNumericParamters;
	std::map<std::string, std::vector<std::string>>mMeshesFilenames;
private:
	std::string mCarCfgFilename;
	//Car meshes
	MeshManager* pMeshManager;
	std::vector<int>mBodyMeshesIndices;
	std::vector<int>mBrakesMeshesIndices[2];
	std::vector<int>mWheelsMeshesIndices[4];

	int mBoundingVolumeStartIdx;
	int mBoundingVolumeEndIdx;
	BoundingBox mCarAABB;
	//Position
	XMMATRIX mCarWorld;
	//Front wheels steering angle
	FLOAT mSteeringAngle;
	INT mSteeringDirection;
	//Wheels spin angle
	FLOAT mSpinAngle;
	//Velocity
	XMVECTOR mVelocity;
	XMVECTOR mCarOrientation;
	//Acceleration
	XMVECTOR mAcceleration;
	INT mAccelerationDirection;
	//Camera
	Camera* mBumperChaseCamera;
private:
	bool Car::ReadConfigurationFile(std::string cfgfilename);
	void Car::UpdateBumperChaseCamera(FLOAT aspectratio);
public:
	Car::Car(MeshManager* pmeshmanager, std::map<BodyIndex, std::vector<MeshInstanceIndex>>&bodies, std::vector<MacroParams>&bodiesParams, ID3DX11EffectTechnique* drawtech, std::string cfgfilename);
	
	void Car::Update(FLOAT dt, FLOAT aspectratio);

	void Car::Steer(INT dir) { mSteeringDirection = dir; };
	void Car::Accelerate(INT dir) { mAccelerationDirection = dir; };

	CarState Car::GetCarState() const;
	void Car::SetCarState(CarState& cs);

	Camera Car::GetBumperChaseCamera() const { return *mBumperChaseCamera; };
};

#endif
