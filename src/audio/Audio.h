// musique MP3 et sons synthétisés en temps réel (miniaudio)
// le callback audio tourne dans un autre thread et ne lit que des variables atomiques
#ifndef AUDIO_H
#define AUDIO_H

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

enum BlipKind { BLIP_MOVE, BLIP_SELECT, BLIP_BACK, BLIP_COUNT, BLIP_GO, BLIP_RECORD };

class Audio
{
public:

	Audio();
	~Audio();
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	bool init();

	void setEngine(float rpm, float throttle, int engineType, bool active);
	void setTires(float screech);
	void setWind(float speedMs);
	void impact(float strength);
	void blip(BlipKind kind);

	void playMusic(const std::vector<std::string>& playlist);
	void update(float dt);

	float musicVolume = 0.55f;
	float sfxVolume = 0.8f;

	struct Impl;

private:

	Impl* mImpl;
};
#endif
