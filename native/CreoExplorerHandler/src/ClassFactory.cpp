#include "ClassFactory.h"

#include <new>
#include <shlwapi.h>

#include "CreoPropertyStore.h"
#include "DllCommon.h"

ClassFactory::ClassFactory() { DllAddRef(); }

ClassFactory::~ClassFactory() { DllRelease(); }

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
  static const QITAB qit[] = {
      QITABENT(ClassFactory, IClassFactory),
      {0},
  };
  return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release() {
  const long count = InterlockedDecrement(&ref_count_);
  if (count == 0) {
    delete this;
  }
  return count;
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                            void** ppv) {
  *ppv = nullptr;
  if (pUnkOuter != nullptr) {
    return CLASS_E_NOAGGREGATION;
  }
  CreoPropertyStore* store = new (std::nothrow) CreoPropertyStore();
  if (store == nullptr) {
    return E_OUTOFMEMORY;
  }
  const HRESULT hr = store->QueryInterface(riid, ppv);
  store->Release();
  return hr;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL fLock) {
  if (fLock) {
    DllAddRef();
  } else {
    DllRelease();
  }
  return S_OK;
}
