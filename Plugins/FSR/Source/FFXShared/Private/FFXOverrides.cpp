// This file is part of the FSR Upscaling Unreal Engine Plugin.
//
// Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "FFXOverrides.h"
#include "FFXSharedBackend.h"
#include "DynamicRHI.h"

#include "Containers/Map.h"

static inline ffxStructType_t GetEffectId(ffxCreateContextDescHeader& Header)
{
	return (Header.type & FFX_API_EFFECT_MASK);
}

static inline IFFXSharedBackend* GetApiAccessor()
{
	IFFXSharedBackend* ApiAccessor = nullptr;

	IFFXSharedBackendModule* DX12Backend = FModuleManager::GetModulePtr<IFFXSharedBackendModule>(TEXT("FFXD3D12Backend"));
	if (DX12Backend)
	{
		ApiAccessor = DX12Backend->GetBackend();
	}
	check(ApiAccessor);
	return ApiAccessor;
}

static inline ffxOverrideVersion* GetVersionOverride(ffxStructType_t effectId)
{
	static TMap<ffxStructType_t, ffxOverrideVersion> EffectToVersionOverride =
	{
		{ FFX_API_EFFECT_ID_UPSCALE, ffxOverrideVersion() },
		{ FFX_API_EFFECT_ID_FRAMEGENERATION, ffxOverrideVersion() },
	};

	ffxOverrideVersion* VersionOverride = EffectToVersionOverride.Find(effectId);
	check(VersionOverride);	
	return VersionOverride;
}

bool FfxTryOverrideContextVersion(ffxCreateContextDescHeader& Header, int VersionNumber, FString* VersionString)
{
	IFFXSharedBackend* ApiAccessor = GetApiAccessor();

	ffxStructType_t effectId = GetEffectId(Header);
	ffxOverrideVersion* VersionOverride = GetVersionOverride(effectId);
	
	if (ApiAccessor && VersionOverride)
	{
		uint64_t numFFXVersions;

		ffxQueryDescGetVersions Desc = {};
		Desc.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
		Desc.header.pNext = nullptr;
		Desc.createDescType = effectId;
		Desc.device = GDynamicRHI->RHIGetNativeDevice();
		Desc.outputCount = &numFFXVersions;
		if (ApiAccessor->ffxQuery(nullptr, (ffxQueryDescHeader*)&Desc) == FFX_API_RETURN_OK && numFFXVersions > 0)
		{
			const uint64_t kVersionStringMaxLength = 16;

			// make sure upcoming dynamic allocations will stay pretty small, so we can safely stack-allocate.
			const uint64_t kVersionSize = numFFXVersions * sizeof(uint64_t);
			const uint64_t kBufferSize = numFFXVersions * sizeof(char) * kVersionStringMaxLength;
			const uint64_t kPointerSize = numFFXVersions * sizeof(char*);
			check(kBufferSize + kPointerSize + kVersionSize < 256);

			Desc.versionIds = (uint64_t*)alloca(kVersionSize);

			char* targetBuffers = (char*)alloca(kBufferSize);
			Desc.versionNames = (const char**)alloca(kPointerSize);
			for (int i = 0; i < numFFXVersions; i++)
			{
				Desc.versionNames[i] = (const char*)(targetBuffers + kVersionStringMaxLength * i / sizeof(char));
			}

			if (ApiAccessor->ffxQuery(nullptr, (ffxQueryDescHeader*)&Desc) == FFX_API_RETURN_OK)
			{
				for (int i = 0; i < numFFXVersions; i++)
				{
					int versionMajor = (int)(Desc.versionNames[i][0] - '0');
					if (versionMajor == VersionNumber)
					{
						VersionOverride->header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
						VersionOverride->header.pNext = Header.pNext;
						VersionOverride->versionId = Desc.versionIds[i];
						Header.pNext = &VersionOverride->header;

						if (VersionString)
						{
							*VersionString = Desc.versionNames[i];
						}
						return true;
					}
				}
			}
		}
	}

	return false;
}