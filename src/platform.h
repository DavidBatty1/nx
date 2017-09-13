//----------------------------------------------------------------------------------------------------------------------
// Platform callbacks
//----------------------------------------------------------------------------------------------------------------------

#pragma once

//----------------------------------------------------------------------------------------------------------------------
// Callback types
//----------------------------------------------------------------------------------------------------------------------

// Manages all memory operations.  The table below demonstrates how to use this:
//
//  Operation   oldAddress      newSize
//  ----------+---------------+------------------
//  ALLOC     | 0             | allocation size
//  REALLOC   | address       | new size
//  FREE      | address       | 0
//
typedef void* (*NxMemoryOperationFunc) (void* oldAddress, i64 newSize);

//----------------------------------------------------------------------------------------------------------------------
// Platform structure
// This is passed around via all the systems.
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    int                     argc;
    char**                  argv;
    NxMemoryOperationFunc   memFunc;        // Callback to handle all operations
}
Platform;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
