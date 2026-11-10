// une partie : compte à rebours, chrono, progression et score de drift
// il y a drift quand l'angle cap/trajectoire dépasse 12° à plus de 25 km/h
// la chaîne rapporte angle * vitesse * 0,02 * multiplicateur points par seconde (multiplicateur max x5),
// encaissée après 0,8 s sans drift et perdue en cas de choc
#ifndef SESSION_H
#define SESSION_H

#include <string>
#include "Car.h"

enum GameMode { MODE_TIME_ATTACK, MODE_FREE_DRIFT };

class Session
{
public:

	void start(GameMode mode, int finishIndex, int numPoints);

	void update(float dt, const Car& car, int roadIndex);

	GameMode mode = MODE_TIME_ATTACK;
	float countdown = 3.0f;
	bool  racing = false;
	bool  finished = false;
	float time = 0.0f;
	float progress = 0.0f;
	float distance = 0.0f;

	float totalScore = 0.0f;
	float chainScore = 0.0f;
	int   multiplier = 1;
	bool  drifting = false;
	float chainTime = 0.0f;
	float bestChain = 0.0f;
	float lastBankedChain = 0.0f;
	float bankFlash = 0.0f;
	float crashFlash = 0.0f;
	float lostChain = 0.0f;

	int numPoints = 0, finishIndex = 0;

	static float loadBestTime(const std::string& carId);
	static void saveBestTime(const std::string& carId, float time);
	static float loadBestScore(const std::string& carId);
	static void saveBestScore(const std::string& carId, float score);

private:

	float mNoDriftTime = 0.0f;
	bool mWasImpact = false;
};
#endif
