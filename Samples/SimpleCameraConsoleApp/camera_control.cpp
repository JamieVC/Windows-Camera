#include <iostream>
#include <windows.h>
#include <dshow.h>
#include <strmif.h>
#include <control.h>
#include <string>
#include <algorithm> // for std::max and std::min
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <comutil.h> // For COM helper functions

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "comsuppw.lib") // For COM string conversions

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

    // Check for background blur capability using Kernel Streaming properties
    bool CheckBackgroundBlurCapability()
    {
        if (!m_isInitialized || !m_pCaptureFilter)
            return false;

        // Query for IKsControl interface
        HRESULT hr = m_pCaptureFilter->QueryInterface(IID_IKsControl, (void**)&m_pKsControl);
        if (FAILED(hr) || !m_pKsControl)
        {
            std::cout << "Camera does not support extended KS controls." << std::endl;
            return false;
        }

        // Set up the property for background segmentation
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Prepare payload structure to receive capability info
        MY_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
        payload.header.Size = sizeof(MY_CAMERA_EXTENDEDPROP_PAYLOAD);
        payload.header.Version = 1;

        ULONG bytesReturned = 0;
        hr = m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload, 
            sizeof(MY_CAMERA_EXTENDEDPROP_PAYLOAD),
            &bytesReturned);

        if (SUCCEEDED(hr)) 
        {
            // Store the capabilities for later use
            m_blurCapabilities = payload.header.Capability;
            
            // Check if background blur is supported in capabilities
            if ((KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & payload.header.Capability) != 0)
            {
                std::cout << "Camera supports background blur!" << std::endl;
                m_isBlurSupported = true;
                return true;
            }
        }
        
        std::cout << "Background blur not supported by this camera." << std::endl;
        return false;
    }

    // Try to initialize Windows Studio Effects API
    bool InitializeStudioEffects()
    {
        if (!m_isInitialized || !m_pCaptureFilter)
            return false;
            
        // Check if already initialized
        if (m_studioEffectsAvailable)
            return true;
            
        std::cout << "Checking for Windows Studio Effects support..." << std::endl;
        
        // Use proper capability detection
        m_studioEffectsAvailable = CheckBackgroundBlurCapability();
        
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
            
        // Set up the property
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = controlId;
        prop.Flags = KSPROPERTY_TYPE_GET;
        
        // Get the property
        return m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            pPayload, 
            payloadSize,
            pBytesReturned);
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
    
    // Set background blur state using KS properties
    bool SetBackgroundBlur(bool enabled)
    {
        // First try to initialize studio effects if not done already
        if (!m_studioEffectsAvailable && !InitializeStudioEffects())
        {
            std::cout << "Background blur is not available on this camera." << std::endl;
            std::cout << "It requires Windows Studio Effects or camera-specific APIs." << std::endl;
            return false;
        }
        
        if (!m_pKsControl)
            return false;
        
        // First get the current property to maintain other flags
        MY_CAMERA_EXTENDEDPROP_PAYLOAD payload = {0};
        ULONG bytesReturned = 0;
        
        HRESULT hr = GetExtendedCameraControlPayload(KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, 
                                                   &payload, 
                                                   sizeof(payload), 
                                                   &bytesReturned);
        if (FAILED(hr))
        {
            std::cout << "Failed to get current background blur state. HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }
        
        // Update flags based on enabled state
        if (enabled)
        {
            // Set the blur flag in Flags field
            payload.header.Flags = KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR;
        }
        else
        {
            // No flags for disabled state
            payload.header.Flags = KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF;
        }
        
        // Set up the property
        KSPROPERTY prop = {0};
        prop.Set = KSPROPERTYSETID_ExtendedCameraControl;
        prop.Id = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION;
        prop.Flags = KSPROPERTY_TYPE_SET;
        
        // Set the property
        hr = m_pKsControl->KsProperty(
            &prop,
            sizeof(KSPROPERTY),
            &payload,
            sizeof(payload),
            &bytesReturned
        );
        
        if (SUCCEEDED(hr))
        {
            m_backgroundBlurEnabled = enabled;
            std::cout << "Background Blur " << (enabled ? "enabled" : "disabled") << std::endl;
            return true;
        }
        else
        {
            std::cout << "Failed to set Background Blur state. HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
            return false;
        }
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
