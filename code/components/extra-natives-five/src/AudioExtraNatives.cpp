#include "StdInc.h"
#include <Hooking.h>
#include <MinHook.h>
#include <ScriptEngine.h>
//#include <ICoreGameInit.h>

// Taken from CodeWalker/RelFile.cs by dexyfex
//
enum Dat151RelType : uint32_t
{
	VehicleCollision = 0x1,
	VehicleTrailer = 0x2,
	VehicleEngine = 0x4,
	AudioMaterial = 0x5,
	StaticEmitter = 0x6,
	EntityEmitter = 0x7,
	Helicopter = 0x8,
	MeleeCombat = 0x9,
	SpeechContext = 0xB,
	SpeechChoice = 0xC,
	VirtualSpeechChoice = 0xD,
	SpeechParams = 0xE,
	SpeechContextList = 0xF,
	Boat = 0x10,
	Shoe = 0x12,
	Unk22 = 0x16,
	Skis = 0x17,
	RadioStationList = 0x18,
	RadioStation = 0x19,
	RadioTrack = 0x1A,
	RadioTrackCategory = 0x1B,
	PoliceScannerCrime = 0x1C,
	RaceToPedVoiceGroup = 0x1D,
	PedVoiceGroup = 0x1E,
	PedType = 0x1F,
	StaticEmitterList = 0x20,
	PoliceScannerReport = 0x21,
	PoliceScannerLocation = 0x23,
	PoliceScannerLocationList = 0x24,
	AmbientZone = 0x25,
	AmbientRule = 0x26,
	AmbientZoneList = 0x27,
	AmbientStreamList = 0x28,
	AmbienceBankMap = 0x29,
	AmbientZoneParams = 0x2A,
	Interior = 0x2C,
	InteriorRoomParams = 0x2D,
	InteriorRoom = 0x2E,
	Door = 0x2F,
	DoorParams = 0x30,
	DoorList = 0x31,
	WeaponAudioItem = 0x32,
	Climbing = 0x33,
	Mod = 0x34,
	Train = 0x35,
	WeatherType = 0x36,
	WeatherTypeList = 0x37,
	Bicycle = 0x38,
	Aeroplane = 0x39,
	StemMix = 0x3B,
	Mood = 0x3E,
	StartTrackAction = 0x3F,
	StopTrackAction = 0x40,
	SetMoodAction = 0x41,
	PlayerAction = 0x42,
	StartOneShotAction = 0x43,
	StopOneShotAction = 0x44,
	MusicBeat = 0x45,
	MusicBar = 0x46,
	DependentAmbience = 0x47,
	Unk72 = 0x48,
	AnimalParams = 0x49,
	AnimalSounds = 0x4A,
	VehicleScannerColourList = 0x4B,
	VehicleScannerParams = 0x4C,
	Unk77 = 0x4D,
	MicrophoneList = 0x4E,
	Microphone = 0x4F,
	VehicleRecording = 0x50,
	VehicleRecordingList = 0x51,
	AnimalFootsteps = 0x52,
	AnimalFootstepsList = 0x53,
	ShoeList = 0x54,
	Cloth = 0x55,
	ClothList = 0x56,
	Explosion = 0x57,
	VehicleEngineGranular = 0x58,
	ShoreLinePool = 0x5A,
	ShoreLineLake = 0x5B,
	ShoreLineRiver = 0x5C,
	ShoreLineOcean = 0x5D,
	ShoreLineList = 0x5E,
	RadioTrackSettings = 0x5F,
	Unk96 = 0x60,
	RadioDJSpeechAction = 0x62,
	Unk99 = 0x63,
	Tunnel = 0x64,
	Alarm = 0x65,
	FadeOutRadioAction = 0x66,
	FadeInRadioAction = 0x67,
	ForceRadioTrackAction = 0x68,
	SlowMoSettings = 0x69,
	Scenario = 0x6A,
	AudioOcclusionOverride = 0x6B,
	ElectricEngine = 0x6C,
	BreathSettings = 0x6D,
	Unk110 = 0x6E,
	AircraftWarningSettings = 0x6F,
	Unk112 = 0x70,
	CopDispatchInteractionSettings = 0x71,
	RadioTrackEvents = 0x72,
	Unk115 = 0x73,
	Unk116 = 0x74,
	DoorModel = 0x75,
	Unk118 = 0x76,
	Foilage = 0x77,
	TrackList = 0x78,
	MacsModelsOverrides = 0x79,
	RadioStationList2 = 0x7C,
};

static hook::cdecl_stub<uintptr_t(uintptr_t, uint32_t, Dat151RelType)> rage__audMetadataManager__GetObject_Verify([]()
{
	return hook::get_call(hook::get_pattern("E8 ? ? ? ? 44 8D 47 2F"));
});

static HookFunction initFunction([]()
{
	static uintptr_t location = (uintptr_t)hook::get_pattern("48 8D 0D ? ? ? ? 41 B8 06 00 00 00 8B D0");
	static uintptr_t audMetadataManager_game = hook::get_address<uintptr_t>(location, 3, 7);

	//TODO: NEVER ever PR this, this is dangerous on so many levels :^)
	//For debug memes only
	fx::ScriptEngine::RegisterNativeHandler("GET_STATIC_EMITTER_ADDRESS", [](fx::ScriptContext& context)
	{
		const char* emitterName = context.CheckArgument<const char*>(0);
		uint32_t emitterHash = HashString(emitterName);

		auto result = rage__audMetadataManager__GetObject_Verify(audMetadataManager_game, emitterHash, Dat151RelType::StaticEmitter);
		if (!result)
		{
			context.SetResult<uintptr_t>(0);
			trace("Static Emitter %s could not be found\n", emitterName);
			return;
		}

		trace("Static Emitter %s at address %p\n", emitterName, (void*)result);
		context.SetResult<uintptr_t>(result);
	});

	//TODO:
	// For the actual implementation, make sure to reset everything on session change
	/*Instance<ICoreGameInit>::Get()->OnShutdownSession.Connect([]()
	{

	});*/
});
