/*
 * Copyright (C) 2006 The Android Open Source Project
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

#ifndef ANDROID_USB_API_SDBWINUSBAPI_H__
#define ANDROID_USB_API_SDBWINUSBAPI_H__

/** \file
  Contains declarations required to link SdbWinApi and SdbWinUsbApi DLLs.
*/

/** \brief Function prototype for InstantiateWinUsbInterface routine exported
  from SdbWinUsbApi.dll

  In order to provide backward compatibility with the systems that still run
  legacy (custom) USB drivers, and have not installed WINUSB.DLL we need to
  split functionality of our SDB API on Windows between two DLLs: SdbWinApi,
  and SdbWinUsbApi. SdbWinApi is fully capable of working on top of the legacy
  driver, but has no traces to WinUsb. SdbWinUsbApi is capable of working on
  top of WinUsb API. We are forced to do this split, because we can have
  dependency on WINUSB.DLL in the DLL that implements legacy API. The problem
  is that customers may have a legacy driver that they don't want to upgrade
  to WinUsb, so they may not have WINUSB.DLL installed on their machines, but
  they still must be able to use SDB. So, the idea behind the split is as
  such. When SdbWinApi.dll is loaded into a process, it will check WINUSB.DLL
  installation (by checking existance of C:\Windows\System32\winusb.dll). If
  WINUSB.DLL is installed, SdbWinApi will also load SdbWinUsbApi.dll (by
  calling LoadLibrary), and will extract address of InstantiateWinUsbInterface
  routine exported from SdbWinUsbApi.dll. Then this routine will be used to
  instantiate SdbInterfaceObject instance on condition that it is confirmed
  that USB driver underneath us is in deed WinUsb.
*/
typedef class SdbInterfaceObject* \
    (__cdecl *PFN_INSTSDBWINUSBINTERFACE)(const wchar_t*);

#endif  // ANDROID_USB_API_SDBWINUSBAPI_H__
