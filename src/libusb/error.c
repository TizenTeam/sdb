/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * USB Error messages
 *
 * Copyright (c) 2000-2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 */

#include <errno.h>
#include <string.h>

#include "usb.h"
#include "error.h"

char usb_error_str[1024] = "";
int usb_error_errno = 0;
usb_error_type_t usb_error_type = USB_ERROR_TYPE_NONE;

char *usb_strerror(void)
{
  switch (usb_error_type) {
  case USB_ERROR_TYPE_NONE:
    return "No error";
  case USB_ERROR_TYPE_STRING:
    return usb_error_str;
  case USB_ERROR_TYPE_ERRNO:
    if (usb_error_errno > -USB_ERROR_BEGIN)
      return strerror(usb_error_errno);
    else
      /* Any error we don't know falls under here */
      return "Unknown error";
  }

  return "Unknown error";
}

