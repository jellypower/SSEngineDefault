#pragma once
#include "SSEngineDefault/ModuleExportKeyword.h"

#include "SSEngineDefault/Public/SSNativeKeywords.h"
#include "SSEngineDefault/Public/SSVector.h"
#include "SSEngineDefault/Public/GlobalVariableSet/GlobalVariableSet.h"


class SSENGINEDEFAULT_MODULE FrameInfoProcessorBase
{
private:
	uint64 _perfFrequency = 0;

	uint64 _frameCount = 0;
	double _deltaTime = 0;
	double _elapsedTime = 0;
	uint64 _previousTick = 0;
	uint64 _currentTick = 0;

	double _FPSCheckStopWatch = 0;
	uint64 _frameCntDuringInFPSCheckterval = 0;
	uint64 _FPS = 0;

	uint64 _FPSCheckCnt = 0;
	uint64 _lastFPSCheckTick = 0;

	Vector2ui32 _screenSize;
	Vector2ui32 _windowSize;

public:

	/**
	* return DeltaTime in ms
	*/
	double GetDeltaTime() const { return _deltaTime; }
	double GetFPS() const { return _FPS; }
	double GetElapsedTime() const { return _elapsedTime; }
	Vector2ui32 GetWindowSize() const { return _windowSize; }
	uint64 GetFrameCnt() const { return _frameCount; }


public:
	void BeginFrameXXX();
	void PerFrameXXX();
	void ProcessWindowResizeXXX(uint32 width, uint32 height);
};

