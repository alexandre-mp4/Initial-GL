#include "Audio.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "third_party/miniaudio.h"

static const float PI2 = 6.2831853f;
static const int SAMPLE_RATE = 44100;

// bruit blanc xorshift dans [-1, 1]
struct Noise
{
	uint32_t s = 2463534242u;
	float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(int32_t)s / 2147483648.0f; }
};

// filtre passe-bas d'ordre 1 : y += a * (x - y), a = 1 - e^(-2 * pi * fc / fe)
static float lpCoef(float fc) { return 1.0f - expf(-PI2 * fc / (float)SAMPLE_RATE); }

struct OneShot { int kind; float t, dur, freq, vol; };

struct Audio::Impl
{
	ma_device device;
	bool deviceOk = false;
	Audio* owner = nullptr;

	// paramètres écrits par le jeu et lus par le callback
	std::atomic<float> rpm{ 1000.0f }, throttle{ 0.0f }, screech{ 0.0f }, wind{ 0.0f };
	std::atomic<int> engineType{ 1 };
	std::atomic<bool> engineOn{ false };
	std::atomic<float> musicGain{ 0.0f };
	std::atomic<float> sfxGain{ 0.8f };
	std::atomic<float> musicVolumeCache{ 0.55f };
	std::vector<OneShot> active;

	std::mutex shotMutex;
	std::vector<OneShot> shots;

	std::mutex musicMutex;
	ma_decoder decoder;
	bool decoderOpen = false;
	std::vector<std::string> playlist;
	size_t trackIndex = 0;
	std::atomic<bool> trackEnded{ false };
	float musicFade = 0.0f;
	bool wantMusic = false;

	float enginePhase = 0.0f, rpmSmooth = 1000.0f, thrSmooth = 0.0f, whistlePhase = 0.0f;
	float lp1 = 0.0f, lp2 = 0.0f, lpWind = 0.0f, lpTire1 = 0.0f, lpTire2 = 0.0f, lpIntake = 0.0f;
	float screechSmooth = 0.0f, windSmooth = 0.0f, tirePhase = 0.0f, pulsePhase = 0.0f, stickPhase = 0.0f, stickJitter = 0.0f, res1a = 0.0f, res1b = 0.0f, res2a = 0.0f, res2b = 0.0f;
	Noise noise;

	void mix(float* out, ma_uint32 frames);
	float oneShot(const OneShot& s, float t) const;
};

float Audio::Impl::oneShot(const OneShot& s, float t) const
{
	float env = expf(-t * 5.0f / s.dur) * (t < 0.004f ? t / 0.004f : 1.0f);
	if (s.kind < 0)
		return 0.0f;
	if (s.kind == 100)
	{
		float f = 110.0f * expf(-t * 7.0f) + 38.0f;
		return (sinf(PI2 * f * t) * 0.9f + sinf(PI2 * 311.0f * t) * 0.18f * expf(-t * 18.0f)) * env * s.vol;
	}

	return (sinf(PI2 * s.freq * t) + 0.35f * sinf(PI2 * 2.0f * s.freq * t)) * env * s.vol * 0.6f;
}

void Audio::Impl::mix(float* out, ma_uint32 frames)
{
	for (ma_uint32 i = 0; i < frames * 2; i++) out[i] = 0.0f;
	float mg = musicGain.load();
	if (mg > 0.0005f && musicMutex.try_lock())
	{
		if (decoderOpen)
		{
			ma_uint64 got = 0;
			ma_decoder_read_pcm_frames(&decoder, out, frames, &got);
			if (got < frames)
				trackEnded = true;
			for (ma_uint32 i = 0; i < frames * 2; i++) out[i] *= mg * musicVolumeCache.load();
		}
		musicMutex.unlock();
	}

	float sfx = sfxGain.load();
	int type = engineType.load();
	bool on = engineOn.load();
	float rpmT = rpm.load(), thrT = throttle.load(), scrT = screech.load(), windT = wind.load();

	// explosions par tour et richesse en harmoniques de chaque moteur (4 cylindres, rotatif, 6 cylindres)
	const float pulsesPerRev[3] = { 2.0f, 3.0f, 3.0f };
	const float brightness[3] = { 0.55f, 0.9f, 0.4f };
	float ppr = pulsesPerRev[type], bright = brightness[type];

	std::vector<OneShot> localShots;
	if (shotMutex.try_lock()) { localShots.swap(shots); shotMutex.unlock(); }
	for (const OneShot& s : localShots) active.push_back(s);

	float aRpm = 1.0f - expf(-PI2 * 9.0f / SAMPLE_RATE);
	float aThr = 1.0f - expf(-PI2 * 6.0f / SAMPLE_RATE);
	float aScr = 1.0f - expf(-PI2 * 14.0f / SAMPLE_RATE);
	float aWind = 1.0f - expf(-PI2 * 3.0f / SAMPLE_RATE);

	for (ma_uint32 i = 0; i < frames; i++)
	{
		rpmSmooth += (rpmT - rpmSmooth) * aRpm;
		thrSmooth += (thrT - thrSmooth) * aThr;
		screechSmooth += (scrT - screechSmooth) * aScr;
		windSmooth += (windT - windSmooth) * aWind;
		float s = 0.0f;

		if (on)
		{
			// fréquence fondamentale = tours par seconde * explosions par tour
			float f0 = rpmSmooth / 60.0f * ppr;
			enginePhase += f0 / SAMPLE_RATE;
			if (enginePhase >= 1.0f) enginePhase -= 1.0f;
			float p = enginePhase;

			// timbre : dent de scie + onde carrée
			float saw = 2.0f * p - 1.0f;
			float sq = (p < 0.5f) ? 1.0f : -1.0f;
			float sub = sinf(PI2 * p * 0.5f);
			float wave = 0.55f * saw + 0.25f * sq * bright + 0.45f * sub + 0.25f * sinf(PI2 * p) + 0.15f * sinf(PI2 * 3.0f * p);

			float fc = 250.0f + 1400.0f * thrSmooth + 1500.0f * bright * (rpmSmooth / 8000.0f);
			float a = lpCoef(fc);
			lp1 += a * (wave - lp1);
			lp2 += a * (lp1 - lp2);

			lpIntake += lpCoef(900.0f) * (noise.next() - lpIntake);
			float n = lpIntake * (0.10f + 0.35f * thrSmooth);
			float loud = 0.15f + 0.20f * thrSmooth + 0.07f * (rpmSmooth / 8000.0f);
			s += (lp2 * 1.5f + n) * loud;

			if (type != 0)
			{
				whistlePhase += (1800.0f + 2600.0f * (rpmSmooth / 8000.0f)) / SAMPLE_RATE;
				if (whistlePhase >= 1.0f) whistlePhase -= 1.0f;
				s += sinf(PI2 * whistlePhase) * 0.035f * thrSmooth * thrSmooth * (rpmSmooth / 8000.0f);
			}

			if (rpmSmooth > 7600.0f && thrSmooth > 0.5f)
			{
				pulsePhase += 28.0f / SAMPLE_RATE; if (pulsePhase >= 1.0f) pulsePhase -= 1.0f;
				s *= (pulsePhase < 0.5f) ? 1.0f : 0.35f;
			}
		}

		// crissement des pneus : bruit résonant et rugueux plutôt qu'une note pure. Du bruit blanc passe dans deux résonateurs passe-bande
		// (1,4 et 2,5 kHz), avec une modulation d'amplitude irrégulière (~45-90 Hz) et un grondement grave
		if (screechSmooth > 0.01f)
		{
			float sc = screechSmooth;
			float nz = noise.next();

			stickJitter += (noise.next() - stickJitter) * 0.0004f;
			stickPhase += (55.0f + 40.0f * sc + 25.0f * stickJitter) / SAMPLE_RATE; if (stickPhase >= 1.0f) stickPhase -= 1.0f;
			float stick = 0.55f + 0.45f * (stickPhase < 0.5f ? 1.0f - 2.0f * stickPhase : 2.0f * stickPhase - 1.0f);
			stick *= 0.85f + 0.15f * noise.next();

			auto resonate = [&](float x, float f0, float Q, float& z1, float& z2) {
				float w0 = PI2 * f0 / SAMPLE_RATE, alpha = sinf(w0) / (2.0f * Q), a0 = 1.0f + alpha;
				float b0 = alpha / a0, a1 = -2.0f * cosf(w0) / a0, a2 = (1.0f - alpha) / a0;

				float y = b0 * x + z1;
				z1 = -a1 * y + z2;
				z2 = -b0 * x - a2 * y;
				return y;
			};
			float r1 = resonate(nz, 1350.0f + 450.0f * sc, 5.0f, res1a, res1b);
			float r2 = resonate(nz, 2500.0f + 500.0f * sc, 4.0f, res2a, res2b);
			lpTire2 += lpCoef(300.0f) * (noise.next() - lpTire2);
			tirePhase += (880.0f + 260.0f * sc) / SAMPLE_RATE; if (tirePhase >= 1.0f) tirePhase -= 1.0f;
			float trace = sinf(PI2 * tirePhase);
			s += ((r1 * 3.0f + r2 * 1.1f) * stick + lpTire2 * 0.9f + trace * 0.05f) * sc * (0.3f + 0.7f * sc) * 0.9f;
		}

		if (windSmooth > 0.5f)
		{
			float v = std::min(windSmooth / 45.0f, 1.0f);
			lpWind += lpCoef(300.0f + 1500.0f * v) * (noise.next() - lpWind);
			s += lpWind * v * v * 0.55f;
		}

		for (const OneShot& e : active)
		{
			float t = e.t + (float)i / SAMPLE_RATE;
			if (t >= 0.0f && t < e.dur) s += oneShot(e, t);
		}

		s *= sfx;

		// limiteur doux : tanh écrase les pics sans craquement
		out[2 * i]     += tanhf(s);
		out[2 * i + 1] += tanhf(s);
	}
	for (OneShot& e : active) e.t += (float)frames / SAMPLE_RATE;
	for (size_t k = 0; k < active.size();)
		if (active[k].t >= active[k].dur) active.erase(active.begin() + k); else k++;
}

static void dataCallback(ma_device* dev, void* output, const void*, ma_uint32 frames)
{
	((Audio::Impl*)dev->pUserData)->mix((float*)output, frames);
}

Audio::Audio() : mImpl(new Impl()) { mImpl->owner = this; }

Audio::~Audio()
{
	if (mImpl->deviceOk) ma_device_uninit(&mImpl->device);
	if (mImpl->decoderOpen) ma_decoder_uninit(&mImpl->decoder);
	delete mImpl;
}

bool Audio::init()
{
	ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
	cfg.playback.format = ma_format_f32;
	cfg.playback.channels = 2;
	cfg.sampleRate = SAMPLE_RATE;
	cfg.dataCallback = dataCallback;
	cfg.pUserData = mImpl;
	cfg.periodSizeInFrames = 512;
	if (ma_device_init(nullptr, &cfg, &mImpl->device) != MA_SUCCESS)
	{
		std::cerr << "Audio : aucun périphérique de sortie (le jeu continue sans son)" << std::endl;
		return false;
	}
	mImpl->deviceOk = (ma_device_start(&mImpl->device) == MA_SUCCESS);
	return mImpl->deviceOk;
}

void Audio::setEngine(float rpm, float throttle, int type, bool active)
{
	mImpl->rpm = rpm; mImpl->throttle = throttle; mImpl->engineType = type; mImpl->engineOn = active;
}
void Audio::setTires(float s) { mImpl->screech = s; }
void Audio::setWind(float v) { mImpl->wind = v; }

void Audio::impact(float strength)
{
	std::lock_guard<std::mutex> lock(mImpl->shotMutex);
	mImpl->shots.push_back({ 100, 0.0f, 0.55f, 0.0f, std::min(strength / 10.0f, 1.0f) });
}

void Audio::blip(BlipKind k)
{
	float f = 660.0f, dur = 0.12f, vol = 0.5f;
	std::vector<OneShot> add;
	switch (k)
	{
	case BLIP_MOVE:   f = 880.0f; dur = 0.06f; vol = 0.3f; break;
	case BLIP_SELECT: add.push_back({ 1, 0.0f, 0.16f, 784.0f, 0.5f }); add.push_back({ 1, -0.07f, 0.22f, 1175.0f, 0.5f }); f = 0; break;
	case BLIP_BACK:   f = 523.0f; dur = 0.12f; vol = 0.4f; break;
	case BLIP_COUNT:  f = 660.0f; dur = 0.22f; vol = 0.7f; break;
	case BLIP_GO:     f = 1320.0f; dur = 0.55f; vol = 0.8f; break;
	case BLIP_RECORD: add.push_back({ 1, 0.0f, 0.2f, 784.0f, 0.5f }); add.push_back({ 1, -0.1f, 0.2f, 988.0f, 0.5f }); add.push_back({ 1, -0.2f, 0.45f, 1319.0f, 0.55f }); f = 0; break;
	}
	if (f > 0) add.push_back({ 1, 0.0f, dur, f, vol });
	std::lock_guard<std::mutex> lock(mImpl->shotMutex);
	for (const OneShot& s : add) mImpl->shots.push_back(s);
}

static void openTrack(Audio::Impl* m)
{
	std::lock_guard<std::mutex> lock(m->musicMutex);
	if (m->decoderOpen) { ma_decoder_uninit(&m->decoder); m->decoderOpen = false; }
	if (m->playlist.empty()) return;
	ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, SAMPLE_RATE);
	const std::string& path = m->playlist[m->trackIndex % m->playlist.size()];
	if (ma_decoder_init_file(path.c_str(), &dc, &m->decoder) == MA_SUCCESS)
		m->decoderOpen = true;
	else
		std::cerr << "Audio : musique illisible '" << path << "'" << std::endl;
	m->trackEnded = false;
}

void Audio::playMusic(const std::vector<std::string>& playlist)
{
	if (playlist == mImpl->playlist && mImpl->wantMusic)
		return;
	mImpl->playlist = playlist;
	mImpl->trackIndex = playlist.empty() ? 0 : (size_t)(rand() % playlist.size());
	mImpl->wantMusic = !playlist.empty();
	mImpl->musicFade = 0.0f;
	mImpl->musicGain = 0.0f;
	if (mImpl->deviceOk) openTrack(mImpl);
}

void Audio::update(float dt)
{
	mImpl->sfxGain = sfxVolume;
	mImpl->musicVolumeCache = musicVolume;
	mImpl->musicFade = std::min(1.0f, mImpl->musicFade + dt / 1.5f);
	mImpl->musicGain = mImpl->wantMusic ? mImpl->musicFade : 0.0f;
	if (mImpl->trackEnded && mImpl->wantMusic)
	{
		mImpl->trackIndex++;
		openTrack(mImpl);
	}
}
