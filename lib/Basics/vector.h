////////////////////////////////////////////////////////////////////////////////
/// @brief vector implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_VECTOR_H
#define ARANGODB_BASICS_VECTOR_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       POD VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pod vector
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vector_s {
  char*                 _buffer;
  TRI_memory_zone_id_t  _memoryZoneX;  // private. do not access from outside
  uint32_t              _lengthX;      // private. do not access from outside!
  uint32_t              _capacityX;    // private. do not access from outside!
  uint32_t              _elementSizeX; // private. do not access from outside!
}
TRI_vector_t;

static_assert(sizeof(TRI_memory_zone_id_t) == 4, "invalid size for TRI_memory_zone_id_t");

static_assert(sizeof(TRI_vector_t) == sizeof(char*) + 4 * sizeof(uint32_t), "invalid size for TRI_vector_t");

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVector (TRI_vector_t*, TRI_memory_zone_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVector2 (TRI_vector_t*,
                     TRI_memory_zone_t*,
                     size_t,
                     size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVector (TRI_vector_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVector (TRI_memory_zone_t*, TRI_vector_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns length of vector
////////////////////////////////////////////////////////////////////////////////

static inline size_t TRI_LengthVector (TRI_vector_t const* vector) {
  return static_cast<size_t>(vector->_lengthX);
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief returns capacity of vector
////////////////////////////////////////////////////////////////////////////////

static inline size_t TRI_CapacityVector(TRI_vector_t const* vector) {
  return static_cast<size_t>(vector->_capacityX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a vector has space for extraCapacity more items
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveVector (TRI_vector_t*,
                       size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief adjusts the length of the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLengthVector (TRI_vector_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the vector
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearVector (TRI_vector_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the vector
////////////////////////////////////////////////////////////////////////////////

int TRI_ResizeVector (TRI_vector_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds and element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVector (TRI_vector_t*, void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVector (TRI_vector_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an element to the vector after borrowing it via 
/// TRI_NextVector. This will also decrease the vector length by one. 
/// The caller must ensure that the element has been fetched from the vector
/// before.
////////////////////////////////////////////////////////////////////////////////

void TRI_ReturnVector (TRI_vector_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief increases vector length by one and returns the address of the
/// (uninitialized) element at the new position
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextVector (TRI_vector_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position, no bounds checking
////////////////////////////////////////////////////////////////////////////////

static inline void* TRI_AddressVector (TRI_vector_t const* vector, size_t pos) {
  return static_cast<void*>(vector->_buffer + pos * static_cast<size_t>(vector->_elementSizeX));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVector (TRI_vector_t const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts an element at a given position
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVector (TRI_vector_t*, void const* , size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an element at a given position
////////////////////////////////////////////////////////////////////////////////

void TRI_SetVector (TRI_vector_t*, size_t, void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the beginning
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginVector (TRI_vector_t const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                   POINTER VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer vector
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vector_pointer_s {
  TRI_memory_zone_t* _memoryZone;
  void** _buffer;
  size_t _length;
  size_t _capacity;
}
TRI_vector_pointer_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorPointer (TRI_vector_pointer_t*, TRI_memory_zone_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorPointer (TRI_vector_pointer_t*,
                           TRI_memory_zone_t*,
                           size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorPointer (TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorPointer (TRI_memory_zone_t*, TRI_vector_pointer_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a vector has space for more items
////////////////////////////////////////////////////////////////////////////////

int TRI_ReserveVectorPointer (TRI_vector_pointer_t*,
                              size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a vector
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_CopyVectorPointer (TRI_memory_zone_t*,
                                             TRI_vector_pointer_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorPointer (TRI_vector_pointer_t*, void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at position n
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVectorPointer (TRI_vector_pointer_t*, void*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, returns this element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveVectorPointer (TRI_vector_pointer_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

void* TRI_AtVectorPointer (TRI_vector_pointer_t const*, size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    STRING VECTORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief string vector
///
/// Destroying a string vector will also free all strings stored inside the
/// vector.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_vector_string_s {
  TRI_memory_zone_t* _memoryZone;
  char** _buffer;
  size_t _length;
  size_t _capacity;
}
TRI_vector_string_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string vector
////////////////////////////////////////////////////////////////////////////////

void TRI_InitVectorString (TRI_vector_string_t*, TRI_memory_zone_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a string vector, with user-definable settings
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVectorString2 (TRI_vector_string_t*,
                           TRI_memory_zone_t*,
                           size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and all strings, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVectorString (TRI_vector_string_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a vector and frees the string
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVectorString (TRI_memory_zone_t*, TRI_vector_string_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element at the end
///
/// Note that the vector claims owenship of element.
////////////////////////////////////////////////////////////////////////////////

int TRI_PushBackVectorString (TRI_vector_string_t*, char* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element, frees this element
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveVectorString (TRI_vector_string_t*, size_t n);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the element at a given position
////////////////////////////////////////////////////////////////////////////////

char* TRI_AtVectorString (TRI_vector_string_t const*, size_t);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
