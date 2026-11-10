#include "Car.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

static const float GRAVITY = 9.81f;
static const float WHEEL_INERTIA = 1.6f;
static const float BRAKE_GRIP = 1.3f;
static const float TIRE_DENOM_MIN = 2.0f;

// au repos le ressort est comprimé de 0,08 m (SPRING_FREE - MOUNT_ABOVE)
static const float SPRING_FREE = 0.30f;
static const float SPRING_MIN = 0.06f;
static const float BUMP_STOP_K = 180000.0f;
static const float ROLL_BAR = 16000.0f;

// valeurs inspirées de vraies voitures, toutes en propulsion
static const CarSpec SPECS[] = {
	// masse, part avant, hauteur du CdG, couple, régime max, rapports, finale, adhérence, adhérence arrière, blocage, nom
	{ 1000.0f, 0.53f, 0.47f, 160.0f, 7800.0f, 5, { 3.587f, 2.022f, 1.384f, 1.000f, 0.861f, 0.0f }, 4.30f, 1.05f, 0.92f, 0.55f, "Toyota AE86" },
	{ 1280.0f, 0.50f, 0.49f, 310.0f, 8000.0f, 5, { 3.483f, 2.015f, 1.391f, 1.000f, 0.806f, 0.0f }, 4.10f, 1.15f, 0.92f, 0.45f, "Mazda RX-7" },
	{ 1560.0f, 0.55f, 0.52f, 430.0f, 8000.0f, 6, { 3.827f, 2.360f, 1.685f, 1.312f, 1.000f, 0.793f }, 3.54f, 1.10f, 0.88f, 0.50f, "Nissan Skyline R34" },
};

const CarSpec& CarSpec::forId(const std::string& id)
{
	if (id == "ae86") return SPECS[0];
	if (id == "r34")  return SPECS[2];
	return SPECS[1];
}

// courbe de couple normalisée selon régime / régime max, interpolée linéairement
static float torqueCurve(float u)
{
	static const float C[9] = { 0.45f, 0.62f, 0.78f, 0.90f, 1.00f, 0.99f, 0.92f, 0.80f, 0.55f };
	u = glm::clamp(u, 0.0f, 1.0f) * 8.0f;
	int i = std::min((int)u, 7);
	return glm::mix(C[i], C[i + 1], u - i);
}

// formule de Pacejka simplifiée : F = D * sin(C * atan(B*x - E*(B*x - atan(B*x))))
static float pacejka(float x, float B, float C, float E)
{
	float bx = B * x;
	return sinf(C * atanf(bx - E * (bx - atanf(bx))));
}

// force d'un pneu en glissement combiné : les glissements longitudinal et latéral sont normalisés par leur valeur au pic
// et regroupés en un seul glissement sigma, auquel on applique une seule courbe de Pacejka
static const float KAPPA_PEAK = 0.14f;
static const float ALPHA_PEAK = 0.20f;

// powerKeep : part du glissement d'accélération prise en compte, pour qu'une roue motrice qui patine garde de la tenue latérale
static glm::vec2 tireForce(float kappa, float alpha, float D, float powerKeep)
{
	float nk = kappa / KAPPA_PEAK;
	float na = tanf(alpha) / tanf(ALPHA_PEAK);
	float nkEff = (nk > 0.0f) ? nk * powerKeep : nk;
	float sigma = sqrtf(nkEff * nkEff + na * na);
	if (sigma < 1e-5f) return glm::vec2(0.0f);
	float F = D * pacejka(sigma, 1.9f, 1.5f, -0.6f);
	return glm::vec2(F * nk / std::max(sqrtf(nk * nk + na * na), 1e-5f) , -F * na / sigma);
}

Car::Car()
	: mWorld(nullptr), mSpec(&SPECS[1]), mHalfWheelbase(1.2f), mHalfTrack(0.8f), mRadius(0.33f),
	  mCom(0.0f), mInertia(1.0f), mPos(0.0f), mVel(0.0f), mAngVel(0.0f), mQ(1.0f, 0.0f, 0.0f, 0.0f), mModelPos(0.0f),
	  mSteer(0.0f), mSteerCmd(0.0f), mThrottleSmooth(0.0f), mBrakeSmooth(0.0f), mHandbrakeTimer(0.0f), mGear(1), mReverse(false), mShiftTimer(0.0f), mShiftCooldown(0.0f), mRpm(1000.0f),
	  mBraking(false), mDriftAngle(0.0f), mLastImpact(0.0f), mRearSlip(0.0f)
{
	for (int i = 0; i < 4; i++) mPrevExtension[i] = SPRING_FREE - Car::MOUNT_ABOVE;
}

void Car::configure(const std::string& carId, float halfWheelbase, float halfTrack, float wheelRadius,
					const glm::vec3& bmin, const glm::vec3& bmax)
{
	mSpec = &CarSpec::forId(carId);
	mHalfWheelbase = halfWheelbase;
	mHalfTrack = halfTrack;
	mRadius = wheelRadius;

	// centre de masse décalé vers l'essieu le plus chargé
	mCom = glm::vec3(0.0f, mSpec->comHeight, (mSpec->frontMassFraction - 0.5f) * 2.0f * halfWheelbase);

	// moments d'inertie d'une boîte pleine, réduits de 15 %
	float W = bmax.x - bmin.x, H = 1.25f, L = bmax.z - bmin.z, m = mSpec->mass;
	mInertia = 0.85f * m / 12.0f * glm::vec3(H * H + L * L, W * W + L * L, W * W + H * H);

	mBodyPoints.clear();
	float hx = 0.5f * W * 0.92f, hz = 0.5f * L * 0.97f, y = 0.50f;
	for (int ix = -1; ix <= 1; ix++)
		for (int iz = -1; iz <= 1; iz++)
			if (ix != 0 || iz != 0)
				mBodyPoints.push_back(glm::vec3(ix * hx, y, iz * hz));
}

void Car::reset(const glm::vec3& groundPosition, float yaw)
{
	// repère de la carrosserie posé sur le sol : haut = normale du sol
	glm::vec3 n = mWorld ? mWorld->groundNormal(groundPosition.x, groundPosition.z) : glm::vec3(0, 1, 0);
	glm::vec3 fwd = glm::vec3(sinf(yaw), 0.0f, cosf(yaw));
	fwd = glm::normalize(fwd - n * glm::dot(fwd, n));
	glm::vec3 left = glm::cross(n, fwd);
	mQ = glm::normalize(glm::quat_cast(glm::mat3(left, n, fwd)));

	float groundY = mWorld ? mWorld->groundHeight(groundPosition.x, groundPosition.z) : groundPosition.y;
	glm::vec3 origin(groundPosition.x, groundY + 0.08f, groundPosition.z);
	mPos = origin + glm::mat3_cast(mQ) * mCom;
	mVel = mAngVel = glm::vec3(0.0f);
	mSteer = mSteerCmd = 0.0f;
	mThrottleSmooth = mBrakeSmooth = mHandbrakeTimer = 0.0f;
	mGear = 1; mReverse = false; mShiftTimer = 0.0f; mRpm = 1000.0f;
	mLastImpact = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		mWheels[i] = WheelState();
		mWheels[i].extension = mPrevExtension[i] = SPRING_FREE - Car::MOUNT_ABOVE;
	}
	mModelPos = origin;
}

float Car::getYaw() const
{
	glm::vec3 f = glm::mat3_cast(mQ) * glm::vec3(0, 0, 1);
	return atan2f(f.x, f.z);
}

float Car::getSpeed() const
{
	return glm::dot(mVel, glm::mat3_cast(mQ) * glm::vec3(0, 0, 1));
}

// en glissade, la caméra regarde en partie vers la trajectoire
float Car::getCameraYaw() const
{
	float heading = getYaw();
	glm::vec2 vh(mVel.x, mVel.z);
	float speed = glm::length(vh);
	if (speed < 3.0f || getSpeed() < 0.0f)
		return heading;
	float course = atan2f(vh.x, vh.y);
	float diff = course - heading;
	diff = atan2f(sinf(diff), cosf(diff));
	float w = 0.55f * glm::smoothstep(3.0f, 10.0f, speed);
	return heading + diff * w;
}

glm::mat4 Car::getModelMatrix() const
{
	return glm::translate(glm::mat4(1.0f), mPos) * glm::mat4_cast(mQ) * glm::translate(glm::mat4(1.0f), -mCom);
}

glm::vec3 Car::localToWorldPoint(const glm::vec3& local) const
{
	return mPos + glm::mat3_cast(mQ) * (local - mCom);
}

glm::vec3 Car::localToWorldDirection(const glm::vec3& local) const
{
	return glm::mat3_cast(mQ) * local;
}

void Car::limitArea(float halfSize)
{
	if (fabsf(mPos.x) > halfSize || fabsf(mPos.z) > halfSize)
	{
		mPos.x = glm::clamp(mPos.x, -halfSize, halfSize);
		mPos.z = glm::clamp(mPos.z, -halfSize, halfSize);
		mVel = glm::vec3(0.0f);
	}
}

void Car::updateControls(const CarInput& in, float& driveCmd, float& brakeCmd)
{
	const float dt = PHYSICS_DT;
	glm::mat3 R = glm::mat3_cast(mQ);
	glm::vec3 fwd = R * glm::vec3(0, 0, 1), left = R * glm::vec3(1, 0, 0);
	float vFwd = glm::dot(mVel, fwd), vLeft = glm::dot(mVel, left);

	// marche arrière : frein appuyé presque à l'arrêt
	if (!mReverse)
	{
		if (in.brake > 0.05f && vFwd < 0.8f && in.throttle < 0.05f) mReverse = true;
	}
	else if (in.throttle > 0.05f && vFwd > -0.8f)
		mReverse = false;

	// lissage des touches (0 ou 1) pour simuler la pression du pied
	float ramp = 3.5f * dt;
	mThrottleSmooth += glm::clamp(in.throttle - mThrottleSmooth, -6.0f * dt, ramp);
	mBrakeSmooth    += glm::clamp(in.brake - mBrakeSmooth, -8.0f * dt, ramp * 3.5f);
	driveCmd = mReverse ? mBrakeSmooth : mThrottleSmooth;
	brakeCmd = mReverse ? mThrottleSmooth : mBrakeSmooth;
	mBraking = brakeCmd > 0.1f;

	float speed = glm::length(mVel);

	// l'angle de braquage max diminue avec la vitesse : 1 / (1 + (v/12)^1.5)
	float maxSteer = 0.55f / (1.0f + powf(speed / 12.0f, 1.5f)) + std::min(fabsf(mDriftAngle) * 0.9f, 0.40f);
	maxSteer = std::min(maxSteer, 0.60f);

	float target = in.steer * maxSteer;
	float rate = (fabsf(target) > fabsf(mSteerCmd) || target * mSteerCmd < 0.0f) ? 3.2f : 5.5f;
	mSteerCmd += glm::clamp(target - mSteerCmd, -rate * maxSteer * dt, rate * maxSteer * dt) * 1.0f;
	mSteerCmd = glm::clamp(mSteerCmd, -maxSteer, maxSteer);

	// aide au contre-braquage : les roues avant se braquent d'une fraction de l'angle de dérive
	mDriftAngle = (vFwd > 1.0f) ? atan2f(vLeft, vFwd) : 0.0f;
	float assist = 0.0f;
	if (vFwd > 3.0f)
		assist = tuning.steerAssist * 0.62f * mDriftAngle * glm::smoothstep(3.0f, 9.0f, vFwd);
	float desired = glm::clamp(mSteerCmd + assist, -0.62f, 0.62f);
	mSteer += (desired - mSteer) * (1.0f - expf(-18.0f * dt));

	// boîte automatique avec hystérésis, basée sur le régime que le moteur aurait à cette vitesse
	if (mShiftTimer > 0.0f) mShiftTimer -= dt;
	mShiftCooldown = std::max(0.0f, mShiftCooldown - dt);
	if (!mReverse && mShiftTimer <= 0.0f && mShiftCooldown <= 0.0f)
	{
		auto rpmAt = [&](int gear) { return fabsf(vFwd) / mRadius * mSpec->gears[gear - 1] * mSpec->finalDrive * 60.0f / (2.0f * glm::pi<float>()); };
		if (mGear < mSpec->numGears && rpmAt(mGear) > 0.90f * mSpec->rpmLimit)
		{ mGear++; mShiftTimer = 0.18f; mShiftCooldown = 0.6f; }
		else if (mGear > 1 && rpmAt(mGear - 1) < 0.82f * mSpec->rpmLimit && rpmAt(mGear) < 0.35f * mSpec->rpmLimit)
		{ mGear--; mShiftTimer = 0.10f; mShiftCooldown = 0.5f; }
	}
	if (mReverse) mGear = 1;
}

void Car::step(const CarInput& in)
{
	if (!mWorld) return;
	const float dt = PHYSICS_DT;
	const float m = mSpec->mass;

	float driveCmd, brakeCmd;
	updateControls(in, driveCmd, brakeCmd);

	mHandbrakeTimer = in.handbrake ? 0.6f : std::max(0.0f, mHandbrakeTimer - dt);

	glm::mat3 R = glm::mat3_cast(mQ);
	glm::vec3 up = R * glm::vec3(0, 1, 0);
	glm::vec3 force(0.0f, -m * GRAVITY, 0.0f);
	glm::vec3 torque(0.0f);

	// régime moteur calculé à partir de la vitesse des roues motrices et du rapport engagé
	float ratio = (mReverse ? -3.2f : mSpec->gears[mGear - 1]) * mSpec->finalDrive;
	float drivenOmega = 0.5f * (mWheels[2].omega + mWheels[3].omega);
	float rpmWheels = fabsf(drivenOmega * ratio) * 60.0f / (2.0f * glm::pi<float>());
	mRpm = std::max(1000.0f, std::min(rpmWheels, mSpec->rpmLimit * 1.02f));

	float rpmTorque = std::max(mRpm, 0.28f * mSpec->rpmLimit);
	float engineTorque = driveCmd * torqueCurve(rpmTorque / mSpec->rpmLimit) * mSpec->peakTorque * tuning.powerScale;
	engineTorque -= (1.0f - driveCmd) * 0.10f * mSpec->peakTorque * (mRpm / mSpec->rpmLimit);
	if (mRpm >= mSpec->rpmLimit || mShiftTimer > 0.0f) engineTorque = std::min(engineTorque, 0.0f);

	// aide à la motricité : coupe une partie du couple si les roues patinent
	if (engineTorque > 0.0f)
		engineTorque *= 1.0f - 0.85f * tuning.tractionAid * glm::clamp((mRearSlip - 0.2f) / 0.6f, 0.0f, 1.0f);
	float wheelTorqueTotal = engineTorque * ratio * 0.90f;

	// suspension : un rayon par roue vers le sol, force = ressort + amortisseur
	const float kFront = 38000.0f * m / 1280.0f * 1.10f, kRear = 38000.0f * m / 1280.0f;
	const float cBump = 2400.0f * m / 1280.0f, cRebound = 3400.0f * m / 1280.0f;
	float compression[4];
	glm::vec3 rel[4], mountW[4];
	for (int i = 0; i < 4; i++)
	{
		WheelState& w = mWheels[i];
		float sx = (i % 2 == 0) ? 1.0f : -1.0f, sz = (i < 2) ? 1.0f : -1.0f;
		glm::vec3 mountLocal(sx * mHalfTrack, mRadius + MOUNT_ABOVE, sz * mHalfWheelbase);
		rel[i] = R * (mountLocal - mCom);
		mountW[i] = mPos + rel[i];

		// intersection du rayon avec le sol par itération de point fixe
		float upY = std::max(up.y, 0.35f);
		float d = (mountW[i].y - mWorld->groundHeight(mountW[i].x, mountW[i].z)) / upY;
		for (int it = 0; it < 3; it++)
		{
			glm::vec3 P = mountW[i] - up * d;
			d = (mountW[i].y - mWorld->groundHeight(P.x, P.z)) / upY;
		}
		float s = d - mRadius;

		// voiture retournée : aucune roue ne touche le sol
		// un sol trop haut est une paroi, gérée par les collisions
		w.contact = (s < SPRING_FREE) && (s > SPRING_MIN - 0.10f) && (up.y > 0.15f);
		float bump = 0.0f;
		if (s < SPRING_MIN)
		{
			// butée très raide mais bornée
			bump = std::min(BUMP_STOP_K * (SPRING_MIN - s), 1.2f * m * GRAVITY);
			s = SPRING_MIN;
		}
		s = std::min(s, SPRING_FREE);

		float sdot = (s - mPrevExtension[i]) / dt;
		mPrevExtension[i] = s;
		w.extension = s;
		compression[i] = SPRING_FREE - s;
		float k = (i < 2) ? kFront : kRear;
		float damper = (sdot < 0.0f) ? cBump * (-sdot) : -cRebound * sdot;
		w.load = w.contact ? std::max(0.0f, k * compression[i] + damper + bump) : 0.0f;
		w.contactPoint = mountW[i] - up * (s + mRadius);
	}

	// barre anti-roulis
	for (int axle = 0; axle < 2; axle++)
	{
		int a = axle * 2, b = a + 1;
		if (mWheels[a].contact && mWheels[b].contact)
		{
			float bar = ROLL_BAR * (compression[a] - compression[b]);
			mWheels[a].load = std::max(0.0f, mWheels[a].load + bar);
			mWheels[b].load = std::max(0.0f, mWheels[b].load - bar);
		}
	}
	for (int i = 0; i < 4; i++)
	{
		mWheels[i].springForce = up * mWheels[i].load;
		force += mWheels[i].springForce;
		torque += glm::cross(rel[i], mWheels[i].springForce);
	}

	// pneus
	float rearSlipMax = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		WheelState& w = mWheels[i];
		bool front = (i < 2), driven = !front;
		float brakeTorque = brakeCmd * (front ? 1900.0f : 1100.0f);
		if (in.handbrake && !front)
		{
			// le frein à main bloque l'arrière, relâché progressivement quand la dérive approche de l'angle max
			float L = tuning.driftAngleMax;
			brakeTorque = 2200.0f * (1.0f - glm::smoothstep(0.7f * L, 1.0f * L, fabsf(mDriftAngle)));
		}
		if (brakeCmd < 0.01f && driveCmd < 0.01f && glm::length(mVel) < 0.6f && !in.handbrake) brakeTorque += 900.0f;
		float driveTorque = driven ? 0.5f * wheelTorqueTotal : 0.0f;

		if (!w.contact || w.load <= 1.0f)
		{
			w.omega += dt * driveTorque / WHEEL_INERTIA;
			float db = brakeTorque * dt / WHEEL_INERTIA;
			w.omega = (fabsf(w.omega) <= db) ? 0.0f : w.omega - copysignf(db, w.omega);
			w.spin += w.omega * dt;
			w.tireForce = glm::vec3(0.0f); w.slipAngle = w.slipRatio = 0.0f;
			continue;
		}

		// repère du pneu dans le plan du sol : f = roulement, l = latéral
		glm::vec3 n = mWorld->groundNormal(w.contactPoint.x, w.contactPoint.z);
		float delta = front ? mSteer : 0.0f;
		glm::vec3 fw = R * glm::vec3(sinf(delta), 0.0f, cosf(delta));
		glm::vec3 f = glm::normalize(fw - n * glm::dot(fw, n));
		glm::vec3 l = glm::cross(n, f);

		glm::vec3 rc = w.contactPoint - mPos;
		glm::vec3 vp = pointVelocity(rc);
		float vx = glm::dot(vp, f), vy = glm::dot(vp, l);

		// kappa = (omega*r - vx) / |vx|, alpha = atan(vy / |vx|)
		float den = std::max(fabsf(vx), TIRE_DENOM_MIN);
		float kappa = (w.omega * mRadius - vx) / den;
		float alpha = atanf(vy / den);

		// une paroi raide n'est pas un sol : pas d'adhérence
		float slopeGrip = glm::smoothstep(0.35f, 0.75f, n.y);

		// adhérence arrière réduite pendant le frein à main et ~0,8 s après
		float rearGrip = 1.0f;
		if (!front)
		{
			float k = glm::clamp(mHandbrakeTimer / 0.4f, 0.0f, 1.0f);
			rearGrip = 1.0f - (1.0f - mSpec->rearGripFactor) * 2.8f * k;
			rearGrip = glm::clamp(rearGrip, 0.7f, 1.0f);
		}
		float mu = mSpec->tireGrip * tuning.gripScale * rearGrip * slopeGrip;
		float D = mu * w.load;

		// ABS : couple de freinage plafonné à ~92 % de l'adhérence du pneu
		if (!(in.handbrake && !front))
			brakeTorque = std::min(brakeTorque, 0.93f * BRAKE_GRIP * D * mRadius);

		// l'aide à la motricité limite la force motrice
		if (driven)
		{
			float limit = glm::mix(1.2f, 0.62f, tuning.tractionAid) * D / mRadius;
			driveTorque = glm::clamp(driveTorque, -limit, limit);
		}

		float bk = (kappa < 0.0f && brakeTorque > 1.0f && !(in.handbrake && !front)) ? BRAKE_GRIP : 1.0f;
		float powerKeep = driven ? 0.35f : 1.0f;
		glm::vec2 F = tireForce(kappa, alpha, D, powerKeep);
		float Fx = F.x * bk, Fy = F.y;

		// pente de Fx en kappa, pour résoudre la roue en implicite
		float J = (tireForce(kappa + 0.01f, alpha, D, powerKeep).x * bk - Fx) / 0.01f / den * mRadius;

		Fx -= 0.012f * w.load * tanhf(vx / 0.5f);

		// dynamique de la roue : I * d(omega)/dt = couple moteur - couple de frein - r * Fx (schéma implicite)
		float dOmega = dt * (driveTorque - mRadius * Fx) / (WHEEL_INERTIA + dt * mRadius * J);
		Fx += J * dOmega;
		w.omega += dOmega;

		float db = brakeTorque * dt / WHEEL_INERTIA;
		w.omega = (fabsf(w.omega) <= db) ? 0.0f : w.omega - copysignf(db, w.omega);
		w.spin += w.omega * dt;

		// différentiel à glissement limité entre les deux roues arrière
		if (driven && mSpec->diffLock > 0.0f)
		{
			float other = mWheels[i == 2 ? 3 : 2].omega;
			float c = glm::clamp(mSpec->diffLock * 55.0f * (other - w.omega), -500.0f, 500.0f);
			w.omega += c * dt / WHEEL_INERTIA;
		}

		w.slipRatio = kappa; w.slipAngle = alpha;
		if (driven) rearSlipMax = std::max(rearSlipMax, fabsf(kappa));
		w.tireForce = f * Fx + l * Fy;
		force += w.tireForce;
		torque += glm::cross(rc, w.tireForce);
	}

	mRearSlip = rearSlipMax;

	// stabilisation : couple de lacet qui ramène le cap vers la trajectoire quand la dérive est trop grande
	if (fabsf(mDriftAngle) > 0.01f)
	{
		float L = tuning.driftAngleMax;
		float gain = tuning.steerAssist * 9.0f;
		float beta = fabsf(mDriftAngle);
		float ramp;
		if (mHandbrakeTimer > 0.0f)

			// limiteur d'angle du drift au frein à main
			ramp = glm::smoothstep(0.55f * L, 1.0f * L, beta) * 2.0f;
		else

			ramp = glm::smoothstep(0.15f, 0.65f, beta);
		torque += up * (mInertia.y * gain * ramp * mDriftAngle);
	}

	// traînée aérodynamique : F = 1/2 * rho * CxS * v^2
	float speed = glm::length(mVel);
	force -= 0.5f * 1.2f * 0.62f * speed * mVel;

	// intégration d'Euler semi-implicite
	glm::mat3 Rt = glm::transpose(R);
	glm::vec3 wLocal = Rt * mAngVel;
	glm::vec3 Iw = mInertia * wLocal;
	glm::vec3 gyro = glm::cross(wLocal, Iw);
	glm::vec3 alphaAng = R * ((Rt * torque - gyro) / mInertia);
	mVel += force / m * dt;
	mAngVel += alphaAng * dt;
	mAngVel *= 1.0f - 0.15f * dt;
	mPos += mVel * dt;

	// orientation : dq/dt = 1/2 * (0, omega) * q, puis renormalisation
	mQ = glm::normalize(mQ + 0.5f * dt * glm::quat(0.0f, mAngVel.x, mAngVel.y, mAngVel.z) * mQ);

	// garde-fous contre les NaN et les vitesses absurdes
	if (glm::any(glm::isnan(mVel)) || glm::any(glm::isnan(mAngVel)) || glm::any(glm::isnan(mPos)))
	{
		mVel = mAngVel = glm::vec3(0.0f);
		mPos = mModelPos + glm::mat3_cast(mQ) * mCom;
	}
	if (glm::length(mAngVel) > 12.0f) mAngVel *= 12.0f / glm::length(mAngVel);
	if (glm::length(mVel) > 95.0f) mVel *= 95.0f / glm::length(mVel);

	// collisions
	solveCollisions();
	mLastImpact = std::max(0.0f, mLastImpact - 6.0f * dt);
	mModelPos = mPos - glm::mat3_cast(mQ) * mCom;
}

// impulsion normale avec rebond amorti, frottement de Coulomb, puis correction de position
void Car::solveCollisions()
{
	glm::mat3 R = glm::mat3_cast(mQ);
	glm::mat3 Rt = glm::transpose(R);
	const float m = mSpec->mass;
	const float restitution = 0.25f;
	glm::vec3 push(0.0f);

	for (const glm::vec3& p : mBodyPoints)
	{
		glm::vec3 r = R * (p - mCom);
		glm::vec3 P = mPos + r;
		Contact contacts[6];
		int nc = mWorld->pointContacts(P, contacts, 6);
		for (int c = 0; c < nc; c++)
		{
			const glm::vec3& n = contacts[c].normal;
			glm::vec3 vp = pointVelocity(r);
			float vn = glm::dot(vp, n);

			glm::vec3 rn = glm::cross(r, n);
			float denom = 1.0f / m + glm::dot(n, glm::cross(R * ((Rt * rn) / mInertia), r));
			if (vn < 0.0f)
			{
				float j = -(1.0f + restitution) * vn / denom;
				mVel += n * (j / m);
				mAngVel += R * ((Rt * glm::cross(r, n * j)) / mInertia);
				mLastImpact = std::max(mLastImpact, -vn);

				vp = pointVelocity(r);
				glm::vec3 vt = vp - n * glm::dot(vp, n);
				float vtLen = glm::length(vt);
				if (vtLen > 0.01f)
				{
					glm::vec3 t = vt / vtLen;
					glm::vec3 rt = glm::cross(r, t);
					float denomT = 1.0f / m + glm::dot(t, glm::cross(R * ((Rt * rt) / mInertia), r));
					float jt = std::min(vtLen / denomT, contacts[c].friction * j);
					mVel -= t * (jt / m);
					mAngVel -= R * ((Rt * glm::cross(r, t * jt)) / mInertia);
				}
			}

			glm::vec3 corr = n * std::min(contacts[c].depth, 0.35f) * 0.5f;
			if (glm::length(corr) > glm::length(push)) push = corr;
		}
	}
	mPos += push;
}
