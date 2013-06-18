/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// SdbWinApi.cpp : Implementation of DLL Exports.

#include "stdafx.h"
#include "sdb_api.h"
#include "sdb_winusb_api.h"

extern "C" {
int _forceCRTManifest;
int _forceMFCManifest;
int _forceAtlDllManifest;
};

/// References InstantiateWinUsbInterface declared in sdb_api.cpp
extern PFN_INSTSDBWINUSBINTERFACE InstantiateSdbWinUsbInterface;

class CSdbWinApiModule : public CAtlDllModuleT< CSdbWinApiModule > {
 public:
  CSdbWinApiModule()
      : CAtlDllModuleT< CSdbWinApiModule >(),
        sdbwinusbapi_handle_(NULL),
        is_initialized_(false) {
  }

  ~CSdbWinApiModule() {
    // Unload SdbWinUsbApi.dll before we exit
    if (NULL != sdbwinusbapi_handle_) {
      FreeLibrary(sdbwinusbapi_handle_);
    }
  }

  /** \brief Loads SdbWinUsbApi.dll and caches its InstantiateWinUsbInterface
    export.

    This method is called from DllMain on DLL_PROCESS_ATTACH event. In this
    method we will check if WINUSB.DLL required by SdbWinUsbApi.dll is
    installed, and if it is we will load SdbWinUsbApi.dll and cache address of
    InstantiateWinUsbInterface routine exported from SdbWinUsbApi.dll
  */
  void AttachToSdbWinUsbApi() {
    // We only need to run this only once.
    if (is_initialized_) {
      return;
    }

    // Just mark that we have ran initialization.
    is_initialized_ = true;

    // Before we can load SdbWinUsbApi.dll we must make sure that WINUSB.DLL
    // has been installed. Build path to the file.
    wchar_t path_to_winusb_dll[MAX_PATH+1];
    if (!GetSystemDirectory(path_to_winusb_dll, MAX_PATH)) {
      return;
    }
    wcscat(path_to_winusb_dll, L"\\WINUSB.DLL");

    if (0xFFFFFFFF == GetFileAttributes(path_to_winusb_dll)) {
      // WINUSB.DLL is not installed. We don't (in fact, can't) load
      // SdbWinUsbApi.dll
      return;
    }

    // WINUSB.DLL is installed. Lets load SdbWinUsbApi.dll and cache its
    // InstantiateWinUsbInterface export.
    // We require that SdbWinUsbApi.dll is located in the same folder
    // where SdbWinApi.dll and sdb.exe are located, so by Windows
    // conventions we can pass just module name, and not the full path.
    sdbwinusbapi_handle_ = LoadLibrary(L"SdbWinUsbApi.dll");
    if (NULL != sdbwinusbapi_handle_) {
      InstantiateSdbWinUsbInterface = reinterpret_cast<PFN_INSTSDBWINUSBINTERFACE>
          (GetProcAddress(sdbwinusbapi_handle_, "InstantiateSdbWinUsbInterface"));
    }
  }

 protected:
  /// Handle to the loaded SdbWinUsbApi.dll
  HINSTANCE sdbwinusbapi_handle_;

  /// Flags whether or not this module has been initialized.
  bool      is_initialized_;
};

CSdbWinApiModule _AtlModule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  // Lets see if we need to initialize InstantiateWinUsbInterface
  // variable. We do that only once, on condition that this DLL is
  // being attached to the process and InstantiateWinUsbInterface
  // address has not been calculated yet.
  if (DLL_PROCESS_ATTACH == reason) {
    _AtlModule.AttachToSdbWinUsbApi();
  }
  return _AtlModule.DllMain(reason, reserved);
}
