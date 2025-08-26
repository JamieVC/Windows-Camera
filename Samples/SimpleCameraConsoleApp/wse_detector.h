#pragma once
#include <windows.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mfidl.h>
#include <strmif.h>    // For IBaseFilter
#include <control.h>   // For IKsControl
#include <string>
#include <iostream>
#include <vector>

// Define IKsControl interface if needed
#ifndef __IKsControl_FWD_DEFINED__
#define __IKsControl_FWD_DEFINED__
typedef interface IKsControl IKsControl;
#endif 

#ifndef __IKsControl_INTERFACE_DEFINED__
#define __IKsControl_INTERFACE_DEFINED__

EXTERN_C const IID IID_IKsControl;

MIDL_INTERFACE("28F54685-06FD-11D2-B27A-00A0C9223196")
IKsControl : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE KsProperty(
        /* [in] */ PKSPROPERTY Property,
        /* [in] */ ULONG PropertyLength,
        /* [out][in] */ LPVOID PropertyData,
        /* [in] */ ULONG DataLength,
        /* [out] */ ULONG *BytesReturned) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE KsMethod(
        /* [in] */ PKSMETHOD Method,
        /* [in] */ ULONG MethodLength,
        /* [out][in] */ LPVOID MethodData,
        /* [in] */ ULONG DataLength,
        /* [out] */ ULONG *BytesReturned) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE KsEvent(
        /* [in] */ PKSEVENT Event,
        /* [in] */ ULONG EventLength,
        /* [out][in] */ LPVOID EventData,
        /* [in] */ ULONG DataLength,
        /* [out] */ ULONG *BytesReturned) = 0;
};
#endif

// Define the IID for IKsControl if needed
#ifndef IID_IKsControl
DEFINE_GUID(IID_IKsControl, 
    0x28F54685, 0x06FD, 0x11D2, 0xB2, 0x7A, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
#endif

// Define KSPROPERTYSETID_ExtendedCameraControl GUID if not already defined
#ifndef KSPROPERTYSETID_ExtendedCameraControl
static const GUID KSPROPERTYSETID_ExtendedCameraControl = 
    { 0x1CB79112, 0xC0D2, 0x4213, { 0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72 } };
#endif

// Define KSPROPERTYSETID_WindowsStudioEffects GUID if not already defined
#ifndef KSPROPERTYSETID_WindowsStudioEffects
static const GUID KSPROPERTYSETID_WindowsStudioEffects = 
    { 0xE33BA076, 0x3C4D, 0x4129, { 0x94, 0x66, 0x93, 0x41, 0x37, 0x3A, 0xA1, 0x91 } };
#endif

// Define background segmentation property IDs if not already defined
#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION
#define KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION 0x7
#endif

// Define flag values
#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF 0
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR 0x0000000000000001
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK 0x0000000000000002
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS 0x0000000000000004
#endif

#ifndef KSCAMERA_EXTENDEDPROP_FILTERSCOPE
#define KSCAMERA_EXTENDEDPROP_FILTERSCOPE 0xFFFFFFFF
#endif

// Windows Studio Effects detector class
class WindowsStudioEffectsDetector {
public:
    // Detect if Windows Studio Effects are available on the specified capture device
    static bool DetectWindowsStudioEffects(IBaseFilter* pCaptureFilter) {
        if (!pCaptureFilter)
            return false;
            
        std::cout << "Detecting Windows Studio Effects capabilities..." << std::endl;
        
        // Get IKsControl interface
        IKsControl* pKsControl = nullptr;
        HRESULT hr = pCaptureFilter->QueryInterface(IID_IKsControl, (void**)&pKsControl);
        if (FAILED(hr) || !pKsControl) {
            std::cout << "Failed to get IKsControl interface. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }
        
        // Try multiple approaches to detect background blur capability
        bool result = false;
        result = DetectBackgroundBlurWithHeader(pKsControl) ||
                 DetectBackgroundBlurWithFullPayload(pKsControl) ||
                 DetectBackgroundBlurWithWindowsStudioPropertySet(pKsControl);
                 
        // Clean up
        pKsControl->Release();
        return result;
    }
    
    // Set background blur state (enable/disable)
    static bool SetBackgroundBlur(IBaseFilter* pCaptureFilter, bool enable) {
        if (!pCaptureFilter)
            return false;
            
        std::cout << "Setting background blur to " << (enable ? "enabled" : "disabled") << std::endl;
        
        // Get IKsControl interface
        IKsControl* pKsControl = nullptr;
        HRESULT hr = pCaptureFilter->QueryInterface(IID_IKsControl, (void**)&pKsControl);
        if (FAILED(hr) || !pKsControl) {
            std::cout << "Failed to get IKsControl interface. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }
        
        // Try multiple approaches to set background blur
        bool result = false;
        result = SetBlurWithExtendedCameraControl(pKsControl, enable) ||
                 SetBlurWithWindowsStudioEffects(pKsControl, enable);
                 
        // Clean up
        pKsControl->Release();
        return result;
    }
    
private:
    // Method 1: Try to detect background blur capability using just the header
    static bool DetectBackgroundBlurWithHeader(IKsControl* pKsControl) {
        std::cout << "Method 1: Checking with ExtendedCameraControl and header only..." << std::endl;
        
        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Prepare header structure to receive capability info
        KSCAMERA_EXTENDEDPROP_HEADER header = {0};
        header.Size = sizeof(KSCAMERA_EXTENDEDPROP_HEADER);
        header.Version = 1;
        header.PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        
        ULONG bytesReturned = 0;
        HRESULT hr = pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &header, 
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER),
            &bytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "Success! Capability flags: 0x" << std::hex << static_cast<unsigned long long>(header.Capability) << std::dec << std::endl;
            std::cout << "Current flags: 0x" << std::hex << static_cast<unsigned long long>(header.Flags) << std::dec << std::endl;
            std::cout << "Bytes returned: " << bytesReturned << std::endl;
            
            // Check if background blur is supported in capabilities
            if (header.Capability & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR) {
                std::cout << "Background blur is supported!" << std::endl;
                return true;
            }
            else {
                std::cout << "Background blur capability not found in capabilities." << std::endl;
            }
        }
        else {
            std::cout << "Method 1 failed. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        }
        
        return false;
    }
    
    // Method 2: Try to detect background blur capability using full payload
    static bool DetectBackgroundBlurWithFullPayload(IKsControl* pKsControl) {
        std::cout << "Method 2: Checking with ExtendedCameraControl and full payload..." << std::endl;
        
        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Prepare payload structure
        struct {
            KSCAMERA_EXTENDEDPROP_HEADER header;
            KSCAMERA_EXTENDEDPROP_VALUE value;
        } payload = {0};
        
        payload.header.Size = sizeof(payload);
        payload.header.Version = 1;
        payload.header.PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        
        ULONG bytesReturned = 0;
        HRESULT hr = pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload, 
            sizeof(payload),
            &bytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "Success! Capability flags: 0x" << std::hex << static_cast<unsigned long long>(payload.header.Capability) << std::dec << std::endl;
            std::cout << "Current flags: 0x" << std::hex << static_cast<unsigned long long>(payload.header.Flags) << std::dec << std::endl;
            std::cout << "Current value: 0x" << std::hex << payload.value.Value.ull << std::dec << std::endl;
            std::cout << "Bytes returned: " << bytesReturned << std::endl;
            
            // Check if background blur is supported in capabilities
            if (payload.header.Capability & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR) {
                std::cout << "Background blur is supported!" << std::endl;
                return true;
            }
            else {
                std::cout << "Background blur capability not found in capabilities." << std::endl;
            }
        }
        else {
            std::cout << "Method 2 failed. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        }
        
        return false;
    }
    
    // Method 3: Try to detect background blur capability using Windows Studio Effects property set
    static bool DetectBackgroundBlurWithWindowsStudioPropertySet(IKsControl* pKsControl) {
        std::cout << "Method 3: Checking with WindowsStudioEffects property set..." << std::endl;
        
        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_WindowsStudioEffects;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Prepare payload structure
        struct {
            KSCAMERA_EXTENDEDPROP_HEADER header;
            KSCAMERA_EXTENDEDPROP_VALUE value;
        } payload = {0};
        
        payload.header.Size = sizeof(payload);
        payload.header.Version = 1;
        payload.header.PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        
        ULONG bytesReturned = 0;
        HRESULT hr = pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload, 
            sizeof(payload),
            &bytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "Success! Capability flags: 0x" << std::hex << static_cast<unsigned long long>(payload.header.Capability) << std::dec << std::endl;
            std::cout << "Current flags: 0x" << std::hex << static_cast<unsigned long long>(payload.header.Flags) << std::dec << std::endl;
            std::cout << "Current value: 0x" << std::hex << payload.value.Value.ull << std::dec << std::endl;
            std::cout << "Bytes returned: " << bytesReturned << std::endl;
            
            // Check if background blur is supported in capabilities
            if (payload.header.Capability & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR) {
                std::cout << "Background blur is supported via Windows Studio Effects!" << std::endl;
                return true;
            }
            else {
                std::cout << "Background blur capability not found in Windows Studio Effects capabilities." << std::endl;
            }
        }
        else {
            std::cout << "Method 3 failed. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        }
        
        return false;
    }
    
    // Method 1 for setting background blur: Use ExtendedCameraControl property set
    static bool SetBlurWithExtendedCameraControl(IKsControl* pKsControl, bool enable) {
        std::cout << "Setting blur with ExtendedCameraControl property set..." << std::endl;
        
        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_SET;
        
        // Prepare payload structure
        struct {
            KSCAMERA_EXTENDEDPROP_HEADER header;
            KSCAMERA_EXTENDEDPROP_VALUE value;
        } payload = {0};
        
        payload.header.Size = sizeof(payload);
        payload.header.Version = 1;
        payload.header.PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        
        // Set flags based on enable/disable
        payload.header.Flags = enable ? KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR : 
                                       KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF;
        
        ULONG bytesReturned = 0;
        HRESULT hr = pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload, 
            sizeof(payload),
            &bytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "Successfully " << (enable ? "enabled" : "disabled") 
                      << " background blur using ExtendedCameraControl." << std::endl;
            return true;
        }
        else {
            std::cout << "Failed to set background blur with ExtendedCameraControl. HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }
    }
    
    // Method 2 for setting background blur: Use WindowsStudioEffects property set
    static bool SetBlurWithWindowsStudioEffects(IKsControl* pKsControl, bool enable) {
        std::cout << "Setting blur with WindowsStudioEffects property set..." << std::endl;
        
        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_WindowsStudioEffects;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_SET;
        
        // Prepare payload structure
        struct {
            KSCAMERA_EXTENDEDPROP_HEADER header;
            KSCAMERA_EXTENDEDPROP_VALUE value;
        } payload = {0};
        
        payload.header.Size = sizeof(payload);
        payload.header.Version = 1;
        payload.header.PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        
        // Set flags based on enable/disable
        payload.header.Flags = enable ? KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR : 
                                       KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF;
        
        ULONG bytesReturned = 0;
        HRESULT hr = pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload, 
            sizeof(payload),
            &bytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "Successfully " << (enable ? "enabled" : "disabled") 
                      << " background blur using WindowsStudioEffects." << std::endl;
            return true;
        }
        else {
            std::cout << "Failed to set background blur with WindowsStudioEffects. HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }
    }
};
