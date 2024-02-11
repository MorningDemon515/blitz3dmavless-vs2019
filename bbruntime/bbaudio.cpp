
#include "std.h"
#include "bbaudio.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <AL\alut.h>

gxAudio* gx_audio;

static inline bool debugSound(gxSound* s, const char* a) {
	if (debug) {
		if (!gx_audio->verifySound(s)) {
			RTEX("Sound does not exist");
			return false;
		}
	}
	else {
		if (!gx_audio->verifySound(s)) {
			RTEX(": Sound does not exist");
			return false;
		}
	}

    return true;
}

int setPan(ALuint source, float pan) {
	// ��panֵӳ�䵽-1.0��1.0�ķ�Χ
	float positionX = pan; // ����ֻ��x���ϵ���λ��
	float positionY = 0.0f; // ��y���z�ᱣ�ֲ���
	float positionZ = 0.0f;

	// ������Դλ��
	alSource3f(source, AL_POSITION, positionX, positionY, positionZ);

	// ���OpenAL����״̬
	ALenum error = alGetError();
	if (error != AL_NO_ERROR) {
		// ������������ӡ������Ϣ
		printf("OpenAL����%s\n", alGetString(error));
		return -1; // ���ش������
	}

	return 0; // ���سɹ�����
}

static gxSound* loadSound(BBStr* f, bool use_3d) {
	string t = *f; delete f;
	return gx_audio ? gxSoundSample::load(gx_audio, t, use_3d) : 0;
}

static gxSound* streamSound(BBStr* f, bool use_3d) {
	string t = *f; delete f;
	return gx_audio ? gxSoundStream::load(gx_audio, t, use_3d) : 0;
}

gxSound* bbLoadSound(BBStr* f, int use_3d) {
	return loadSound(f, !!use_3d);
}

gxSound* bbStreamSound(BBStr* f) {
	return streamSound(f, false);
}

void bbFreeSound(gxSound* sound) {
	if (!sound) return;
	if (!debugSound(sound, "FreeSound")) return;
	sound->free();
}

void bbLoopSound(gxSound* sound, int loop) {
	if (!sound) return;
	if (!debugSound(sound, "LoopSound")) return;
	sound->setLoop(!!loop);
}

void bbSoundPitch(gxSound* sound, float pitch) {
	if (!sound) return;
	if (!debugSound(sound, "SoundPitch")) return;
	sound->setPitch(pitch);
}

void bbSoundVolume(gxSound* sound, float volume) {
	if (!sound) return;
	if (!debugSound(sound, "SoundVolume")) return;
	sound->setVolume(volume);
}

void bbSoundRange(gxSound* sound, float inNear, float inFar) {
	if (!sound) return;
	if (!debugSound(sound, "SoundRange")) return;
	sound->setRange(inNear, inFar);
}

gxChannel* bbPlaySound(gxSound* sound, float x, float y, float z, float vx, float vy, float vz) {
	if (!sound) return 0;
	if (!debugSound(sound, "PlaySound")) return 0;
	float pos[3] = { x,y,z }; float vel[3] = { vx,vy,vz };
	gxChannel* chan = sound->play();
	if (!isnan(x)) {
		chan->set3d(pos, vel);
	}
	return chan;
}

void bbStopChannel(gxChannel* channel) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->stop();
}

void bbPauseChannel(gxChannel* channel) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setPaused(true);
}

void bbResumeChannel(gxChannel* channel) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setPaused(false);
}

void bbChannelPitch(gxChannel* channel, float pitch) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setPitch(pitch);
}

void bbChannelVolume(gxChannel* channel, float volume) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setVolume(volume);
}

void bbChannelRange(gxChannel* channel, float inNear, float inFar) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setRange(inNear, inFar);
}

void bbChannelPos(gxChannel* channel, float x, float y, float z, float vx, float vy, float vz) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	float pos[3] = { x,y,z };
	float vel[3] = { vx,vy,vz };
	channel->set3d(pos, vel);
}

void bbChannelSeek(gxChannel* channel, float seconds) {
	if (!channel) return;
	if (!gx_audio->verifyChannel(channel)) return;
	channel->setTime(seconds);
}

int bbChannelPlaying(gxChannel* channel) {
	if (!gx_audio->verifyChannel(channel)) return 0;
	return channel ? channel->isPlaying() : 0;
}

bool audio_create() {
	gx_audio = gx_runtime->openAudio(0);
	return true;
}

bool audio_destroy() {
	if (gx_audio) gx_runtime->closeAudio(gx_audio);
	gx_audio = 0;
	return true;
}

void audio_link(void(*rtSym)(const char*, void*)) {
	rtSym("%LoadSound$filename%is3d=1", bbLoadSound);
	rtSym("FreeSound%sound", bbFreeSound);
	rtSym("LoopSound%sound%loop=1", bbLoopSound);
	rtSym("%StreamSound$filename%is3d=1", bbStreamSound);
	rtSym("SoundPitch%sound#pitch", bbSoundPitch);
	rtSym("SoundVolume%sound#volume", bbSoundVolume);
	rtSym("SoundRange%sound#near#far", bbSoundRange);
	rtSym("%PlaySound%sound#x=0#y=0#z=0#vx=0#vy=0#vz=0", bbPlaySound);
	rtSym("StopChannel%channel", bbStopChannel);
	rtSym("PauseChannel%channel", bbPauseChannel);
	rtSym("ResumeChannel%channel", bbResumeChannel);
	rtSym("ChannelPitch%channel#pitch", bbChannelPitch);
	rtSym("ChannelVolume%channel#volume", bbChannelVolume);
	rtSym("ChannelRange%channel#near#far", bbChannelRange);
	rtSym("ChannelPos%channel#x#y#z#vx=0#vy=0#vz=0", bbChannelPos);
	//rtSym("ChannelPan%source#pan",bbChannelPan);
	rtSym("ChannelSeek(BBChannel)channel#seconds", bbChannelSeek);
	rtSym("%ChannelPlaying%channel", bbChannelPlaying);
}