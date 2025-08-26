#include <iostream>
#include <windows.h>
#include <dshow.h>
#include <strmif.h>
#include <control.h>
#include <ks.h>         // For Kernel Streaming definitions
#include <ksmedia.h>    // For KS media-specific definitions
#include <vidcap.h>     // For video capture device definitions
#include <initguid.h>   // Required for DEFINE_GUID to create symbols
#include <string>
#include <algorithm>    // for std::max and std::min
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <comutil.h>    // For COM helper functions
#include <mfapi.h>      // For Media Foundation
#include <mfidl.h>      // For IMFMediaSource and related interfaces
#include "wse_detector.h"  // For Windows Studio Effects detection

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "comsuppw.lib")  // For COM string conversions
#pragma comment(lib, "ksuser.lib")    // For KS properties
#pragma comment(lib, "mfplat.lib")    // For Media Foundation
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

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

// All GUIDs are already defined in wse_detector.h
// No need to redefine them here
// IID_IKsControl, KSPROPERTYSETID_ExtendedCameraControl, and KSPROPERTYSETID_WindowsStudioEffects
// are all defined in wse_detector.h

// Define Camera Control Extended property IDs if not already defined
#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION
#define KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION 0x7
#endif

#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION
#define KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION 0x11
#endif

#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_EYECONTACT
#define KSPROPERTY_CAMERACONTROL_EXTENDED_EYECONTACT 0x22
#endif

#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW
#define KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW 0x0D
#endif

#ifndef KSPROPERTY_CAMERACONTROL_EXTENDED_AUTOFRAMING
#define KSPROPERTY_CAMERACONTROL_EXTENDED_AUTOFRAMING 0x1F
#endif

// Flag definitions for camera properties
#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR 0x0000000000000001
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF 0
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR 0x0000000000000001
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK 0x0000000000000003
#endif

#ifndef KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
#define KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS 0x0000000000000004
#endif

#ifndef KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF
#define KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF 0
#endif

#ifndef KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
#define KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON 0x0000000000000001
#endif

#ifndef KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_OFF
#define KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_OFF 0
#endif

#ifndef KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTO
#define KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTO 0x0000000000000001
#endif

#ifndef KSCAMERA_EXTENDEDPROP_FILTERSCOPE
#define KSCAMERA_EXTENDEDPROP_FILTERSCOPE 0xFFFFFFFF
#endif

// Define our own structure for camera extended properties
// Note: These structures need to exactly match the Windows SDK definitions
typedef struct {
    ULONG Size;
    ULONG Version;
    ULONGLONG Flags;
    ULONGLONG Capability;
} MY_CAMERA_EXTENDEDPROP_HEADER;

typedef struct {
    ULONGLONG Value;
} MY_CAMERA_EXTENDEDPROP_VALUE;

typedef struct {
    MY_CAMERA_EXTENDEDPROP_HEADER header;
    MY_CAMERA_EXTENDEDPROP_VALUE value;
} MY_CAMERA_EXTENDEDPROP_PAYLOAD;

// Define exact Windows Studio Effects structures for direct matching
typedef struct {
    KSCAMERA_EXTENDEDPROP_HEADER header;
    KSCAMERA_EXTENDEDPROP_VALUE value;
} STUDIO_CAMERA_EXTENDEDPROP_PAYLOAD;

// Using the VideoProcAmpProperty from strmif.h
// VideoProcAmp_Brightness = 0
// VideoProcAmp_Contrast = 1
// VideoProcAmp_Hue = 2, etc.

// Event types for property changes
enum PropertyChangeEventType {
    BRIGHTNESS_CHANGED,
    CONTRAST_CHANGED,
    BACKGROUND_BLUR_CHANGED,
    OTHER_PROPERTY_CHANGED
};

// Global variables for the callback
static PropertyChangeEventType g_lastEventType;
static long g_lastPropertyValue;
static bool g_propertyChanged = false;

// Global callback function
void OnPropertyChanged(PropertyChangeEventType eventType, long newValue) {
    g_lastEventType = eventType;
    g_lastPropertyValue = newValue;
    g_propertyChanged = true;
    
    // Print the change notification
    switch (eventType) {
        case BRIGHTNESS_CHANGED:
            std::cout << "\n[External Change Detected] Brightness changed to: " << newValue << std::endl;
            std::cout << "Press Enter to continue...";
            break;
            
        case CONTRAST_CHANGED:
            std::cout << "\n[External Change Detected] Contrast changed to: " << newValue << std::endl;
            std::cout << "Press Enter to continue...";
            break;
            
        case BACKGROUND_BLUR_CHANGED:
            std::cout << "\n[External Change Detected] Background Blur changed to: " << (newValue ? "Enabled" : "Disabled") << std::endl;
            std::cout << "Press Enter to continue...";
            break;
            
        default:
            std::cout << "\n[External Change Detected] Camera property changed" << std::endl;
            std::cout << "Press Enter to continue...";
            break;
    }
}

// Callback type for property changes
typedef void (*PropertyChangedCallback)(PropertyChangeEventType, long);

// Camera control class to manage camera properties
class CameraController
{
private:
    IBaseFilter* m_pCaptureFilter = nullptr;
    IAMVideoProcAmp* m_pProcAmp = nullptr;
    bool m_isInitialized = false;
    
    // Monitoring properties
    std::thread m_monitorThread;
    std::atomic<bool> m_isMonitoring;
    std::mutex m_callbackMutex;
    std::condition_variable m_stopSignal;
    PropertyChangedCallback m_propertyChangedCallback = nullptr;
    
    // Last known values for detecting changes
    long m_lastBrightness = 0;
    long m_lastContrast = 0;
    long m_brightnessFlags = 0;
    long m_contrastFlags = 0;
    bool m_backgroundBlurEnabled = false;
    
    // Background blur capability properties
    IKsControl* m_pKsControl = nullptr;    // For accessing extended camera properties
    bool m_isBlurSupported = false;
    ULONGLONG m_blurCapabilities = 0;
    bool m_studioEffectsAvailable = false;
    int m_deviceIndex = 0;
    
    // Get camera device information
    std::wstring GetCameraName()
    {
        std::wstring cameraName = L"Unknown Camera";
        
        // Create the System Device Enumerator
        ICreateDevEnum* pDevEnum = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
            IID_ICreateDevEnum, (void**)&pDevEnum);

        if (SUCCEEDED(hr))
        {
            // Create an enumerator for video capture devices
            IEnumMoniker* pEnum = nullptr;
            hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);

            if (hr == S_OK && pEnum)
            {
                // Navigate to the requested device index
                IMoniker* pMoniker = nullptr;
                ULONG fetched;
                int index = 0;

                while (pEnum->Next(1, &pMoniker, &fetched) == S_OK)
                {
                    if (index == m_deviceIndex)
                    {
                        IPropertyBag* pPropBag = nullptr;
                        hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);

                        if (SUCCEEDED(hr) && pPropBag)
                        {
                            // Get friendly name
                            VARIANT varName;
                            VariantInit(&varName);
                            hr = pPropBag->Read(L"FriendlyName", &varName, NULL);

                            if (SUCCEEDED(hr))
                            {
                                cameraName = varName.bstrVal;
                                VariantClear(&varName);
                            }

                            pPropBag->Release();
                        }
                        pMoniker->Release();
                        break;
                    }

                    pMoniker->Release();
                    index++;
                }
                pEnum->Release();
            }
            pDevEnum->Release();
        }
        
        return cameraName;
    }

public:
    CameraController() : m_isMonitoring(false) {}
    
    ~CameraController()
    {
        StopMonitoring();
        if (m_pProcAmp) m_pProcAmp->Release();
        if (m_pCaptureFilter) m_pCaptureFilter->Release();
        if (m_pKsControl) m_pKsControl->Release();
    }

    bool Initialize(int deviceIndex = 0)
    {
        // Initialize COM if not already initialized
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool comInitializedHere = SUCCEEDED(hr);

        // Get and print camera information that might help with debugging
        m_deviceIndex = deviceIndex;
        
        // Create the System Device Enumerator
        ICreateDevEnum* pDevEnum = nullptr;
        hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
            IID_ICreateDevEnum, (void**)&pDevEnum);

        if (SUCCEEDED(hr))
        {
            // Create an enumerator for video capture devices
            IEnumMoniker* pEnum = nullptr;
            hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);

            if (hr == S_OK && pEnum)
            {
                // Skip to the requested device index
                pEnum->Reset();
                if (deviceIndex > 0)
                {
                    pEnum->Skip(deviceIndex);
                }

                // Get the moniker for the requested device
                IMoniker* pMoniker = nullptr;
                ULONG fetched;
                hr = pEnum->Next(1, &pMoniker, &fetched);

                if (hr == S_OK && pMoniker)
                {
                    // Get friendly name
                    IPropertyBag* pPropBag = nullptr;
                    hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);

                    if (SUCCEEDED(hr) && pPropBag)
                    {
                        VARIANT varName;
                        VariantInit(&varName);
                        hr = pPropBag->Read(L"FriendlyName", &varName, NULL);

                        if (SUCCEEDED(hr))
                        {
                            std::wcout << L"Initializing camera: " << varName.bstrVal << std::endl;
                            VariantClear(&varName);
                        }

                        pPropBag->Release();
                    }

                    // Bind to the capture filter
                    hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&m_pCaptureFilter);
                    
                    if (SUCCEEDED(hr) && m_pCaptureFilter)
                    {
                        // Get the IAMVideoProcAmp interface for adjusting video settings
                        hr = m_pCaptureFilter->QueryInterface(IID_IAMVideoProcAmp, (void**)&m_pProcAmp);
                        
                        if (SUCCEEDED(hr) && m_pProcAmp)
                        {
                            m_isInitialized = true;
                            std::cout << "Camera controller initialized successfully!" << std::endl;
                        }
                        else
                        {
                            std::cerr << "Failed to get IAMVideoProcAmp interface: 0x" << std::hex << hr << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr << "Failed to bind to capture filter: 0x" << std::hex << hr << std::endl;
                    }

                    pMoniker->Release();
                }
                else
                {
                    std::cerr << "No video capture device found at index " << deviceIndex << std::endl;
                }

                pEnum->Release();
            }
            else
            {
                std::cerr << "No video capture devices found" << std::endl;
            }

            pDevEnum->Release();
        }
        else
        {
            std::cerr << "Failed to create system device enumerator: 0x" << std::hex << hr << std::endl;
        }

        if (!m_isInitialized && comInitializedHere)
        {
            CoUninitialize();
        }

        return m_isInitialized;
    }

    bool IsInitialized() const
    {
        return m_isInitialized;
    }
    
    // Register a callback to be notified when camera properties change
    void RegisterPropertyChangedCallback(PropertyChangedCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_propertyChangedCallback = callback;
    }
    
    // Start monitoring for property changes (similar to the TryGetStoredDefaultValue functionality)
    void StartMonitoring()
    {
        if (m_isMonitoring.load() || !m_isInitialized || !m_pProcAmp)
            return;
            
        // Store initial values
        GetPropertyValue(VideoProcAmp_Brightness, &m_lastBrightness, &m_brightnessFlags);
        GetPropertyValue(VideoProcAmp_Contrast, &m_lastContrast, &m_contrastFlags);
        
        // Try to get initial background blur state if studio effects are available
        bool currentBlur = IsBackgroundBlurEnabled();
        m_backgroundBlurEnabled = currentBlur;
        
        m_isMonitoring.store(true);
        m_monitorThread = std::thread([this]() {
            while (m_isMonitoring.load())
            {
                // Check for changes in brightness and contrast
                long currentBrightness = 0, currentContrast = 0;
                long brightnessFlags = 0, contrastFlags = 0;
                
                if (GetPropertyValue(VideoProcAmp_Brightness, &currentBrightness, &brightnessFlags) && 
                    currentBrightness != m_lastBrightness)
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    if (m_propertyChangedCallback)
                    {
                        m_propertyChangedCallback(BRIGHTNESS_CHANGED, currentBrightness);
                    }
                    m_lastBrightness = currentBrightness;
                }
                
                if (GetPropertyValue(VideoProcAmp_Contrast, &currentContrast, &contrastFlags) && 
                    currentContrast != m_lastContrast)
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    if (m_propertyChangedCallback)
                    {
                        m_propertyChangedCallback(CONTRAST_CHANGED, currentContrast);
                    }
                    m_lastContrast = currentContrast;
                }
                
                // Check for changes in background blur if studio effects are available
                if (m_studioEffectsAvailable && m_pKsControl)
                {
                    // Use the KS property to get the current state
                    MY_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
                    ULONG bytesReturned = 0;
                    
                    HRESULT hr = GetExtendedCameraControlPayload(
                        KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION,
                        &payload,
                        sizeof(payload),
                        &bytesReturned);
                        
                    if (SUCCEEDED(hr))
                    {
                        // Check if blur flag is set in the current value
                        bool currentBlur = ((payload.header.Flags & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR) == 
                                          KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR);
                        
                        // If value changed, trigger the callback
                        if (currentBlur != m_backgroundBlurEnabled)
                        {
                            std::lock_guard<std::mutex> lock(m_callbackMutex);
                            if (m_propertyChangedCallback)
                            {
                                m_propertyChangedCallback(BACKGROUND_BLUR_CHANGED, currentBlur ? 1 : 0);
                            }
                            m_backgroundBlurEnabled = currentBlur;
                        }
                    }
                }
                
                // Poll for changes every 1 second
                std::unique_lock<std::mutex> lock(m_callbackMutex);
                m_stopSignal.wait_for(lock, std::chrono::milliseconds(1000));
            }
        });
    }
    
    // Stop monitoring for property changes
    void StopMonitoring()
    {
        if (!m_isMonitoring.load())
            return;
            
        m_isMonitoring.store(false);
        
        // Signal the monitoring thread to stop
        m_stopSignal.notify_all();
        
        if (m_monitorThread.joinable())
            m_monitorThread.join();
    }

    bool GetPropertyRange(VideoProcAmpProperty property, long* pMin, long* pMax, long* pSteppingDelta, long* pDefault, long* pFlags)
    {
        if (!m_isInitialized || !m_pProcAmp)
        {
            std::cerr << "Camera not initialized" << std::endl;
            return false;
        }

        HRESULT hr = m_pProcAmp->GetRange(property, pMin, pMax, pSteppingDelta, pDefault, pFlags);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get property range: 0x" << std::hex << hr << std::endl;
            return false;
        }

        return true;
    }

    bool GetPropertyValue(VideoProcAmpProperty property, long* pValue, long* pFlags)
    {
        if (!m_isInitialized || !m_pProcAmp)
        {
            std::cerr << "Camera not initialized" << std::endl;
            return false;
        }

        HRESULT hr = m_pProcAmp->Get(property, pValue, pFlags);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get property value: 0x" << std::hex << hr << std::endl;
            return false;
        }

        return true;
    }

    bool SetPropertyValue(VideoProcAmpProperty property, long value, long flags = 0x01) // VideoProcAmp_Flags_Manual = 0x01
    {
        if (!m_isInitialized || !m_pProcAmp)
        {
            std::cerr << "Camera not initialized" << std::endl;
            return false;
        }

        HRESULT hr = m_pProcAmp->Set(property, value, flags);
        if (FAILED(hr))
        {
            std::cerr << "Failed to set property value: 0x" << std::hex << hr << std::endl;
            return false;
        }

        return true;
    }

    // Convenience methods for commonly used properties
    bool SetBrightness(long value)
    {
        return SetPropertyValue(VideoProcAmp_Brightness, value);
    }

    bool SetContrast(long value)
    {
        return SetPropertyValue(VideoProcAmp_Contrast, value);
    }

    // Check for background blur capability using Windows Studio Effects detector
    bool CheckBackgroundBlurCapability()
    {
        if (!m_isInitialized || !m_pCaptureFilter)
            return false;

        std::cout << "Checking for background blur capabilities..." << std::endl;
        
        // STEP 1: Get IKsControl interface (equivalent to VideoDeviceController in UWP)
        if (!m_pKsControl)
        {
            HRESULT hr = m_pCaptureFilter->QueryInterface(IID_IKsControl, (void**)&m_pKsControl);
            if (FAILED(hr) || !m_pKsControl)
            {
                std::cout << "Camera does not support extended KS controls." << std::endl;
                return false;
            }
        }
        
        // STEP 2: Set up KS property (equivalent to GetExtendedCameraControlPayload)
        KSPROPERTY prop;
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // STEP 3: Set up property header to receive data
        KSCAMERA_EXTENDEDPROP_HEADER header = {0};
        header.Size = sizeof(header);
        header.Version = 1;
        
        // STEP 4: Make KsProperty call (equivalent to VideoDeviceController.GetDevicePropertyByExtendedId)
        ULONG bytesReturned = 0;
        HRESULT hr = m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &header, 
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER),
            &bytesReturned);
            
        if (SUCCEEDED(hr))
        {
            // STEP 5: Check capability (exactly matching UWP's bit test)
            // if (isBlurControlSupported && (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~getPayload.Capability) == 0))
            if ((KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~header.Capability) == 0)
            {
                std::cout << "Background blur is supported on this camera!" << std::endl;
                std::cout << "Capability flags: 0x" << std::hex << header.Capability << std::dec << std::endl;
                m_blurCapabilities = header.Capability;
                m_isBlurSupported = true;
                m_studioEffectsAvailable = true;
                
                // STEP 6: Store the capabilities for later use (like m_backgroundBlurController in UWP)
                // In UWP this would create a DefaultController
                // In Win32 we just store the capabilities in member variables
                
                return true;
            }
            else
            {
                std::cout << "Camera supports background segmentation but not blur." << std::endl;
                std::cout << "Capability flags: 0x" << std::hex << header.Capability << std::dec << std::endl;
            }
        }
        else
        {
            // Try Windows Studio Effects property set as fallback
            std::cout << "Failed to get capability: 0x" << std::hex << hr << std::dec << std::endl;
            std::cout << "Trying with Windows Studio Effects property set..." << std::endl;
            
            prop.Set = KSPROPERTYSETID_WindowsStudioEffects;
            hr = m_pKsControl->KsProperty(
                &prop,
                sizeof(KSPROPERTY),
                &header, 
                sizeof(KSCAMERA_EXTENDEDPROP_HEADER),
                &bytesReturned);
                
            if (SUCCEEDED(hr) && (KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~header.Capability) == 0)
            {
                std::cout << "Background blur is supported through Windows Studio Effects!" << std::endl;
                m_blurCapabilities = header.Capability;
                m_isBlurSupported = true;
                m_studioEffectsAvailable = true;
                return true;
            }
        }
        
        // If the direct KsProperty approach fails, fall back to our WSE detector
        std::cout << "Trying alternative detection methods..." << std::endl;
        m_isBlurSupported = WindowsStudioEffectsDetector::DetectWindowsStudioEffects(m_pCaptureFilter);
        
        if (m_isBlurSupported) {
            std::cout << "Background blur is supported via alternative detection!" << std::endl;
            m_studioEffectsAvailable = true;
            return true;
        }
        
        std::cout << "Background blur not supported by this camera." << std::endl;
        return false;
    }
    
    // Helper method to get device property by extended ID (similar to PropertyInquiry.cpp)
    HRESULT GetDevicePropertyByExtendedId(const GUID& propSet, ULONG propId, ULONGLONG* pCapabilities)
    {
        if (!m_pKsControl || !pCapabilities)
            return E_INVALIDARG;
            
        // First try standard version
        {
            // Set up the property
            KSPROPERTY prop = {0};
            prop.Set = propSet;
            prop.Id = propId;
            prop.Flags = KSPROPERTY_TYPE_GET;
            
            // Use the Windows SDK struct directly
            KSCAMERA_EXTENDEDPROP_HEADER header = {0};
            header.Size = sizeof(KSCAMERA_EXTENDEDPROP_HEADER);
            header.Version = 1;
            
            ULONG bytesReturned = 0;
            HRESULT hr = m_pKsControl->KsProperty(
                &prop,
                sizeof(KSPROPERTY),
                &header, 
                sizeof(KSCAMERA_EXTENDEDPROP_HEADER),
                &bytesReturned);
                
            if (SUCCEEDED(hr))
            {
                *pCapabilities = header.Capability;
                return hr;
            }
        }
        
        // Try with full payload
        {
            // Set up the property
            KSPROPERTY prop = {0};
            prop.Set = propSet;
            prop.Id = propId;
            prop.Flags = KSPROPERTY_TYPE_GET;
            
            // Use the complete payload
            STUDIO_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
            payload.header.Size = sizeof(STUDIO_CAMERA_EXTENDEDPROP_PAYLOAD);
            payload.header.Version = 1;
            
            ULONG bytesReturned = 0;
            HRESULT hr = m_pKsControl->KsProperty(
                &prop,
                sizeof(KSPROPERTY),
                &payload, 
                sizeof(STUDIO_CAMERA_EXTENDEDPROP_PAYLOAD),
                &bytesReturned);
                
            if (SUCCEEDED(hr))
            {
                *pCapabilities = payload.header.Capability;
                return hr;
            }
        }
        
        return E_FAIL;
    }

    // Try to initialize Windows Studio Effects API
    bool InitializeStudioEffects()
    {
        if (!m_isInitialized || !m_pCaptureFilter)
            return false;
            
        // Check if already initialized
        if (m_studioEffectsAvailable)
            return true;
            
        std::wstring cameraName = GetCameraName();
        std::wcout << L"Camera Name: " << cameraName << std::endl;
        
        // Check for known Windows Studio Effects cameras
        if (cameraName.find(L"Windows Studio") != std::wstring::npos ||
            cameraName.find(L"5MP Camera") != std::wstring::npos ||
            cameraName.find(L"Surface") != std::wstring::npos)
        {
            std::cout << "Detected potential Windows Studio Effects compatible camera" << std::endl;
        }
            
        std::cout << "Initializing Windows Studio Effects support..." << std::endl;
        
        // Query for IKsControl interface if not already done
        if (!m_pKsControl)
        {
            HRESULT hr = m_pCaptureFilter->QueryInterface(IID_IKsControl, (void**)&m_pKsControl);
            if (FAILED(hr) || !m_pKsControl)
            {
                std::cout << "Camera does not support extended KS controls." << std::endl;
                return false;
            }
        }
        
        // Use proper capability detection
        bool hasBackgroundBlur = CheckBackgroundBlurCapability();
        
        // If we found background blur support, mark studio effects as available
        if (hasBackgroundBlur)
        {
            m_studioEffectsAvailable = true;
            std::cout << "Windows Studio Effects initialized successfully!" << std::endl;
            return true;
        }
        
        // Attempt to set background blur to see if that works
        std::cout << "Testing background blur by attempting to enable it..." << std::endl;
        bool setResult = SetBackgroundBlur(true);
        if (setResult)
        {
            std::cout << "Successfully enabled background blur! Camera supports Windows Studio Effects." << std::endl;
            m_studioEffectsAvailable = true;
            // Turn it back off since we were just testing
            SetBackgroundBlur(false);
            return true;
        }
        
        std::cout << "Windows Studio Effects not available on this camera." << std::endl;
        return m_studioEffectsAvailable;
    }
    
    // Helper method to get extended camera control payload with proper error handling
    HRESULT GetExtendedCameraControlPayload(int controlId, 
                                          void* pPayload, 
                                          ULONG payloadSize, 
                                          ULONG* pBytesReturned)
    {
        if (!m_pKsControl)
            return E_FAIL;
        
        std::cout << "GetExtendedCameraControlPayload: Getting control ID " << controlId << ", payload size: " << payloadSize << std::endl;
            
        // Initialize the payload's header before calling KsProperty
        if (payloadSize >= sizeof(KSCAMERA_EXTENDEDPROP_HEADER)) {
            KSCAMERA_EXTENDEDPROP_HEADER* pHeader = static_cast<KSCAMERA_EXTENDEDPROP_HEADER*>(pPayload);
            pHeader->Size = payloadSize;
            pHeader->Version = 1;
        }
        
        // First try with standard Extended Camera Control property set
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = controlId;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Get the property
        HRESULT hr = m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            pPayload, 
            payloadSize,
            pBytesReturned);
        
        if (SUCCEEDED(hr)) {
            std::cout << "  Success with KSPROPERTYSETID_ExtendedCameraControl, bytes returned: " << *pBytesReturned << std::endl;
            return hr;
        }
        
        std::cout << "  Failed with KSPROPERTYSETID_ExtendedCameraControl: 0x" << std::hex << hr << std::dec << std::endl;
            
        // If that fails, try with Windows Studio Effects property set
        prop.Set = KSPROPERTYSETID_WindowsStudioEffects;
        
        // Reset the payload's header
        if (payloadSize >= sizeof(KSCAMERA_EXTENDEDPROP_HEADER)) {
            KSCAMERA_EXTENDEDPROP_HEADER* pHeader = static_cast<KSCAMERA_EXTENDEDPROP_HEADER*>(pPayload);
            pHeader->Size = payloadSize;
            pHeader->Version = 1;
        }
        
        hr = m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            pPayload, 
            payloadSize,
            pBytesReturned);
            
        if (SUCCEEDED(hr)) {
            std::cout << "  Success with KSPROPERTYSETID_WindowsStudioEffects, bytes returned: " << *pBytesReturned << std::endl;
        } else {
            std::cout << "  Failed with KSPROPERTYSETID_WindowsStudioEffects: 0x" << std::hex << hr << std::dec << std::endl;
        }
        
        return hr;
    }

    // Check if background blur is enabled using KS properties
    bool IsBackgroundBlurEnabled()
    {
        if (!m_studioEffectsAvailable || !m_pKsControl)
            return false;
        
        MY_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
        ULONG bytesReturned = 0;

        HRESULT hr = GetExtendedCameraControlPayload(KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, 
                                                    &payload, 
                                                    sizeof(payload), 
                                                    &bytesReturned);
        if (SUCCEEDED(hr))
        {
            // Check if blur flag is set in the current value
            bool isEnabled = ((payload.header.Flags & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR) == 
                             KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR);
            
            // Update our stored value
            m_backgroundBlurEnabled = isEnabled;
            return isEnabled;
        }
        
        return m_backgroundBlurEnabled;
    }
    
    // Helper to get stored default value for background blur
    bool TryGetStoredDefaultValue(bool* pDefaultValue)
    {
        if (!m_pKsControl || !pDefaultValue)
            return false;
            
        MY_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
        ULONG bytesReturned = 0;

        HRESULT hr = GetExtendedCameraControlPayload(KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, 
                                                   &payload, 
                                                   sizeof(payload), 
                                                   &bytesReturned);
        if (SUCCEEDED(hr))
        {
            // The default value is stored in the Flags field with DEFAULT_VALUE flag
            // or it could be in the Capability field depending on implementation
            if (payload.header.Capability & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR)
            {
                *pDefaultValue = true;
                return true;
            }
            else
            {
                *pDefaultValue = false;
                return true;
            }
        }
        
        return false;
    }
    
    // Set background blur state using Windows Studio Effects detector
    bool SetBackgroundBlur(bool enabled)
    {
        // First try to initialize studio effects if not done already
        if (!m_studioEffectsAvailable && !m_isBlurSupported)
        {
            // Initialize the Windows Studio Effects detector
            bool detected = CheckBackgroundBlurCapability();
            if (!detected)
            {
                std::cout << "Background blur is not available on this camera." << std::endl;
                return false;
            }
        }
        
        if (!m_pCaptureFilter)
            return false;

        std::cout << "Setting background blur to " << (enabled ? "enabled" : "disabled") << "..." << std::endl;
        
        // Use the Windows Studio Effects detector to set the background blur state
        bool result = WindowsStudioEffectsDetector::SetBackgroundBlur(m_pCaptureFilter, enabled);
        
        if (result)
        {
            m_backgroundBlurEnabled = enabled;
            std::cout << "Successfully " << (enabled ? "enabled" : "disabled") << " background blur." << std::endl;
        }
        else
        {
            std::cout << "Failed to set background blur state." << std::endl;
        }
        
        return result;
    }
};

// Helper to list all available camera devices
int ListCameras()
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool comInitializedHere = SUCCEEDED(hr);

    int deviceCount = 0;

    // Create the System Device Enumerator
    ICreateDevEnum* pDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum, (void**)&pDevEnum);

    if (SUCCEEDED(hr))
    {
        // Create an enumerator for video capture devices
        IEnumMoniker* pEnum = NULL;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);

        if (hr == S_OK && pEnum)
        {
            std::cout << "Available video capture devices:" << std::endl;
            std::cout << "--------------------------------" << std::endl;

            // Enumerate devices
            IMoniker* pMoniker = NULL;
            ULONG fetched;

            while (pEnum->Next(1, &pMoniker, &fetched) == S_OK)
            {
                IPropertyBag* pPropBag = NULL;
                hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);

                if (SUCCEEDED(hr) && pPropBag)
                {
                    // Get friendly name
                    VARIANT varName;
                    VariantInit(&varName);
                    hr = pPropBag->Read(L"FriendlyName", &varName, NULL);

                    if (SUCCEEDED(hr))
                    {
                        deviceCount++;
                        std::wcout << deviceCount << ". " << varName.bstrVal << std::endl;
                        VariantClear(&varName);
                    }

                    pPropBag->Release();
                }

                pMoniker->Release();
            }

            if (deviceCount == 0)
            {
                std::cout << "No video capture devices found." << std::endl;
            }

            pEnum->Release();
        }
        else
        {
            std::cout << "No video capture device was found." << std::endl;
        }

        pDevEnum->Release();
    }

    if (comInitializedHere)
    {
        CoUninitialize();
    }

    return deviceCount;
}

// Display a simple menu
void DisplayMenu(bool studioEffectsSupported)
{
    std::cout << "\n===== Camera Control Menu =====" << std::endl;
    std::cout << "1. Set Brightness" << std::endl;
    std::cout << "2. Set Contrast" << std::endl;
    std::cout << "3. Toggle Background Blur " << (studioEffectsSupported ? "" : "(Simulated)") << std::endl;
    std::cout << "4. Show Current Settings" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "Note: External changes made to camera settings will be detected automatically" << std::endl;
    std::cout << "Enter choice: ";
}

int main()
{
    // List available cameras and let user select one
    int cameraCount = ListCameras();
    if (cameraCount == 0) {
        std::cout << "No cameras found. Press Enter to exit...";
        std::getchar();
        return -1;
    }

    int selectedCamera = 0;
    if (cameraCount > 1) {
        std::cout << "\nSelect camera (1-" << cameraCount << "): ";
        std::cin >> selectedCamera;
        
        if (selectedCamera < 1 || selectedCamera > cameraCount) {
            std::cout << "Invalid selection. Using camera 1." << std::endl;
            selectedCamera = 1;
        }
    } else {
        std::cout << "\nUsing the only available camera." << std::endl;
        selectedCamera = 1;
    }
    
    // Initialize camera controller
    CameraController camera;
    if (!camera.Initialize(selectedCamera - 1)) {
        std::cout << "Failed to initialize camera controller. Press Enter to exit...";
        std::getchar(); // Clear any remaining input
        std::getchar();
        return -1;
    }
    
    // Check if Windows Studio Effects are available
    bool studioEffectsSupported = camera.InitializeStudioEffects();
    
    // Get property ranges
    long minBrightness = 0, maxBrightness = 100, stepBrightness = 1, defaultBrightness = 50, flagsBrightness = 0;
    camera.GetPropertyRange(VideoProcAmp_Brightness, &minBrightness, &maxBrightness, &stepBrightness, &defaultBrightness, &flagsBrightness);
    
    long minContrast = 0, maxContrast = 100, stepContrast = 1, defaultContrast = 50, flagsContrast = 0;
    camera.GetPropertyRange(VideoProcAmp_Contrast, &minContrast, &maxContrast, &stepContrast, &defaultContrast, &flagsContrast);
    
    // Register our global callback function
    camera.RegisterPropertyChangedCallback(OnPropertyChanged);
    
    // Start monitoring for property changes
    std::cout << "Starting monitoring for external camera changes..." << std::endl;
    camera.StartMonitoring();
    
    // Main menu loop
    int choice = -1;
    do {
        DisplayMenu(studioEffectsSupported);
        std::cin >> choice;
        
        switch (choice) {
            case 1: { // Brightness
                long currentValue = 0, flags = 0;
                camera.GetPropertyValue(VideoProcAmp_Brightness, &currentValue, &flags);
                
                std::cout << "Current brightness: " << currentValue 
                          << " (Range: " << minBrightness << " - " << maxBrightness << ")" << std::endl;
                          
                std::cout << "Enter new brightness value: ";
                long newValue;
                std::cin >> newValue;
                
                if (newValue < minBrightness || newValue > maxBrightness) {
                    std::cout << "Value out of range. Using closest valid value." << std::endl;
                    newValue = (newValue < minBrightness) ? minBrightness : ((newValue > maxBrightness) ? maxBrightness : newValue);
                }
                
                if (camera.SetBrightness(newValue)) {
                    std::cout << "Brightness set to " << newValue << std::endl;
                }
                break;
            }
            case 2: { // Contrast
                long currentValue = 0, flags = 0;
                camera.GetPropertyValue(VideoProcAmp_Contrast, &currentValue, &flags);
                
                std::cout << "Current contrast: " << currentValue 
                          << " (Range: " << minContrast << " - " << maxContrast << ")" << std::endl;
                          
                std::cout << "Enter new contrast value: ";
                long newValue;
                std::cin >> newValue;
                
                if (newValue < minContrast || newValue > maxContrast) {
                    std::cout << "Value out of range. Using closest valid value." << std::endl;
                    newValue = (newValue < minContrast) ? minContrast : ((newValue > maxContrast) ? maxContrast : newValue);
                }
                
                if (camera.SetContrast(newValue)) {
                    std::cout << "Contrast set to " << newValue << std::endl;
                }
                break;
            }
            case 3: { // Background Blur
                bool currentState = camera.IsBackgroundBlurEnabled();
                std::cout << "Current background blur state: " << (currentState ? "Enabled" : "Disabled") << std::endl;
                std::cout << "Enter 1 to enable or 0 to disable background blur: ";
                int blurEnabled;
                std::cin >> blurEnabled;
                
                if (camera.SetBackgroundBlur(blurEnabled != 0)) {
                    std::cout << "Background blur " << (blurEnabled != 0 ? "enabled" : "disabled") << std::endl;
                }
                break;
            }
            case 4: { // Show current settings
                long brightness = 0, contrastVal = 0, flags = 0;
                
                if (camera.GetPropertyValue(VideoProcAmp_Brightness, &brightness, &flags)) {
                    std::cout << "Current brightness: " << brightness << std::endl;
                }
                
                if (camera.GetPropertyValue(VideoProcAmp_Contrast, &contrastVal, &flags)) {
                    std::cout << "Current contrast: " << contrastVal << std::endl;
                }
                
                bool blurEnabled = camera.IsBackgroundBlurEnabled();
                std::cout << "Background blur: " << (camera.InitializeStudioEffects() ? 
                    (blurEnabled ? "Enabled" : "Disabled") : 
                    "Not supported on this camera") << std::endl;
                break;
            }
            case 0: // Exit
                std::cout << "Exiting..." << std::endl;
                break;
            default:
                std::cout << "Invalid choice. Please try again." << std::endl;
        }
        
    } while (choice != 0);
    
    // Stop monitoring before exiting
    std::cout << "Stopping monitoring for external camera changes..." << std::endl;
    camera.StopMonitoring();
    
    return 0;
}
