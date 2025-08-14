#include "pch.h"
#include "WSERuntimeControl.h"

HRESULT GetWSEControllerFromVirtualCamera(IMFVirtualCamera* pVirtualCamera, IWSEController** ppController)
{
    RETURN_HR_IF_NULL(E_POINTER, pVirtualCamera);
    RETURN_HR_IF_NULL(E_POINTER, ppController);
    
    *ppController = nullptr;
    
    // Get the media source
    wil::com_ptr<IMFMediaSource> spMediaSource;
    RETURN_IF_FAILED(pVirtualCamera->GetMediaSource(&spMediaSource));
    
    // Query for IMFGetService
    wil::com_ptr<IMFGetService> spGetService;
    RETURN_IF_FAILED(spMediaSource->QueryInterface(&spGetService));
    
    // Get WSE controller
    return spGetService->GetService(MF_WSE_SERVICE, __uuidof(IWSEController), reinterpret_cast<void**>(ppController));
}

HRESULT ToggleBlurEffect(IMFVirtualCamera* pVirtualCamera, BOOL enable)
{
    wil::com_ptr<IWSEController> spWSEController;
    RETURN_IF_FAILED(GetWSEControllerFromVirtualCamera(pVirtualCamera, &spWSEController));
    
    return spWSEController->EnableBackgroundBlur(enable);
}

HRESULT SetBlurIntensity(IMFVirtualCamera* pVirtualCamera, float intensity)
{
    wil::com_ptr<IWSEController> spWSEController;
    RETURN_IF_FAILED(GetWSEControllerFromVirtualCamera(pVirtualCamera, &spWSEController));
    
    return spWSEController->SetBlurIntensity(intensity);
}

HRESULT GetBlurState(IMFVirtualCamera* pVirtualCamera, BOOL* pEnabled, float* pIntensity)
{
    wil::com_ptr<IWSEController> spWSEController;
    RETURN_IF_FAILED(GetWSEControllerFromVirtualCamera(pVirtualCamera, &spWSEController));
    
    return spWSEController->GetBlurState(pEnabled, pIntensity);
}

void RunWSERuntimeControl(IMFVirtualCamera* pVirtualCamera)
{
    if (!pVirtualCamera)
    {
        wprintf(L"Invalid virtual camera pointer\n");
        return;
    }
    
    wil::com_ptr<IWSEController> spWSEController;
    HRESULT hr = GetWSEControllerFromVirtualCamera(pVirtualCamera, &spWSEController);
    
    if (FAILED(hr))
    {
        wprintf(L"WSE Controller not available (HRESULT: 0x%08x)\n", hr);
        return;
    }
    
    wprintf(L"\n=== WSE Runtime Control Started ===\n");
    wprintf(L"You can now control Windows Studio Effects in real-time!\n");
    
    int choice;
    do {
        // Get current state
        BOOL blurEnabled = FALSE;
        float blurIntensity = 0.0f;
        hr = spWSEController->GetBlurState(&blurEnabled, &blurIntensity);
        
        if (FAILED(hr))
        {
            wprintf(L"Failed to get blur state: 0x%08x\n", hr);
            blurEnabled = FALSE;
            blurIntensity = 0.0f;
        }
        
        wprintf(L"\n=== Current WSE Status ===\n");
        wprintf(L"Background Blur: %s (Intensity: %.2f)\n", blurEnabled ? L"ON" : L"OFF", blurIntensity);
        wprintf(L"\n=== WSE Control Menu ===\n");
        wprintf(L"1. Toggle Background Blur\n");
        wprintf(L"2. Set Blur Intensity\n");
        wprintf(L"3. Enable Eye Contact Correction\n");
        wprintf(L"4. Enable Auto Framing\n");
        wprintf(L"5. Exit WSE Control\n");
        wprintf(L"\nChoice: ");
        
        // Read user input
        if (wscanf_s(L"%d", &choice) != 1)
        {
            choice = 0;
            // Clear input buffer
            int c;
            while ((c = getwchar()) != L'\n' && c != WEOF);
        }
        
        switch (choice)
        {
        case 1:
            {
                hr = spWSEController->EnableBackgroundBlur(!blurEnabled);
                if (SUCCEEDED(hr))
                {
                    wprintf(L"✓ Background blur %s\n", !blurEnabled ? L"ENABLED" : L"DISABLED");
                }
                else
                {
                    wprintf(L"✗ Failed to toggle blur: 0x%08x\n", hr);
                }
            }
            break;
            
        case 2:
            {
                float newIntensity;
                wprintf(L"Enter blur intensity (0.0 - 1.0): ");
                if (wscanf_s(L"%f", &newIntensity) == 1)
                {
                    hr = spWSEController->SetBlurIntensity(newIntensity);
                    if (SUCCEEDED(hr))
                    {
                        wprintf(L"✓ Blur intensity set to %.2f\n", newIntensity);
                    }
                    else
                    {
                        wprintf(L"✗ Failed to set blur intensity: 0x%08x\n", hr);
                    }
                }
                else
                {
                    wprintf(L"✗ Invalid input\n");
                }
            }
            break;
            
        case 3:
            {
                hr = spWSEController->EnableEyeContact(TRUE);
                if (SUCCEEDED(hr))
                {
                    wprintf(L"✓ Eye contact correction enabled\n");
                }
                else
                {
                    wprintf(L"✗ Failed to enable eye contact: 0x%08x\n", hr);
                }
            }
            break;
            
        case 4:
            {
                hr = spWSEController->EnableAutoFraming(TRUE);
                if (SUCCEEDED(hr))
                {
                    wprintf(L"✓ Auto framing enabled\n");
                }
                else
                {
                    wprintf(L"✗ Failed to enable auto framing: 0x%08x\n", hr);
                }
            }
            break;
            
        case 5:
            wprintf(L"Exiting WSE control...\n");
            break;
            
        default:
            wprintf(L"Invalid choice. Please select 1-5.\n");
            break;
        }
        
        if (choice != 5 && choice >= 1 && choice <= 4)
        {
            wprintf(L"Press Enter to continue...");
            getwchar(); // Consume the newline
            getwchar(); // Wait for Enter
        }
        
    } while (choice != 5);
    
    wprintf(L"\n=== WSE Runtime Control Ended ===\n");
}
