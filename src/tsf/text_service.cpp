#include "tsf/text_service.hpp"

#include <cstdio>
#include <new>
#include <string>
#include <utility>
#include <windows.h>

#include "tsf/edit_session.hpp"
#include "util/log.hpp"
#include "windows/module.hpp"
#include "windows/unicode.hpp"

namespace
{

    bool IsSystemModifierPressed()
    {
        return (::GetKeyState(VK_CONTROL) & 0x8000) != 0 ||
               (::GetKeyState(VK_LCONTROL) & 0x8000) != 0 ||
               (::GetKeyState(VK_RCONTROL) & 0x8000) != 0 ||
               (::GetKeyState(VK_LWIN) & 0x8000) != 0 ||
               (::GetKeyState(VK_RWIN) & 0x8000) != 0;
    }

    std::wstring Utf8ToWide(const std::string &utf8)
    {
        if (utf8.empty())
            return {};

        const int length = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            static_cast<int>(utf8.size()),
            nullptr,
            0);

        if (length <= 0)
            return {};

        std::wstring result(
            static_cast<std::size_t>(length),
            L'\0');

        if (::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                utf8.data(),
                static_cast<int>(utf8.size()),
                result.data(),
                length) <= 0)
        {
            return {};
        }

        return result;
    }

    bool VirtualKeyToLatin(WPARAM wParam, char *latin)
    {
        if (!latin)
            return false;

        *latin = '\0';

        if (wParam < 'A' || wParam > 'Z')
            return false;

        const bool shift =
            (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (shift)
            *latin = static_cast<char>(wParam);
        else
            *latin = static_cast<char>(wParam - 'A' + 'a');

        return true;
    }

    bool IsLatinKey(WPARAM vk)
    {
        return vk >= 'A' && vk <= 'Z';
    }

    bool IsHandledKey(WPARAM vk)
    {
        return IsLatinKey(vk) ||
               vk == VK_BACK ||
               vk == VK_SPACE ||
               vk == VK_RETURN ||
               vk == VK_OEM_1 ||      // ;
               vk == VK_OEM_COMMA ||  // ,
               vk == VK_OEM_PERIOD || // .
               vk == VK_OEM_3;        // `;
    }

    std::string Hex(unsigned long value)
    {
        char buffer[19] = {};

        std::snprintf(
            buffer,
            sizeof(buffer),
            "0x%08lX",
            value);

        return buffer;
    }

} // namespace

namespace okkhor_windows
{

    OkkhorTextService::OkkhorTextService()
        : ref_count_(1)
    {
        ModuleAddRef();
    }

    OkkhorTextService::~OkkhorTextService()
    {
        DetachThreadManager();
        ModuleRelease();
    }

    // -----------------------------------------------------------------------------
    // IUnknown
    // -----------------------------------------------------------------------------

    STDMETHODIMP OkkhorTextService::QueryInterface(
        REFIID riid,
        void **ppv)
    {
        if (!ppv)
            return E_INVALIDARG;

        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ITfTextInputProcessor))
        {
            *ppv = static_cast<ITfTextInputProcessor *>(this);
        }
        else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
        {
            *ppv = static_cast<ITfTextInputProcessorEx *>(this);
        }
        else if (IsEqualIID(riid, IID_ITfKeyEventSink))
        {
            *ppv = static_cast<ITfKeyEventSink *>(this);
        }

        if (!*ppv)
            return E_NOINTERFACE;

        AddRef();

        return S_OK;
    }

    STDMETHODIMP_(ULONG)
    OkkhorTextService::AddRef()
    {
        return static_cast<ULONG>(
            ::InterlockedIncrement(&ref_count_));
    }

    STDMETHODIMP_(ULONG)
    OkkhorTextService::Release()
    {
        const LONG remaining =
            ::InterlockedDecrement(&ref_count_);

        if (remaining == 0)
            delete this;

        return static_cast<ULONG>(remaining);
    }

    // -----------------------------------------------------------------------------
    // ITfTextInputProcessor
    // -----------------------------------------------------------------------------

    STDMETHODIMP OkkhorTextService::Activate(
        ITfThreadMgr *thread_mgr,
        TfClientId client_id)
    {
        return ActivateEx(
            thread_mgr,
            client_id,
            0);
    }

    STDMETHODIMP OkkhorTextService::ActivateEx(
        ITfThreadMgr *thread_mgr,
        TfClientId client_id,
        DWORD flags)
    {
        log::Initialize();

        OKKHOR_LOG_INFO(
            "TSF activation requested");

        const HRESULT hr =
            AttachThreadManager(
                thread_mgr,
                client_id,
                flags);

        if (FAILED(hr))
        {
            OKKHOR_LOG_ERROR(
                "activation failed, hr=" +
                Hex(static_cast<unsigned long>(hr)));

            DetachThreadManager();
        }

        return hr;
    }

    STDMETHODIMP OkkhorTextService::Deactivate()
    {
        OKKHOR_LOG_INFO(
            "TSF deactivated");

        DetachThreadManager();

        return S_OK;
    }

    // -----------------------------------------------------------------------------
    // TSF attachment
    // -----------------------------------------------------------------------------

    HRESULT OkkhorTextService::AttachThreadManager(
        ITfThreadMgr *thread_mgr,
        TfClientId client_id,
        DWORD flags)
    {
        if (!thread_mgr)
            return E_INVALIDARG;

        thread_mgr_ = thread_mgr;
        client_id_ = client_id;
        activate_flags_ = flags;

        OKKHOR_LOG_INFO(
            "thread manager attached, clientId=" +
            std::to_string(client_id) +
            " flags=" +
            Hex(flags));

        if (flags & TF_TMAE_SECUREMODE)
        {
            OKKHOR_LOG_INFO(
                "running on a secure desktop");
        }

        HRESULT hr =
            thread_mgr_->QueryInterface(
                IID_PPV_ARGS(&keystroke_mgr_));

        if (FAILED(hr))
        {
            OKKHOR_LOG_ERROR(
                "failed to obtain ITfKeystrokeMgr, hr=" +
                Hex(static_cast<unsigned long>(hr)));

            return hr;
        }

        hr =
            keystroke_mgr_->AdviseKeyEventSink(
                client_id_,
                static_cast<ITfKeyEventSink *>(this),
                TRUE);

        if (FAILED(hr))
        {
            OKKHOR_LOG_ERROR(
                "AdviseKeyEventSink failed, hr=" +
                Hex(static_cast<unsigned long>(hr)));

            keystroke_mgr_.Reset();

            return hr;
        }

        OKKHOR_LOG_INFO(
            "ITfKeyEventSink advised");

        return S_OK;
    }

    void OkkhorTextService::DetachThreadManager()
    {
        if (keystroke_mgr_)
        {
            keystroke_mgr_->UnadviseKeyEventSink(
                client_id_);

            keystroke_mgr_.Reset();

            OKKHOR_LOG_INFO(
                "ITfKeyEventSink unadvised");
        }

        composition_.Reset();
        active_context_.Reset();

        composition_text_.clear();
        latin_buffer_.clear();

        client_id_ = TF_CLIENTID_NULL;
        activate_flags_ = 0;

        if (thread_mgr_)
        {
            thread_mgr_.Reset();

            OKKHOR_LOG_INFO(
                "thread manager detached");
        }
    }

    // -----------------------------------------------------------------------------
    // ITfKeyEventSink
    // -----------------------------------------------------------------------------

    STDMETHODIMP OkkhorTextService::OnSetFocus(
        BOOL foreground)
    {
        OKKHOR_LOG_INFO(
            std::string("OnSetFocus foreground=") +
            (foreground ? "true" : "false"));

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnTestKeyDown(
        ITfContext *,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
            return E_INVALIDARG;

        *eaten = FALSE;

        if (IsSystemModifierPressed())
            return S_OK;

        if (IsHandledKey(wParam))
        {
            *eaten = TRUE;

            OKKHOR_LOG_INFO(
                "OnTestKeyDown handled vk=" +
                Hex(static_cast<unsigned long>(wParam)));

            return S_OK;
        }

        OKKHOR_LOG_INFO(
            "OnTestKeyDown ignored vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnTestKeyUp(
        ITfContext *,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
            return E_INVALIDARG;

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnTestKeyUp vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnKeyDown(
        ITfContext *context,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
            return E_INVALIDARG;

        *eaten = FALSE;

        if (IsSystemModifierPressed())
            return S_OK;

        if (!context)
            return E_INVALIDARG;

        OKKHOR_LOG_INFO(
            "OnKeyDown vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        // -------------------------------------------------------------------------
        // Backspace
        // -------------------------------------------------------------------------

        if (wParam == VK_BACK)
        {
            TF_SELECTION selection{};

            HRESULT selection_hr =
                E_FAIL;

            ULONG fetched = 0;

            // We need an edit session to safely inspect the selection.
            // For now, if an Okkhor composition exists, let the
            // composition edit session handle the selection.
            if (composition_)
            {
                selection_hr =
                    context->GetSelection(
                        TF_INVALID_COOKIE,
                        TF_DEFAULT_SELECTION,
                        1,
                        &selection,
                        &fetched);
            }

            if (SUCCEEDED(selection_hr) &&
                fetched > 0 &&
                selection.range)
            {
                LONG selection_start = 0;
                LONG selection_length = 0;

                if (SUCCEEDED(
                        selection.range->GetExtent(
                            &selection_start,
                            &selection_length)) &&
                    selection_length > 0)
                {
                    selection.range->Release();

                    HRESULT hr =
                        DeleteSelection(context);

                    if (SUCCEEDED(hr))
                        *eaten = TRUE;

                    return hr;
                }

                selection.range->Release();
            }

            if (latin_buffer_.empty())
                return S_OK;

            latin_buffer_.pop_back();

            OKKHOR_LOG_INFO(
                "Backspace latin buffer=\"" +
                latin_buffer_ +
                "\"");

            const HRESULT hr =
                UpdateComposition(context);

            OKKHOR_LOG_INFO(
                "Backspace UpdateComposition hr=" +
                Hex(static_cast<unsigned long>(hr)));

            if (SUCCEEDED(hr))
                *eaten = TRUE;

            return hr;
        }

        // -------------------------------------------------------------------------
        // Space / Enter
        // -------------------------------------------------------------------------

        if (wParam == VK_SPACE ||
            wParam == VK_RETURN)
        {
            OKKHOR_LOG_INFO(
                "Space/Enter received");

            if (composition_)
            {
                const HRESULT hr =
                    EndComposition(context);

                OKKHOR_LOG_INFO(
                    "EndComposition hr=" +
                    Hex(static_cast<unsigned long>(hr)));

                if (FAILED(hr))
                    return hr;
            }

            latin_buffer_.clear();
            composition_text_.clear();

            *eaten = FALSE;

            return S_OK;
        }

        // -------------------------------------------------------------------------
        // A-Z
        // -------------------------------------------------------------------------

        char latin = 0;

        if (!VirtualKeyToLatin(
                wParam,
                &latin))
        {
            OKKHOR_LOG_INFO(
                "OnKeyDown key not handled");

            return S_OK;
        }

        latin_buffer_.push_back(latin);

        OKKHOR_LOG_INFO(
            "Latin buffer=\"" +
            latin_buffer_ +
            "\"");

        const HRESULT hr =
            UpdateComposition(context);

        OKKHOR_LOG_INFO(
            "UpdateComposition returned hr=" +
            Hex(static_cast<unsigned long>(hr)));

        if (SUCCEEDED(hr))
            *eaten = TRUE;

        return hr;
    }

    STDMETHODIMP OkkhorTextService::OnKeyUp(
        ITfContext *,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
            return E_INVALIDARG;

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnKeyUp vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnPreservedKey(
        ITfContext *,
        REFGUID rguid,
        BOOL *eaten)
    {
        if (!eaten)
            return E_INVALIDARG;

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnPreservedKey guid=" +
            Hex(static_cast<unsigned long>(rguid.Data1)));

        return S_OK;
    }

    // -----------------------------------------------------------------------------
    // Composition
    // -----------------------------------------------------------------------------

    HRESULT OkkhorTextService::UpdateComposition(
        ITfContext *context)
    {
        if (!context)
            return E_INVALIDARG;

        // -------------------------------------------------------------------------
        // Core transliteration
        // -------------------------------------------------------------------------

        std::string bangla_utf8;

        if (!engine_.Transliterate(
                latin_buffer_,
                &bangla_utf8))
        {
            OKKHOR_LOG_ERROR(
                "UpdateComposition: core transliteration failed");

            return E_FAIL;
        }

        OKKHOR_LOG_TEXT(
            "core output",
            bangla_utf8);

        std::wstring bangla =
            Utf8ToWide(bangla_utf8);

        if (!bangla_utf8.empty() &&
            bangla.empty())
        {
            OKKHOR_LOG_ERROR(
                "UpdateComposition: UTF-8 to UTF-16 conversion failed");

            return E_FAIL;
        }

        composition_text_ =
            std::move(bangla);

        active_context_ = context;

        // -------------------------------------------------------------------------
        // Create edit session
        // -------------------------------------------------------------------------

        OKKHOR_LOG_INFO(
            "creating CompositionEditSession");

        auto *session =
            new CompositionEditSession(
                this,
                context,
                CompositionEditOperation::Update);

        if (!session)
            return E_OUTOFMEMORY;

        HRESULT session_result = E_FAIL;

        OKKHOR_LOG_INFO(
            "calling RequestEditSession");

        const HRESULT hr =
            context->RequestEditSession(
                client_id_,
                session,
                TF_ES_SYNC | TF_ES_READWRITE,
                &session_result);

        OKKHOR_LOG_INFO(
            "RequestEditSession returned hr=" +
            Hex(static_cast<unsigned long>(hr)) +
            " session_result=" +
            Hex(static_cast<unsigned long>(session_result)));

        session->Release();

        if (FAILED(hr))
            return hr;

        return session_result;
    }

    HRESULT OkkhorTextService::DoCompositionUpdate(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        if (!context)
            return E_INVALIDARG;

        log::Write(
            log::Level::Info,
            "DoCompositionUpdate entered cookie=" +
                std::to_string(edit_cookie));

        HRESULT hr = S_OK;

        if (!composition_)
        {
            log::Write(
                log::Level::Info,
                "no existing composition; obtaining composition services");

            Microsoft::WRL::ComPtr<ITfContextComposition>
                composition_services;

            hr = context->QueryInterface(
                IID_ITfContextComposition,
                reinterpret_cast<void **>(
                    composition_services.GetAddressOf()));

            log::Write(
                log::Level::Info,
                "QueryInterface(ITfContextComposition) hr=" +
                    Hex(static_cast<unsigned long>(hr)));

            if (FAILED(hr))
                return hr;

            Microsoft::WRL::ComPtr<ITfInsertAtSelection>
                insert_at_selection;

            hr = context->QueryInterface(
                IID_ITfInsertAtSelection,
                reinterpret_cast<void **>(
                    insert_at_selection.GetAddressOf()));

            log::Write(
                log::Level::Info,
                "QueryInterface(ITfInsertAtSelection) hr=" +
                    Hex(static_cast<unsigned long>(hr)));

            if (FAILED(hr))
                return hr;

            Microsoft::WRL::ComPtr<ITfRange>
                composition_range;

            hr = insert_at_selection->InsertTextAtSelection(
                edit_cookie,
                TF_IAS_QUERYONLY,
                nullptr,
                0,
                composition_range.GetAddressOf());

            log::Write(
                log::Level::Info,
                "InsertTextAtSelection(TF_IAS_QUERYONLY) hr=" +
                    Hex(static_cast<unsigned long>(hr)));

            if (FAILED(hr))
                return hr;

            if (!composition_range)
            {
                log::Write(
                    log::Level::Error,
                    "InsertTextAtSelection returned NULL range");

                return E_UNEXPECTED;
            }

            log::Write(
                log::Level::Info,
                "starting composition with ITfCompositionSink");

            composition_.Reset();

            hr = composition_services->StartComposition(
                edit_cookie,
                composition_range.Get(),
                this,
                composition_.GetAddressOf());

            log::Write(
                log::Level::Info,
                "StartComposition returned hr=" +
                    Hex(static_cast<unsigned long>(hr)));

            if (FAILED(hr))
                return hr;

            if (!composition_)
            {
                log::Write(
                    log::Level::Error,
                    "StartComposition returned S_OK but composition is NULL");

                return E_FAIL;
            }

            log::Write(
                log::Level::Info,
                "composition started successfully");
        }

        Microsoft::WRL::ComPtr<ITfRange>
            active_range;

        hr = composition_->GetRange(
            active_range.GetAddressOf());

        log::Write(
            log::Level::Info,
            "composition->GetRange hr=" +
                Hex(static_cast<unsigned long>(hr)));

        if (FAILED(hr))
            return hr;

        if (!active_range)
            return E_UNEXPECTED;

        hr = active_range->SetText(
            edit_cookie,
            0,
            composition_text_.c_str(),
            static_cast<LONG>(composition_text_.size()));

        if (FAILED(hr))
            return hr;

        hr = active_range->Collapse(
            edit_cookie,
            TF_ANCHOR_END);

        if (FAILED(hr))
            return hr;

        TF_SELECTION selection{};

        selection.range = active_range.Get();
        selection.style.ase = TF_AE_NONE;
        selection.style.fInterimChar = FALSE;

        hr = context->SetSelection(
            edit_cookie,
            1,
            &selection);

        if (FAILED(hr))
            return hr;

        log::Write(
            log::Level::Info,
            "DoCompositionUpdate completed successfully");

        return S_OK;
    }

    HRESULT OkkhorTextService::EndComposition(
        ITfContext *context)
    {
        if (!composition_)
            return S_OK;

        if (!context)
            return E_INVALIDARG;

        OKKHOR_LOG_INFO(
            "creating End CompositionEditSession");

        auto *session =
            new CompositionEditSession(
                this,
                context,
                CompositionEditOperation::End);

        if (!session)
            return E_OUTOFMEMORY;

        HRESULT session_result = E_FAIL;

        const HRESULT hr =
            context->RequestEditSession(
                client_id_,
                session,
                TF_ES_SYNC | TF_ES_READWRITE,
                &session_result);

        OKKHOR_LOG_INFO(
            "End RequestEditSession returned hr=" +
            Hex(static_cast<unsigned long>(hr)) +
            " session_result=" +
            Hex(static_cast<unsigned long>(session_result)));

        session->Release();

        if (FAILED(hr))
            return hr;

        return session_result;
    }

    HRESULT OkkhorTextService::DeleteSelection(
        ITfContext *context)
    {
        if (!context)
            return E_INVALIDARG;

        auto *session =
            new CompositionEditSession(
                this,
                context,
                CompositionEditOperation::DeleteSelection);

        if (!session)
            return E_OUTOFMEMORY;

        HRESULT session_result = E_FAIL;

        const HRESULT hr =
            context->RequestEditSession(
                client_id_,
                session,
                TF_ES_SYNC | TF_ES_READWRITE,
                &session_result);

        session->Release();

        if (FAILED(hr))
            return hr;

        return session_result;
    }

    HRESULT OkkhorTextService::DoCompositionEnd(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        OKKHOR_LOG_INFO(
            "DoCompositionEnd entered cookie=" +
            Hex(static_cast<unsigned long>(edit_cookie)));

        if (!context)
            return E_INVALIDARG;

        if (!composition_)
            return S_OK;

        const HRESULT hr =
            composition_->EndComposition(
                edit_cookie);

        OKKHOR_LOG_INFO(
            "ITfComposition::EndComposition returned hr=" +
            Hex(static_cast<unsigned long>(hr)));

        if (SUCCEEDED(hr))
        {
            OKKHOR_LOG_INFO(
                "TSF composition ended");

            composition_.Reset();
            active_context_.Reset();
            composition_text_.clear();
            latin_buffer_.clear();
        }

        return hr;
    }

    HRESULT OkkhorTextService::DoDeleteSelection(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        if (!context)
            return E_INVALIDARG;

        TF_SELECTION selection{};

        ULONG fetched = 0;

        HRESULT hr = context->GetSelection(
            edit_cookie,
            TF_DEFAULT_SELECTION,
            1,
            &selection,
            &fetched);

        if (FAILED(hr))
            return hr;

        if (fetched == 0 || !selection.range)
            return S_OK;

        LONG start = 0;
        LONG end = 0;

        hr = selection.range->GetExtent(
            &start,
            &end);

        if (FAILED(hr))
            return hr;

        if (end == 0)
            return S_OK;

        hr = selection.range->SetText(
            edit_cookie,
            0,
            L"",
            0);

        if (FAILED(hr))
            return hr;

        // The application text was deleted, so Okkhor's
        // composition state must no longer represent it.
        composition_.Reset();
        active_context_.Reset();
        composition_text_.clear();
        latin_buffer_.clear();

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnCompositionTerminated(
        TfEditCookie edit_cookie,
        ITfComposition *composition)
    {
        log::Write(
            log::Level::Info,
            "OnCompositionTerminated cookie=" +
                std::to_string(edit_cookie));

        if (composition_.Get() == composition)
        {
            log::Write(
                log::Level::Info,
                "active composition was terminated by TSF");

            composition_.Reset();
            active_context_.Reset();
            composition_text_.clear();
            latin_buffer_.clear();
        }

        return S_OK;
    }

} // namespace okkhor_windows