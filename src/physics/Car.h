// voiture simulée comme un corps rigide : suspension, pneus, moteur et collisions
// repère local : +Z avant, +Y haut, +X gauche, origine au sol au milieu de l'empattement
#ifndef CAR_H
#define CAR_H

#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct Contact
{
	glm::vec3 normal;
	float depth;
	float friction;
};

// ce que la voiture doit savoir du monde, implémenté par Scene
class PhysicsWorld
{
public:
	virtual ~PhysicsWorld() {}
	virtual float groundHeight(float x, float z) const = 0;
	virtual glm::vec3 groundNormal(float x, float z) const = 0;

	virtual int pointContacts(const glm::vec3& p, Contact* out, int maxOut) const = 0;
};

struct CarSpec
{
	float mass;
	float frontMassFraction;
	float comHeight;
	float peakTorque;
	float rpmLimit;
	int   numGears;
	float gears[6];
	float finalDrive;
	float tireGrip;
	float rearGripFactor;
	float diffLock;
	const char* name;

	static const CarSpec& forId(const std::string& id);
};

struct CarTuning
{
	float steerAssist = 0.65f;
	float gripScale = 1.0f;
	float powerScale = 1.0f;
	float driftAngleMax = 0.70f;
	float tractionAid = 0.7f;
};

struct CarInput
{
	float throttle = 0.0f;
	float brake = 0.0f;
	float steer = 0.0f;
	bool  handbrake = false;
};

struct WheelState
{
	float extension = 0.0f;
	float omega = 0.0f;
	float spin = 0.0f;
	bool  contact = false;
	glm::vec3 contactPoint = glm::vec3(0.0f);
	float load = 0.0f;
	float slipAngle = 0.0f;
	float slipRatio = 0.0f;
	glm::vec3 tireForce = glm::vec3(0.0f);
	glm::vec3 springForce = glm::vec3(0.0f);
};

class Car
{
public:

	Car();

	void configure(const std::string& carId, float halfWheelbase, float halfTrack, float wheelRadius,
				   const glm::vec3& boundsMin, const glm::vec3& boundsMax);
	void setWorld(const PhysicsWorld* world) { mWorld = world; }

	void reset(const glm::vec3& groundPosition, float yaw);

	// pas de temps fixe de la simulation
	static constexpr float PHYSICS_DT = 1.0f / 240.0f;
	void step(const CarInput& input);

	CarTuning tuning;

	const glm::vec3& getPosition() const { return mModelPos; }
	float getYaw() const;
	float getCameraYaw() const;
	float getSpeed() const;
	float getSpeedKmh() const { return fabsf(getSpeed()) * 3.6f; }
	const glm::vec3& getVelocity() const { return mVel; }
	float getDriftAngle() const { return mDriftAngle; }
	int   getGear() const { return mReverse ? -1 : mGear; }
	float getRpm() const { return mRpm; }
	float getRpmLimit() const { return mSpec->rpmLimit; }
	bool  isBraking() const { return mBraking; }
	float getLastImpact() const { return mLastImpact; }
	const CarSpec& getSpec() const { return *mSpec; }

	const WheelState& getWheel(int i) const { return mWheels[i]; }
	float getWheelSteer() const { return mSteer; }

	float getWheelOffset(int i) const { return MOUNT_ABOVE - mWheels[i].extension; }

	glm::mat4 getModelMatrix() const;
	glm::vec3 localToWorldPoint(const glm::vec3& local) const;
	glm::vec3 localToWorldDirection(const glm::vec3& local) const;

	void limitArea(float halfSize);

	float getWheelRadius() const { return mRadius; }

	static constexpr float MOUNT_ABOVE = 0.22f;

private:

	void updateControls(const CarInput& in, float& driveCmd, float& brakeCmd);
	void solveCollisions();
	glm::vec3 pointVelocity(const glm::vec3& r) const { return mVel + glm::cross(mAngVel, r); }

	const PhysicsWorld* mWorld;
	const CarSpec* mSpec;

	float mHalfWheelbase, mHalfTrack, mRadius;
	glm::vec3 mCom;
	glm::vec3 mInertia;
	std::vector<glm::vec3> mBodyPoints;

	glm::vec3 mPos, mVel, mAngVel;
	glm::quat mQ;
	glm::vec3 mModelPos;

	WheelState mWheels[4];
	float mPrevExtension[4];
	float mSteer, mSteerCmd, mThrottleSmooth, mBrakeSmooth, mHandbrakeTimer;
	int   mGear;
	bool  mReverse;
	float mShiftTimer, mShiftCooldown, mRpm;
	bool  mBraking;
	float mDriftAngle, mLastImpact;
	float mRearSlip;
};
#endif
