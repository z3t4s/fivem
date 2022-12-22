#include "StdInc.h"

#include <atArray.h>
#include <Local.h>
#include <Hooking.h>
#include <ScriptEngine.h>
#include <nutsnbolts.h>
#include <atPool.h>
#include <DirectXMath.h>
#include <CrossBuildRuntime.h>
#include <MinHook.h>

#define DECLARE_ACCESSOR(x) \
	decltype(impl.m2060.x)& x()        \
	{                       \
		return (xbr::IsGameBuildOrGreater<2060>() ? impl.m2060.x : impl.m1604.x);   \
	} \
	const decltype(impl.m2060.x)& x() const                         \
	{                                                    \
		return (xbr::IsGameBuildOrGreater<2060>() ? impl.m2060.x : impl.m1604.x);  \
	}

using Matrix3x4 = DirectX::XMFLOAT3X4;

struct Vector
{
	float x; // +0
	float y; // +4
	float z; // +8
	float w; // +16
};

struct iCEntityDef
{
	void* vtable; // +0
	uint32_t archetypeName; // +8
	uint32_t flags; // +12
	char pad1[16]; // +16
	Vector position; // +32
	Vector rotation; // +48
	float scaleXY; // +64
	float scaleZ; // +68
	int16_t parentIndex; // +72
	uint16_t lodDist; // +74
	uint16_t childLodDist; // +74
	char pad2[36]; // +76 (todo: extensions)
	float ambientOcclusionMultiplier; // +112
	float artificialAmbientOcclusion; // +116
	int16_t tintValue; // +120
	char pad3[6]; // +122
};

struct CMloRoomDef
{
	void* vtable; // +0
	char* name; // +8
	int64_t unkFlag; // +16
	char pad1[8]; // +24
	Vector bbMin; // +32
	Vector bbMax; // +48
	float blend; // +64
	uint32_t timecycleName; // +68
	uint32_t secondaryTimecycleName;
	uint32_t flags; // +76
	uint32_t portalsCount; // +80
	int32_t floorId; // +84
	int16_t exteriorVisibilityDepth; // +88
	atArray<uint32_t> attachedObjects; // +90
};

struct CMloPortalDef
{
	void* vtable; // +0
	uint32_t roomFrom; // +8
	uint32_t roomTo; // +12
	uint32_t flags; // +16
	int32_t mirrorPriority; // +20
	float opacity;	// +24
	int32_t audioOcclusion; // +28
	atArray<Vector> corners; // +32
	atArray<uint32_t> attachedObjects; // +48
};

struct CMloEntitySet
{
	void* vtable; // +0
	uint64_t name; // +8
	atArray<int32_t> locations; // +12
	atArray<iCEntityDef> entities; // +28
};

struct CMloTimeCycleModifier
{
	void* vtable; // +0
	uint64_t name; // +8
	Vector sphere; // +16
	float percentage; // +32
	float range; // +36
	uint32_t startHour; // +40
	uint32_t endHour; // +44
};

struct CMloModelInfo
{
	char pad1[200]; // +0
	atArray<iCEntityDef*>* entities; // +200
	atArray<CMloRoomDef>* rooms; // +208
	atArray<CMloPortalDef>* portals; // +216
	atArray<CMloEntitySet>* entitySets; // +224
	atArray<CMloTimeCycleModifier>* timecycleModifiers; // +232
};

struct CInteriorInst
{
	char pad1[32]; // +0
	CMloModelInfo* mloModel; // +32
	char pad2[56]; // +40
	Matrix3x4 matrix; // +96
	Vector position; // +144
	char pad3[228]; // +164
	// CInteriorProxy* proxy; // +392
};

struct InteriorProxy
{
public:
	struct Impl1604
	{
		void* vtbl;
		uint32_t mapIndex; // +8
		char pad1[4]; // +12
		uint32_t occlusionIndex; // +16
		char pad2[44]; // +20
		CInteriorInst* instance; // +64
		char pad3[24]; // +72
		Vector rotation; // +96
		Vector position; // +112
		Vector entitiesExtentsMin; // +128
		Vector entitiesExtentsMax; // +144
		char pad4[68]; // +160
		uint32_t archetypeHash; // +228
		char pad5[8]; // +232
	};

	struct Impl2060
	{
		void* vtbl;
		uint32_t mapIndex; // +8
		char pad1[4]; // +12
		uint32_t occlusionIndex; // +16
		char pad2[44]; // +20
		uint32_t unkFlag; // +64, some flag added in 1868, hardcoded for ch_dlc_arcade
		char pad3[4]; // +68
		CInteriorInst* instance; // +72
		char pad4[16]; // +80
		Vector rotation; // +96
		Vector position; // +112
		Vector entitiesExtentsMin; // +128
		Vector entitiesExtentsMax; // +144
		char pad5[68]; // +160
		uint32_t archetypeHash; // +228
		char pad6[8]; // +232
	};

	union
	{
		Impl1604 m1604;
		Impl2060 m2060;
	} impl;

public:
	DECLARE_ACCESSOR(instance);
	DECLARE_ACCESSOR(rotation);
	DECLARE_ACCESSOR(position);
	DECLARE_ACCESSOR(entitiesExtentsMin);
	DECLARE_ACCESSOR(entitiesExtentsMax);
};

template<int Build>
using CInteriorProxy = std::conditional_t<(Build >= 2060), InteriorProxy::Impl2060, InteriorProxy::Impl1604>;

template<int Build>
static atPool<CInteriorProxy<Build>>** g_interiorProxyPool;

template<int Build>
static CInteriorProxy<Build>* GetInteriorProxy(int handle)
{
	return (*g_interiorProxyPool<Build>)->GetAtHandle<CInteriorProxy<Build>>(handle);
}

static CMloModelInfo* GetInteriorArchetype(int interiorId)
{
	CInteriorInst* instance = nullptr;

	if (xbr::IsGameBuildOrGreater<2060>())
	{
		CInteriorProxy<2060>* proxy = GetInteriorProxy<2060>(interiorId);
		if (proxy)
		{
			instance = proxy->instance;
		}
	}
	else
	{
		CInteriorProxy<1604>* proxy = GetInteriorProxy<1604>(interiorId);
		if (proxy)
		{
			instance = proxy->instance;
		}
	}

	if (instance && instance->mloModel)
	{
		return instance->mloModel;
	}

	return nullptr;
}

static CMloRoomDef* GetInteriorRoomDef(int interiorId, int roomId)
{
	CMloModelInfo* arch = GetInteriorArchetype(interiorId);

	if (arch == nullptr || roomId < 0 || roomId > arch->rooms->GetCount())
	{
		return nullptr;
	}

	return &(arch->rooms->Get(roomId));
}

static CMloPortalDef* GetInteriorPortalDef(int interiorId, int portalId)
{
	CMloModelInfo* arch = GetInteriorArchetype(interiorId);

	if (arch == nullptr || portalId < 0 || portalId > arch->portals->GetCount())
	{
		return nullptr;
	}

	return &(arch->portals->Get(portalId));
}

static CMloEntitySet* GetInteriorEntitySet(int interiorId, int setId)
{
	CMloModelInfo* arch = GetInteriorArchetype(interiorId);

	if (arch == nullptr || setId < 0 || setId > arch->entitySets->GetCount())
	{
		return nullptr;
	}

	return &(arch->entitySets->Get(setId));
}

static iCEntityDef* GetInteriorPortalEntityDef(int interiorId, int portalId, int entityId)
{
	CMloModelInfo* arch = GetInteriorArchetype(interiorId);
	if (arch == nullptr)
	{
		return nullptr;
	}

	CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
	if (portalDef == nullptr)
	{
		return nullptr;
	}

	if (entityId < 0 || entityId > (portalDef->attachedObjects.GetCount()) - 1)
	{
		return nullptr;
	}

	return arch->entities->Get(portalDef->attachedObjects[entityId]);
}

static CMloTimeCycleModifier* GetInteriorTimecycleModifier(int interiorId, int modId)
{
	CMloModelInfo* arch = GetInteriorArchetype(interiorId);

	if (arch == nullptr || modId < 0 || modId > arch->timecycleModifiers->GetCount())
	{
		return nullptr;
	}

	return &(arch->timecycleModifiers->Get(modId));
}

static int GetInteriorRoomIdByHash(CMloModelInfo* arch, int searchHash)
{
	auto count = arch->rooms->GetCount();

	for (int i = 0; i < count; ++i)
	{
		CMloRoomDef* room = &(arch->rooms->Get(i));

		if (HashString(room->name) == searchHash)
		{
			return i;
		}
	}

	return -1;
}

static void (*g_origsub_14034F024)(__int64 a1, void* envGroup, uint64_t a3, float* a4, float* a5);

static void sub_14034F024(__int64 a1, void* envGroup, uint64_t a3, float* a4, float* a5)
{
	return g_origsub_14034F024(a1, envGroup, a3, a4, a5);
}

#include <shared_mutex>
#include <map>
#include <ScriptSerialization.h>

template<std::size_t Index, typename ReturnType, typename... Args>
inline ReturnType call_virtual(void* instance, Args... args)
{
	using Fn = ReturnType(__thiscall*)(void*, Args...);

	auto function = (*reinterpret_cast<Fn**>(instance))[Index];
	return function(instance, args...);
}

struct CPedFactory
{
	virtual ~CPedFactory() = 0;

	CPed* localPlayerPed;
};

static CPedFactory** g_pedFactory;

struct VirtualBase
{
	virtual ~VirtualBase()
	{
	}
};

struct VirtualDerivative : public VirtualBase
{
	virtual ~VirtualDerivative() override
	{
	}
};

struct SoundRoutingEntry
{
	float routeEndX;
	float routeEndY;
	float routeEndZ;

	float routeStartX;
	float routeStartY;
	float routeStartZ;

	float occlusionFactor;

	std::string envGroupName;
	std::string audEntName;

	uint64_t envGroup;

	struct SoundRoutingHop
	{
		float x;
		float y;
		float z;

		float occlusionFactor;
		float distance;
		MSGPACK_DEFINE_ARRAY(x, y, z, occlusionFactor, distance)
	};
	std::vector<SoundRoutingHop> hops;

	MSGPACK_DEFINE_ARRAY(routeEndX, routeEndY, routeEndZ, routeStartX, routeStartY, routeStartZ, occlusionFactor, envGroupName, audEntName, envGroup, hops)
};
std::vector<SoundRoutingEntry> g_soundRoutingEntries;

class __struct_v9
{
public:
	DirectX::XMFLOAT4 vec1; //0x0000
	DirectX::XMFLOAT4 vec2; //0x0010
	DirectX::XMFLOAT4 vec3; //0x0020
	__struct_v9* nextInPath; //0x0030
	DirectX::XMFLOAT4X4* portalCoords; //0x0038
	uint32_t occlusionHash; //0x0040
	uint32_t roomIndex; //0x0044
	float audCurveFactor; //0x0048
	float occlusionFactor; //0x004C
	float distance; //0x0050
	float N00002B20; //0x0054
	char pad_0058[4]; //0x0058
	bool processedToEnd; //0x005C
	char pad_005D[3]; //0x005D
	atArray<__struct_v9> linkedNodes;//0x0060
}; //Size: 0x0070
constexpr int dsd = sizeof(__struct_v9);

std::map<int8_t*, const char*> g_EnvironmentGroups;
static int8_t* (*g_orig__naEnvironmentGroup__Allocate)(const char* name);
static int8_t* naEnvironmentGroup__Allocate(const char* name)
{
	auto envGroup = g_orig__naEnvironmentGroup__Allocate(name);
	g_EnvironmentGroups.insert({ envGroup, name });

	return envGroup;
}

static void (*g_orig__naEnvironmentGroupManager__Update)(__int64 a1, __int64 envGroups, int MaxEnvironmentGroupsToUpdate, unsigned int gameTime);
static void naEnvironmentGroupManager__Update(__int64 a1, __int64 envGroups, int MaxEnvironmentGroupsToUpdate, unsigned int gameTime)
{
	g_orig__naEnvironmentGroupManager__Update(a1, envGroups, MaxEnvironmentGroupsToUpdate, gameTime);
}

static __int64 (*g_origsub_1403BC95C)(__int64 a1, __struct_v9* a2, uint64_t envGroup, Vector3* a4);
static __int64 sub_1403BC95C(__int64 a1, __struct_v9* a2, uint64_t envGroup, Vector3* a4)
{
	__int64 retval = g_origsub_1403BC95C(a1, a2, envGroup, a4);

	if (g_pedFactory && (*g_pedFactory)->localPlayerPed)
	{
		void* localAudioEntity = call_virtual<22, void*>((*g_pedFactory)->localPlayerPed);
		uint64_t localEnvGroup = call_virtual<19, uint64_t, bool>(localAudioEntity, false);
		if (localEnvGroup == envGroup)
			return retval;
	}

	if (a2->processedToEnd && (a4->x != 0.0f && a4->y != 0.0f && a4->z != 0.0f))
	{
		const char* envGroupName = "unkown envGroup";
		const char* audEntName = "unkown audEntity";

		rage::fwInteriorLocation interiorLocation;
		float audEntityPos[4] = { 0 };

		if (envGroup)
		{
			auto it = g_EnvironmentGroups.find((int8_t*)envGroup);
			if (it != g_EnvironmentGroups.end())
				envGroupName = it->second;

			interiorLocation = *(rage::fwInteriorLocation*)(envGroup + 0xF4);
			if (*(int8_t*)(envGroup + 0x58) && *(int8_t**)(envGroup + 0x58))
			{
				int8_t* audEntity = *(int8_t**)(envGroup + 0x58);
				if (audEntity != nullptr)
				{
					call_virtual<10, uint64_t, void*>(audEntity, (void*)&audEntityPos[0]);

					VirtualBase* self = (VirtualBase*)audEntity;
					audEntName = typeid(*self).name();
				}
			}
		}

		/*if (audEntityPos[0] == 0.0f && audEntityPos[1] == 0.0f && audEntityPos[2] == 0.0f)
			return retval;*/

		//*(uint8_t*)(a2 + 0x60)
		std::vector<SoundRoutingEntry::SoundRoutingHop> hops;
		std::vector<std::pair<uint8_t, __struct_v9*>> routes;
		auto recursive_path_finder = [a2, &routes, &interiorLocation](__struct_v9* root, auto self) -> void
		{
			for (uint16_t i = 0; i < root->linkedNodes.GetCount(); i++)
			{
				__struct_v9* child = &root->linkedNodes.Get(i);
				if (child->nextInPath != root)
					continue;

				if (child->occlusionFactor > a2->occlusionFactor)
					continue;

				/*if (floor(child->distance + 0.5f) > floor(a2->distance + 0.5f))
					continue;*/

				__struct_v9* child2 = child;
				uint8_t length = 0;

				while (true)
				{
					bool isValidNode = child->roomIndex <= 32 && child->occlusionFactor <= 1.0f && child->nextInPath && child->nextInPath != child;
					if (isValidNode)
					{
						child = child->nextInPath;
						length++;
					}
					else
						break;

					//Todo: Figure out the actual amount of max hops 
					if (length > 8)
						break;
				}
				if (child != a2)
					continue;

				bool isCorrectRoom = !interiorLocation.IsPortal() && interiorLocation.GetRoomIndex() == child2->roomIndex;
				if (isCorrectRoom)
					routes.push_back({ length, child2 });

				self(child2, self);
			}
		};
		recursive_path_finder(a2, recursive_path_finder);

		if (routes.size())
		{
			std::sort(routes.begin(), routes.end(), [](std::pair<uint8_t, __struct_v9*>& lhs, std::pair<uint8_t, __struct_v9*>& rhs)
			{
				__struct_v9* step = lhs.second;
				float lhsDistance = 0.0f;
				while (step && step->nextInPath != nullptr)
				{
					lhsDistance += step->occlusionFactor;
					step = step->nextInPath;
				}

				step = rhs.second;
				float rhsDistance = 0.0f;
				while (step && step->nextInPath != nullptr)
				{
					rhsDistance += step->occlusionFactor;
					step = step->nextInPath;
				}

				return lhsDistance < rhsDistance;
			});

			__struct_v9* step = routes[0].second;
			while (step)
			{
				hops.push_back({ step->vec2.x, step->vec2.y, step->vec2.z, step->occlusionFactor, step->distance });
				step = step->nextInPath;
			}
		}		

		/*std::vector <std::pair<uint64_t, std::vector<SoundRoutingEntry::SoundRoutingHop>>> hops = {};

		auto recursive_path_finder = [&hops, &interiorLocation](__struct_v9* root, auto self) -> bool
		{
			if (!root->processedToEnd)
				return false;

			for (uint16_t i = 0; i < root->linkedNodes.GetCount(); i++)
			{
				__struct_v9* it = &root->linkedNodes.Get(i);
				if (!interiorLocation.IsPortal() && interiorLocation.GetRoomIndex() == it->roomIndex && std::find(hops))
				{
					std::vector<SoundRoutingEntry::SoundRoutingHop> path = {};
					uint64_t startNode = (uint64_t)it;
					while (it->nextInPath)
					{
						path.push_back({ it->vec1.x, it->vec1.y, it->vec1.z, it->occlusionFactor, it->distance });

						it = it->nextInPath;
					}

					hops.push_back({ startNode, path });
				}
				if (self(it, self))
					return true;
			}
			return false;
		};
		recursive_path_finder(a2, recursive_path_finder);

		std::vector<SoundRoutingEntry::SoundRoutingHop>* bestPath;
		size_t shortestPath = 99;
		for (auto& it : hops)
		{
			if (it.second.size() < shortestPath)
			{
				bestPath = &it.second;
				shortestPath = it.second.size();
			}
		}*/

		auto entry = std::find_if(g_soundRoutingEntries.begin(), g_soundRoutingEntries.end(), [envGroup](const SoundRoutingEntry& entry)
		{
			return bool(entry.envGroup == envGroup);
		});

		if (entry != g_soundRoutingEntries.end())
		{
			*entry = { a4->x, a4->y, a4->z, audEntityPos[0], audEntityPos[1], audEntityPos[2], a2->occlusionFactor, envGroupName, audEntName, envGroup, hops };
		}
		else
			g_soundRoutingEntries.push_back({ a4->x, a4->y, a4->z, audEntityPos[0], audEntityPos[1], audEntityPos[2], a2->occlusionFactor, envGroupName, audEntName, envGroup, hops });
	}

	return retval;
}

static HookFunction initFunction([]()
{
	{
		auto location = hook::get_call(hook::get_pattern("E8 ? ? ? ? 84 C0 74 3C F3 0F 10 4D"));
		MH_Initialize();
		MH_CreateHook(location, sub_14034F024, (void**)&g_origsub_14034F024);
		MH_EnableHook(location);

		location = hook::get_call(hook::get_pattern("E8 ? ? ? ? 40 38 BE ? ? ? ? 0F 84 ? ? ? ? F3 0F 10 8E"));
		MH_CreateHook(location, sub_1403BC95C, (void**)&g_origsub_1403BC95C);
		MH_EnableHook(location);

		location = hook::get_call(hook::get_pattern("E8 ? ? ? ? 48 8B F8 48 85 C0 74 73 F3"));
		MH_CreateHook(location, naEnvironmentGroup__Allocate, (void**)&g_orig__naEnvironmentGroup__Allocate);
		MH_EnableHook(location);

		location = hook::get_pattern("48 8D 0D ? ? ? ? 41 8B F1 41 8B D8 48 8B EA E8 ? ? ? ? 84 C0", -0x1A);
		MH_CreateHook(location, naEnvironmentGroupManager__Update, (void**)&g_orig__naEnvironmentGroupManager__Update);
		MH_EnableHook(location);

		g_pedFactory = hook::get_address<decltype(g_pedFactory)>(hook::get_pattern("E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 8B 58 08 48 8B CB E8", 8));

		fx::ScriptEngine::RegisterNativeHandler("GET_OCCLUSION_SOUND_SOURCES", [=](fx::ScriptContext& context)
		{
			context.SetResult(fx::SerializeObject(g_soundRoutingEntries));
			return true;
		});

		fx::ScriptEngine::RegisterNativeHandler("RESET_OCCLUSION_SOUND_SOURCES", [=](fx::ScriptContext& context)
		{
			g_soundRoutingEntries.clear();
			return true;
		});
	}

	{
		auto location = hook::get_pattern<char>("BA A1 85 94 52 41 B8 01", 0x34);

		if (xbr::IsGameBuildOrGreater<2060>())
		{
			g_interiorProxyPool<2060> = hook::get_address<decltype(g_interiorProxyPool<2060>)>(location);
		}
		else
		{
			g_interiorProxyPool<1604> = hook::get_address<decltype(g_interiorProxyPool<1604>)>(location);
		}
	}

#ifdef _DEBUG
	fx::ScriptEngine::RegisterNativeHandler("INTERIOR_DEBUG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);

		CMloModelInfo* arch = GetInteriorArchetype(interiorId);
		if (arch == nullptr)
		{
			return;
		}

		auto roomCount = arch->rooms->GetCount();
		auto portalCount = arch->portals->GetCount();
		auto entitySetCount = arch->entitySets->GetCount();
		auto timecycleModifierCount = arch->timecycleModifiers->GetCount();

		for (int roomId = 0; roomId < roomCount; ++roomId)
		{
			auto room = GetInteriorRoomDef(interiorId, roomId);
			trace("\nSearching for CMloRoomDef %d/%d at %08x\n", roomId + 1, roomCount, (uint64_t)room);

			trace(" - Found room %s with index %d\n", std::string(room->name), roomId);
			trace(" - BbMin: %f %f %f\n", room->bbMin.x, room->bbMin.y, room->bbMin.z);
			trace(" - BbMax: %f %f %f\n", room->bbMax.x, room->bbMax.y, room->bbMax.z);
			trace(" - Blend: %f | Flags: %d | Portals: %d\n", room->blend, room->flags, room->portalsCount);
			trace(" - TimecycleName Hash: %d / %d\n", room->timecycleName, room->secondaryTimecycleName);
			trace(" - FloorId: %d | ExteriorVisibilityDepth: %d\n", room->floorId, room->exteriorVisibilityDepth);
			trace(" - AttachedObjects: %d\n", room->attachedObjects.GetCount());
		}

		for (int portalId = 0; portalId < portalCount; ++portalId)
		{
			auto portal = GetInteriorPortalDef(interiorId, portalId);
			trace("\nSearching for CMloPortalDef %d/%d at %08x\n", portalId + 1, portalCount, (uint64_t)portal);


			trace(" - Found portal with index %d\n", portalId);
			trace(" - RoomFrom: %d | RoomTo: %d\n", portal->roomFrom, portal->roomTo);
			trace(" - Flags: %d | Opacity: %f\n", portal->flags, portal->opacity);
			trace(" - MirrorPrior: %d | AudioOcclusion: %d\n", portal->mirrorPriority, portal->audioOcclusion);

			for (int c = 0; c < portal->corners.GetCount(); ++c)
			{
				trace(" - Corner%d: %f %f %f\n", c, portal->corners[c].x, portal->corners[c].y, portal->corners[c].z);
			}

			trace(" - AttachedObjects: %d\n", portal->attachedObjects.GetCount());
		}

		for (int setId = 0; setId < entitySetCount; ++setId)
		{
			auto entitySet = GetInteriorEntitySet(interiorId, setId);
			trace("\nSearching for CMloEntitySet %d/%d at %08x\n", setId + 1, entitySetCount, (uint64_t)entitySet);

			trace(" - Found entitySet with hash %08x\n", entitySet->name);
			trace(" - Locations: %d | Entities: %d\n", entitySet->locations.GetCount(), entitySet->entities.GetCount());
		}

		for (int modId = 0; modId < timecycleModifierCount; ++modId)
		{
			auto modifier = GetInteriorTimecycleModifier(interiorId, modId);
			trace("\nSearching for CMloTimeCycleModifier %d/%d at %08x\n", modId + 1, timecycleModifierCount, (uint64_t)modifier);

			trace(" - Found timecycleModifier with hash %08x\n", modifier->name);
			trace(" - Sphere: %f %f %f %f\n", modifier->sphere.x, modifier->sphere.y, modifier->sphere.z, modifier->sphere.w);
			trace(" - Percentage: %f | Range: %f\n", modifier->percentage, modifier->range);
			trace(" - Hours: %d - %d\n", modifier->startHour, modifier->endHour);
		}
	});
#endif

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_INDEX_BY_HASH", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto searchHash = context.GetArgument<int>(1);
		auto result = -1;

		CMloModelInfo* arch = GetInteriorArchetype(interiorId);
		if (arch != nullptr)
		{
			result = GetInteriorRoomIdByHash(arch, searchHash);
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_NAME", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);
		char* result = "";

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef != nullptr)
		{
			result = roomDef->name;
		}

		context.SetResult<char*>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);
		auto result = -1;

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef != nullptr)
		{
			result = roomDef->flags;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_ROOM_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);
		auto newFlag = context.GetArgument<int>(2);

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef == nullptr)
		{
			return false;
		}

		roomDef->flags = newFlag;
		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_EXTENTS", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef == nullptr)
		{
			return false;
		}

		*context.GetArgument<float*>(2) = roomDef->bbMin.x;
		*context.GetArgument<float*>(3) = roomDef->bbMin.y;
		*context.GetArgument<float*>(4) = roomDef->bbMin.z;

		*context.GetArgument<float*>(5) = roomDef->bbMax.x;
		*context.GetArgument<float*>(6) = roomDef->bbMax.y;
		*context.GetArgument<float*>(7) = roomDef->bbMax.z;

		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_ROOM_EXTENTS", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef == nullptr)
		{
			return false;
		}

		roomDef->bbMin.x = context.GetArgument<float>(2);
		roomDef->bbMin.y = context.GetArgument<float>(3);
		roomDef->bbMin.z = context.GetArgument<float>(4);

		roomDef->bbMax.x = context.GetArgument<float>(5);
		roomDef->bbMax.y = context.GetArgument<float>(6);
		roomDef->bbMax.z = context.GetArgument<float>(7);

		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_TIMECYCLE", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);
		int result = 0;

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef != nullptr)
		{
			result = roomDef->timecycleName;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_ROOM_TIMECYCLE", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto roomId = context.GetArgument<int>(1);
		auto timecycleHash = context.GetArgument<int>(2);

		CMloRoomDef* roomDef = GetInteriorRoomDef(interiorId, roomId);
		if (roomDef == nullptr)
		{
			return false;
		}

		roomDef->timecycleName = timecycleHash;
		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_CORNER_POSITION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto cornerIndex = context.GetArgument<int>(2);

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);

		if (cornerIndex < 0 || cornerIndex > portalDef->corners.GetCount())
		{
			return false;
		}

		*context.GetArgument<float*>(3) = portalDef->corners[cornerIndex].x;
		*context.GetArgument<float*>(4) = portalDef->corners[cornerIndex].y;
		*context.GetArgument<float*>(5) = portalDef->corners[cornerIndex].z;

		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_PORTAL_CORNER_POSITION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto cornerIndex = context.GetArgument<int>(2);

		auto posX = context.GetArgument<float>(3);
		auto posY = context.GetArgument<float>(4);
		auto posZ = context.GetArgument<float>(5);

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef == nullptr)
		{
			return false;
		}

		portalDef->corners[cornerIndex].x = posX;
		portalDef->corners[cornerIndex].y = posY;
		portalDef->corners[cornerIndex].z = posZ;

		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ROOM_FROM", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto result = -1;

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			result = portalDef->roomFrom;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_PORTAL_ROOM_FROM", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto newValue = context.GetArgument<int>(2);

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			portalDef->roomFrom = newValue;
			return true;
		}

		return false;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ROOM_TO", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto result = -1;

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			result = portalDef->roomTo;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_PORTAL_ROOM_TO", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto newValue = context.GetArgument<int>(2);

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			portalDef->roomTo = newValue;
			return true;
		}

		return false;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto result = -1;

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			result = portalDef->flags;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_PORTAL_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto newValue = context.GetArgument<int>(2);

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			portalDef->flags = newValue;
			return true;
		}

		return false;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_POSITION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);

		Vector* position;

		if (xbr::IsGameBuildOrGreater<2060>())
		{
			CInteriorProxy<2060>* proxy = GetInteriorProxy<2060>(interiorId);
			position = &proxy->position;
		}
		else
		{
			CInteriorProxy<1604>* proxy = GetInteriorProxy<1604>(interiorId);
			position = &proxy->position;
		}

		*context.GetArgument<float*>(1) = position->x;
		*context.GetArgument<float*>(2) = position->y;
		*context.GetArgument<float*>(3) = position->z;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROTATION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);

		Vector* rotation;

		if (xbr::IsGameBuildOrGreater<2060>())
		{
			CInteriorProxy<2060>* proxy = GetInteriorProxy<2060>(interiorId);
			rotation = &proxy->rotation;
		}
		else
		{
			CInteriorProxy<1604>* proxy = GetInteriorProxy<1604>(interiorId);
			rotation = &proxy->rotation;
		}

		*context.GetArgument<float*>(1) = rotation->x;
		*context.GetArgument<float*>(2) = rotation->y;
		*context.GetArgument<float*>(3) = rotation->z;
		*context.GetArgument<float*>(4) = rotation->w;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ENTITIES_EXTENTS", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);

		Vector* bbMin;
		Vector* bbMax;

		if (xbr::IsGameBuildOrGreater<2060>())
		{
			CInteriorProxy<2060>* proxy = GetInteriorProxy<2060>(interiorId);
			bbMin = &proxy->entitiesExtentsMin;
			bbMax = &proxy->entitiesExtentsMax;
		}
		else
		{
			CInteriorProxy<1604>* proxy = GetInteriorProxy<1604>(interiorId);
			bbMin = &proxy->entitiesExtentsMin;
			bbMax = &proxy->entitiesExtentsMax;
		}

		*context.GetArgument<float*>(1) = bbMin->x;
		*context.GetArgument<float*>(2) = bbMin->y;
		*context.GetArgument<float*>(3) = bbMin->z;

		*context.GetArgument<float*>(4) = bbMax->x;
		*context.GetArgument<float*>(5) = bbMax->y;
		*context.GetArgument<float*>(6) = bbMax->z;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_COUNT", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto result = 0;

		auto arch = GetInteriorArchetype(interiorId);
		if (arch != nullptr)
		{
			result = arch->portals->GetCount();
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_ROOM_COUNT", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto result = 0;

		auto arch = GetInteriorArchetype(interiorId);
		if (arch != nullptr)
		{
			result = arch->rooms->GetCount();
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ENTITY_COUNT", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto result = 0;

		CMloPortalDef* portalDef = GetInteriorPortalDef(interiorId, portalId);
		if (portalDef != nullptr)
		{
			result = portalDef->attachedObjects.GetCount();
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ENTITY_ARCHETYPE", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto entityId = context.GetArgument<int>(2);
		auto result = 0;

		iCEntityDef* entityDef = GetInteriorPortalEntityDef(interiorId, portalId, entityId);
		if (entityDef != nullptr)
		{
			result = entityDef->archetypeName;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ENTITY_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto entityId = context.GetArgument<int>(2);
		auto result = -1;

		iCEntityDef* entityDef = GetInteriorPortalEntityDef(interiorId, portalId, entityId);
		if (entityDef != nullptr)
		{
			result = entityDef->flags;
		}

		context.SetResult<int>(result);
	});

	fx::ScriptEngine::RegisterNativeHandler("SET_INTERIOR_PORTAL_ENTITY_FLAG", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto entityId = context.GetArgument<int>(2);
		auto newValue = context.GetArgument<int>(3);

		iCEntityDef* entityDef = GetInteriorPortalEntityDef(interiorId, portalId, entityId);
		if (entityDef != nullptr)
		{
			entityDef->flags = newValue;
			return true;
		}

		return false;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ENTITY_POSITION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto entityId = context.GetArgument<int>(2);

		iCEntityDef* entityDef = GetInteriorPortalEntityDef(interiorId, portalId, entityId);
		if (entityDef == nullptr)
		{
			return false;
		}

		*context.GetArgument<float*>(3) = entityDef->position.x;
		*context.GetArgument<float*>(4) = entityDef->position.y;
		*context.GetArgument<float*>(5) = entityDef->position.z;

		return true;
	});

	fx::ScriptEngine::RegisterNativeHandler("GET_INTERIOR_PORTAL_ENTITY_ROTATION", [=](fx::ScriptContext& context)
	{
		auto interiorId = context.GetArgument<int>(0);
		auto portalId = context.GetArgument<int>(1);
		auto entityId = context.GetArgument<int>(2);

		iCEntityDef* entityDef = GetInteriorPortalEntityDef(interiorId, portalId, entityId);
		if (entityDef == nullptr)
		{
			return false;
		}

		*context.GetArgument<float*>(3) = entityDef->rotation.x;
		*context.GetArgument<float*>(4) = entityDef->rotation.y;
		*context.GetArgument<float*>(5) = entityDef->rotation.z;
		*context.GetArgument<float*>(6) = entityDef->rotation.w;

		return true;
	});
});
