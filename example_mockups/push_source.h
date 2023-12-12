#include "../zenoh_shm.h"

// The example on how to work with push sources!
// This is a callback for push source, it is called by some external code (eg. device library, timer - whatever)
void push_source_callback(
    z_owned_publisher_t *pub,
    z_shared_memory_provider_t provider,
    // push source should provide implementation-defined chunk (like custom shared memory provider do in it's alloc implementation)
    z_allocated_chunk_t *chunk,
    size_t size)
{
    z_owned_str_t error;

    // map the provided chunk into shared memory buf
    zc_owned_shmbuf_t shmbuf = z_shared_memory_provider_map(provider, chunk, size, &error);

    // check if the mapping is ok
    if (z_check(shmbuf))
    {
        // publish the shared memory buffer
        z_publisher_put_options_t options = z_publisher_put_options_default();
        options.encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, NULL);
        zc_owned_payload_t payload = zc_shmbuf_into_payload(z_move(shmbuf));
        zc_publisher_put_owned(z_loan(*pub), z_move(payload), &options);
    }
    else
    {
        // handle error
    }
}
