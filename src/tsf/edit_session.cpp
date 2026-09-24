#include "tsf/edit_session.hpp"

#include <string>

#include "tsf/text_service.hpp"
#include "util/log.hpp"

namespace okkhor_windows
{

    namespace
    {

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

    CompositionEditSession::CompositionEditSession(
        OkkhorTextService *service,
        ITfContext *context,
        CompositionEditOperation operation)
        : service_(service),
          context_(context),
          operation_(operation)
    {
        OKKHOR_LOG_INFO(
            std::string("CompositionEditSession created operation=") +
            (operation_ == CompositionEditOperation::Update
                 ? "Update"
                 : "End"));
    }

    STDMETHODIMP CompositionEditSession::QueryInterface(
        REFIID riid,
        void **ppv)
    {
        if (!ppv)
            return E_INVALIDARG;

        *ppv = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ITfEditSession))
        {
            *ppv = static_cast<ITfEditSession *>(this);
        }

        if (!*ppv)
            return E_NOINTERFACE;

        AddRef();

        return S_OK;
    }

    STDMETHODIMP_(ULONG)
    CompositionEditSession::AddRef()
    {
        return static_cast<ULONG>(
            ::InterlockedIncrement(&ref_count_));
    }

    STDMETHODIMP_(ULONG)
    CompositionEditSession::Release()
    {
        const LONG remaining =
            ::InterlockedDecrement(&ref_count_);

        if (remaining == 0)
            delete this;

        return static_cast<ULONG>(remaining);
    }

    STDMETHODIMP CompositionEditSession::DoEditSession(
        TfEditCookie edit_cookie)
    {
        OKKHOR_LOG_INFO(
            "CompositionEditSession::DoEditSession cookie=" +
            Hex(static_cast<unsigned long>(edit_cookie)));

        if (!service_)
        {
            OKKHOR_LOG_ERROR(
                "CompositionEditSession has no service");

            return E_UNEXPECTED;
        }

        if (!context_)
        {
            OKKHOR_LOG_ERROR(
                "CompositionEditSession has no context");

            return E_UNEXPECTED;
        }

        HRESULT hr = E_UNEXPECTED;

        switch (operation_)
        {
        case CompositionEditOperation::Update:

            OKKHOR_LOG_INFO(
                "CompositionEditSession dispatching Update");

            hr =
                service_->DoCompositionUpdate(
                    context_.Get(),
                    edit_cookie);

            break;

        case CompositionEditOperation::End:

            OKKHOR_LOG_INFO(
                "CompositionEditSession dispatching End");

            hr =
                service_->DoCompositionEnd(
                    context_.Get(),
                    edit_cookie);

            break;

        case CompositionEditOperation::DeleteSelection:
            return service_->DoDeleteSelection(
                context_.Get(),
                edit_cookie);

        default:

            OKKHOR_LOG_ERROR(
                "CompositionEditSession unknown operation");

            return E_UNEXPECTED;
        }

        OKKHOR_LOG_INFO(
            "CompositionEditSession completed hr=" +
            Hex(static_cast<unsigned long>(hr)));

        return hr;
    }

} // namespace okkhor_windows