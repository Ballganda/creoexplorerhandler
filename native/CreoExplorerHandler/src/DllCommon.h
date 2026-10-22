#pragma once

#include <windows.h>
#include <unknwn.h>

// {A7C3E9F1-4B2D-4E6A-9C81-2F0D5A7B13E4}
static const CLSID CLSID_CreoExplorerHandler = {
    0xa7c3e9f1,
    0x4b2d,
    0x4e6a,
    {0x9c, 0x81, 0x2f, 0x0d, 0x5a, 0x7b, 0x13, 0xe4}};

void DllAddRef();
void DllRelease();
HINSTANCE DllInstance();
HRESULT RegisterHandler();
HRESULT UnregisterHandler();
