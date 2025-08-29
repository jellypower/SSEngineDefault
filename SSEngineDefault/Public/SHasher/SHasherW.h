#pragma once
#include "IHasherPool.h"
#include "SSEngineDefault/Public/SSDebugLogger.h"

#include "SSEngineDefault/Public/SSEngineInlineSettings.h"
#include "SSEngineDefault/Public/GlobalVariableSet/GlobalVariableSet.h"
#include "SSEngineDefault/Public/SSContainer/CityHash.h"
#include "SSEngineDefault/Public/SSContainer/SSString/SSStringW.h"
#include "SSEngineDefault/Public/SSContainer/SSString/StringUtilityFunctions.h"


namespace SS {
	class StringW;
}

namespace SS {

	// String Hasher -> �̸� �ؽ̵� String���� ���Ͽ� ��Ʈ�� �� ȿ���� �����ִ� Ŭ����
	class SHasherW
	{
	public:
		static SHasherW GetEmpty() { return SHasherW(); }


	private:
		union {
			struct {
				uint32 _HashedValue; // �ؽ� ���� 32��Ʈ
				uint32 _CurNodeCnt; // �ؽ� ���� 32��Ʈ
			};
			uint64 _hashX; // �ؽ� 64��Ʈ ��ü��
		};

	public:
		SHasherW()
		{
			_hashX = 0;
		}

		SHasherW(const utf16* inStr)
		{
			if (inStr == nullptr)
			{
				SS_INTERRUPT();
			}

			uint32 StrLen = wcslen(inStr);
			if (StrLen > SHASHER_STRLEN_MAX)
			{
				SS_INTERRUPT();
			}

			utf16 loweredStr[SHASHER_STRLEN_MAX + 1];
			LowerStr(inStr, loweredStr);
			uint32 HashedValue = CityHash32(reinterpret_cast<const char*>(loweredStr), StrLen * (sizeof(utf16) / sizeof(char)));
			_hashX = g_HasherPool->FindOrAddHasherValue(loweredStr, StrLen, HashedValue);
		}

		SHasherW(const char* inStr)
		{
			SS::StringW InStrW = inStr;
			new(this) SHasherW(InStrW.C_Str()); // �ٸ� ������ ȣ��
		}


		explicit SHasherW(const SS::StringW& inStr)
		{
			new(this) SHasherW(inStr.C_Str());
		}

		SHasherW(const SS::SHasherW& rhs)
		{
			_hashX = rhs._hashX;
		}

		SHasherW& operator=(SHasherW rhs)
		{
			_hashX = rhs._hashX;
			return *this;
		}





		bool operator==(SHasherW rhs) const
		{
			return this->_hashX == rhs._hashX;
		}

		bool IsEmpty() const
		{
			return _hashX == 0;
		}

		const utf16* C_Str(uint32* const OutStrLen = nullptr) const
		{
			const utf16* Result = g_HasherPool->FindC_Str(_hashX, OutStrLen);
			return Result;
		}

		uint64 GetDirectValue() const { return _hashX; }

	};
};