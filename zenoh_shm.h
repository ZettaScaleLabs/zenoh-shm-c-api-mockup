#include <stdint.h>
#include <assert.h>
#include <memory.h>

#include "zenoh_c_api/zenoh.h"

//// TYPES ////

// Unique protocol identifier
// Here is a contract: it is up to the user to make sure that incompatible SharedMemoryClient
// and SharedMemoryProviderBackend implementations will never use the same z_protocol_id_t
typedef uint32_t z_protocol_id_t;

// Unique segment identifier
typedef uint32_t z_segment_id_t;

// Chunk id within it's segment
typedef uint32_t z_chunk_id_t;

// ChunkDescriptor uniquely identifies the particular chunk within particular segment
struct z_chunk_descriptor_t
{
    z_segment_id_t segment;
    z_chunk_id_t chunk;
};

// Allocation result enum
enum z_alloc_result_t
{
    OK = 0,              // successful allocation
    NEED_DEFRAGMENT = 1, // the chunk cannot be allocated because defragmentation is needed
    OUT_OF_MEMORY = 2,   // the chunk cannot be allocated because the provider is out of memory
    OTHER_ERROR = 3      // other error occured
};

// Structure that represents an allocated chunk
struct z_allocated_chunk_t
{
    z_chunk_descriptor_t descriptor;
    uint8_t *data;
};

//// INTERFACES ////

typedef struct z_owned_shared_memory_segment_t
{
    void *context;
    /// Obtain the actual region of memory identified by it's id
    /// @param chunk chunk identifier within a segment
    /// @param error error message if error occured
    /// @param context context
    /// @returns pointer to mapped data or NULL if error occured
    uint8_t *(*map)(z_chunk_id_t chunk, z_owned_str_t *error, void *context);
    void (*drop)(void *);
} z_owned_shared_memory_segment_t;

typedef struct z_owned_shared_memory_client_t
{
    void *context;
    /// Attach to particular shared memory segment
    /// @param id identifier of a segment
    /// @param segment the result of attachment
    /// @param context context
    /// @returns non-empty error if any error occured
    z_owned_str_t (*attach)(z_segment_id_t id, z_owned_shared_memory_segment_t *segment, void *context);
    void (*drop)(void *);
} z_owned_shared_memory_client_t;

// ^these two^ interface entities are designed to optimize Segment lookup under the hood:
// Key: z_protocol_id_t << 32 | z_segment_id_t
// Value: SharedMemorySegment
// segments: associative_container<Key, Value>
// The idea is simple: instead of making two lookups (z_protocol_id_t lookup and then z_segment_id_t lookup) each time, we can make one.

typedef struct z_owned_shared_memory_provider_backend_t
{
    void *context;
    /// Allocate the chunk of desired size
    /// @param len the desired data len
    /// @param chunk the allocated chunk if succeed
    /// @param context context
    /// @returns allocation result
    z_alloc_result_t (*alloc)(size_t len, z_allocated_chunk_t *chunk, void *context);

    /// Deallocate the chunk
    /// @param chunk the allocation result
    /// @param context context
    void (*free)(z_chunk_descriptor_t *chunk, void *context);

    /// Defragment the memory
    void (*defragment)(void *);

    void (*drop)(void *);
} z_owned_shared_memory_provider_backend_t;

// Shared memory provider handle
typedef void *z_shared_memory_provider_t;

/// Allocate the buffer of desired size
ZENOHC_API z_alloc_result_t z_shared_memory_provider_alloc(
    z_shared_memory_provider_t provider,
    size_t len,
    zc_owned_shmbuf_t *result);

/// Defragment the memory
ZENOHC_API void z_shared_memory_provider_defragment(z_shared_memory_provider_t provider);

/// Map externally-allocated chunk into zc_owned_shmbuf_t
/// This method is designed to be used with push data sources
/// @param provider the provider instance
/// @param chunk the allocated chunk to map
/// @param size the size of a chunk
/// @param error will contain error if any error occured
/// @returns valid shm buffer handle, or invalid if error occured
ZENOHC_API zc_owned_shmbuf_t z_shared_memory_provider_map(
    z_shared_memory_provider_t provider,
    z_allocated_chunk_t *chunk,
    size_t size,
    z_owned_str_t *error);

// struct for provider mappings
typedef struct z_shared_memory_mapped_providers_t
{
    z_protocol_id_t id;
    z_owned_shared_memory_provider_backend_t backend;
};

// struct for client mappings
typedef struct z_shared_memory_mapped_clients_t
{
    z_protocol_id_t id;
    z_owned_shared_memory_client_t client;
};

// the shared memory factory
typedef void *z_owned_shared_memory_factory_t;

/// Create shared memory factory
/// @param map pointer to array containing providers and\or clients for particular protocols
/// @param map_len number of elements in array
/// @param error will contain error if smth went wrong
/// @returns the shared memory factory handle
ZENOHC_API z_owned_shared_memory_factory_t z_shared_memory_factory_make(
    z_shared_memory_mapped_providers_t *providers,
    size_t providers_count,
    z_shared_memory_mapped_clients_t *clients,
    size_t clients_count,
    z_owned_str_t *error);

/// Get the shared memory provider
/// @param id protocol id
/// @returns the shared memory provider (can be invalid if there is no provider for particular id)
ZENOHC_API z_shared_memory_provider_t z_shared_memory_factory_provider(z_protocol_id_t id);

/// Get the shared memory provider
/// @param session zenoh session
/// @param id protocol id
/// @returns the shared memory provider (can be invalid if there is no provider for particular id)
ZENOHC_API z_shared_memory_provider_t z_session_shared_memory_provider(z_owned_session_t session, z_protocol_id_t id);
