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

#ifndef ANDROID_USB_API_SDB_OBJECT_HANDLE_H__
#define ANDROID_USB_API_SDB_OBJECT_HANDLE_H__
/** \file
  This file consists of declaration of a class SdbObjectHandle that
  encapsulates an internal API object that is visible to the outside
  of the API through a handle.
*/

#include "sdb_api.h"
#include "sdb_api_private_defines.h"

/** \brief Defines types of internal API objects
*/
enum SdbObjectType {
  /// Object is SdbInterfaceEnumObject.
  SdbObjectTypeInterfaceEnumerator,

  /// Object is SdbInterfaceObject.
  SdbObjectTypeInterface,

  /// Object is SdbEndpointObject.
  SdbObjectTypeEndpoint,

  /// Object is SdbIOCompletion.
  SdbObjectTypeIoCompletion,

  SdbObjectTypeMax
};

/** \brief Encapsulates an internal API basic object that is visible to the
  outside of the API through a handle.
  
  In order to prevent crashes when API client tries to access an object through
  an invalid or already closed handle, we keep track of all opened handles in
  SdbObjectHandleMap that maps association between valid SDBAPIHANDLE and
  an object that this handle represents. All objects that are exposed to the
  outside of API via SDBAPIHANDLE are self-destructing referenced objects.
  The reference model for these objects is as such:
  1. When CreateHandle() method is called on an object, a handle (SDBAPIHANDLE
     that is) is assigned to it, a pair <handle, object> is added to the global
     SdbObjectHandleMap instance, object is referenced and then handle is
     returned to the API client.
  2. Every time API is called with a handle, a lookup is performed in 
     SdbObjectHandleMap to find an object that is associated with the handle.
     If object is not found then ERROR_INVALID_HANDLE is immediatelly returned
     (via SetLastError() call). If object is found then it is referenced and
     API call is dispatched to appropriate method of the found object. Upon
     return from this method, just before returning from the API call, object
     is dereferenced back to match lookup reference.
  3. When object handle gets closed, assuming object is found in the map, that
     <handle, object> pair is deleted from the map and object's refcount is
     decremented to match refcount increment performed when object has been
     added to the map.
  4. When object's refcount drops to zero, the object commits suicide by
     calling "delete this".
  All API objects that have handles that are sent back to API client must be
  derived from this class.
*/
class SDBWIN_API_CLASS SdbObjectHandle {
 public:
  /** \brief Constructs the object

    Refernce counter is set to 1 in the constructor.
    @param[in] obj_type Object type from SdbObjectType enum
  */
  explicit SdbObjectHandle(SdbObjectType obj_type);

 protected:
  /** \brief Destructs the object.

   We hide destructor in order to prevent ourseves from accidentaly allocating
   instances on the stack. If such attempt occurs, compiler will error.
  */
  virtual ~SdbObjectHandle();

 public:
  /** \brief References the object.

    @return Value of the reference counter after object is referenced in this
            method.
  */
  virtual LONG AddRef();

  /** \brief Releases the object.

    If refcount drops to zero as the result of this release, the object is
    destroyed in this method. As a general rule, objects must not be touched
    after this method returns even if returned value is not zero.
    @return Value of the reference counter after object is released in this
            method.
  */
  virtual LONG Release();

  /** \brief Creates handle to this object.

    In this call a handle for this object is generated and object is added
    to the SdbObjectHandleMap.
    @return A handle to this object on success or NULL on an error.
            If NULL is returned GetLastError() provides extended error
            information. ERROR_GEN_FAILURE is set if an attempt was
            made to create already opened object.
  */
  virtual SDBAPIHANDLE CreateHandle();

  /** \brief This method is called when handle to this object gets closed.

    In this call object is deleted from the SdbObjectHandleMap.
    @return true on success or false if object is already closed. If
            false is returned GetLastError() provides extended error
            information.
  */
  virtual bool CloseHandle();

  /** \brief Checks if this object is of the given type.

    @param[in] obj_type One of the SdbObjectType types to check
    @return true is this object type matches obj_type, or false otherwise.
  */
  virtual bool IsObjectOfType(SdbObjectType obj_type) const;

  /** \brief Looks up SdbObjectHandle instance associated with the given handle
    in the SdbObjectHandleMap.

    This method increments reference counter for the returned found object.
    @param[in] sdb_handle SDB handle to the object
    @return API object associated with the handle or NULL if object is not
            found. If NULL is returned GetLastError() provides extended error
            information.
  */
  static SdbObjectHandle* Lookup(SDBAPIHANDLE sdb_handle);

 protected:
  /** \brief Called when last reference to this object is released.

    Derived object should override this method to perform cleanup that is not
    suitable for destructors.
  */
  virtual void LastReferenceReleased();

 public:
  /// Gets SDB handle associated with this object
  SDBAPIHANDLE sdb_handle() const {
    return sdb_handle_;
  }

  /// Gets type of this object
  SdbObjectType object_type() const {
    return object_type_;
  }

  /// Checks if object is still opened. Note that it is not guaranteed that
  /// object remains opened when this method returns.
  bool IsOpened() const {
    return (NULL != sdb_handle());
  }

 protected:
  /// API handle associated with this object
  SDBAPIHANDLE  sdb_handle_;

  /// Type of this object
  SdbObjectType object_type_;

  /// This object's reference counter
  LONG          ref_count_;
};

/// Maps SDBAPIHANDLE to associated SdbObjectHandle object
typedef std::map< SDBAPIHANDLE, SdbObjectHandle* > SdbObjectHandleMap;

/** \brief Template routine that unifies extracting of objects of different
  types from the SdbObjectHandleMap

  @param[in] sdb_handle API handle for the object
  @return Object associated with the handle or NULL on error. If NULL is
          returned GetLastError() provides extended error information.
*/
template<class obj_class>
obj_class* LookupObject(SDBAPIHANDLE sdb_handle) {
  // Lookup object for the handle in the map
  SdbObjectHandle* sdb_object = SdbObjectHandle::Lookup(sdb_handle);
  if (NULL != sdb_object) {
    // Make sure it's of the correct type
    if (!sdb_object->IsObjectOfType(obj_class::Type())) {
      sdb_object->Release();
      sdb_object = NULL;
      SetLastError(ERROR_INVALID_HANDLE);
    }
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
  }
  return (sdb_object != NULL) ? reinterpret_cast<obj_class*>(sdb_object) :
                                NULL;
}

#endif  // ANDROID_USB_API_SDB_OBJECT_HANDLE_H__
