This is a mockup for future zenoh-c SHM API
The structure of this folder is:

1. zenoh_c_api - folder with already-existing zenoh-c headers (they needed becuse SHM API depends on it)

2. zenoh_shm.h - file containing mocked-up SHM API placed separately from zenoh-c API for better understanding

3. example_mockups - folder with various examples on how to use the future zenoh-c SHM API:
    - custom_shared_memory_provider.h: illustrates how to implement custom shared memory provider (uses POSIX shared memory)
    - push_source.h: illustrates how to work with push source that proactively produces allocated shared memory buffers in it's own thread
    - simple_shm_publisher.h: publication of SHM data
    - simple_shm_subscriber.h: subscribtion to SHM data
    - two_processes_shm_usage.h: example how to split custom SHM Provider and Client codebases between different executables
    
    
A few words about the API

The following entities make SHM API:

SharedMemoryProvider
A zenoh interface for interacting with specific SHM provider. Wraps SharedMemoryProviderBackend and does some common under the hood job to provide high-level interface
for SHM buffer allocation.

SharedMemoryProviderBackend
An SHM provider's implementation that is able to allocate\deallocate SHM buffers. In other words, this is a shared memory allocator that implements some specific API.

SharedMemoryClient
An SHM client's implementation that is able to map SHM buffers allocated by the specific provider.

Protocol
Protocol is SHM protocol. Obviously, as we have different SHM clients and different SHM providers, they are not necessary compatible between each other. This ID is used to distinguish between compatible entities.

The more precise doc is in *.h files of the API
