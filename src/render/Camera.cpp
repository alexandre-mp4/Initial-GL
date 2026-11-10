#include "Camera.h"
#include "glm/gtc/matrix_transform.hpp"

const float DEF_FOV = 45.0f;

Camera::Camera()
: mPosition(glm::vec3(0.0f, 0.0f, 0.0f)),
  mTargetPos(glm::vec3(0.0f, 0.0f, 0.0f)),
  mLook(glm::vec3(0.0f, 0.0f, -1.0f)),
  mUp(glm::vec3(0.0f, 1.0f, 0.0f)),
  mRight(1.0f, 0.0f, 0.0f),
  WORLD_UP(0.0f, 1.0f, 0.0f),
  mYaw(glm::pi<float>()),
  mPitch(0.0f),
  mFOV(DEF_FOV)
{
}

glm::mat4 Camera::getViewMatrix()const
{
	return glm::lookAt(mPosition, mTargetPos, mUp);
}

const glm::vec3& Camera::getLook() const
{
	return mLook;
}

const glm::vec3& Camera::getRight() const
{
	return mRight;
}

const glm::vec3& Camera::getUp() const
{
	return mUp;
}

const glm::vec3& Camera::getPosition() const
{
	return mPosition;
}

FPSCamera::FPSCamera(glm::vec3 position, float yaw, float pitch)
{
	mPosition = position;
	mYaw = yaw;
	mPitch = pitch;
}

void FPSCamera::setPosition(const glm::vec3& position)
{
	mPosition = position;
}

void FPSCamera::move(const glm::vec3& offsetPos)
{
	mPosition += offsetPos;
	updateCameraVectors();
}

void FPSCamera::rotate(float yaw, float pitch)
{
	mYaw += glm::radians(yaw);
	mPitch += glm::radians(pitch);

	mPitch = glm::clamp(mPitch, -glm::pi<float>() / 2.0f + 0.1f, glm::pi<float>() / 2.0f - 0.1f);
	updateCameraVectors();
}

void FPSCamera::setYawPitch(float yawDeg, float pitchDeg)
{
	mYaw = glm::radians(yawDeg);
	mPitch = glm::clamp(glm::radians(pitchDeg), -glm::pi<float>() / 2.0f + 0.1f, glm::pi<float>() / 2.0f - 0.1f);
	updateCameraVectors();
}

void FPSCamera::updateCameraVectors()
{
	glm::vec3 look;
	look.x = cosf(mPitch) * sinf(mYaw);
	look.y = sinf(mPitch);
	look.z = cosf(mPitch) * cosf(mYaw);

	mLook = glm::normalize(look);

	mRight = glm::normalize(glm::cross(mLook, WORLD_UP));
	mUp = glm::normalize(glm::cross(mRight, mLook));

	mTargetPos = mPosition + mLook;
}

OrbitCamera::OrbitCamera()
	: mRadius(10.0f)
{}

void OrbitCamera::setLookAt(const glm::vec3& target)
{
	mTargetPos = target;
}

void OrbitCamera::setRadius(float radius)
{
	mRadius = glm::clamp(radius, 2.0f, 80.0f);
}

void OrbitCamera::rotate(float yaw, float pitch)
{
	mYaw = glm::radians(yaw);
	mPitch = glm::radians(pitch);

	mPitch = glm::clamp(mPitch, -glm::pi<float>() / 2.0f + 0.1f, glm::pi<float>() / 2.0f - 0.1f);

	updateCameraVectors();
}

void OrbitCamera::updateCameraVectors()
{
	mPosition.x = mTargetPos.x + mRadius * cosf(mPitch) * sinf(mYaw);
	mPosition.y = mTargetPos.y + mRadius * sinf(mPitch);
	mPosition.z = mTargetPos.z + mRadius * cosf(mPitch) * cosf(mYaw);

	// met aussi à jour la direction du regard : getLook() sert à d'autres classes (lampe torche)
	mLook = glm::normalize(mTargetPos - mPosition);
	mRight = glm::normalize(glm::cross(mLook, WORLD_UP));
	mUp = glm::normalize(glm::cross(mRight, mLook));
}

void HoodCamera::setPose(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
{
	mPosition = position;
	mLook = glm::normalize(forward);
	mTargetPos = position + mLook;
	mUp = glm::normalize(up);
	mRight = glm::normalize(glm::cross(mLook, mUp));
}

void ChaseCamera::raiseTo(float minY)
{
	if (mPosition.y < minY)
	{
		mPosition.y = minY;
		updateCameraVectors();
	}
}

ChaseCamera::ChaseCamera(float distance, float height, float stiffness)
	: mDistance(distance), mHeight(height), mStiffness(stiffness)
{}

glm::vec3 ChaseCamera::getIdealPosition(const glm::vec3& targetPos, float targetYaw) const
{
	glm::vec3 forward(sinf(targetYaw), 0.0f, cosf(targetYaw));
	return targetPos - forward * mDistance + glm::vec3(0.0f, mHeight, 0.0f);
}

void ChaseCamera::reset(const glm::vec3& targetPos, float targetYaw)
{
	mPosition = getIdealPosition(targetPos, targetYaw);

	glm::vec3 forward(sinf(targetYaw), 0.0f, cosf(targetYaw));
	mTargetPos = targetPos + glm::vec3(0.0f, 1.2f, 0.0f) + forward * 3.0f;
	updateCameraVectors();
}

void ChaseCamera::follow(const glm::vec3& targetPos, float targetYaw, float deltaTime)
{
	glm::vec3 ideal = getIdealPosition(targetPos, targetYaw);

	// lissage exponentiel : t = 1 - e^(-stiffness * deltaTime), indépendant du nombre d'images par seconde
	float t = 1.0f - expf(-mStiffness * deltaTime);
	mPosition = glm::mix(mPosition, ideal, t);

	glm::vec3 forward(sinf(targetYaw), 0.0f, cosf(targetYaw));
	mTargetPos = targetPos + glm::vec3(0.0f, 1.2f, 0.0f) + forward * 3.0f;

	updateCameraVectors();
}

void ChaseCamera::updateCameraVectors()
{
	mLook = glm::normalize(mTargetPos - mPosition);
	mRight = glm::normalize(glm::cross(mLook, WORLD_UP));
	mUp = glm::normalize(glm::cross(mRight, mLook));
}
