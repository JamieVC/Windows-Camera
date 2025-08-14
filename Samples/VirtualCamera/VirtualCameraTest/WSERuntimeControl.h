#pragma once
#ifndef WSE_RUNTIME_CONTROL_H
#define WSE_RUNTIME_CONTROL_H

//#include "pch.h"
#include "WSEController.h"

// Runtime WSE Control Functions
HRESULT ToggleBlurEffect(IMFVirtualCamera* pVirtualCamera, BOOL enable);
HRESULT SetBlurIntensity(IMFVirtualCamera* pVirtualCamera, float intensity);
HRESULT GetBlurState(IMFVirtualCamera* pVirtualCamera, BOOL* pEnabled, float* pIntensity);

// Interactive runtime control menu
void RunWSERuntimeControl(IMFVirtualCamera* pVirtualCamera);

// Helper function to get WSE controller from virtual camera
HRESULT GetWSEControllerFromVirtualCamera(IMFVirtualCamera* pVirtualCamera, IWSEController** ppController);

#endif // WSE_RUNTIME_CONTROL_H