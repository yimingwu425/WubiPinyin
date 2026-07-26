#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"

#include <inputscope.h>

namespace {

const GUID kGuidPropInputScope = {
    0x1713dd5a, 0x68e7, 0x4a5b,
    {0x9a, 0xf6, 0x59, 0x2a, 0x59, 0x5c, 0x77, 0x8d}};

bool IsSensitiveInputScope(InputScope scope) {
  switch (scope) {
    case IS_PASSWORD:
    case IS_NUMERIC_PASSWORD:
    case IS_NUMERIC_PIN:
    case IS_ALPHANUMERIC_PIN:
    case IS_ALPHANUMERIC_PIN_SET:
      return true;
    default:
      return false;
  }
}

}  // namespace

class CPasswordInputScopeEditSession : public CEditSession {
 public:
  CPasswordInputScopeEditSession(com_ptr<WeaselTSF> pTextService,
                                 com_ptr<ITfContext> pContext)
      : CEditSession(pTextService, pContext) {}

  STDMETHODIMP DoEditSession(TfEditCookie ec) override {
    if (_pTextService == nullptr) {
      return E_FAIL;
    }
    _pTextService->_ReadPasswordInputScope(_pContext, ec);
    return S_OK;
  }
};

static BOOL IsRangeCovered(TfEditCookie ec,
                           ITfRange* pRangeTest,
                           ITfRange* pRangeCover) {
  LONG lResult;

  if (pRangeCover->CompareStart(ec, pRangeTest, TF_ANCHOR_START, &lResult) !=
          S_OK ||
      lResult > 0)
    return FALSE;
  if (pRangeCover->CompareEnd(ec, pRangeTest, TF_ANCHOR_END, &lResult) !=
          S_OK ||
      lResult < 0)
    return FALSE;
  return TRUE;
}

STDAPI WeaselTSF::OnEndEdit(ITfContext* pContext,
                            TfEditCookie ecReadOnly,
                            ITfEditRecord* pEditRecord) {
  BOOL fSelectionChanged;
  IEnumTfRanges* pEnumTextChanges;
  ITfRange* pRange;

  /* did the selection change? */
  if (pEditRecord->GetSelectionStatus(&fSelectionChanged) == S_OK &&
      fSelectionChanged) {
    if (_IsComposing()) {
      /* if the caret moves out of composition range, stop the composition */
      TF_SELECTION tfSelection;
      ULONG cFetched;

      if (pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1,
                                 &tfSelection, &cFetched) == S_OK &&
          cFetched == 1) {
        ITfRange* pRangeComposition;
        if (_pComposition->GetRange(&pRangeComposition) == S_OK) {
          if (!IsRangeCovered(ecReadOnly, tfSelection.range, pRangeComposition))
            _EndComposition(pContext, true);
          pRangeComposition->Release();
        }
      }
    }
  }

  /* text modification? */
  if (pEditRecord->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, NULL, 0,
                                             &pEnumTextChanges) == S_OK) {
    if (pEnumTextChanges->Next(1, &pRange, NULL) == S_OK) {
      pRange->Release();
    }
    pEnumTextChanges->Release();
  }

  if (pContext == _pTextEditSinkContext) {
    _ReadPasswordInputScope(pContext, ecReadOnly);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnLayoutChange(ITfContext* pContext,
                                 TfLayoutCode lcode,
                                 ITfContextView* pContextView) {
  if (!_IsComposing())
    return S_OK;

  if (pContext != _pTextEditSinkContext)
    return S_OK;

  if (lcode == TF_LC_CHANGE)
    _UpdateCompositionWindow(pContext);
  return S_OK;
}

BOOL WeaselTSF::_InitTextEditSink(com_ptr<ITfDocumentMgr> pDocMgr) {
  com_ptr<ITfSource> pSource;
  const com_ptr<ITfContext> previous_context = _pTextEditSinkContext;
  BOOL fRet;

  /* clear out any previous sink first */
  if (_dwTextEditSinkCookie != TF_INVALID_COOKIE) {
    _pTextEditSinkContext->QueryInterface(&pSource);
    if (pSource != nullptr) {
      pSource->UnadviseSink(_dwTextEditSinkCookie);
      pSource->UnadviseSink(_dwTextLayoutSinkCookie);
    }
    _pTextEditSinkContext = nullptr;
    _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  }
  if (pDocMgr == NULL) {
    _SetPasswordInputScopeState(PasswordInputScopeState::kUnknown);
    return TRUE;
  }

  if (pDocMgr->GetTop(&_pTextEditSinkContext) != S_OK)
    return FALSE;

  if (_pTextEditSinkContext == NULL)
    return TRUE;

  fRet = FALSE;

  pSource.Release();

  if (_pTextEditSinkContext->QueryInterface(IID_ITfSource, (void**)&pSource) ==
      S_OK) {
    if (pSource->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink*)this,
                            &_dwTextEditSinkCookie) == S_OK)
      fRet = TRUE;
    else
      _dwTextEditSinkCookie = TF_INVALID_COOKIE;
    if (pSource->AdviseSink(IID_ITfTextLayoutSink, (ITfTextLayoutSink*)this,
                            &_dwTextLayoutSinkCookie) == S_OK) {
      fRet = TRUE;
    } else
      _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  }
  if (fRet == FALSE) {
    _pTextEditSinkContext = nullptr;
  }

  if (previous_context != _pTextEditSinkContext) {
    _SetPasswordInputScopeState(PasswordInputScopeState::kUnknown);
  }
  _RequestPasswordInputScope(_pTextEditSinkContext);

  return fRet;
}

void WeaselTSF::_RequestPasswordInputScope(com_ptr<ITfContext> pContext) {
  if (pContext == nullptr) {
    return;
  }

  auto* session = new CPasswordInputScopeEditSession(this, pContext);
  if (session == nullptr) {
    return;
  }

  HRESULT hr = E_FAIL;
  pContext->RequestEditSession(_tfClientId, session,
                               TF_ES_ASYNCDONTCARE | TF_ES_READ, &hr);
  session->Release();
}

void WeaselTSF::_ReadPasswordInputScope(ITfContext* pContext,
                                         TfEditCookie ecReadOnly) {
  if (pContext == nullptr || pContext != _pTextEditSinkContext) {
    return;
  }

  PasswordInputScopeState state = PasswordInputScopeState::kUnknown;
  TF_SELECTION selection = {};
  ULONG selection_count = 0;
  if (FAILED(pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1,
                                    &selection, &selection_count)) ||
      selection_count != 1 || selection.range == nullptr) {
    _SetPasswordInputScopeState(state);
    return;
  }

  com_ptr<ITfProperty> property;
  if (SUCCEEDED(pContext->GetProperty(kGuidPropInputScope, &property)) &&
      property != nullptr) {
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(property->GetValue(ecReadOnly, selection.range, &value))) {
      if (value.vt == VT_EMPTY) {
        state = PasswordInputScopeState::kNonSensitive;
      } else if (value.vt == VT_UNKNOWN && value.punkVal != nullptr) {
        com_ptr<ITfInputScope> input_scope;
        if (SUCCEEDED(value.punkVal->QueryInterface(&input_scope)) &&
            input_scope != nullptr) {
          InputScope* scopes = nullptr;
          UINT scope_count = 0;
          if (SUCCEEDED(input_scope->GetInputScopes(&scopes, &scope_count)) &&
              (scope_count == 0 || scopes != nullptr)) {
            bool sensitive = false;
            for (UINT i = 0; i < scope_count; ++i) {
              sensitive = sensitive || IsSensitiveInputScope(scopes[i]);
            }
            state = sensitive ? PasswordInputScopeState::kSensitive
                              : PasswordInputScopeState::kNonSensitive;
          }
          CoTaskMemFree(scopes);
        }
      }
    }
    VariantClear(&value);
  }

  selection.range->Release();
  _SetPasswordInputScopeState(state);
}

void WeaselTSF::_SetPasswordInputScopeState(PasswordInputScopeState state) {
  if (_password_input_scope_state == state) {
    return;
  }

  const bool was_protected = _ShouldBypassForPasswordInput();
  const bool will_be_protected =
      _password_input_protection &&
      state != PasswordInputScopeState::kNonSensitive;
  if (was_protected != will_be_protected) {
    _fTestKeyDownPending = FALSE;
    _fTestKeyUpPending = FALSE;
    _AbortComposition();
  }
  _password_input_scope_state = state;
}

void WeaselTSF::_SetPasswordInputProtection(bool enabled) {
  if (_password_input_protection == enabled) {
    return;
  }

  const bool was_protected = _ShouldBypassForPasswordInput();
  const bool will_be_protected =
      enabled && _password_input_scope_state !=
                     PasswordInputScopeState::kNonSensitive;
  if (was_protected != will_be_protected) {
    _fTestKeyDownPending = FALSE;
    _fTestKeyUpPending = FALSE;
    _AbortComposition();
  }
  _password_input_protection = enabled;
}

bool WeaselTSF::_ShouldBypassForPasswordInput() const {
  return _password_input_protection &&
         _password_input_scope_state !=
             PasswordInputScopeState::kNonSensitive;
}
