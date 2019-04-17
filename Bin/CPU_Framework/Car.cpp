#include "Car.h"

Car::Car(MeshManager* pmeshmanager, std::map<BodyIndex, std::vector<MeshInstanceIndex>>&bodies, std::vector<MacroParams>&bodiesParams, ID3DX11EffectTechnique* drawtech, std::string cfgfilename):
mSteeringAngle(0),
mSteeringDirection(0),
mSpinAngle(0.0f),
mCarWorld(XMMatrixIdentity()),
mVelocity(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
mCarOrientation(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f)),
mAcceleration(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f)),
mAccelerationDirection(0),
mCarCfgFilename(cfgfilename)
{
	pMeshManager = pmeshmanager;

	mBumperChaseCamera = new Camera();

	if(!ReadConfigurationFile(cfgfilename))
		return;
	else
	{
		///<init car parameters>
		mSteeringDelta = mNumericParamters.at("STEERING_DELTA");
		mMaxSteeringAngle = mNumericParamters.at("MAX_STEERING_ANGLE");
		mWheelsRadius = mNumericParamters.at("WHEELS_RADIUS");
		mCarLength = mNumericParamters.at("CAR_LENGTH");
		
		mAccFrontMagnitude = mNumericParamters.at("ACC_FRONT_MAGNITUDE");
		mAccBackMagnitude = mNumericParamters.at("ACC_BACK_MAGNITUDE");
		
		mScale.x = mNumericParamters.at("SCALE_X");
		mScale.y = mNumericParamters.at("SCALE_Y");
		mScale.z = mNumericParamters.at("SCALE_Z");

		mTranslation.x = mNumericParamters.at("TRANSLATION_X");
		mTranslation.y = mNumericParamters.at("TRANSLATION_Y");
		mTranslation.z = mNumericParamters.at("TRANSLATION_Z");

		mRotation.x = mNumericParamters.at("ROTATION_X");
		mRotation.y = mNumericParamters.at("ROTATION_Y");
		mRotation.z = mNumericParamters.at("ROTATION_Z");

		mFrontLeftWheelOffset.x = mNumericParamters.at("FRONT_LEFT_WHEEL_OFFSET_X");
		mFrontLeftWheelOffset.y = mNumericParamters.at("FRONT_LEFT_WHEEL_OFFSET_Y");
		mFrontLeftWheelOffset.z = mNumericParamters.at("FRONT_LEFT_WHEEL_OFFSET_Z");

		mRearLeftWheelOffset.x = mNumericParamters.at("REAR_LEFT_WHEEL_OFFSET_X");
		mRearLeftWheelOffset.y = mNumericParamters.at("REAR_LEFT_WHEEL_OFFSET_Y");
		mRearLeftWheelOffset.z = mNumericParamters.at("REAR_LEFT_WHEEL_OFFSET_Z");

		mBumperChaseCameraPosition.x = mNumericParamters.at("BUMPER_CHASE_CAMERA_POS_X");
		mBumperChaseCameraPosition.y = mNumericParamters.at("BUMPER_CHASE_CAMERA_POS_Y");
		mBumperChaseCameraPosition.z = mNumericParamters.at("BUMPER_CHASE_CAMERA_POS_Z");

		///</init car parameters>

		XMMATRIX R = XMMatrixRotationX(mRotation.x) * XMMatrixRotationY(mRotation.y) * XMMatrixRotationZ(mRotation.z);
		XMMATRIX S = XMMatrixScaling(mScale.x, mScale.y, mScale.z);
		XMMATRIX T = XMMatrixTranslation(mTranslation.x, mTranslation.y, mTranslation.z);
		mCarWorld = S * R * T;
		
		mVelocity = XMVector3Transform(mVelocity, R);
		mAcceleration = XMVector3Transform(mAcceleration, R);
		mCarOrientation = XMVector3Normalize(XMVector3Transform(mCarOrientation, R));

		std::vector<std::wstring>nullvec(0);
		///<load car>
		///<load body parts>
		int bodyStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("NOT_TRANSPARENT_BODY_PARTS_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &mCarWorld, drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		for (auto fname : mMeshesFilenames.at("TRANSPARENT_BODY_PARTS_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &mCarWorld, drawtech, fname,
				fname, true, true, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int bodyEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = bodyStartIdx; i <= bodyEndIdx; ++i)
		{
			mBodyMeshesIndices.push_back(i);
		}
		///</load body parts>
		///<load wheels>
		///<load front wheels>
		FLOAT maxRadius;
		UINT lfWheelBsIdx;
		XMMATRIX frontLeftWheelOffset = XMMatrixTranslation(mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
		int leftFrontWheelStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("LEFT_FRONT_WHEEL_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationX(mSpinAngle) * XMMatrixRotationY(mSteeringAngle) * frontLeftWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int leftFrontWheelEndIdx = pMeshManager->GetMeshCount() - 1;
		maxRadius = 0.0f;
		lfWheelBsIdx = 0;
		for (int i = leftFrontWheelStartIdx; i <= leftFrontWheelEndIdx; ++i) {
			mWheelsMeshesIndices[FRONT_LEFT].push_back(i);
			if (pMeshManager->GetMeshBS(i).Radius > maxRadius)
			{
				maxRadius = pMeshManager->GetMeshBS(i).Radius;
				lfWheelBsIdx = i;
			}
		}
		int leftFrontBrakeStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("LEFT_FRONT_BRAKE_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationY(mSteeringAngle) * frontLeftWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int leftFrontBrakeEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = leftFrontBrakeStartIdx; i <= leftFrontBrakeEndIdx; ++i)
			mBrakesMeshesIndices[FRONT_LEFT].push_back(i);

		XMMATRIX frontRightWheelOffset = XMMatrixRotationY(XM_PI) * XMMatrixTranslation(-mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
		int rightFrontWheelStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("RIGHT_FRONT_WHEEL_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationX(-mSpinAngle) * XMMatrixRotationY(mSteeringAngle) * frontRightWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int rightFrontWheelEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = rightFrontWheelStartIdx; i <= rightFrontWheelEndIdx; ++i)
			mWheelsMeshesIndices[FRONT_RIGHT].push_back(i);

		int rightFrontBrakeStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("RIGHT_FRONT_BRAKE_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationY(mSteeringAngle) * XMMatrixRotationY(XM_PI) * frontRightWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int rightFrontBrakeEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = rightFrontBrakeStartIdx; i <= rightFrontBrakeEndIdx; ++i)
			mBrakesMeshesIndices[FRONT_RIGHT].push_back(i);
		///</load front wheels>
		///<load rear wheels>
		XMMATRIX rearLeftWheelOffset = XMMatrixTranslation(mRearLeftWheelOffset.x, mRearLeftWheelOffset.y, mRearLeftWheelOffset.z);
		int leftRearWheelStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("LEFT_REAR_WHEEL_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationX(mSpinAngle) * rearLeftWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int leftRearWheelEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = leftRearWheelStartIdx; i <= leftRearWheelEndIdx; ++i)
			mWheelsMeshesIndices[REAR_LEFT].push_back(i);

		XMMATRIX rearRightWheelOffset = XMMatrixRotationY(XM_PI) * XMMatrixTranslation(-mRearLeftWheelOffset.x, mRearLeftWheelOffset.y, mRearLeftWheelOffset.z);
		int rightRearWheelStartIdx = pMeshManager->GetMeshCount();
		for (auto fname : mMeshesFilenames.at("RIGHT_REAR_WHEEL_FBX_FILENAMES"))
		{
			pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &(XMMatrixRotationX(-mSpinAngle) * rearRightWheelOffset * mCarWorld), drawtech, fname,
				fname, true, false, 1, ReflectionType::REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, RefractionType::REFRACTION_TYPE_NONE);
		}
		int rightRearWheelEndIdx = pMeshManager->GetMeshCount() - 1;
		for (int i = rightRearWheelStartIdx; i <= rightRearWheelEndIdx; ++i)
			mWheelsMeshesIndices[REAR_RIGHT].push_back(i);
		
		///</load rear wheels>
		///</load wheels>
		///</load car>
		std::vector<XMFLOAT3>ptsf3;
		//body bounding boxes
		for (int i = bodyStartIdx; i <= bodyEndIdx; ++i)
		{
			XMVECTOR c = XMVectorSet(pMeshManager->GetMeshAABB(i).Center.x, pMeshManager->GetMeshAABB(i).Center.y, pMeshManager->GetMeshAABB(i).Center.z, 0.0f);
			XMVECTOR pts[8];
			pts[0] = c + XMVectorSet(pMeshManager->GetMeshAABB(i).Extents.x, pMeshManager->GetMeshAABB(i).Extents.y, pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[1] = c + XMVectorSet(-pMeshManager->GetMeshAABB(i).Extents.x, pMeshManager->GetMeshAABB(i).Extents.y, pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[2] = c + XMVectorSet(pMeshManager->GetMeshAABB(i).Extents.x, -pMeshManager->GetMeshAABB(i).Extents.y, pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[3] = c + XMVectorSet(-pMeshManager->GetMeshAABB(i).Extents.x, -pMeshManager->GetMeshAABB(i).Extents.y, pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[4] = c + XMVectorSet(pMeshManager->GetMeshAABB(i).Extents.x, pMeshManager->GetMeshAABB(i).Extents.y, -pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[5] = c + XMVectorSet(-pMeshManager->GetMeshAABB(i).Extents.x, pMeshManager->GetMeshAABB(i).Extents.y, -pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[6] = c + XMVectorSet(pMeshManager->GetMeshAABB(i).Extents.x, -pMeshManager->GetMeshAABB(i).Extents.y, -pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);
			pts[7] = c + XMVectorSet(-pMeshManager->GetMeshAABB(i).Extents.x, -pMeshManager->GetMeshAABB(i).Extents.y, -pMeshManager->GetMeshAABB(i).Extents.z, 0.0f);

			XMMATRIX world = pMeshManager->GetMeshWorldMatrixXM(i, 0) * XMMatrixInverse(&XMMatrixDeterminant(mCarWorld), mCarWorld);
			for (int i = 0; i < 8; ++i)
			{
				pts[i] = XMVector3TransformCoord(pts[i], world);
				XMFLOAT3 f3;
				XMStoreFloat3(&f3, pts[i]);
				ptsf3.push_back(f3);
			}
		}
		BoundingBox::CreateFromPoints(mCarAABB, ptsf3.size(), ptsf3.data(), sizeof(XMFLOAT3));
		
		std::vector<MeshInstanceIndex>null(0);
		int bodyIdx = bodies.size();
		bodies.insert({ bodyIdx, null });
	
		mBoundingVolumeStartIdx = pMeshManager->GetMeshCount();
		
		pMeshManager->CreateBox(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, DrawType::INDEXED_DRAW, &I, &(XMMatrixTranslation(mCarAABB.Center.x, mCarAABB.Center.y, mCarAABB.Center.z) * mCarWorld), drawtech, nullvec, nullvec, 0, false, true, false, true, false, false, 1, ReflectionType::REFLECTION_TYPE_NONE,
			RefractionType::REFRACTION_TYPE_NONE, Materials::Glass_BAF10_optical_glass, mCarAABB.Extents.x * 2, mCarAABB.Extents.y * 2, mCarAABB.Extents.z * 2);
		bodies[bodyIdx].push_back({ pMeshManager->GetMeshCount() - 1, 0 });

		pMeshManager->CreateSphere(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, DrawType::INDEXED_DRAW, &I, &(XMMatrixTranslation(pMeshManager->GetMeshBS(lfWheelBsIdx).Center.x, pMeshManager->GetMeshBS(lfWheelBsIdx).Center.y, pMeshManager->GetMeshBS(lfWheelBsIdx).Center.z) * pMeshManager->GetMeshWorldMatrixXM(lfWheelBsIdx, 0)), drawtech, nullvec, nullvec, 0, false, true, false, true, false, false, 1, ReflectionType::REFLECTION_TYPE_NONE,
			RefractionType::REFRACTION_TYPE_NONE, Materials::Glass_BAF10_optical_glass, pMeshManager->GetMeshBS(lfWheelBsIdx).Radius, 16, 16);
		bodies[bodyIdx].push_back({ pMeshManager->GetMeshCount() - 1, 0 });

		mBoundingVolumeEndIdx = pMeshManager->GetMeshCount() - 1;
		
		XMVECTOR scale, rotQuat, trans;
		XMFLOAT4 Quaternion;
		XMMatrixDecompose(&scale, &rotQuat, &trans, pMeshManager->GetMeshWorldMatrixXM(bodies[bodyIdx][0].first, 0));
		XMStoreFloat4(&Quaternion, rotQuat);

		XMVECTOR c = XMVector3TransformCoord(XMVectorSet(mCarAABB.Center.x, 0.5f * mCarAABB.Center.y, mCarAABB.Center.z, 0.0f), mCarWorld);
		XMFLOAT3 cf3;
		XMStoreFloat3(&cf3, c);
		bodiesParams.push_back({ 0.1f, 20.0f, cf3, XMFLOAT3(0.0f, 0.0f, 0.0f) , XMFLOAT3(0.0f, 0.0f, 0.0f) , Quaternion });
	}
}

bool Car::ReadConfigurationFile(std::string cfgfilename)
{
	enum CfgFileReadMode {PARAMETERS, MESHES, NONE};
	std::vector<std::string>nullstringvec(0);
	std::ifstream cfgfile;
	cfgfile.open(cfgfilename.c_str(), std::ios_base::in);
	if (cfgfile.is_open())
	{
		CfgFileReadMode readMode = CfgFileReadMode::NONE;
		char parsinglineChar[400];
		std::string parsinglineString;
		std::string paramname, paramval;
		size_t spacepos;
		FLOAT val;
		for (;!cfgfile.eof();)
		{
			cfgfile.getline(parsinglineChar, 400);
			parsinglineString = parsinglineChar;
			if (parsinglineString == "<PARAMETERS>")
				readMode = CfgFileReadMode::PARAMETERS;
			else if(parsinglineString == "</PARAMETERS>")
				readMode = CfgFileReadMode::NONE;
			else if (parsinglineString == "<MESHES>")
				readMode = CfgFileReadMode::MESHES;
			else if (parsinglineString == "</MESHES>")
				readMode = CfgFileReadMode::NONE;
			else
			{
				switch (readMode)
				{
				case CfgFileReadMode::PARAMETERS:
					spacepos = parsinglineString.find(' ');
					paramname = parsinglineString.substr(0, spacepos);
					paramval = parsinglineString.substr(spacepos + 1);
					val = atof(paramval.c_str());
					mNumericParamters.insert(std::make_pair(paramname, val));
					break;
				case CfgFileReadMode::MESHES:
					spacepos = parsinglineString.find(' ');
					paramname = parsinglineString.substr(0, spacepos);
					paramval = parsinglineString.substr(spacepos + 1);
					if (mMeshesFilenames.find(paramname) != mMeshesFilenames.end())
					{
						mMeshesFilenames.at(paramname).push_back(paramval);
					}
					else
					{
						mMeshesFilenames.insert(std::make_pair(paramname, nullstringvec));
						mMeshesFilenames.at(paramname).push_back(paramval);
					}
					break;
				default:
					break;
				}
			}
		}
		cfgfile.close();
		return true;
	}
	return false;
}

void Car::Update(FLOAT dt, FLOAT aspectratio)
{
	mCarWorld = XMMatrixInverse(&XMMatrixDeterminant(XMMatrixTranslation(mCarAABB.Center.x, mCarAABB.Center.y, mCarAABB.Center.z)), XMMatrixTranslation(mCarAABB.Center.x, mCarAABB.Center.y, mCarAABB.Center.z)) * pMeshManager->GetMeshWorldMatrixXM(mBoundingVolumeStartIdx, 0);

	FLOAT mVelocityMagnitude = XMVectorGetX(XMVector3Length(mVelocity));
	FLOAT SteerRadius = mCarLength / (mSteeringAngle + EPSILON);

	//Car movement direction [front/back]
	//FLOAT MoveDir = XMVectorGetX(XMVector3Dot(mVelocity, mCarOrientation));
	//New wheel spin angle calculation
	//mSpinAngle += (MoveDir > 0 ? 1 : (MoveDir < 0 ? -1 : 0)) * mVelocityMagnitude / mWheelsRadius * dt;
	////Car steering-caused rotation angle calculation
	//FLOAT deltaSteerAngle = -(MoveDir > 0 ? 1 : (MoveDir < 0 ? -1 : 0)) * mVelocityMagnitude / SteerRadius * dt;

	////Pivots calculation
	//XMVECTOR offset = XMVector3Normalize(XMVector3Cross(mVelocity, up)) * SteerRadius;
	//XMMATRIX OffsetToPivot = XMMatrixTranslation(XMVectorGetX(offset), XMVectorGetY(offset), XMVectorGetZ(offset));
	//offset = XMVector3Transform(offset, XMMatrixRotationY(deltaSteerAngle));
	//XMMATRIX OffsetFromPivot = XMMatrixTranslation(XMVectorGetX(-offset), XMVectorGetY(-offset), XMVectorGetZ(-offset));

	////Car steering-caused rotation matrix calculation
	//XMMATRIX SteerRotate = OffsetToPivot * XMMatrixRotationY(deltaSteerAngle) * OffsetFromPivot;

	//mVelocity = XMVector3Transform(mVelocity, XMMatrixRotationY(deltaSteerAngle));
	//mAcceleration = XMVector3Transform(mAcceleration, XMMatrixRotationY(deltaSteerAngle));
	//mCarOrientation = XMVector3Normalize(XMVector3Transform(mCarOrientation, XMMatrixRotationY(deltaSteerAngle)));

	//XMVECTOR globalScale, globalRotate, globalTranslate;
	//XMMatrixDecompose(&globalScale, &globalRotate, &globalTranslate, mCarWorld);

	//XMMATRIX R = XMMatrixRotationQuaternion(globalRotate) * SteerRotate;
	//XMMATRIX T = XMMatrixTranslation(XMVectorGetX(globalTranslate + mVelocity * dt),
	//	XMVectorGetY(globalTranslate + mVelocity * dt),
	//	XMVectorGetZ(globalTranslate + mVelocity * dt));
	//XMMATRIX S = XMMatrixScaling(XMVectorGetX(globalScale),
	//	XMVectorGetY(globalScale),
	//	XMVectorGetZ(globalScale));

	//mCarWorld = S * R * T;

	for (int j = 0; j < 4; ++j)
	{
		for (int i = 0; i < mWheelsMeshesIndices[j].size(); ++i)
		{
			XMMATRIX locmatrix = pMeshManager->GetMeshLocalMatrixXM(mWheelsMeshesIndices[j][i]);
			if (j == FRONT_LEFT)
			{
				XMMATRIX WheelOffset = XMMatrixTranslation(mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationX(mSpinAngle) * XMMatrixRotationY(mSteeringAngle) * WheelOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mWheelsMeshesIndices[j][i], 0, &newworldmatrix);
			}
			else if (j == FRONT_RIGHT)
			{
				XMMATRIX WheelOffset = XMMatrixTranslation(-mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationX(-mSpinAngle) * XMMatrixRotationY(XM_PI) * XMMatrixRotationY(mSteeringAngle) * WheelOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mWheelsMeshesIndices[j][i], 0, &newworldmatrix);
			}
			else if (j == REAR_LEFT)
			{
				XMMATRIX WheelOffset = XMMatrixTranslation(mRearLeftWheelOffset.x, mRearLeftWheelOffset.y, mRearLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationX(mSpinAngle) * WheelOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mWheelsMeshesIndices[j][i], 0, &newworldmatrix);
			}
			else if (j == REAR_RIGHT)
			{
				XMMATRIX WheelOffset = XMMatrixTranslation(-mRearLeftWheelOffset.x, mRearLeftWheelOffset.y, mRearLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationX(-mSpinAngle) * XMMatrixRotationY(XM_PI) * WheelOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mWheelsMeshesIndices[j][i], 0, &newworldmatrix);
			}
		}
	}
	for (int j = 0; j < 4; ++j)
	{
		for (int i = 0; i < mBrakesMeshesIndices[j].size(); ++i)
		{
			XMMATRIX locmatrix = pMeshManager->GetMeshLocalMatrixXM(mBrakesMeshesIndices[j][i]);
			if (j == FRONT_LEFT)
			{
				XMMATRIX BrakeOffset = XMMatrixTranslation(mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationY(mSteeringAngle) * BrakeOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mBrakesMeshesIndices[j][i], 0, &newworldmatrix);
			}
			else if (j == FRONT_RIGHT)
			{
				XMMATRIX BrakeOffset = XMMatrixTranslation(-mFrontLeftWheelOffset.x, mFrontLeftWheelOffset.y, mFrontLeftWheelOffset.z);
				XMMATRIX newworldmatrix = locmatrix * XMMatrixRotationY(XM_PI) * XMMatrixRotationY(XM_PI) * XMMatrixRotationY(mSteeringAngle) * BrakeOffset * mCarWorld;
				pMeshManager->SetMeshWorldMatrixXM(mBrakesMeshesIndices[j][i], 0, &newworldmatrix);
			}
		}
	}
	for (int i = 0; i < mBodyMeshesIndices.size(); ++i)
	{
		XMMATRIX locmatrix = pMeshManager->GetMeshLocalMatrixXM(mBodyMeshesIndices[i]);
		XMMATRIX newworldmatrix = locmatrix * mCarWorld;
		pMeshManager->SetMeshWorldMatrixXM(mBodyMeshesIndices[i], 0, &newworldmatrix);
	}

	FLOAT VelDotAcc = XMVectorGetX(XMVector3Dot(mVelocity, mAcceleration * mAccelerationDirection));
	INT AccForce = (VelDotAcc > 0 ? mAccFrontMagnitude : mAccBackMagnitude);

	//Integrate velocity
	mVelocity += dt * mAcceleration * mAccelerationDirection * AccForce;

	//Integrate steering angle
	if (mSteeringAngle <= mMaxSteeringAngle && mSteeringDirection == STEER_RIGHT)
		mSteeringAngle += mSteeringDelta;
	else if (mSteeringAngle >= -mMaxSteeringAngle && mSteeringDirection == STEER_LEFT)
		mSteeringAngle -= mSteeringDelta;

	mSteeringDirection = 0;
	mAccelerationDirection = 0;

	UpdateBumperChaseCamera(aspectratio);
}

void Car::UpdateBumperChaseCamera(FLOAT aspectratio)
{
	mBumperChaseCamera->SetLens(0.25f*MathHelper::Pi, aspectratio, Z_NEAR, Z_FAR);
	XMVECTOR camPos = XMLoadFloat3(&mBumperChaseCameraPosition);
	camPos = XMVector3TransformCoord(camPos, mCarWorld);
	mBumperChaseCamera->LookAt(camPos, camPos - mCarOrientation, up);
	mBumperChaseCamera->UpdateViewMatrix();
}

CarState Car::GetCarState() const 
{
	CarState cs;
	cs.mCarWorld = mCarWorld;
	cs.mSteeringAngle = mSteeringAngle;
	cs.mSteeringDirection = mSteeringDirection;
	cs.mSpinAngle = mSpinAngle;
	cs.mCarOrientation = mCarOrientation;
	cs.mVelocity = mVelocity;
	cs.mAcceleration = mAcceleration;
	cs.mAccelerationDirection = mAccelerationDirection;
	return cs;
}

void Car::SetCarState(CarState& cs)
{
	mCarWorld = cs.mCarWorld;
	mSteeringAngle = cs.mSteeringAngle;
	mSteeringDirection = cs.mSteeringDirection;
	mSpinAngle = cs.mSpinAngle;
	mVelocity = cs.mVelocity;
	mCarOrientation = cs.mCarOrientation;
	mAcceleration = cs.mAcceleration;
	mAccelerationDirection = cs.mAccelerationDirection;
}