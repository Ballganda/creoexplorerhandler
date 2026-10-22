#pragma once

#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <shlwapi.h>
#include <unknwn.h>

#include <cstdint>
#include <string>
#include <vector>

#include "creo_param_extractor.hpp"

class CreoPropertyStore final : public IInitializeWithStream,
                                public IInitializeWithFile,
                                public IPropertyStore,
                                public IPropertyStoreCapabilities {
 public:
  CreoPropertyStore();

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;

  // IInitializeWithStream
  IFACEMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;

  // IInitializeWithFile
  IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

  // IPropertyStore
  IFACEMETHODIMP GetCount(DWORD* cProps) override;
  IFACEMETHODIMP GetAt(DWORD iProp, PROPERTYKEY* pkey) override;
  IFACEMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) override;
  IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
  IFACEMETHODIMP Commit() override;

  // IPropertyStoreCapabilities
  IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

 private:
  ~CreoPropertyStore();

  HRESULT InitializeFromBytes(const std::uint8_t* data, std::size_t size,
                              const std::wstring& path_hint);
  HRESULT CacheString(REFPROPERTYKEY key, const std::string& utf8);
  HRESULT CacheStringVector(REFPROPERTYKEY key, const std::string& utf8);
  HRESULT CacheAuthor(const std::string& utf8);

  long ref_count_ = 1;
  bool initialized_ = false;
  IPropertyStoreCache* cache_ = nullptr;
};
