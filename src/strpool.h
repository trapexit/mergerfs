/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

strpool.h - v1.4 - Highly efficient string pool for C/C++.

Do this:
    #define STRPOOL_IMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef strpool_h
#define strpool_h

#ifndef STRPOOL_U32
    #define STRPOOL_U32 unsigned int
#endif
#ifndef STRPOOL_U64
    #define STRPOOL_U64 unsigned long long
#endif

typedef struct strpool_t strpool_t;

typedef struct strpool_config_t
    {
    void* memctx;
    int ignore_case;
    int counter_bits;
    int index_bits;
    int entry_capacity;
    int block_capacity;
    int block_size;
    int min_length;
    } strpool_config_t;

extern strpool_config_t const strpool_default_config;

void strpool_init( strpool_t* pool, strpool_config_t const* config );
void strpool_term( strpool_t* pool );

void strpool_defrag( strpool_t* pool );

STRPOOL_U64 strpool_inject( strpool_t* pool, char const* string, int length );
void strpool_discard( strpool_t* pool, STRPOOL_U64 handle );

int strpool_incref( strpool_t* pool, STRPOOL_U64 handle );
int strpool_decref( strpool_t* pool, STRPOOL_U64 handle );
int strpool_getref( strpool_t* pool, STRPOOL_U64 handle );

int strpool_isvalid( strpool_t const* pool, STRPOOL_U64 handle );

char const* strpool_cstr( strpool_t const* pool, STRPOOL_U64 handle );
int strpool_length( strpool_t const* pool, STRPOOL_U64 handle );

char* strpool_collate( strpool_t const* pool, int* count );
void strpool_free_collated( strpool_t const* pool, char* collated_ptr );

#endif /* strpool_h */


/**
strpool.h
=========

Highly efficient string pool for C/C++.


Example
-------

    #define  STRPOOL_IMPLEMENTATION
    #include "strpool.h"

    #include <stdio.h> // for printf
    #include <string.h> // for strlen

    int main( int argc, char** argv ) {

        strpool_config_t conf = strpool_default_config;
        //conf.ignore_case = true;

        strpool_t pool;
        strpool_init( &pool, &conf );

        STRPOOL_U64 str_a = strpool_inject( &pool, "This is a test string", (int) strlen( "This is a test string" ) );
        STRPOOL_U64 str_b = strpool_inject( &pool, "THIS IS A TEST STRING", (int) strlen( "THIS IS A TEST STRING" ) );
    
        printf( "%s\n", strpool_cstr( &pool, str_a ) );
        printf( "%s\n", strpool_cstr( &pool, str_b ) );
        printf( "%s\n", str_a == str_b ? "Strings are the same" : "Strings are different" );
    
        strpool_term( &pool );
        return 0;
    }


API Documentation
-----------------

strpool.h is a system for string interning, where each string is stored only once. It makes comparing strings very fast,
as it will just be uint64 comparison rather than looping over strings and comparing character by character. strpool.h is
also optimized for fast creation of strings, and with an efficient memory allocation scheme.

strpool.h is a single-header library, and does not need any .lib files or other binaries, or any build scripts. To use 
it, you just include strpool.h to get the API declarations. To get the definitions, you must include strpool.h from 
*one* single C or C++ file, and #define the symbol `STRPOOL_IMPLEMENTATION` before you do. 


### Customization

There are a few different things in strpool.h which are configurable by #defines. Most of the API use the `int` data 
type, for integer values where the exact size is not important. However, for some functions, it specifically makes use 
of 32 and 64 bit data types. These default to using `unsigned int` and `unsigned long long` by default, but can be
redefined by #defining STRPOOL_U32 and STRPOOL_U64 respectively, before including strpool.h. This is useful if you, for 
example, use the types from `<stdint.h>` in the rest of your program, and you want strpool.h to use compatible types. In 
this case, you would include strpool.h using the following code:

    #define STRPOOL_U32 uint32_t
    #define STRPOOL_U64 uint64_t
    #include "strpool.h"

Note that when customizing the data types, you need to use the same definition in every place where you include 
strpool.h, as they affect the declarations as well as the definitions.

The rest of the customizations only affect the implementation, so will only need to be defined in the file where you
have the #define STRPOOL_IMPLEMENTATION.

Note that if all customizations are utilized, strpool.h will include no external files whatsoever, which might be useful
if you need full control over what code is being built.


#### Custom memory allocators

To store strings and the internal structures (entry list, hashtable etc) strpool.h needs to dodynamic allocation by 
calling `malloc`. Programs might want to keep track of allocations done, or use custom defined pools to allocate memory 
from. strpool.h allows for specifying custom memory allocation functions for `malloc` and `free`.
This is done with the following code:

    #define STRPOOL_IMPLEMENTATION
    #define STRPOOL_MALLOC( ctx, size ) ( my_custom_malloc( ctx, size ) )
    #define STRPOOL_FREE( ctx, ptr ) ( my_custom_free( ctx, ptr ) )
    #include "strpool.h"

where `my_custom_malloc` and `my_custom_free` are your own memory allocation/deallocation functions. The `ctx` parameter
is an optional parameter of type `void*`. When `strpool_init` is called, you can set the `memctx` field of the `config`
parameter, to a pointer to anything you like, and which will be passed through as the `ctx` parameter to every 
`STRPOOL_MALLOC`/`STRPOOL_FREE` call. For example, if you are doing memory tracking, you can pass a pointer to your 
tracking data as `memctx`, and in your custom allocation/deallocation function, you can cast the `ctx` param back to the 
right type, and access the tracking data.

If no custom allocator is defined, strpool.h will default to `malloc` and `free` from the C runtime library.


#### Custom assert

strpool.h makes use of asserts to report usage errors and failed allocation errors. By default, it makes use of the C 
runtime library `assert` macro, which only executes in debug builds. However, it allows for substituting with your own
assert function or macro using the following code:

    #define STRPOOL_IMPLEMENTATION
    #define STRPOOL_ASSERT( condition ) ( my_custom_assert( condition ) )
    #include "strpool.h"

Note that if you only want the asserts to trigger in debug builds, you must add a check for this in your custom assert.


#### Custom C runtime function

The library makes use of four additional functions from the C runtime library, and for full flexibility, it allows you 
to substitute them for your own. Here's an example:

    #define STRPOOL_IMPLEMENTATION
    #define STRPOOL_MEMSET( ptr, val, cnt ) ( my_memset_func( ptr, val, cnt ) )
    #define STRPOOL_MEMCPY( dst, src, cnt ) ( my_memcpy_func( dst, src, cnt ) )
    #define STRPOOL_MEMCMP( pr1, pr2, cnt ) ( my_memcmp_func( pr1, pr2, cnt ) )
    #define STRPOOL_STRNICMP( s1, s2, len ) ( my_strnicmp_func( s1, s2, len ) )
    #include "strpool.h"

If no custom function is defined, strpool.h will default to the C runtime library equivalent.


strpool_init
------------

    void strpool_init( strpool_t* pool, strpool_config_t const* config )

Initializes an instance of the string pool. The `config` parameter allows for controlling the behavior of the pool
instance. It contains the following fields:

* memctx - pointer to user defined data which will be passed through to custom STRPOOL_MALLOC/STRPOOL_FREE calls. May 
    be NULL.
* ignore_case - set to 0 to make strings case sensitive, set to 1 to make strings case insensitive. Default is 0.
* counter_bits - how many bits of the string handle to use for keeping track of handle reuse and invalidation. Default
    is 32. See below for details about the handle bits.
* index_bits - how many bits of the string handle to use for referencing string instances. Default is 32. See below for 
    details about the handle bits.
* entry_capacity - number of string instance entries to pre-allocate space for when pool is initialized. Default 
    is space for 4096 string entries.
* block_capacity - number of string storage block entries to pre-allocate space for when pool is initialized. Default 
    is space for 32 entries of block information - though only a single block will be pre-allocated.
* block_size - size to allocate for each string storage block. A higher value might mean you often have more memory 
    allocated than is actually being used. A lower value means you will be making allocations more often. Default 
    is 256 kilobyte. 
* min_length - minimum space to allocate for each string. A higher value wastes more space, but makes it more likely 
    that recycled storage can be re-used by subsequent requests. Default is a string length of 23 characters.

The function `strpool_inject` returns a 64-bit handle. Using the settings `counter_bits`/`index_bits`, you can control
how many bits of the handle is in use, and how many are used for index vs counter. For example, setting `counter_bits`
to 8 and `index_bits` to 24, you will get a handle which only uses 32 bits in total, can store some 16 million different
strings (2^24 -1), with a reuse counter going up to 255 before wrapping around. In this scenario, the 64-bit handle 
returned from strpool_inject can be safely truncated and stored in a 32-bit variable. Any combination of counter vs 
index bits is possible, but the number of strings which can be stored will be limited by `index_bits`. 

The counter bits need a more detailed description. When a string is added via `strpool_inject`, you get a handle back, 
which is used to reference the string. You might store this handle in various data structures. When removing a string by
calling `strpool_discard`, the index part of that handle will be recycled, and re-used for future `strpool_inject`.
However, when a string is discarded, the counter part of the handle will be increased, so the system knows that any 
handles that are still being kept around in any data structure, does no longer point to a valid string (and thus will 
return a NULL string pointer when queried through `strpool_cstr`). If your use case involves creating a bunch of strings
and just keeping them around until the application terminates, it is fine to specify 0 for `counter_bits`, thereby
effectively disabling the handle validation all together. If you have a case where strings are being repeatedly created 
and removed, but strings are queried very frequently, then you can specify a low number for `counter_bits` (since you 
can invalidate any stored handles as soon as you find out it's been invalidated). If you have a case where handles might
sit inside data structures for a long period of time before you check their validity, you best specify a high value for
`counter_bits`. The default value is 32 bits for index and 32 bits for counter.


strpool_term
------------

    void strpool_term( strpool_t* pool )

Terminates a string pool instance, releasing all memory used by it. No further calls to the strpool API are valid until
the instance is reinitialized by another call to `strpool_init`.


strpool_defrag
--------------

    void strpool_defrag( strpool_t* pool )

As strings are added to and removed from the pool, the memory blocks holding the strings may get fragmented, making them
take up more allocated memory than is actually needed to store the set of strings, should they be packed tightly,
`strpool_defrag` consolidates all strings into a single memory block (which might well be larger than the block size 
specified when the pool was initialized), and recreates the internal hash table. Any additionally allocated memory 
blocks will be deallocated, making the memory used by the pool as little as it can be to fit the current set of strings.
All string handles remain valid after a call to `strpool_defrag`.


strpool_inject
--------------

    STRPOOL_U64 strpool_inject( strpool_t* pool, char const* string, int length )

Adds a string to the pool, and returns a handle for the added string. If the string doesn't exist, it will be stored in
an unused part of an already allocated storage block (or a new block will be allocated if there is no free space large
enough to hold the string), and inserted into the internal hash table. If the string already exists, a handle to the
existing string will be returned. If the `string` parameter is already pointing to a string stored in the pool, there 
are specific optimizations for avoiding to loop over it to calculate the hash for it, with the idea being that you 
should be able to use char* types when you're passing strings around, and use the string pool for storage, without too
much of a performance penalty. `string` does not have to be null terminated, but when it is retrieved from the string 
pool, it will be. If `string` is NULL or length is 0, a handle with a value of 0 will be returned, to signify empty
string.


strpool_discard
---------------

    void strpool_discard( strpool_t* pool, STRPOOL_U64 handle )

Removes a string from the pool. Any handles held for the string will be invalid after this. Memory used for storing the
string will be recycled and used for further `strpool_inject` calls. If `handle` is invalid, `strpool_discard` will do
nothing.


strpool_incref
--------------

    int strpool_incref( strpool_t* pool, STRPOOL_U64 handle )

`strpool.h` supports reference counting of strings. It is optional and not automatic, and does not automatically discard
strings when the reference count reaches 0. To use reference counting, make sure to call `strpool_incref` whenever you
add a string (after having called `strpool_inject`), and to call `strpool_decref` whenever you want to remove a string
(but only call `strpool_discard` if reference count have reached 0). It would be advisable to write wrapper functions
to ensure consistency in this, or if C++ is used, a wrapper class with constructors/destructor. `strpool_incref` returns
the reference count after increasing it. If `handle` is invalid, `strpool_incref` will do nothing, and return 0.


strpool_decref
--------------

    int strpool_decref( strpool_t* pool, STRPOOL_U64 handle )

Decreases the reference count of the specified string by 1, returning the reference count after decrementing. If the
reference count is less than 1, an assert will be triggered. If `handle` is invalid, `strpool_decref` will do nothing, 
and return 0.


strpool_getref
--------------

    int strpool_getref( strpool_t* pool, STRPOOL_U64 handle )

Returns the current reference count for the specified string. If `handle` is invalid, `strpool_getref` will do nothing, 
and return 0.


strpool_isvalid
---------------
    
    int strpool_isvalid( strpool_t const* pool, STRPOOL_U64 handle )

Returns 1 if the specified string handle is valid, and 0 if it is not. 


strpool_cstr
------------

    char const* strpool_cstr( strpool_t const* pool, STRPOOL_U64 handle )

Returns the zero-terminated C string for the specified string handle. The resulting string pointer is only valid as long
as no call is made to `strpool_init`, `strpool_term`, `strpool_defrag` or `strpool_discard`. It is therefor recommended
to never store the C string pointer, and always grab it fresh by another call to `strpool_cstr` when it is needed.
`strpool_cstr` is a very fast function to call - it does little more than an array lookup. If `handle` is invalid, 
`strpool_cstr` returns NULL. 


strpool_length
--------------

    int strpool_length( strpool_t const* pool, STRPOOL_U64 handle )

Returns the length, in characters, of the specified string. The resulting value is only valid as long as no call is made 
to `strpool_init`, `strpool_term`, `strpool_defrag` or `strpool_discard`. It is therefor recommended to never store the 
value, and always grab it fresh by another call to `strpool_length` when it is needed. `strpool_length` is a very fast 
function to call - it does little more than an array lookup. If `handle` is invalid, `strpool_length` returns 0.


strpool_collate
---------------

    char* strpool_collate( strpool_t const* pool, int* count )

Returns a list of all the strings currently stored in the string pool, and stores the number of strings in the int
variable pointed to by `count`. If there are no strings in the string pool, `strpool_collate` returns NULL. The pointer
returned points to the first character of the first string. Strings are zero-terminated, and immediately after the 
termination character, comes the first character of the next string.


strpool_free_collated
---------------------

    void strpool_free_collated( strpool_t const* pool, char* collated_ptr )

Releases the memory returned by `strpool_collate`. 

*/


/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifndef strpool_impl
#define strpool_impl

struct strpool_internal_hash_slot_t;
struct strpool_internal_entry_t;
struct strpool_internal_handle_t;
struct strpool_internal_block_t;

struct strpool_t
    {
    void* memctx;
    int ignore_case;
    int counter_shift;
    STRPOOL_U64 counter_mask;
    STRPOOL_U64 index_mask;

    int initial_entry_capacity;
    int initial_block_capacity;
    int block_size;
    int min_data_size;

    struct strpool_internal_hash_slot_t* hash_table;
    int hash_capacity;

    struct strpool_internal_entry_t* entries;
    int entry_capacity;
    int entry_count;

    struct strpool_internal_handle_t* handles;
    int handle_capacity;
    int handle_count;
    int handle_freelist_head;
    int handle_freelist_tail;

    struct strpool_internal_block_t* blocks;
    int block_capacity;
    int block_count;
    int current_block;
    };


#endif /* strpool_impl */


#ifdef STRPOOL_IMPLEMENTATION
#undef STRPOOL_IMPLEMENTATION

#define _CRT_NONSTDC_NO_DEPRECATE 
#define _CRT_SECURE_NO_WARNINGS
#include <stddef.h>

#ifndef STRPOOL_ASSERT
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <assert.h>
    #define STRPOOL_ASSERT( expression, message ) assert( ( expression ) && ( message ) )
#endif

#ifndef STRPOOL_MEMSET
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <string.h>
    #define STRPOOL_MEMSET( ptr, val, cnt ) ( memset( ptr, val, cnt ) )
#endif 

#ifndef STRPOOL_MEMCPY
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <string.h>
    #define STRPOOL_MEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif 

#ifndef STRPOOL_MEMCMP
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <string.h>
    #define STRPOOL_MEMCMP( pr1, pr2, cnt ) ( memcmp( pr1, pr2, cnt ) )
#endif 

#ifndef STRPOOL_STRNICMP
    #ifdef _WIN32
        #define _CRT_NONSTDC_NO_DEPRECATE 
        #define _CRT_SECURE_NO_WARNINGS
        #include <string.h>
        #define STRPOOL_STRNICMP( s1, s2, len ) ( strnicmp( s1, s2, len ) )
    #else
        #include <string.h>
        #define STRPOOL_STRNICMP( s1, s2, len ) ( strncasecmp( s1, s2, len ) )        
    #endif
#endif 

#ifndef STRPOOL_MALLOC
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <stdlib.h>
    #define STRPOOL_MALLOC( ctx, size ) ( malloc( size ) )
    #define STRPOOL_FREE( ctx, ptr ) ( free( ptr ) )
#endif


typedef struct strpool_internal_hash_slot_t
    {
    STRPOOL_U32 hash_key;
    int entry_index;
    int base_count;
    } strpool_internal_hash_slot_t;


typedef struct strpool_internal_entry_t
    {
    int hash_slot;
    int handle_index;
    char* data;
    int size;
    int length;
    int refcount;
    } strpool_internal_entry_t;


typedef struct strpool_internal_handle_t
    {
    int entry_index;
    int counter;
    } strpool_internal_handle_t;


typedef struct strpool_internal_block_t
    {
    int capacity;
    char* data;
    char* tail;
    int free_list;
    } strpool_internal_block_t;


typedef struct strpool_internal_free_block_t
    {
    int size;
    int next;
    } strpool_internal_free_block_t;


strpool_config_t const strpool_default_config = 
    { 
    /* memctx         = */ 0,
    /* ignore_case    = */ 0,
    /* counter_bits   = */ 32,
    /* index_bits     = */ 32,
    /* entry_capacity = */ 4096, 
    /* block_capacity = */ 32, 
    /* block_size     = */ 256 * 1024, 
    /* min_length     = */ 23,
    };



static STRPOOL_U32 strpool_internal_pow2ceil( STRPOOL_U32 v )
    {
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    v += ( v == 0 );
    return v;
    }


static int strpool_internal_add_block( strpool_t* pool, int size )
    {
    if( pool->block_count >= pool->block_capacity ) 
        {
        pool->block_capacity *= 2;
        strpool_internal_block_t* new_blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
            pool->block_capacity * sizeof( *pool->blocks ) );
        STRPOOL_ASSERT( new_blocks, "Allocation failed" );
        STRPOOL_MEMCPY( new_blocks, pool->blocks, pool->block_count * sizeof( *pool->blocks ) );
        STRPOOL_FREE( pool->memctx, pool->blocks );
        pool->blocks = new_blocks;
        }
    pool->blocks[ pool->block_count ].capacity = size;
    pool->blocks[ pool->block_count ].data = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) size );
    STRPOOL_ASSERT( pool->blocks[ pool->block_count ].data, "Allocation failed" );
    pool->blocks[ pool->block_count ].tail = pool->blocks[ pool->block_count ].data;
    pool->blocks[ pool->block_count ].free_list = -1;
    return pool->block_count++;
    }


void strpool_init( strpool_t* pool, strpool_config_t const* config )
    {
    if( !config ) config = &strpool_default_config;

    pool->memctx = config->memctx;
    pool->ignore_case = config->ignore_case;

    STRPOOL_ASSERT( config->counter_bits + config->index_bits <= 64, "Total bit count exceeds 64" );
    pool->counter_shift = config->index_bits;
    pool->counter_mask = ( 1ULL << (STRPOOL_U64) config->counter_bits ) - 1;
    pool->index_mask = ( 1ULL << (STRPOOL_U64) config->index_bits ) - 1;

    pool->initial_entry_capacity = 
        (int) strpool_internal_pow2ceil( config->entry_capacity > 1 ? (STRPOOL_U32)config->entry_capacity : 2U );
    pool->initial_block_capacity = 
        (int) strpool_internal_pow2ceil( config->block_capacity > 1 ? (STRPOOL_U32)config->block_capacity : 2U );
    pool->block_size = 
        (int) strpool_internal_pow2ceil( config->block_size > 256 ? (STRPOOL_U32)config->block_size : 256U );
    pool->min_data_size = 
        (int) ( sizeof( int ) * 2 + 1 + ( config->min_length > 8 ? (STRPOOL_U32)config->min_length : 8U ) );

    pool->hash_capacity = pool->initial_entry_capacity * 2;
    pool->entry_capacity = pool->initial_entry_capacity;
    pool->handle_capacity = pool->initial_entry_capacity;
    pool->block_capacity = pool->initial_block_capacity;    

    pool->handle_freelist_head = -1;
    pool->handle_freelist_tail = -1;
    pool->block_count = 0;
    pool->handle_count = 0;
    pool->entry_count = 0;
    
    pool->hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_ASSERT( pool->hash_table, "Allocation failed" );
    STRPOOL_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );
    pool->entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_ASSERT( pool->entries, "Allocation failed" );
    pool->handles = (strpool_internal_handle_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_ASSERT( pool->handles, "Allocation failed" );
    pool->blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->block_capacity * sizeof( *pool->blocks ) );
    STRPOOL_ASSERT( pool->blocks, "Allocation failed" );

    pool->current_block = strpool_internal_add_block( pool, pool->block_size );
    }


void strpool_term( strpool_t* pool )
    {
#if 0
    // Debug statistics
    printf( "\n\n" );
    printf( "Handles: %d/%d\n", pool->handle_count, pool->handle_capacity );
    printf( "Entries: %d/%d\n", pool->entry_count, pool->entry_capacity );
    printf( "Hashtable: %d/%d\n", pool->entry_count, pool->hash_capacity );
    printf( "Blocks: %d/%d\n", pool->block_count, pool->block_capacity );
    for( int i = 0; i < pool->block_count; ++i )
        {
        printf( "\n" );
        printf( "BLOCK: %d\n", i );
        printf( "Capacity: %d\n", pool->blocks[ i ].capacity );
        printf( "Free: [ %d ]", (int)( pool->blocks[ i ].capacity - ( pool->blocks[ i ].tail - pool->blocks[ i ].data ) ) );
        int fl = pool->blocks[ i ].free_list;
        int count = 0;
        int size = 0;
        int total = 0;
        while( fl >= 0 )
            {
            strpool_internal_free_block_t* free_entry = (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + fl );
            total += free_entry->size;
            if( size == 0 ) { size = free_entry->size; }
            if( size != free_entry->size )
                {
                printf( ", %dx%d", count, size );
                count = 1;
                size = free_entry->size;
                }
            else
                {
                ++count;
                }
            fl = free_entry->next;
            }
        if( size != 0 ) printf( ", %dx%d", count, size );
        printf( ", { %d }\n", total );
        }
    printf( "\n\n" );
#endif

    for( int i = 0; i < pool->block_count; ++i ) STRPOOL_FREE( pool->memctx, pool->blocks[ i ].data );
    STRPOOL_FREE( pool->memctx, pool->blocks );         
    STRPOOL_FREE( pool->memctx, pool->handles );            
    STRPOOL_FREE( pool->memctx, pool->entries );            
    STRPOOL_FREE( pool->memctx, pool->hash_table );         
    }


void strpool_defrag( strpool_t* pool )
    {
    int data_size = 0;
    int count = 0;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        strpool_internal_entry_t* entry = &pool->entries[ i ];
        if( entry->refcount > 0 )
            {
            data_size += entry->size;
            ++count;
            }
        }

    int data_capacity = data_size < pool->block_size ? 
        pool->block_size : (int)strpool_internal_pow2ceil( (STRPOOL_U32)data_size );

    int hash_capacity = count + count / 2;
    hash_capacity = hash_capacity < ( pool->initial_entry_capacity * 2 ) ? 
        ( pool->initial_entry_capacity * 2 ) : (int)strpool_internal_pow2ceil( (STRPOOL_U32)hash_capacity );
    strpool_internal_hash_slot_t* hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        hash_capacity * sizeof( *hash_table ) );
    STRPOOL_ASSERT( hash_table, "Allocation failed" );
    STRPOOL_MEMSET( hash_table, 0, hash_capacity * sizeof( *hash_table ) );

    char* data = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) data_capacity );
    STRPOOL_ASSERT( data, "Allocation failed" );
    int capacity = count < pool->initial_entry_capacity ? 
        pool->initial_entry_capacity : (int)strpool_internal_pow2ceil( (STRPOOL_U32)count );
    strpool_internal_entry_t* entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        capacity * sizeof( *entries ) );
    STRPOOL_ASSERT( entries, "Allocation failed" );
    int index = 0;
    char* tail = data;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        strpool_internal_entry_t* entry = &pool->entries[ i ];
        if( entry->refcount > 0 )
            {
            entries[ index ] = *entry;

            STRPOOL_U32 hash = pool->hash_table[ entry->hash_slot ].hash_key;
            int base_slot = (int)( hash & (STRPOOL_U32)( hash_capacity - 1 ) );
            int slot = base_slot;
            while( hash_table[ slot ].hash_key )
                slot = (slot + 1 ) & ( hash_capacity - 1 );
            STRPOOL_ASSERT( hash, "Invalid hash" );
            hash_table[ slot ].hash_key = hash;
            hash_table[ slot ].entry_index = index;
            ++hash_table[ base_slot ].base_count;

            entries[ index ].hash_slot = slot;
            entries[ index ].data = tail;
            entries[ index ].handle_index = entry->handle_index;
            pool->handles[ entry->handle_index ].entry_index = index;
            STRPOOL_MEMCPY( tail, entry->data, entry->length + 1 + 2 * sizeof( STRPOOL_U32 ) );
            tail += entry->size;
            ++index;
            }
        }


    STRPOOL_FREE( pool->memctx, pool->hash_table );
    STRPOOL_FREE( pool->memctx, pool->entries );
    for( int i = 0; i < pool->block_count; ++i ) STRPOOL_FREE( pool->memctx, pool->blocks[ i ].data );

    if( pool->block_capacity != pool->initial_block_capacity )
        {
        STRPOOL_FREE( pool->memctx, pool->blocks );
        pool->blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
            pool->initial_block_capacity * sizeof( *pool->blocks ) );
        STRPOOL_ASSERT( pool->blocks, "Allocation failed" );
        }
    pool->block_capacity = pool->initial_block_capacity;
    pool->block_count = 1;
    pool->current_block = 0;
    pool->blocks[ 0 ].capacity = data_capacity;
    pool->blocks[ 0 ].data = data;
    pool->blocks[ 0 ].tail = tail;
    pool->blocks[ 0 ].free_list = -1;
    
    pool->hash_table = hash_table;
    pool->hash_capacity = hash_capacity;

    pool->entries = entries;
    pool->entry_capacity = capacity;
    pool->entry_count = count;
    }


static STRPOOL_U64 strpool_internal_make_handle( int index, int counter, STRPOOL_U64 index_mask, int counter_shift, 
    STRPOOL_U64 counter_mask )
    {
    STRPOOL_U64 i = (STRPOOL_U64) ( index + 1 );
    STRPOOL_U64 c = (STRPOOL_U64) counter;
    return ( ( c & counter_mask ) << counter_shift ) | ( i & index_mask );
    }


static int strpool_internal_counter_from_handle( STRPOOL_U64 handle, int counter_shift, STRPOOL_U64 counter_mask  )
    {
    return (int) ( ( handle >> counter_shift ) & counter_mask ) ;
    }
    

static int strpool_internal_index_from_handle( STRPOOL_U64 handle, STRPOOL_U64 index_mask )
    {
    return ( (int) ( handle & index_mask ) ) - 1;
    }


static strpool_internal_entry_t* strpool_internal_get_entry( strpool_t const* pool, STRPOOL_U64 handle )
    {
    int index = strpool_internal_index_from_handle( handle, pool->index_mask );
    int counter = strpool_internal_counter_from_handle( handle, pool->counter_shift, pool->counter_mask );

    if( index >= 0 && index < pool->handle_count && 
        counter == (int) ( pool->handles[ index ].counter & pool->counter_mask ) )
            return &pool->entries[ pool->handles[ index ].entry_index ];

    return 0;
    }


static STRPOOL_U32 strpool_internal_find_in_blocks( strpool_t const* pool, char const* string, int length )
    {
    for( int i = 0; i < pool->block_count; ++i )
        {
        strpool_internal_block_t* block = &pool->blocks[ i ];
        // Check if string comes from pool
        if( string >= block->data + 2 * sizeof( STRPOOL_U32 ) && string < block->data + block->capacity ) 
            {
            STRPOOL_U32* ptr = (STRPOOL_U32*) string;
            int stored_length = (int)( *( ptr - 1 ) ); // Length is stored immediately before string
            if( stored_length != length || string[ length ] != '\0' ) return 0; // Invalid string
            STRPOOL_U32 hash = *( ptr - 2 ); // Hash is stored before the length field
            return hash;
            }
        }

    return 0;
    }


static STRPOOL_U32 strpool_internal_calculate_hash( char const* string, int length, int ignore_case )
    {
    STRPOOL_U32 hash = 5381U; 

    if( ignore_case) 
        {
        for( int i = 0; i < length; ++i )
            {
            char c = string[ i ];
            c = ( c <= 'z' && c >= 'a' ) ? c - ( 'a' - 'A' ) : c;
            hash = ( ( hash << 5U ) + hash) ^ c;
            }
        }
    else
        {
        for( int i = 0; i < length; ++i )
            {
            char c = string[ i ];
            hash = ( ( hash << 5U ) + hash) ^ c;
            }
        }

    hash = ( hash == 0 ) ? 1 : hash; // We can't allow 0-value hash keys, but dupes are ok
    return hash;
    }


static void strpool_internal_expand_hash_table( strpool_t* pool )
    {
    int old_capacity = pool->hash_capacity;
    strpool_internal_hash_slot_t* old_table = pool->hash_table;

    pool->hash_capacity *= 2;

    pool->hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_ASSERT( pool->hash_table, "Allocation failed" );
    STRPOOL_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );

    for( int i = 0; i < old_capacity; ++i )
        {
        STRPOOL_U32 hash_key = old_table[ i ].hash_key;
        if( hash_key )
            {
            int base_slot = (int)( hash_key & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
            int slot = base_slot;
            while( pool->hash_table[ slot ].hash_key )
                slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            STRPOOL_ASSERT( hash_key, "Invalid hash" );
            pool->hash_table[ slot ].hash_key = hash_key;
            pool->hash_table[ slot ].entry_index = old_table[ i ].entry_index;  
            pool->entries[ pool->hash_table[ slot ].entry_index ].hash_slot = slot; 
            ++pool->hash_table[ base_slot ].base_count;
            }               
        }

    STRPOOL_FREE( pool->memctx, old_table );
    }


static void strpool_internal_expand_entries( strpool_t* pool )
    {
    pool->entry_capacity *= 2;
    strpool_internal_entry_t* new_entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_ASSERT( new_entries, "Allocation failed" );
    STRPOOL_MEMCPY( new_entries, pool->entries, pool->entry_count * sizeof( *pool->entries ) );
    STRPOOL_FREE( pool->memctx, pool->entries );
    pool->entries = new_entries;    
    }


static void strpool_internal_expand_handles( strpool_t* pool )
    {
    pool->handle_capacity *= 2;
    strpool_internal_handle_t* new_handles = (strpool_internal_handle_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_ASSERT( new_handles, "Allocation failed" );
    STRPOOL_MEMCPY( new_handles, pool->handles, pool->handle_count * sizeof( *pool->handles ) );
    STRPOOL_FREE( pool->memctx, pool->handles );
    pool->handles = new_handles;
    }


static char* strpool_internal_get_data_storage( strpool_t* pool, int size, int* alloc_size )
    {
    if( size < sizeof( strpool_internal_free_block_t ) ) size = sizeof( strpool_internal_free_block_t );
    if( size < pool->min_data_size ) size = pool->min_data_size;
    size = (int)strpool_internal_pow2ceil( (STRPOOL_U32)size );
    
    // Try to find a large enough free slot in existing blocks
    for( int i = 0; i < pool->block_count; ++i )
        {
        int free_list = pool->blocks[ i ].free_list;
        int prev_list = -1;
        while( free_list >= 0 )
            {
            strpool_internal_free_block_t* free_entry = 
                (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + free_list );
            if( free_entry->size / 2 < size ) 
                {
                // At this point, all remaining slots are too small, so bail out if the current slot is not large enough
                if( free_entry->size < size ) break; 

                if( prev_list < 0 )
                    {
                    pool->blocks[ i ].free_list = free_entry->next;         
                    }
                else
                    {
                    strpool_internal_free_block_t* prev_entry = 
                        (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + prev_list );
                    prev_entry->next = free_entry->next;
                    }
                *alloc_size = free_entry->size;
                return (char*) free_entry;
                }
            prev_list = free_list;
            free_list = free_entry->next;
            }
        }

    // Use current block, if enough space left
    int offset = (int) ( pool->blocks[ pool->current_block ].tail - pool->blocks[ pool->current_block ].data );
    if( size <= pool->blocks[ pool->current_block ].capacity - offset )
        {
        char* data = pool->blocks[ pool->current_block ].tail;
        pool->blocks[ pool->current_block ].tail += size;
        *alloc_size = size;
        return data;
        }

    // Allocate a new block
    pool->current_block = strpool_internal_add_block( pool, size > pool->block_size ? size : pool->block_size );
    char* data = pool->blocks[ pool->current_block ].tail;
    pool->blocks[ pool->current_block ].tail += size;
    *alloc_size = size;
    return data;
    }
    

STRPOOL_U64 strpool_inject( strpool_t* pool, char const* string, int length )
    {
    if( !string || length <= 0 ) return 0;

    STRPOOL_U32 hash = strpool_internal_find_in_blocks( pool, string, length );
    // If no stored hash, calculate it from data
    if( !hash ) hash = strpool_internal_calculate_hash( string, length, pool->ignore_case ); 

    // Return handle to existing string, if it is already in pool
    int base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
    int base_count = pool->hash_table[ base_slot ].base_count;
    int slot = base_slot;
    int first_free = slot;
    while( base_count > 0 )
        {
        STRPOOL_U32 slot_hash = pool->hash_table[ slot ].hash_key;
        if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
        int slot_base = (int)( slot_hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        if( slot_base == base_slot ) 
            {
            STRPOOL_ASSERT( base_count > 0, "Invalid base count" );
            --base_count;
            if( slot_hash == hash )
                {
                int index = pool->hash_table[ slot ].entry_index;
                strpool_internal_entry_t* entry = &pool->entries[ index ];
                if( entry->length == length && 
                    ( 
                       ( !pool->ignore_case &&   STRPOOL_MEMCMP( entry->data + 2 * sizeof( STRPOOL_U32 ), string, (size_t)length ) == 0 )
                    || (  pool->ignore_case && STRPOOL_STRNICMP( entry->data + 2 * sizeof( STRPOOL_U32 ), string, (size_t)length ) == 0 ) 
                    ) 
                  )
                    {
                    int handle_index = entry->handle_index;
                    return strpool_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, 
                        pool->index_mask, pool->counter_shift, pool->counter_mask );
                    }
                }
            }
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
        }   

    // This is a new string, so let's add it

    if( pool->entry_count >= ( pool->hash_capacity  - pool->hash_capacity / 3 ) )
        {
        strpool_internal_expand_hash_table( pool );

        base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        slot = base_slot;
        first_free = slot;
        while( base_count )
            {
            STRPOOL_U32 slot_hash = pool->hash_table[ slot ].hash_key;
            if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
            int slot_base = (int)( slot_hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
            if( slot_base == base_slot )  --base_count;
            slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            }       
        }
        
    slot = first_free;
    while( pool->hash_table[ slot ].hash_key )
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );

    if( pool->entry_count >= pool->entry_capacity )
        strpool_internal_expand_entries( pool );

    STRPOOL_ASSERT( !pool->hash_table[ slot ].hash_key && ( hash & ( (STRPOOL_U32) pool->hash_capacity - 1 ) ) == (STRPOOL_U32) base_slot, "Invalid slot" );
    STRPOOL_ASSERT( hash, "Invalid hash" );
    pool->hash_table[ slot ].hash_key = hash;
    pool->hash_table[ slot ].entry_index = pool->entry_count;
    ++pool->hash_table[ base_slot ].base_count;

    int handle_index;

    if( pool->handle_count < pool->handle_capacity )
        {
        handle_index = pool->handle_count;
        pool->handles[ pool->handle_count ].counter = 1;
        ++pool->handle_count;           
        }
    else if( pool->handle_freelist_head >= 0 )
        {
        handle_index = pool->handle_freelist_head;
        if( pool->handle_freelist_tail == pool->handle_freelist_head ) 
            pool->handle_freelist_tail = pool->handles[ pool->handle_freelist_head ].entry_index;
        pool->handle_freelist_head = pool->handles[ pool->handle_freelist_head ].entry_index;                       
        }
    else
        {
        strpool_internal_expand_handles( pool );
        handle_index = pool->handle_count;
        pool->handles[ pool->handle_count ].counter = 1;
        ++pool->handle_count;           
        }

    pool->handles[ handle_index ].entry_index = pool->entry_count;
        
    strpool_internal_entry_t* entry = &pool->entries[ pool->entry_count ];
    ++pool->entry_count;
        
    int data_size = length + 1 + (int) ( 2 * sizeof( STRPOOL_U32 ) );
    char* data = strpool_internal_get_data_storage( pool, data_size, &data_size );
    entry->hash_slot = slot;
    entry->handle_index = handle_index;
    entry->data = data;
    entry->size = data_size;
    entry->length = length;
    entry->refcount = 0;

    *(STRPOOL_U32*)(data) = hash;
    data += sizeof( STRPOOL_U32 );
    *(STRPOOL_U32*)(data) = (STRPOOL_U32) length;
    data += sizeof( STRPOOL_U32 );
    STRPOOL_MEMCPY( data, string, (size_t) length ); 
    data[ length ] = 0; // Ensure trailing zero

    return strpool_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, pool->index_mask, 
        pool->counter_shift, pool->counter_mask );
    }


void strpool_discard( strpool_t* pool, STRPOOL_U64 handle )
    {   
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry && entry->refcount == 0 )
        {
        int entry_index = pool->handles[ entry->handle_index ].entry_index;

        // recycle string mem
        for( int i = 0; i < pool->block_count; ++i )
            {
            strpool_internal_block_t* block = &pool->blocks[ i ];
            if( entry->data >= block->data && entry->data <= block->tail )
                {
                if( block->free_list < 0 )
                    {
                    strpool_internal_free_block_t* new_entry = (strpool_internal_free_block_t*) ( entry->data );
                    block->free_list = (int) ( entry->data - block->data );
                    new_entry->next = -1;
                    new_entry->size = entry->size;
                    }
                else
                    {
                    int free_list = block->free_list;
                    int prev_list = -1;
                    while( free_list >= 0 )
                        {
                        strpool_internal_free_block_t* free_entry = 
                            (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + free_list );
                        if( free_entry->size <= entry->size ) 
                            {
                            strpool_internal_free_block_t* new_entry = (strpool_internal_free_block_t*) ( entry->data );
                            if( prev_list < 0 )
                                {
                                new_entry->next = pool->blocks[ i ].free_list;
                                pool->blocks[ i ].free_list = (int) ( entry->data - block->data );          
                                }
                            else
                                {
                                strpool_internal_free_block_t* prev_entry = 
                                    (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + prev_list );
                                prev_entry->next = (int) ( entry->data - block->data );
                                new_entry->next = free_entry->next;
                                }
                            new_entry->size = entry->size;
                            break;
                            }
                        prev_list = free_list;
                        free_list = free_entry->next;
                        }
                    }
                break;
                }
            }

        // recycle handle
        if( pool->handle_freelist_tail < 0 )
            {
            STRPOOL_ASSERT( pool->handle_freelist_head < 0, "Freelist error" );
            pool->handle_freelist_head = entry->handle_index;
            pool->handle_freelist_tail = entry->handle_index;
            }
        else
            {
            pool->handles[ pool->handle_freelist_tail ].entry_index = entry->handle_index;
            pool->handle_freelist_tail = entry->handle_index;
            }
        ++pool->handles[ entry->handle_index ].counter; // invalidate handle via counter
        pool->handles[ entry->handle_index ].entry_index = -1;

        // recycle hash slot
        STRPOOL_U32 hash = pool->hash_table[ entry->hash_slot ].hash_key;
        int base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        STRPOOL_ASSERT( hash, "Invalid hash" );
        --pool->hash_table[ base_slot ].base_count;
        pool->hash_table[ entry->hash_slot ].hash_key = 0;

        // recycle entry
        if( entry_index != pool->entry_count - 1 )
            {
            pool->entries[ entry_index ] = pool->entries[ pool->entry_count - 1 ];
            pool->hash_table[ pool->entries[ entry_index ].hash_slot ].entry_index = entry_index;
            pool->handles[ pool->entries[ entry_index ].handle_index ].entry_index = entry_index;
            }
        --pool->entry_count;
        }       

    }


int strpool_incref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry )
        {
        ++entry->refcount;
        return entry->refcount;
        }
    return 0;
    }


int strpool_decref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry )
        {
        STRPOOL_ASSERT( entry->refcount > 0, "Invalid ref count" );
        --entry->refcount;
        return entry->refcount;
        }
    return 0;
    }


int strpool_getref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->refcount;
    return 0;
    }


int strpool_isvalid( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return 1;
    return 0;
    }


char const* strpool_cstr( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->data + 2 * sizeof( STRPOOL_U32 ); // Skip leading hash value
    return NULL;
    }


int strpool_length( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->length;
    return 0;
    }


char* strpool_collate( strpool_t const* pool, int* count )
    {
    int size = 0;
    for( int i = 0; i < pool->entry_count; ++i ) size += pool->entries[ i ].length + 1;
    if( size == 0 ) return NULL;

    char* strings = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) size );
    STRPOOL_ASSERT( strings, "Allocation failed" );
    *count = pool->entry_count;
    char* ptr = strings;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        int len = pool->entries[ i ].length + 1;
        char* src = pool->entries[ i ].data + 2 * sizeof( STRPOOL_U32 );
        STRPOOL_MEMCPY( ptr, src, (size_t) len );
        ptr += len;
        }
    return strings;
    }


void strpool_free_collated( strpool_t const* pool, char* collated_ptr )
    {
    (void) pool;
    STRPOOL_FREE( pool->memctx, collated_ptr );
    }


#endif /* STRPOOL_IMPLEMENTATION */


/*
revision history:
    1.4     fixed find_in_blocks substring bug, removed realloc, added docs
    1.3     fixed typo in mask bit shift
    1.2     made it possible to override standard library functions
    1.1     added is_valid function to query a handles validity
    1.0     first released version  
*/


/*
------------------------------------------------------------------------------

This software is available under 2 licenses - you may choose the one you like.

------------------------------------------------------------------------------

ALTERNATIVE A - MIT License

Copyright (c) 2015 Mattias Gustavsson

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.

------------------------------------------------------------------------------

ALTERNATIVE B - Public Domain (www.unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

------------------------------------------------------------------------------
*/
