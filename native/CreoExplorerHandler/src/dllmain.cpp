#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <new>
#include <string>
#include <string_view>

#include "ClassFactory.h"
#include "DllCommon.h"
#include "MappingConfig.h"

namespace {

long g_dll_ref = 0;
HINSTANCE g_module = nullptr;

constexpr wchar_t kHandlerName[] = L"Creo Explorer Property Handler";

HRESULT GetModulePath(wchar_t* path, DWORD cch) {
  const DWORD got = GetModuleFileNameW(g_module, path, cch);
  if (got == 0 || got >= cch) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return S_OK;
}

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

mapping::Config LoadMappingConfig() {
  wchar_t module_path[MAX_PATH] = {};
  if (FAILED(GetModulePath(module_path, ARRAYSIZE(module_path)))) {
    return mapping::default_config();
  }
  return mapping::load_config_beside_module(WideToUtf8(module_path));
}

HRESULT SetRegistryKeyValue(HKEY root, const wchar_t* subkey,
                            const wchar_t* value_name, const wchar_t* value) {
  HKEY key = nullptr;
  LONG status =
      RegCreateKeyExW(root, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }
  status = RegSetValueExW(key, value_name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value),
                          static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return HRESULT_FROM_WIN32(status);
}

HRESULT SetRegistryDword(HKEY root, const wchar_t* subkey,
                         const wchar_t* value_name, DWORD value) {
  HKEY key = nullptr;
  LONG status =
      RegCreateKeyExW(root, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }
  status = RegSetValueExW(key, value_name, 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(key);
  return HRESULT_FROM_WIN32(status);
}

HRESULT DeleteRegistryTree(HKEY root, const wchar_t* subkey) {
  const LONG status = RegDeleteTreeW(root, subkey);
  if (status == ERROR_FILE_NOT_FOUND) {
    return S_OK;
  }
  return HRESULT_FROM_WIN32(status);
}

HRESULT RegisterPropertyHandlerExtension(const wchar_t* extension) {
  wchar_t clsid[64] = {};
  StringFromGUID2(CLSID_CreoExplorerHandler, clsid, ARRAYSIZE(clsid));
  wchar_t key_path[256] = {};
  HRESULT hr = StringCchPrintfW(
      key_path, ARRAYSIZE(key_path),
      L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\"
      L"PropertyHandlers\\%s",
      extension);
  if (FAILED(hr)) {
    return hr;
  }
  return SetRegistryKeyValue(HKEY_LOCAL_MACHINE, key_path, nullptr, clsid);
}

HRESULT UnregisterPropertyHandlerExtension(const wchar_t* extension) {
  wchar_t key_path[256] = {};
  HRESULT hr = StringCchPrintfW(
      key_path, ARRAYSIZE(key_path),
      L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\"
      L"PropertyHandlers\\%s",
      extension);
  if (FAILED(hr)) {
    return hr;
  }
  return DeleteRegistryTree(HKEY_LOCAL_MACHINE, key_path);
}

HRESULT RegisterAssociationPropLists(const wchar_t* extension,
                                     const wchar_t* prop_list) {
  wchar_t assoc_key[256] = {};
  HRESULT hr = StringCchPrintfW(assoc_key, ARRAYSIZE(assoc_key),
                                L"SystemFileAssociations\\%s", extension);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, assoc_key, L"FullDetails",
                           prop_list);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, assoc_key, L"PreviewDetails",
                           prop_list);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, assoc_key, L"InfoTip", prop_list);
  if (FAILED(hr)) {
    return hr;
  }

  // If this extension already has a ProgID (e.g. Creo Versioned File), also
  // publish the property lists there so Explorer Details binds the columns.
  wchar_t ext_key[64] = {};
  hr = StringCchPrintfW(ext_key, ARRAYSIZE(ext_key), L"%s", extension);
  if (FAILED(hr)) {
    return hr;
  }
  HKEY ext = nullptr;
  LONG status =
      RegOpenKeyExW(HKEY_CLASSES_ROOT, ext_key, 0, KEY_READ | KEY_WOW64_64KEY, &ext);
  if (status != ERROR_SUCCESS) {
    return S_OK;
  }
  wchar_t prog_id[256] = {};
  DWORD prog_id_bytes = sizeof(prog_id);
  DWORD type = 0;
  status = RegQueryValueExW(ext, nullptr, nullptr, &type,
                            reinterpret_cast<LPBYTE>(prog_id), &prog_id_bytes);
  RegCloseKey(ext);
  if (status != ERROR_SUCCESS || type != REG_SZ || prog_id[0] == L'\0') {
    return S_OK;
  }

  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, prog_id, L"FullDetails",
                           prop_list);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, prog_id, L"PreviewDetails",
                           prop_list);
  if (FAILED(hr)) {
    return hr;
  }
  return SetRegistryKeyValue(HKEY_CLASSES_ROOT, prog_id, L"InfoTip", prop_list);
}

HRESULT UnregisterAssociationPropLists(const wchar_t* extension) {
  wchar_t assoc_key[256] = {};
  if (SUCCEEDED(StringCchPrintfW(assoc_key, ARRAYSIZE(assoc_key),
                                 L"SystemFileAssociations\\%s", extension))) {
    // Only remove the values we created; do not delete whole Creo ProgID trees.
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, assoc_key, 0,
                      KEY_SET_VALUE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
      RegDeleteValueW(key, L"FullDetails");
      RegDeleteValueW(key, L"PreviewDetails");
      RegDeleteValueW(key, L"InfoTip");
      RegCloseKey(key);
    }
  }
  return S_OK;
}

HRESULT RegisterOneExtension(const wchar_t* extension,
                             const wchar_t* prop_list) {
  HRESULT hr = RegisterPropertyHandlerExtension(extension);
  if (FAILED(hr)) {
    return hr;
  }
  return RegisterAssociationPropLists(extension, prop_list);
}

HRESULT UnregisterOneExtension(const wchar_t* extension) {
  UnregisterAssociationPropLists(extension);
  return UnregisterPropertyHandlerExtension(extension);
}

}  // namespace

void DllAddRef() { InterlockedIncrement(&g_dll_ref); }

void DllRelease() { InterlockedDecrement(&g_dll_ref); }

HINSTANCE DllInstance() { return g_module; }

HRESULT RegisterHandler() {
  wchar_t module_path[MAX_PATH] = {};
  HRESULT hr = GetModulePath(module_path, ARRAYSIZE(module_path));
  if (FAILED(hr)) {
    return hr;
  }

  const mapping::Config map_cfg = LoadMappingConfig();
  const std::wstring prop_list = mapping::build_property_list(map_cfg);

  wchar_t clsid[64] = {};
  StringFromGUID2(CLSID_CreoExplorerHandler, clsid, ARRAYSIZE(clsid));

  wchar_t clsid_key[128] = {};
  hr = StringCchPrintfW(clsid_key, ARRAYSIZE(clsid_key), L"CLSID\\%s", clsid);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, clsid_key, nullptr, kHandlerName);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryDword(HKEY_CLASSES_ROOT, clsid_key, L"ManualSafeSave", 1);
  if (FAILED(hr)) {
    return hr;
  }

  wchar_t inproc_key[160] = {};
  hr = StringCchPrintfW(inproc_key, ARRAYSIZE(inproc_key),
                        L"CLSID\\%s\\InProcServer32", clsid);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, inproc_key, nullptr, module_path);
  if (FAILED(hr)) {
    return hr;
  }
  hr = SetRegistryKeyValue(HKEY_CLASSES_ROOT, inproc_key, L"ThreadingModel",
                           L"Apartment");
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegisterOneExtension(L".prt", prop_list.c_str());
  if (FAILED(hr)) {
    return hr;
  }
  hr = RegisterOneExtension(L".asm", prop_list.c_str());
  if (FAILED(hr)) {
    return hr;
  }
  for (int i = 1; i <= 99; ++i) {
    wchar_t ext[8] = {};
    hr = StringCchPrintfW(ext, ARRAYSIZE(ext), L".%d", i);
    if (FAILED(hr)) {
      return hr;
    }
    hr = RegisterOneExtension(ext, prop_list.c_str());
    if (FAILED(hr)) {
      return hr;
    }
  }

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return S_OK;
}

HRESULT UnregisterHandler() {
  wchar_t clsid[64] = {};
  StringFromGUID2(CLSID_CreoExplorerHandler, clsid, ARRAYSIZE(clsid));
  wchar_t clsid_key[128] = {};
  HRESULT hr =
      StringCchPrintfW(clsid_key, ARRAYSIZE(clsid_key), L"CLSID\\%s", clsid);
  if (SUCCEEDED(hr)) {
    DeleteRegistryTree(HKEY_CLASSES_ROOT, clsid_key);
  }

  UnregisterOneExtension(L".prt");
  UnregisterOneExtension(L".asm");
  for (int i = 1; i <= 99; ++i) {
    wchar_t ext[8] = {};
    if (SUCCEEDED(StringCchPrintfW(ext, ARRAYSIZE(ext), L".%d", i))) {
      UnregisterOneExtension(ext);
    }
  }
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return S_OK;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      g_module = module;
      DisableThreadLibraryCalls(module);
      break;
    default:
      break;
  }
  return TRUE;
}

STDAPI DllCanUnloadNow() {
  return g_dll_ref > 0 ? S_FALSE : S_OK;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
  *ppv = nullptr;
  if (clsid != CLSID_CreoExplorerHandler) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  ClassFactory* factory = new (std::nothrow) ClassFactory();
  if (factory == nullptr) {
    return E_OUTOFMEMORY;
  }
  const HRESULT hr = factory->QueryInterface(riid, ppv);
  factory->Release();
  return hr;
}

STDAPI DllRegisterServer() { return RegisterHandler(); }

STDAPI DllUnregisterServer() { return UnregisterHandler(); }
