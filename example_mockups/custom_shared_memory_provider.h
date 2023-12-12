#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../zenoh_shm.h"

///////////////////////////////////
/// THIS CODE IS SHARED BETWEEN ///
///   THE PROVIDER AND CLIENTS  ///
///////////////////////////////////
#define POSIX_SHMEM_BUFFER_COUNT 1024
#define POSIX_SHMEM_BUFFER_SIZE 1024
// This structure is placed into shared memory and shared between provider and clients
// It is very simple in this example, but for more advanced provider implementations
// it may contain some complicated fields: index tables, atomics, etc
typedef struct posix_shm_segment_t
{
    // data array, that is used for allocations. As long as this example uses
    // very simple allocator, it is able to allocate chunks of size <= POSIX_SHMEM_BUFFER_SIZE
    unsigned char data[POSIX_SHMEM_BUFFER_COUNT][POSIX_SHMEM_BUFFER_SIZE];
} posix_shm_segment_t;

///////////////////////////////////
///       PROVIDER'S CODE       ///
///////////////////////////////////

// context for the provider backend side
typedef struct posix_shm_provider_backend_context_t
{
    // segment id is generated upon segment creation and used in posix_shm_backend_alloc to pass segment id to remote clients
    z_segment_id_t segment_id;

    // shared memory segment (clients will see this memory)
    posix_shm_segment_t *segment;

    // flags for marking used\unused chunks
    bool chunk_usage[POSIX_SHMEM_BUFFER_COUNT];
} posix_shm_provider_backend_context_t;

// Alloc function implemenytation
// Selects the first available chunk and makes and allocation on it's memory
z_alloc_result_t posix_shm_backend_alloc(size_t len, z_allocated_chunk_t *chunk, void *context)
{
    // this allocator is dummy, only chunk sizes <= POSIX_SHMEM_BUFFER_SIZE are supported!
    if (len > POSIX_SHMEM_BUFFER_SIZE)
        return z_alloc_result_t::OTHER_ERROR;

    // find a free chunk
    posix_shm_provider_backend_context_t *c = (posix_shm_provider_backend_context_t *)context;
    for (size_t i = 0; i < POSIX_SHMEM_BUFFER_COUNT; ++i)
    {
        if (!c->chunk_usage[i])
        {
            // fill the data field - it points to an appropriate place in shared memory segment
            chunk->data = c->segment->data[i];

            // fill segment id - it is necessary on the Client side to attach to our shared memory segment!
            chunk->descriptor.segment = c->segment_id;
            // here we are using chunk index as a chunk id! in well-designed allocators it is better to use
            // address offset as a chunk id, which gives ~14G segment size support for 4-byte-aligned allocations (MAX_UINT_32 * 4)
            chunk->descriptor.chunk = i;

            // mark the chunk as used
            c->chunk_usage[i] = true;

            // we're done! the chunk is allocated
            return z_alloc_result_t::OK;
        }
    }
    return z_alloc_result_t::OUT_OF_MEMORY;
}

// Free function implementation
// Frees the particular chunk identified by it's id
void posix_shm_backend_free(z_chunk_descriptor_t *chunk, void *context)
{
    posix_shm_provider_backend_context_t *c = (posix_shm_provider_backend_context_t *)context;
    //  check if the chunk matches our segment and it is marked as allocated and then mark it as free
    if (c->segment_id == chunk->segment &&
        c->chunk_usage[chunk->chunk])
    {
        c->chunk_usage[chunk->chunk] = false;
    }
    else
    {
        // critical error?
    }
}
void posix_shm_backend_defragment(void *context)
{
    // nothing to do here
}
void posix_shm_backend_drop(void *context)
{
    posix_shm_provider_backend_context_t *c = (posix_shm_provider_backend_context_t *)context;

    // unmap the POSIX shared memory segment
    munmap(c->segment, sizeof(*c->segment));

    // make filename from segment identifier
    char filename[64];
    sprintf(filename, "%u", c->segment_id);
    // unlink the file
    shm_unlink(filename);

    // delete the context
    free(context);
}

// Creates shm provider backend
// Generates random segment id and uses it as a filename to create a new named shared memory segment.
// This allows remote client to find and attach to  this segment by it's segment id
z_owned_shared_memory_provider_backend_t make_posix_shm_backend()
{
    // allocate memory for the context
    posix_shm_provider_backend_context_t *context = (posix_shm_provider_backend_context_t *)calloc(1, sizeof(*context));

    // generate segment identifier and store it in the context
    // this id will be used to generate filename to attach to the segment both at the Provider and Client side!
    context->segment_id = rand();

    // make filename from segment identifier
    char filename[64];
    sprintf(filename, "%u", context->segment_id);

    // create named POSIX shared memory segment
    int fd = shm_open(filename, O_CREAT | O_EXCL | O_RDWR, 0777);
    if (fd == -1)
        exit(-1);

    // resize segment to our desired size
    if (ftruncate(fd, sizeof(posix_shm_segment_t)) == -1)
        exit(-1);

    // attach the current process to the segment
    context->segment = (posix_shm_segment_t *)mmap(NULL, sizeof(posix_shm_segment_t), PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, fd, 0);
    if (context->segment == NULL)
        exit(-1);

    // close FD
    // "After the mmap() call has returned, the file descriptor, fd, can
    // be closed immediately without invalidating the mapping."
    close(fd);

    // fill the result
    z_owned_shared_memory_provider_backend_t result;
    result.alloc = &posix_shm_backend_alloc;
    result.defragment = &posix_shm_backend_defragment;
    result.drop = &posix_shm_backend_drop;
    result.free = &posix_shm_backend_free;
    result.context = context;
    return result;
}

///////////////////////////////////
///        CLIENT'S CODE        ///
///////////////////////////////////

// context for client's segment part
typedef struct posix_shm_client_segment_context_t
{
    // shared memory segment
    posix_shm_segment_t *segment;
} posix_shm_client_segment_context_t;

// Maps the chunk id to pointer to exact memory of a segment
uint8_t *posix_shm_client_segment_context_map(z_chunk_id_t chunk, z_owned_str_t *error, void *context)
{
    posix_shm_client_segment_context_t *c = (posix_shm_client_segment_context_t *)context;

    // check the arguments for safety
    if (chunk >= POSIX_SHMEM_BUFFER_COUNT)
    {
        // error = ...
        return NULL;
    }

    // return the pointer to the chunk data
    return c->segment->data[chunk];
}

void posix_shm_client_segment_context_drop(void *context)
{
    posix_shm_client_segment_context_t *c = (posix_shm_client_segment_context_t *)context;

    // unmap the POSIX shared memory segment
    munmap(c->segment, sizeof(*c->segment));

    // delete the context
    free(context);
}

// Attach to a new segment
// This code mmaps to particular named shared memory segment identified by it's segment id
// and initializes a segment context that is used for mappings within this segment (see posix_shm_client_segment_context_map)
z_owned_str_t posix_shm_client_attach(z_segment_id_t id, z_owned_shared_memory_segment_t *segment, void *context)
{
    // allocate memory for the segment context
    posix_shm_client_segment_context_t *segment_context = (posix_shm_client_segment_context_t *)calloc(1, sizeof(*segment_context));

    // make filename from segment identifier
    char filename[64];
    sprintf(filename, "%u", id);

    // open named POSIX shared memory segment
    int fd = shm_open(filename, O_RDWR, 0777);
    if (fd == -1)
        exit(-1);

    // attach to the segment
    segment_context->segment = (posix_shm_segment_t *)mmap(NULL, sizeof(posix_shm_segment_t), PROT_READ | PROT_WRITE,
                                                           MAP_SHARED, fd, 0);
    if (segment_context->segment == NULL)
        exit(-1);

    // close FD
    // "After the mmap() call has returned, the file descriptor, fd, can
    // be closed immediately without invalidating the mapping."
    close(fd);

    // fill the result
    segment->context = segment_context;
    segment->drop = &posix_shm_client_segment_context_drop;
    segment->map = &posix_shm_client_segment_context_map;

    // return "no error"
    z_owned_str_t err;
    memset(&err, 0, sizeof(err));
    return err;
}
void posix_shm_client_drop(void *context)
{
    // do nothing here
}

z_owned_shared_memory_client_t make_posix_shm_client()
{
    // fill the result
    z_owned_shared_memory_client_t result;
    result.context = NULL;
    result.attach = &posix_shm_client_attach;
    result.drop = &posix_shm_client_drop;
    return result;
}

//// USAGE ////
void use_posix_shm()
{
    // create map with shared memory protocol providers
    z_shared_memory_mapped_providers_t providers[1];
    providers[0].id = 0;
    providers[0].backend = make_posix_shm_backend();

    // create map with shared memory protocol clients
    z_shared_memory_mapped_clients_t clients[1];
    clients[0].id = 0;
    clients[0].client = make_posix_shm_client();

    z_owned_str_t error;

    // create shared memory factory with desired shm protocols
    z_owned_shared_memory_factory_t shmf = z_shared_memory_factory_make(providers, 2, clients, 2, &error);

    // .....
}
