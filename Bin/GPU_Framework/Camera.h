#ifndef _CAMERA_H_
#define _CAMERA_H_

#include "../Common/d3dUtil.h"

#define Z_FAR 2000.0f
#define Z_NEAR 1.0f

class Camera
{
private:
	// Camera coordinate system with coordinates relative to world space.
	XMFLOAT3 mPosition;
	XMFLOAT3 mRight;
	XMFLOAT3 mUp;
	XMFLOAT3 mLook;

	// Cache frustum properties.
	float mNearZ;
	float mFarZ;
	float mAspect;
	float mFovY;
	float mNearWindowHeight;
	float mFarWindowHeight;
	float mRotationLookAngle;

	bool mRotate;
	bool mPitch;

	float mSpeed;

	// Cache View/Proj matrices.
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;
public:
	Camera();
	~Camera();

	// Get/Set world camera position.
	XMVECTOR GetPositionXM()const;
	XMFLOAT3 GetPosition()const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const XMFLOAT3& v);

	// Get camera basis vectors.
	XMVECTOR GetRightXM()const;
	XMFLOAT3 GetRight()const;
	XMVECTOR GetUpXM()const;
	XMFLOAT3 GetUp()const;
	XMVECTOR GetLookXM()const;
	XMFLOAT3 GetLook()const;

	// Get frustum properties.
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;

	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
	void LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up);
	void SetLook(const XMFLOAT3 &v) { mLook = v; };

	void SetView(const XMFLOAT4X4 View) { mView = View; };
	void SetProj(const XMFLOAT4X4 Proj) { mProj = Proj; };

	// Get View/Proj matrices.
	XMMATRIX View()const;
	XMMATRIX Proj()const;
	XMMATRIX ViewProj()const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void RotateY(float angle);
	void Roll(float angle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

	void LockMouseCameraControl();
	void UnlockMouseCameraControl();
	
	bool IsMouseCameraControlLocked() const;

	float GetCamSpeed() const;
	void SetCamSpeed(float speed);
};
#endif