#pragma once

#include <msctf.h>
#include <windows.h>
#include <string>
#include <wrl/client.h>

#include "core/engine_host.hpp"

namespace okkhor_windows
{

    class CompositionEditSession;
    class OkkhorTextService
        : public ITfTextInputProcessorEx,
          public ITfKeyEventSink,
          public ITfCompositionSink
    {

    public:
        OkkhorTextService();

        STDMETHODIMP OnCompositionTerminated(
            TfEditCookie edit_cookie,
            ITfComposition *composition) override;

        // -------------------------------------------------------------------------
        // IUnknown
        // -------------------------------------------------------------------------

        STDMETHODIMP QueryInterface(
            REFIID riid,
            void **ppv) override;

        STDMETHODIMP_(ULONG)
        AddRef() override;

        STDMETHODIMP_(ULONG)
        Release() override;

        // -------------------------------------------------------------------------
        // ITfTextInputProcessor
        // -------------------------------------------------------------------------

        STDMETHODIMP Activate(
            ITfThreadMgr *thread_mgr,
            TfClientId client_id) override;

        STDMETHODIMP Deactivate() override;

        // -------------------------------------------------------------------------
        // ITfTextInputProcessorEx
        // -------------------------------------------------------------------------

        STDMETHODIMP ActivateEx(
            ITfThreadMgr *thread_mgr,
            TfClientId client_id,
            DWORD flags) override;

        // -------------------------------------------------------------------------
        // ITfKeyEventSink
        // -------------------------------------------------------------------------

        STDMETHODIMP OnSetFocus(
            BOOL foreground) override;

        STDMETHODIMP OnTestKeyDown(
            ITfContext *context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL *eaten) override;

        STDMETHODIMP OnTestKeyUp(
            ITfContext *context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL *eaten) override;

        STDMETHODIMP OnKeyDown(
            ITfContext *context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL *eaten) override;

        STDMETHODIMP OnKeyUp(
            ITfContext *context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL *eaten) override;

        STDMETHODIMP OnPreservedKey(
            ITfContext *context,
            REFGUID rguid,
            BOOL *eaten) override;


        // -------------------------------------------------------------------------
        // Composition edit-session entry points
        // -------------------------------------------------------------------------

        HRESULT DoCompositionUpdate(
            ITfContext *context,
            TfEditCookie edit_cookie);

        HRESULT DoCompositionEnd(
            ITfContext *context,
            TfEditCookie edit_cookie);

    private:
        ~OkkhorTextService();

        HRESULT AttachThreadManager(
            ITfThreadMgr *thread_mgr,
            TfClientId client_id,
            DWORD flags);

        void DetachThreadManager();

        void LoadEngine();

        HRESULT UpdateComposition(
            ITfContext *context);

        HRESULT EndComposition(
            ITfContext *context);

        LONG ref_count_;

        // -------------------------------------------------------------------------
        // TSF state
        // -------------------------------------------------------------------------

        Microsoft::WRL::ComPtr<ITfThreadMgr> thread_mgr_;

        Microsoft::WRL::ComPtr<ITfKeystrokeMgr> keystroke_mgr_;

        TfClientId client_id_ = TF_CLIENTID_NULL;

        DWORD activate_flags_ = 0;

        // -------------------------------------------------------------------------
        // Okkhor core
        // -------------------------------------------------------------------------

        EngineHost engine_;

        // -------------------------------------------------------------------------
        // Current phonetic composition
        // -------------------------------------------------------------------------

        std::string latin_buffer_;

        std::wstring composition_text_;

        Microsoft::WRL::ComPtr<ITfContext> active_context_;

        Microsoft::WRL::ComPtr<ITfComposition> composition_;

        friend class CompositionEditSession;
    };

} // namespace okkhor_windows