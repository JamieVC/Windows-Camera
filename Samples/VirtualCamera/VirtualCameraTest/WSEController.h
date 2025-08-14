#include <windows.h>
#include <initguid.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>
#include <functional>
#include <propkey.h>

// WSE Effect Types
enum class WSEEffectType : UINT32
{
    None = 0,
    BackgroundBlur = 1,
    EyeContact = 2,
    AutoFraming = 4,
    PortraitLighting = 8
};

DEFINE_ENUM_FLAG_OPERATORS(WSEEffectType);

// Custom attributes for WSE
DEFINE_PROPERTYKEY(MF_WSE_BLUR_ENABLED, 
    0xa1b2c3d4, 0xe5f6, 0x7890, 0xab, 0xcd, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 2);

DEFINE_PROPERTYKEY(MF_WSE_BLUR_INTENSITY,
    0xa1b2c3d5, 0xe5f6, 0x7890, 0xab, 0xcd, 0x12, 0x34, 0x56, 0x78, 0x90, 0xac, 3);

DEFINE_GUID(MF_WSE_SERVICE,
    0xa1b2c3d6, 0xe5f6, 0x7890, 0xab, 0xcd, 0x12, 0x34, 0x56, 0x78, 0x90, 0xad);

// WSE Dynamic Controller Interface
MIDL_INTERFACE("a1b2c3d7-e5f6-7890-abcd-123456789abc")
IWSEController : public IUnknown
{
public:
    STDMETHOD(EnableBackgroundBlur)(BOOL enable) = 0;
    STDMETHOD(SetBlurIntensity)(float intensity) = 0;
    STDMETHOD(GetBlurState)(BOOL* pEnabled, float* pIntensity) = 0;
    STDMETHOD(EnableEyeContact)(BOOL enable) = 0;
    STDMETHOD(EnableAutoFraming)(BOOL enable) = 0;
    STDMETHOD(ApplyEffectsRealtime)(BOOL enable) = 0;
    STDMETHOD(ProcessFrame)(IMFSample* pInputSample, IMFSample** ppOutputSample) = 0;
};

// Runtime WSE Configuration - Simple C++ class, not WinRT
class WSEController : public IWSEController
{
public:
    WSEController();
    ~WSEController();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IWSEController methods
    STDMETHOD(EnableBackgroundBlur)(BOOL enable) override;
    STDMETHOD(SetBlurIntensity)(float intensity) override;
    STDMETHOD(GetBlurState)(BOOL* pEnabled, float* pIntensity) override;
    STDMETHOD(EnableEyeContact)(BOOL enable) override;
    STDMETHOD(EnableAutoFraming)(BOOL enable) override;
    STDMETHOD(ApplyEffectsRealtime)(BOOL enable) override;
    STDMETHOD(ProcessFrame)(IMFSample* pInputSample, IMFSample** ppOutputSample) override;

    // Additional helper methods
    void SetBackgroundBlurTransform(Microsoft::WRL::ComPtr<IMFTransform> blurTransform);
    void RegisterEffectCallback(std::function<void(WSEEffectType, bool)> callback);

private:
    // Reference counting
    ULONG m_cRef;
    
    // Thread safety using WIL
    wil::critical_section m_lock;
    
    // Effect states
    bool m_blurEnabled;
    float m_blurIntensity;
    bool m_eyeContactEnabled;
    bool m_autoFramingEnabled;
    bool m_realtimeProcessing;
    
    // MF Transforms for effects
    Microsoft::WRL::ComPtr<IMFTransform> m_blurTransform;
    Microsoft::WRL::ComPtr<IMFTransform> m_eyeContactTransform;
    Microsoft::WRL::ComPtr<IMFTransform> m_autoFramingTransform;
    
    // Effect callback
    std::function<void(WSEEffectType, bool)> m_effectCallback;
    
    // Helper methods
    HRESULT InitializeBlurTransform();
    HRESULT InitializeEyeContactTransform();
    HRESULT InitializeAutoFramingTransform();
    HRESULT ApplyBlurEffect(IMFSample* pInputSample, IMFSample** ppOutputSample);
    void NotifyEffectStateChange(WSEEffectType effectType, bool enabled);
};
