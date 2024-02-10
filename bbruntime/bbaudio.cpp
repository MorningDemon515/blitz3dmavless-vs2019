
#include "std.h"
#include "bbaudio.h"

gxAudio *gx_audio;

static inline void debugSound( gxSound *s ){
	if( debug ){
		if( !gx_audio->verifySound( s ) ) RTEX( "Sound does not exist" );
	}
}

static gxSound *loadSound( BBStr *f,bool use_3d ){
	string t=*f;delete f;
	return gx_audio ? gx_audio->loadSound( t,use_3d ) : 0;
}

gxSound* bbLoadSound(BBStr* f, int use_3d) {
	return loadSound(f, !!use_3d);
}

void bbFreeSound( gxSound *sound ){
	if( !sound ) return;
	debugSound( sound );
	gx_audio->freeSound( sound );
}

void bbLoopSound( gxSound *sound, int loop){
	if( !sound ) return;
	debugSound( sound );
	sound->setLoop(!!loop);
}

void bbSoundPitch( gxSound *sound, float pitch ){
	if( !sound ) return;
	debugSound( sound );
	sound->setPitch( pitch );
}

void bbSoundVolume( gxSound *sound,float volume ){
	if( !sound ) return;
	debugSound( sound );
	sound->setVolume( volume );
}

gxChannel *bbPlaySound( gxSound *sound, float x, float y, float z, float vx, float vy, float vz){
	if( !sound ) return 0;
	debugSound( sound );
	float pos[3] = { x,y,z }; float vel[3] = { vx,vy,vz };
	return sound->play3d(pos, vel);
}

void bbStopChannel( gxChannel *channel ){
	if( !channel ) return;
	channel->stop();
}

void bbPauseChannel( gxChannel *channel ){
	if( !channel ) return;
	channel->setPaused( true );
}

void bbResumeChannel( gxChannel *channel ){
	if( !channel ) return;
	channel->setPaused( false );
}

void bbChannelPitch( gxChannel *channel, float pitch ){
	if( !channel ) return;
	channel->setPitch( pitch );
}

void bbChannelVolume( gxChannel *channel,float volume ){
	if( !channel ) return;
	channel->setVolume( volume );
}

int bbChannelPlaying( gxChannel *channel ){
	return channel ? channel->isPlaying() : 0;
}

bool audio_create(){
	gx_audio=gx_runtime->openAudio( 0 );
	return true;
}

bool audio_destroy(){
	if( gx_audio ) gx_runtime->closeAudio( gx_audio );
	gx_audio=0;
	return true;
}

void audio_link( void(*rtSym)(const char*,void*) ){
	rtSym( "%LoadSound$filename%is3d=1",bbLoadSound );
	rtSym( "FreeSound%sound",bbFreeSound );
	rtSym( "LoopSound%sound%loop=1",bbLoopSound );
	rtSym( "SoundPitch%sound#pitch",bbSoundPitch );
	rtSym( "SoundVolume%sound#volume",bbSoundVolume );
	//rtSym( "SoundPan%sound#pan",bbSoundPan );
	rtSym("%PlaySound%sound#x=0#y=0#z=0#vx=0#vy=0#vz=0", bbPlaySound);
	rtSym( "StopChannel%channel",bbStopChannel );
	rtSym( "PauseChannel%channel",bbPauseChannel );
	rtSym( "ResumeChannel%channel",bbResumeChannel );
	rtSym( "ChannelPitch%channel#pitch",bbChannelPitch );
	rtSym( "ChannelVolume%channel#volume",bbChannelVolume );
	//rtSym( "ChannelPan%channel#pan",bbChannelPan );
	rtSym( "%ChannelPlaying%channel",bbChannelPlaying );
}
