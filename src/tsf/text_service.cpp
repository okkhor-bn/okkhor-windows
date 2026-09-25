#include "tsf/text_service.hpp"

#include <cstdio>
#include <new>
#include <string>
#include <utility>
#include <windows.h>

#include "app/configuration.hpp"
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
        {
            return {};
        }

        const int length = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            static_cast<int>(utf8.size()),
            nullptr,
            0);

        if (length <= 0)
        {
            return {};
        }

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

    // -----------------------------------------------------------------------------
    // Keyboard mapping
    // -----------------------------------------------------------------------------

    //
    // NOTE:
    //
    // 0x0409 is the LANGID for English (United States), but ToUnicodeEx expects
    // an HKL. This is kept here to match your current implementation.
    //
    // We can improve this later by loading the US keyboard layout with
    // LoadKeyboardLayoutW(L"00000409", ...).
    //
    const HKL kUsQwertyHkl =
        reinterpret_cast<HKL>(
            static_cast<UINT_PTR>(0x00000409));

    bool KeyboardKeyToLatin(
        WPARAM wParam,
        LPARAM lParam,
        char *latin)
    {
        if (!latin)
        {
            return false;
        }

        *latin = '\0';

        BYTE keyboard_state[256] = {};

        if (!::GetKeyboardState(keyboard_state))
        {
            return false;
        }

        const UINT scan_code =
            (static_cast<UINT>(lParam) >> 16) & 0xFF;

        wchar_t buffer[8] = {};

        const int result = ::ToUnicodeEx(
            static_cast<UINT>(wParam),
            scan_code,
            keyboard_state,
            buffer,
            ARRAYSIZE(buffer),
            1 << 2,
            kUsQwertyHkl);

        // We only accept one ordinary character.
        //
        // result < 0:
        //     dead key
        //
        // result == 0:
        //     no character
        //
        // result > 1:
        //     multiple UTF-16 code units
        //
        if (result != 1)
        {
            return false;
        }

        // The Okkhor input buffer is currently byte-based.
        // Therefore only accept ASCII here.
        if (buffer[0] > 0x7F)
        {
            return false;
        }

        *latin = static_cast<char>(buffer[0]);

        return true;
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

    // =============================================================================
    // Construction / destruction
    // =============================================================================

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

    // =============================================================================
    // IUnknown
    // =============================================================================

    STDMETHODIMP OkkhorTextService::QueryInterface(
        REFIID riid,
        void **ppv)
    {
        if (!ppv)
        {
            return E_INVALIDARG;
        }

        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ITfTextInputProcessor))
        {

            *ppv = static_cast<ITfTextInputProcessor *>(this);
        }
        else if (IsEqualIID(
                     riid,
                     IID_ITfTextInputProcessorEx))
        {

            *ppv = static_cast<ITfTextInputProcessorEx *>(this);
        }
        else if (IsEqualIID(
                     riid,
                     IID_ITfKeyEventSink))
        {

            *ppv = static_cast<ITfKeyEventSink *>(this);
        }

        if (!*ppv)
        {
            return E_NOINTERFACE;
        }

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
        {
            delete this;
        }

        return static_cast<ULONG>(remaining);
    }

    // =============================================================================
    // ITfTextInputProcessor
    // =============================================================================

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

        const HRESULT hr = AttachThreadManager(
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

    // =============================================================================
    // TSF attachment
    // =============================================================================

    HRESULT OkkhorTextService::AttachThreadManager(
        ITfThreadMgr *thread_mgr,
        TfClientId client_id,
        DWORD flags)
    {
        if (!thread_mgr)
        {
            return E_INVALIDARG;
        }

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

        HRESULT hr = thread_mgr_->QueryInterface(
            IID_PPV_ARGS(&keystroke_mgr_));

        if (FAILED(hr))
        {
            OKKHOR_LOG_ERROR(
                "failed to obtain ITfKeystrokeMgr, hr=" +
                Hex(static_cast<unsigned long>(hr)));

            return hr;
        }

        hr = keystroke_mgr_->AdviseKeyEventSink(
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

        ResetOkkhorState();

        client_id_ = TF_CLIENTID_NULL;
        activate_flags_ = 0;

        if (thread_mgr_)
        {
            thread_mgr_.Reset();

            OKKHOR_LOG_INFO(
                "thread manager detached");
        }
    }

    // =============================================================================
    // Okkhor state
    // =============================================================================

    void OkkhorTextService::ResetOkkhorState()
    {
        OKKHOR_LOG_INFO(
            "resetting Okkhor state");

        latin_buffer_.clear();
        composition_text_.clear();
        owned_range_.Reset();
        active_context_.Reset();
    }

    // =============================================================================
    // Focus
    // =============================================================================

    STDMETHODIMP OkkhorTextService::OnSetFocus(
        BOOL foreground)
    {
        OKKHOR_LOG_INFO(
            std::string("OnSetFocus foreground=") +
            (foreground ? "true" : "false"));

        return S_OK;
    }

    // =============================================================================
    // Key testing
    // =============================================================================

    STDMETHODIMP OkkhorTextService::OnTestKeyDown(
        ITfContext *,
        WPARAM wParam,
        LPARAM lParam,
        BOOL *eaten)
    {
        if (!eaten)
        {
            return E_INVALIDARG;
        }

        *eaten = FALSE;

        if (IsSystemModifierPressed())
        {
            return S_OK;
        }

        // ---------------------------------------------------------------------------
        // Backspace
        // ---------------------------------------------------------------------------

        if (wParam == VK_BACK)
        {
            *eaten = TRUE;

            return S_OK;
        }

        // ---------------------------------------------------------------------------
        // Space / Enter
        // ---------------------------------------------------------------------------

        if (wParam == VK_SPACE ||
            wParam == VK_RETURN)
        {

            *eaten = TRUE;

            return S_OK;
        }

        // ---------------------------------------------------------------------------
        // US QWERTY character
        // ---------------------------------------------------------------------------

        char latin = 0;

        if (KeyboardKeyToLatin(
                wParam,
                lParam,
                &latin))
        {

            *eaten = TRUE;

            OKKHOR_LOG_INFO(
                "OnTestKeyDown handled vk=" +
                Hex(static_cast<unsigned long>(wParam)) +
                " latin=" +
                std::string(1, latin));
        }
        else
        {

            OKKHOR_LOG_INFO(
                "OnTestKeyDown ignored vk=" +
                Hex(static_cast<unsigned long>(wParam)));
        }

        return S_OK;
    }

    STDMETHODIMP OkkhorTextService::OnTestKeyUp(
        ITfContext *,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
        {
            return E_INVALIDARG;
        }

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnTestKeyUp vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        return S_OK;
    }

    // =============================================================================
    // Key down
    // =============================================================================

    STDMETHODIMP OkkhorTextService::OnKeyDown(
        ITfContext *context,
        WPARAM wParam,
        LPARAM lParam,
        BOOL *eaten)
    {
        if (!eaten)
        {
            return E_INVALIDARG;
        }

        *eaten = FALSE;

        if (IsSystemModifierPressed())
        {
            return S_OK;
        }

        if (!context)
        {
            return E_INVALIDARG;
        }

        OKKHOR_LOG_INFO(
            "OnKeyDown vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        // ===========================================================================
        // Backspace
        // ===========================================================================

        if (wParam == VK_BACK)
        {
            // Always eat backspace and let DoBackspace (inside a single
            // edit session, with a real TfEditCookie) decide what it means:
            //   - pop the last Latin character if we're still mid-composition
            //     and the caret hasn't moved, or
            //   - fall back to deleting whatever is actually selected/at the
            //     caret if there's no live composition, or if the caret has
            //     moved away from the range Okkhor owns.
            //
            // We deliberately do NOT touch latin_buffer_/owned_range_ here:
            // deciding which of those cases applies requires checking the
            // current TSF selection, which needs an edit cookie.
            *eaten = TRUE;

            const HRESULT hr =
                RunEditSession(
                    context,
                    CompositionEditOperation::Backspace);

            if (FAILED(hr))
            {
                OKKHOR_LOG_ERROR(
                    "Backspace edit session failed, hr=" +
                    Hex(static_cast<unsigned long>(hr)));
            }

            return hr;
        }

        // ===========================================================================
        // Space / Enter
        // ===========================================================================

        if (wParam == VK_SPACE ||
            wParam == VK_RETURN)
        {

            OKKHOR_LOG_INFO(
                "Space/Enter received");

            //
            // There is no active ITfComposition anymore.
            //
            // The current Bangla output has already been committed.
            //
            // Simply release our ownership of the current word.
            //
            ResetOkkhorState();

            //
            // Let the application receive Space / Enter normally.
            //
            *eaten = FALSE;

            return S_OK;
        }

        // ===========================================================================
        // US QWERTY character
        // ===========================================================================

        char latin = 0;

        if (!KeyboardKeyToLatin(
                wParam,
                lParam,
                &latin))
        {

            OKKHOR_LOG_INFO(
                "OnKeyDown key not handled");

            return S_OK;
        }

        // ---------------------------------------------------------------------------
        // Append Latin character to our authoritative buffer and re-run
        // transliteration inside a single edit session.
        // ---------------------------------------------------------------------------

        latin_buffer_.push_back(latin);

        // Okkhor has claimed this key.
        // Never let Windows insert the original Latin character.
        *eaten = TRUE;

        const HRESULT hr =
            RunEditSession(
                context,
                CompositionEditOperation::Update);

        if (FAILED(hr))
        {
            OKKHOR_LOG_ERROR(
                "Update edit session failed, hr=" +
                Hex(static_cast<unsigned long>(hr)));
        }

        return hr;
    }

    // =============================================================================
    // Key up
    // =============================================================================

    STDMETHODIMP OkkhorTextService::OnKeyUp(
        ITfContext *,
        WPARAM wParam,
        LPARAM,
        BOOL *eaten)
    {
        if (!eaten)
        {
            return E_INVALIDARG;
        }

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnKeyUp vk=" +
            Hex(static_cast<unsigned long>(wParam)));

        return S_OK;
    }

    // =============================================================================
    // Preserved key
    // =============================================================================

    STDMETHODIMP OkkhorTextService::OnPreservedKey(
        ITfContext *,
        REFGUID rguid,
        BOOL *eaten)
    {
        if (!eaten)
        {
            return E_INVALIDARG;
        }

        *eaten = FALSE;

        OKKHOR_LOG_INFO(
            "OnPreservedKey guid=" +
            Hex(static_cast<unsigned long>(rguid.Data1)));

        return S_OK;
    }

    // =============================================================================
    // Check whether our owned range is still immediately before the caret
    // =============================================================================

    bool OkkhorTextService::IsOwnedRangeAtSelection(
        ITfContext *context,
        TfEditCookie edit_cookie) const
    {
        if (!context || !owned_range_)
        {
            return false;
        }

        // The range belongs to the context in which it was created.
        if (active_context_.Get() != context)
        {
            return false;
        }

        TF_SELECTION selection{};
        ULONG fetched = 0;

        HRESULT hr = context->GetSelection(
            edit_cookie,
            TF_DEFAULT_SELECTION,
            1,
            &selection,
            &fetched);

        if (FAILED(hr) ||
            fetched == 0 ||
            !selection.range)
        {
            return false;
        }

        // The user's selection must be a collapsed caret.
        BOOL selection_empty = FALSE;

        hr = selection.range->IsEmpty(
            edit_cookie,
            &selection_empty);

        if (FAILED(hr) || !selection_empty)
        {
            selection.range->Release();
            return false;
        }

        // The caret must be exactly at the end of our owned range.
        BOOL equal_end = FALSE;

        hr = selection.range->IsEqualEnd(
            edit_cookie,
            owned_range_.Get(),
            TF_ANCHOR_END,
            &equal_end);

        selection.range->Release();

        if (FAILED(hr))
        {
            return false;
        }

        return equal_end != FALSE;
    }

    // =============================================================================
    // Shared edit-session dispatch
    // =============================================================================
    //
    // NOTE: this used to be two separate functions (UpdateComposition and
    // an inline block in DeleteSelection) that each transliterated the
    // buffer and requested an edit session. DoCompositionUpdate below
    // *also* transliterated, so every keystroke ran the Okkhor engine
    // twice for no reason. All per-key work now happens exactly once,
    // inside the DoEditSession callback where we actually have a
    // TfEditCookie.
    //
    HRESULT OkkhorTextService::RunEditSession(
        ITfContext *context,
        CompositionEditOperation operation)
    {
        if (!context)
        {
            return E_INVALIDARG;
        }

        auto *session =
            new (std::nothrow) CompositionEditSession(
                this,
                context,
                operation);

        if (!session)
        {
            return E_OUTOFMEMORY;
        }

        HRESULT session_result = E_FAIL;

        const HRESULT hr =
            context->RequestEditSession(
                client_id_,
                session,
                TF_ES_SYNC | TF_ES_READWRITE,
                &session_result);

        session->Release();

        if (FAILED(hr))
        {
            return hr;
        }

        return session_result;
    }

    // =============================================================================
    // Perform committed-range replacement
    // =============================================================================

    HRESULT
    OkkhorTextService::DoCompositionUpdate(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        if (!context)
            return E_INVALIDARG;

        // -------------------------------------------------------------------------
        // If we still think we own a range, make sure the caret is actually
        // still sitting right after it. If the user clicked elsewhere,
        // selected different text, etc., owned_range_ is stale: abandon it
        // instead of blindly rewriting whatever it happens to point at.
        // Falling into the "no owned range" branch below then inserts a
        // fresh composition at the real, current selection.
        // -------------------------------------------------------------------------

        if (owned_range_ &&
            !IsOwnedRangeAtSelection(context, edit_cookie))
        {
            OKKHOR_LOG_INFO(
                "DoCompositionUpdate: owned range is stale; starting fresh");

            owned_range_.Reset();
            active_context_.Reset();
        }

        // -------------------------------------------------------------------------
        // Convert the current Latin buffer to Bangla.
        // -------------------------------------------------------------------------

        std::string bangla_utf8;

        if (!engine_.Transliterate(
                latin_buffer_,
                &bangla_utf8))
        {
            OKKHOR_LOG_ERROR(
                "DoCompositionUpdate: transliteration failed");
            return E_FAIL;
        }

        std::wstring bangla = Utf8ToWide(bangla_utf8);

        if (!bangla_utf8.empty() && bangla.empty())
        {
            OKKHOR_LOG_ERROR(
                "DoCompositionUpdate: UTF-8 -> UTF-16 conversion failed");
            return E_FAIL;
        }

        composition_text_ = std::move(bangla);

        // -------------------------------------------------------------------------
        // Empty output means delete our previously committed output.
        // -------------------------------------------------------------------------

        if (composition_text_.empty())
        {
            if (owned_range_)
            {
                HRESULT hr = owned_range_->SetText(
                    edit_cookie,
                    0,
                    L"",
                    0);

                if (FAILED(hr))
                    return hr;
            }

            if (latin_buffer_.empty())
            {
                // Buffer itself is empty (e.g. backspaced down to nothing):
                // there's nothing left to compose, so fully reset.
                ResetOkkhorState();
            }
            else
            {
                // The engine returned no output for a still non-empty
                // buffer (e.g. an incomplete phonetic sequence). Keep
                // latin_buffer_ so the user's typing isn't lost; just drop
                // the now-invalid owned range, since there's nothing on
                // screen for it to point at anymore.
                owned_range_.Reset();
                active_context_.Reset();
            }

            return S_OK;
        }

        // -------------------------------------------------------------------------
        // First character/word: insert the Bangla output and keep the exact
        // returned range as our owned range.
        // -------------------------------------------------------------------------

        if (!owned_range_)
        {
            OKKHOR_LOG_INFO(
                "no owned range; inserting first Okkhor output");

            Microsoft::WRL::ComPtr<ITfInsertAtSelection>
                insert_at_selection;

            HRESULT hr = context->QueryInterface(
                IID_PPV_ARGS(&insert_at_selection));

            if (FAILED(hr))
            {
                OKKHOR_LOG_ERROR(
                    "QueryInterface(ITfInsertAtSelection) failed, hr=" +
                    Hex(static_cast<unsigned long>(hr)));
                return hr;
            }

            Microsoft::WRL::ComPtr<ITfRange> inserted_range;

            hr = insert_at_selection->InsertTextAtSelection(
                edit_cookie,
                0,
                composition_text_.c_str(),
                static_cast<LONG>(composition_text_.size()),
                inserted_range.GetAddressOf());

            if (FAILED(hr))
                return hr;

            if (!inserted_range)
            {
                OKKHOR_LOG_ERROR(
                    "InsertTextAtSelection returned NULL range");
                return E_UNEXPECTED;
            }

            owned_range_ = std::move(inserted_range);

            active_context_ = context;

        }
        else
        {
            // ---------------------------------------------------------------------
            // Subsequent character: replace the exact range owned by Okkhor.
            // ---------------------------------------------------------------------

            OKKHOR_LOG_INFO(
                "replacing existing Okkhor-owned range");

            HRESULT hr = owned_range_->SetText(
                edit_cookie,
                0,
                composition_text_.c_str(),
                static_cast<LONG>(composition_text_.size()));

            if (FAILED(hr))
                return hr;
        }

        // -------------------------------------------------------------------------
        // Clone the owned range.
        //
        // IMPORTANT:
        // Do NOT collapse owned_range_ itself.
        // We need to keep the complete range so that the next key can replace it.
        // -------------------------------------------------------------------------

        Microsoft::WRL::ComPtr<ITfRange> caret_range;

        HRESULT hr = owned_range_->Clone(
            caret_range.GetAddressOf());

        if (FAILED(hr))
            return hr;

        if (!caret_range)
        {
            OKKHOR_LOG_ERROR(
                "owned_range Clone returned NULL range");
            return E_UNEXPECTED;
        }

        // -------------------------------------------------------------------------
        // Collapse ONLY the temporary caret range to the end of our output.
        // -------------------------------------------------------------------------

        hr = caret_range->Collapse(
            edit_cookie,
            TF_ANCHOR_END);

        if (FAILED(hr))
            return hr;

        // -------------------------------------------------------------------------
        // Move the application selection to the end of the committed Bangla text.
        // -------------------------------------------------------------------------

        TF_SELECTION selection{};

        selection.range = caret_range.Get();
        selection.style.ase = TF_AE_NONE;
        selection.style.fInterimChar = FALSE;

        hr = context->SetSelection(
            edit_cookie,
            1,
            &selection);

        if (FAILED(hr))
            return hr;

        active_context_ = context;

        OKKHOR_LOG_INFO(
            "DoCompositionUpdate completed successfully; "
            "output is committed and not underlined");

        return S_OK;
    }

    // =============================================================================
    // End current Okkhor word
    // =============================================================================

    HRESULT OkkhorTextService::EndComposition(
        ITfContext *)
    {
        //
        // There is no ITfComposition anymore.
        //
        // The output is already ordinary committed text.
        //
        OKKHOR_LOG_INFO(
            "EndComposition: releasing Okkhor-owned range");

        ResetOkkhorState();

        return S_OK;
    }

    // =============================================================================
    // Request end operation
    // =============================================================================

    HRESULT OkkhorTextService::DoCompositionEnd(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        OKKHOR_LOG_INFO(
            "DoCompositionEnd entered cookie=" +
            Hex(static_cast<unsigned long>(edit_cookie)));

        if (!context)
        {
            return E_INVALIDARG;
        }

        //
        // Nothing needs to be committed because the text was already committed
        // on every key.
        //
        ResetOkkhorState();

        OKKHOR_LOG_INFO(
            "Okkhor committed state released");

        return S_OK;
    }

    // =============================================================================
    // Backspace
    // =============================================================================
    //
    // Backspace means one of two different things depending on state, and we
    // can only tell which by checking the live TSF selection — which needs
    // an edit cookie. So this always runs inside a single edit session:
    //
    //   1. We're mid-composition AND the caret is still exactly where we
    //      left it (IsOwnedRangeAtSelection): undo the last typed Latin
    //      character and re-run transliteration, same as before.
    //
    //   2. Anything else — no active composition, or the caret/selection
    //      has moved away from owned_range_ (user clicked elsewhere,
    //      selected different text, etc.): abandon any stale composition
    //      and perform a normal delete of whatever is actually selected,
    //      or the character immediately before the caret.
    //
    HRESULT OkkhorTextService::DoBackspace(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        if (!context)
        {
            return E_INVALIDARG;
        }

        if (!latin_buffer_.empty() &&
            IsOwnedRangeAtSelection(context, edit_cookie))
        {
            latin_buffer_.pop_back();
            return DoCompositionUpdate(context, edit_cookie);
        }

        ResetOkkhorState();
        return DoDeleteSelection(context, edit_cookie);
    }

    // =============================================================================
    // Delete current selection (or, if collapsed, the character before it)
    // =============================================================================

    HRESULT OkkhorTextService::DoDeleteSelection(
        ITfContext *context,
        TfEditCookie edit_cookie)
    {
        if (!context)
        {
            return E_INVALIDARG;
        }

        TF_SELECTION selection{};
        ULONG fetched = 0;

        HRESULT hr =
            context->GetSelection(
                edit_cookie,
                TF_DEFAULT_SELECTION,
                1,
                &selection,
                &fetched);

        if (FAILED(hr))
        {
            return hr;
        }

        if (fetched == 0 ||
            !selection.range)
        {
            return S_OK;
        }

        // GetSelection AddRefs the range(s) it returns; take ownership here
        // so it's released even on early return (the previous version leaked
        // one reference on every call).
        Microsoft::WRL::ComPtr<ITfRange> range;
        range.Attach(selection.range);

        BOOL is_empty = FALSE;

        hr = range->IsEmpty(
            edit_cookie,
            &is_empty);

        if (FAILED(hr))
        {
            return hr;
        }

        if (is_empty)
        {
            // Nothing highlighted: a plain backspace deletes the character
            // immediately before the caret, so extend the range one
            // character to the left before deleting it.
            LONG shifted = 0;

            hr = range->ShiftStart(
                edit_cookie,
                -1,
                &shifted,
                nullptr);

            if (FAILED(hr))
            {
                return hr;
            }

            if (shifted == 0)
            {
                // Already at the start of the document/context.
                return S_OK;
            }
        }

        hr = range->SetText(
            edit_cookie,
            0,
            L"",
            0);

        if (FAILED(hr))
        {
            return hr;
        }

        ResetOkkhorState();

        return S_OK;
    }

} // namespace okkhor_windows