#include "StdInc.h"

#include "Hooking.h"
#include <MinHook.h>
#include <d3d11_1.h>

enum grcShaderType
{
	GRC_VERTEX = 0,
	GRC_FRAGMENT = 1,
	GRC_COMPUTE = 2
};

const char* shaderTypes[] = { "vsh", "psh" };

#define LOW_IND(x, part_type) 0
#define BYTEn(x, n) (*((unsigned char*)&(x) + n))
#define _LOBYTE(x) BYTEn(x, LOW_IND(x, unsigned char))

__int64 __fastcall sub_180011B70(__int64 a1, unsigned __int8* bytecodeBuffer, int bytecodeLength)
{
	unsigned int i; // er11
	unsigned __int64 v4; // r10

	for (i = -1; bytecodeLength; --bytecodeLength)
	{
		v4 = (unsigned __int8)i ^ (unsigned __int64)*bytecodeBuffer++;
		i = (i >> 8) ^ *(uint32_t*)(a1 + 4 * v4);
	}
	return i;
}

__int64 __fastcall sub_180011BB0(unsigned __int8* bytecodeBuffer, int bytecodeLength, int a3)
{
	unsigned int v3; // er9
	int i; // er10
	int v6; // eax
	unsigned int v7; // eax
	unsigned int v8; // eax
	unsigned int v9; // eax

	v3 = -1;
	for (i = bytecodeLength; i; --i)
	{
		v6 = *bytecodeBuffer++;
		v7 = ((v6 ^ v3) >> 1) ^ a3 & -(((unsigned __int8)v6 ^ (unsigned __int8)v3) & 1);
		v8 = (((v7 >> 1) ^ a3 & -(v7 & 1)) >> 1) ^ a3 & -(((unsigned __int8)(v7 >> 1) ^ (unsigned __int8)(a3 & -(v7 & 1))) & 1);
		v9 = (((v8 >> 1) ^ a3 & -(v8 & 1)) >> 1) ^ a3 & -(((unsigned __int8)(v8 >> 1) ^ (unsigned __int8)(a3 & -(v8 & 1))) & 1);
		v3 = (((((v9 >> 1) ^ a3 & -(v9 & 1)) >> 1) ^ a3 & -(((unsigned __int8)(v9 >> 1) ^ (unsigned __int8)(a3 & -(v9 & 1))) & 1)) >> 1) ^ a3 & -(((unsigned __int8)(((v9 >> 1) ^ a3 & -(v9 & 1)) >> 1) ^ (unsigned __int8)(a3 & -(((v9 >> 1) ^ a3 & -(v9 & 1)) & 1))) & 1);
	}
	return v3;
}

__int64 __fastcall sub_180011AD0(__int64 a1, int a2)
{
	int v2; // er10
	__int64 v3; // r11
	char v4; // r8
	unsigned int v5; // eax
	unsigned int v6; // er8
	unsigned int v7; // eax
	unsigned int v8; // eax

	v2 = 0;
	v3 = a1;
	do
	{
		v4 = v2;
		v5 = v2++;
		v3 += 4i64;
		v6 = (v5 >> 1) ^ a2 & -(v4 & 1);
		_LOBYTE(v5) = v6;
		v6 >>= 1;
		v7 = ((((v6 ^ a2 & -(v5 & 1)) >> 1) ^ a2 & -(((unsigned __int8)v6 ^ (unsigned __int8)(a2 & -(v5 & 1))) & 1)) >> 1) ^ a2 & -(((unsigned __int8)((v6 ^ a2 & -(v5 & 1)) >> 1) ^ (unsigned __int8)(a2 & -((v6 ^ a2 & -(v5 & 1)) & 1))) & 1);
		v8 = (((v7 >> 1) ^ a2 & -(v7 & 1)) >> 1) ^ a2 & -(((unsigned __int8)(v7 >> 1) ^ (unsigned __int8)(a2 & -(v7 & 1))) & 1);
		*(uint32_t*)(v3 - 4) = (((v8 >> 1) ^ a2 & -(v8 & 1)) >> 1) ^ a2 & -(((unsigned __int8)(v8 >> 1) ^ (unsigned __int8)(a2 & -(v8 & 1))) & 1);
	} while (v2 < 256);

	return a1;
}

__int64 __fastcall generateShaderHash(void* bytecodeBuffer, unsigned int bytecodeLength)
{
	unsigned int v5; // ebx
	char v6[1032]; // [rsp+20h] [rbp-408h] BYREF

	if (!bytecodeBuffer || !bytecodeLength)
		return 0i64;

	sub_180011AD0((__int64)v6, 0xEDB88320);
	v5 = sub_180011B70((__int64)v6, (unsigned __int8*)bytecodeBuffer, bytecodeLength);
	if (v5 != (unsigned int)sub_180011BB0((unsigned __int8*)bytecodeBuffer, bytecodeLength, 0xEDB88320))
		v5 = 0;

	return v5;
}

FILE* file = nullptr;

typedef ID3D11DeviceChild* (__fastcall* shaderLoaderStruct__createD3D11Shader_t)(const char* shaderDebugName, void* byteCode, unsigned int byteCodeLength, grcShaderType shaderType, uint64_t a5);
shaderLoaderStruct__createD3D11Shader_t org_shaderLoaderStruct__createD3D11Shader;

ID3D11DeviceChild* __fastcall shaderLoaderStruct__createD3D11Shader(const char* shaderDebugName, void* byteCode, unsigned int byteCodeLength, grcShaderType shaderType, uint64_t a5)
{
	if (shaderType < 2) 
	{
		char enbStyledName[255] = {};
		sprintf_s(enbStyledName, 255, "%s%08X", shaderTypes[shaderType], (uint32_t)generateShaderHash(byteCode, byteCodeLength));
		fprintf(file, "%-125s%-20s\n", shaderDebugName, enbStyledName);
		fflush(file);
	}
	
	return org_shaderLoaderStruct__createD3D11Shader(shaderDebugName, byteCode, byteCodeLength, shaderType, a5);
}

static HookFunction hookFunction([] ()
{
	file = fopen("C:\\data\\scratch\\shaderDump.txt", "a");

	const auto address = hook::get_call<void*>(hook::get_pattern("E8 ? ? ? ? 48 89 87 ? ? ? ? 8B 45 E0"));

	MH_Initialize();
	MH_CreateHook(address, shaderLoaderStruct__createD3D11Shader, (void**)&org_shaderLoaderStruct__createD3D11Shader);
	MH_EnableHook(MH_ALL_HOOKS);
});
