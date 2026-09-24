#pragma once

#include <msctf.h>
#include <wrl/client.h>

namespace okkhor_windows
{

class OkkhorTextService;

enum class CompositionEditOperation
{
    Update,
    End,
    DeleteSelection
};

class CompositionEditSession
    : public ITfEditSession
{
public:

    CompositionEditSession(
        OkkhorTextService* service,
        ITfContext* context,
        CompositionEditOperation operation);

    STDMETHODIMP QueryInterface(
        REFIID riid,
        void** ppv) override;

    STDMETHODIMP_(ULONG) AddRef() override;

    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP DoEditSession(
        TfEditCookie edit_cookie) override;

private:

    ~CompositionEditSession() = default;

    LONG ref_count_ = 1;

    OkkhorTextService* service_ = nullptr;

    Microsoft::WRL::ComPtr<ITfContext> context_;

    CompositionEditOperation operation_;
};

} // namespace okkhor_windows