#pragma once
#include "SSEngineDefault/ModuleExportKeyword.h"

#include "SSEngineDefault/Public/SSEngineInlineSettings.h"


namespace SS {
	class StringW;
}

namespace SS {

	// String Hasher -> �̸� �ؽ̵� String���� ���Ͽ� ��Ʈ�� �� ȿ���� �����ִ� Ŭ����
	class SSENGINEDEFAULT_MODULE SHasherW
	{
	public:
		static SHasherW Empty;


	private:
		union {
			struct {
				uint32 _HashedValue; // �ؽ� ���� 32��Ʈ
				uint32 _CurNodeCnt; // �ؽ� ���� 32��Ʈ
			};
			uint64 _hashX; // �ؽ� 64��Ʈ ��ü��
		};

	public:
		SHasherW();
		SHasherW(const utf16* str);
		SHasherW(const char* inStr);
		explicit SHasherW(const SS::StringW& inStr);

		SHasherW(const SS::SHasherW& rhs);
		SHasherW& operator=(SHasherW rhs);

		bool operator==(SHasherW rhs) const;

		bool IsEmpty() const;
		const utf16* C_Str(uint32* const OutStrLen = nullptr) const;
		uint64 GetDirectValue() const { return _hashX; }

	};
};