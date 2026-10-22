#include "CreoPropertyStore.h"

#include <propvarutil.h>
#include <shlwapi.h>

#include <cctype>
#include <cstring>
#include <string_view>
#include <vector>

#include "DllCommon.h"
#include "MappingConfig.h"

namespace {

std::string WideToUtf8(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                        static_cast<int>(wide.size()), nullptr,
                                        0, nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string out(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      out.data(), bytes, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                        static_cast<int>(utf8.size()), nullptr, 0);
  if (chars <= 0) {
    return {};
  }
  std::wstring out(static_cast<std::size_t>(chars), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      out.data(), chars);
  return out;
}

mapping::Config LoadMappingConfig() {
  wchar_t module_path[MAX_PATH] = {};
  const DWORD got =
      GetModuleFileNameW(DllInstance(), module_path, ARRAYSIZE(module_path));
  if (got == 0 || got >= ARRAYSIZE(module_path)) {
    return mapping::default_config();
  }
  return mapping::load_config_beside_module(WideToUtf8(module_path));
}

// Keys that some Windows builds omit from PSGetPropertyKeyFromName but that
// Explorer still uses for Details columns (VERSIONINFO / Software group).
bool HardcodedPropertyKey(const std::string& canonical, PROPERTYKEY* key) {
  // Case-insensitive compare of ASCII canonical names.
  auto eq = [](const std::string& a, const char* b) {
    const std::size_t blen = std::strlen(b);
    if (a.size() != blen) {
      return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
      const auto ca =
          static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
      const auto cb =
          static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
      if (ca != cb) {
        return false;
      }
    }
    return true;
  };

  // PSFMTID_VERSION {0CEF7D53-FA64-11D1-A203-0000F81FEDEE}
  PROPERTYKEY pk = {};
  pk.fmtid.Data1 = 0x0cef7d53;
  pk.fmtid.Data2 = 0xfa64;
  pk.fmtid.Data3 = 0x11d1;
  pk.fmtid.Data4[0] = 0xa2;
  pk.fmtid.Data4[1] = 0x03;
  pk.fmtid.Data4[2] = 0x00;
  pk.fmtid.Data4[3] = 0x00;
  pk.fmtid.Data4[4] = 0xf8;
  pk.fmtid.Data4[5] = 0x1f;
  pk.fmtid.Data4[6] = 0xed;
  pk.fmtid.Data4[7] = 0xee;

  if (eq(canonical, "System.Software.ProductVersion") ||
      eq(canonical, "System.ProductVersion")) {
    // ExtendedProperty("System.Software.ProductVersion") / column "Product version"
    pk.pid = 8;
    *key = pk;
    return true;
  }
  if (eq(canonical, "System.FileVersion")) {
    pk.pid = 4;
    *key = pk;
    return true;
  }
  if (eq(canonical, "System.Software.ProductName")) {
    pk.pid = 7;
    *key = pk;
    return true;
  }
  if (eq(canonical, "System.FileDescription")) {
    pk.pid = 3;
    *key = pk;
    return true;
  }
  return false;
}

HRESULT ResolvePropertyKey(const std::string& canonical, PROPERTYKEY* key) {
  if (key == nullptr || canonical.empty()) {
    return E_INVALIDARG;
  }
  const std::wstring wide = Utf8ToWide(canonical);
  const HRESULT hr = PSGetPropertyKeyFromName(wide.c_str(), key);
  if (SUCCEEDED(hr)) {
    return hr;
  }
  if (HardcodedPropertyKey(canonical, key)) {
    return S_OK;
  }
  return hr;
}

}  // namespace

CreoPropertyStore::CreoPropertyStore() { DllAddRef(); }

CreoPropertyStore::~CreoPropertyStore() {
  if (cache_ != nullptr) {
    cache_->Release();
    cache_ = nullptr;
  }
  DllRelease();
}

IFACEMETHODIMP CreoPropertyStore::QueryInterface(REFIID riid, void** ppv) {
  static const QITAB qit[] = {
      QITABENT(CreoPropertyStore, IInitializeWithStream),
      QITABENT(CreoPropertyStore, IInitializeWithFile),
      QITABENT(CreoPropertyStore, IPropertyStore),
      QITABENT(CreoPropertyStore, IPropertyStoreCapabilities),
      {0},
  };
  return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CreoPropertyStore::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) CreoPropertyStore::Release() {
  const long count = InterlockedDecrement(&ref_count_);
  if (count == 0) {
    delete this;
  }
  return count;
}

HRESULT CreoPropertyStore::CacheString(REFPROPERTYKEY key,
                                       const std::string& utf8) {
  if (cache_ == nullptr || utf8.empty()) {
    return S_OK;
  }
  const std::wstring wide = Utf8ToWide(utf8);
  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = InitPropVariantFromString(wide.c_str(), &var);
  if (SUCCEEDED(hr)) {
    hr = cache_->SetValueAndState(key, &var, PSC_NORMAL);
    PropVariantClear(&var);
  }
  return hr;
}

HRESULT CreoPropertyStore::CacheAuthor(const std::string& utf8) {
  if (cache_ == nullptr || utf8.empty()) {
    return S_OK;
  }
  const std::wstring wide = Utf8ToWide(utf8);
  PCWSTR values[] = {wide.c_str()};
  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = InitPropVariantFromStringVector(values, 1, &var);
  if (SUCCEEDED(hr)) {
    hr = cache_->SetValueAndState(PKEY_Author, &var, PSC_NORMAL);
    PropVariantClear(&var);
  }
  return hr;
}

HRESULT CreoPropertyStore::InitializeFromBytes(const std::uint8_t* data,
                                               std::size_t size,
                                               const std::wstring& path_hint) {
  if (initialized_) {
    return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
  }

  if (!path_hint.empty()) {
    const std::string utf8_path = WideToUtf8(path_hint);
    if (!creo::is_creo_model_filename(utf8_path)) {
      // Not a Creo model name (e.g. unrelated .2 file) — empty store.
      HRESULT hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
      if (FAILED(hr)) {
        return hr;
      }
      initialized_ = true;
      return S_OK;
    }
  }

  HRESULT hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
  if (FAILED(hr)) {
    return hr;
  }

  if (creo::is_creo_ugc_file(data, size)) {
    const mapping::Config cfg = LoadMappingConfig();
    for (const mapping::Entry& entry : cfg.entries) {
      const auto canonical = mapping::resolve_canonical_name(entry.explorer_name);
      if (!canonical) {
        continue;
      }
      const auto value =
          creo::extract_string_param(data, size, entry.creo_param);
      if (!value || value->empty()) {
        continue;
      }
      PROPERTYKEY key = {};
      if (FAILED(ResolvePropertyKey(*canonical, &key))) {
        continue;
      }
      if (mapping::is_author_property(*canonical)) {
        CacheAuthor(*value);
      } else {
        CacheString(key, *value);
      }
    }
  }

  initialized_ = true;
  return S_OK;
}

IFACEMETHODIMP CreoPropertyStore::Initialize(IStream* pstream, DWORD grfMode) {
  UNREFERENCED_PARAMETER(grfMode);
  if (pstream == nullptr) {
    return E_INVALIDARG;
  }
  if (initialized_) {
    return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
  }

  STATSTG stat = {};
  HRESULT hr = pstream->Stat(&stat, STATFLAG_NONAME);
  if (FAILED(hr)) {
    return hr;
  }
  if (stat.cbSize.QuadPart == 0 || stat.cbSize.QuadPart > 256ull * 1024ull * 1024ull) {
    // Empty or absurdly large — still initialize an empty cache.
    hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
    if (SUCCEEDED(hr)) {
      initialized_ = true;
    }
    return hr;
  }

  const auto size = static_cast<std::size_t>(stat.cbSize.QuadPart);
  std::vector<std::uint8_t> buffer(size);
  ULONG read = 0;
  hr = pstream->Read(buffer.data(), static_cast<ULONG>(size), &read);
  if (FAILED(hr)) {
    return hr;
  }
  // Stream init has no path; skip filename gate and rely on UGC magic.
  return InitializeFromBytes(buffer.data(), read, L"");
}

IFACEMETHODIMP CreoPropertyStore::Initialize(LPCWSTR pszFilePath,
                                             DWORD grfMode) {
  UNREFERENCED_PARAMETER(grfMode);
  if (pszFilePath == nullptr) {
    return E_INVALIDARG;
  }
  if (initialized_) {
    return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
  }

  const std::wstring path(pszFilePath);
  if (!creo::is_creo_model_filename(WideToUtf8(path))) {
    HRESULT hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
    if (SUCCEEDED(hr)) {
      initialized_ = true;
    }
    return hr;
  }

  HANDLE file =
      CreateFileW(pszFilePath, GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  LARGE_INTEGER file_size = {};
  if (!GetFileSizeEx(file, &file_size)) {
    const DWORD err = GetLastError();
    CloseHandle(file);
    return HRESULT_FROM_WIN32(err);
  }
  if (file_size.QuadPart <= 0 || file_size.QuadPart > 256ll * 1024ll * 1024ll) {
    CloseHandle(file);
    HRESULT hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
    if (SUCCEEDED(hr)) {
      initialized_ = true;
    }
    return hr;
  }

  const auto size = static_cast<std::size_t>(file_size.QuadPart);
  std::vector<std::uint8_t> buffer(size);
  DWORD read = 0;
  const BOOL ok =
      ReadFile(file, buffer.data(), static_cast<DWORD>(size), &read, nullptr);
  const DWORD err = GetLastError();
  CloseHandle(file);
  if (!ok) {
    return HRESULT_FROM_WIN32(err);
  }
  return InitializeFromBytes(buffer.data(), read, path);
}

IFACEMETHODIMP CreoPropertyStore::GetCount(DWORD* cProps) {
  if (cProps == nullptr) {
    return E_POINTER;
  }
  if (cache_ == nullptr) {
    *cProps = 0;
    return S_OK;
  }
  return cache_->GetCount(cProps);
}

IFACEMETHODIMP CreoPropertyStore::GetAt(DWORD iProp, PROPERTYKEY* pkey) {
  if (pkey == nullptr) {
    return E_POINTER;
  }
  if (cache_ == nullptr) {
    return E_INVALIDARG;
  }
  return cache_->GetAt(iProp, pkey);
}

IFACEMETHODIMP CreoPropertyStore::GetValue(REFPROPERTYKEY key, PROPVARIANT* pv) {
  if (pv == nullptr) {
    return E_POINTER;
  }
  PropVariantInit(pv);
  if (cache_ == nullptr) {
    return S_OK;
  }
  return cache_->GetValue(key, pv);
}

IFACEMETHODIMP CreoPropertyStore::SetValue(REFPROPERTYKEY /*key*/,
                                          REFPROPVARIANT /*propvar*/) {
  return STG_E_ACCESSDENIED;
}

IFACEMETHODIMP CreoPropertyStore::Commit() { return STG_E_ACCESSDENIED; }

IFACEMETHODIMP CreoPropertyStore::IsPropertyWritable(REFPROPERTYKEY /*key*/) {
  return S_FALSE;
}
