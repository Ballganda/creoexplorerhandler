#pragma once

#include <windows.h>
#include <unknwn.h>

class ClassFactory final : public IClassFactory {
 public:
  ClassFactory();

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;

  // IClassFactory
  IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                void** ppv) override;
  IFACEMETHODIMP LockServer(BOOL fLock) override;

 private:
  ~ClassFactory();
  long ref_count_ = 1;
};
