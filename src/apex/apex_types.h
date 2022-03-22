/*
 * Copyright (c) 2014-2021 Kevin Huck
 * Copyright (c) 2014-2021 University of Oregon
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

/* required for Doxygen */

/** @file */

#include <stdint.h>
#include <stdbool.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif

/** The address of a C++ object in APEX.
 * Not useful for the caller that gets it back, but required
 * for stopping the timer later.
 */
typedef void* apex_profiler_handle; // address of internal C++ object

/** A null pointer representing an APEX profiler handle.
 * Used when a null APEX profile handle is to be passed in to
 * apex::stop when the profiler object wasn't retained locally.
 */
#define APEX_NULL_PROFILER_HANDLE (apex_profiler_handle)(nullptr) // for comparisons

/** Rather than use void pointers everywhere, be explicit about
 * what the functions are expecting.
 */
typedef uintptr_t apex_function_address; // generic function pointer

/**
 * \struct apex_profiler_type
 * \brief Typedef for enumerating the different timer types
 *
 * \var APEX_FUNCTION_ADDRESS
 * \var APEX_NAME_STRING
 */
typedef enum _apex_profiler_type {
    APEX_FUNCTION_ADDRESS = 0, /*!< The ID is a function (or instruction) address */
    APEX_NAME_STRING           /*!< The ID is a character string */
} apex_profiler_type;

/**
 * Typedef for enumerating the different event types
 */
typedef enum _error_codes {
  APEX_NOERROR = 0, /*!< No error occurred */
  APEX_ERROR        /*!< Some error occurred - check stderr output for details */
} apex_error_code;

#define APEX_MAX_EVENTS INT32_MAX /*!< The maximum number of event types.
Allows for many custom events. */

/**
 * \struct apex_event_type
 * \brief Typedef for enumerating the different event types
 */
typedef enum _event_type {
  APEX_INVALID_EVENT = -1,
  APEX_STARTUP = 0,         /*!< APEX is initialized */
  APEX_SHUTDOWN,     /* 1*/ /*!< APEX is terminated */
  APEX_DUMP,         /* 2*/ /*!< APEX is dumping output */
  APEX_RESET,        /* 3*/ /*!< APEX is resetting data structures */
  APEX_NEW_NODE,     /* 4*/ /*!< APEX has registered a new process ID */
  APEX_NEW_THREAD,   /* 5*/ /*!< APEX has registered a new OS thread */
  APEX_EXIT_THREAD,  /* 6*/ /*!< APEX has exited an OS thread */
  APEX_START_EVENT,  /* 7*/ /*!< APEX has processed a timer start event */
  APEX_RESUME_EVENT, /* 8*/ /*!< APEX has processed a timer resume event (the number
                                 of calls is not incremented) */
  APEX_STOP_EVENT,   /* 9*/ /*!< APEX has processed a timer stop event */
  APEX_YIELD_EVENT,  /*10*/ /*!< APEX has processed a timer yield event */
  APEX_SAMPLE_VALUE, /*11*/ /*!< APEX has processed a sampled value */
  APEX_SEND,         /*12*/ /*!< APEX has processed a send event */
  APEX_RECV,         /*13*/ /*!< APEX has processed a recv event */
  APEX_PERIODIC,     /*14*/ /*!< APEX has processed a periodic timer */
  APEX_CUSTOM_EVENT_1,      /*!< APEX has processed a custom event - useful for large
                                 granularity application control events */
  APEX_CUSTOM_EVENT_2,      // these are just here for padding, and so we can
  APEX_CUSTOM_EVENT_3,      // test with them.
  APEX_CUSTOM_EVENT_4,
  APEX_CUSTOM_EVENT_5,
  APEX_CUSTOM_EVENT_6,
  APEX_CUSTOM_EVENT_7,
  APEX_CUSTOM_EVENT_8,
  APEX_UNUSED_EVENT = APEX_MAX_EVENTS // can't have more custom events than this
} apex_event_type;

/**
 * Typedef for enumerating the thread states.
 */
typedef enum _thread_state {
    APEX_IDLE,          /*!< Thread is idle */
    APEX_BUSY,          /*!< Thread is working */
    APEX_THROTTLED,     /*!< Thread is throttled (sleeping) */
    APEX_WAITING,       /*!< Thread is waiting for a resource */
    APEX_BLOCKED        /*!< Thread is blocked */
} apex_thread_state;

/**
 * Typedef for enumerating the different optimization strategies
 * for throttling.
 */
typedef enum {APEX_MAXIMIZE_THROUGHPUT,   /*!< maximize the number of calls to a
                                              timer/counter */
              APEX_MAXIMIZE_ACCUMULATED,  /*!< maximize the accumulated value of
                                              a timer/counter */
              APEX_MINIMIZE_ACCUMULATED   /*!< minimize the accumulated value of
                                              a timer/counter */
} apex_optimization_criteria_t;

/**
 * Typedef for enumerating the different optimization methods
 * for throttling.
 */
typedef enum {APEX_SIMPLE_HYSTERESIS,      /*!< optimize using sliding window of
                                               historical observations. A running
                                               average of the most recent N observations
                                               are used as the measurement. */
              APEX_DISCRETE_HILL_CLIMBING, /*!< Use a discrete hill climbing algorithm
                                               for optimization */
              APEX_ACTIVE_HARMONY          /*!< Use Active Harmony for optimization. */
} apex_optimization_method_t;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 * Typedef for enumerating the different asynchronous activity types
 */
typedef enum {APEX_ASYNC_KERNEL,          /* Kernel */
              APEX_ASYNC_MEMORY,          /* memory copy */
              APEX_ASYNC_SYNCHRONIZE,     /* Synchronization events */
              APEX_ASYNC_OTHER            /* OpenACC or "other" events */
} apex_async_activity_t;

/**
 * Structure that holds a profiler ID
 */
typedef struct _apex_profiler_id
{
    apex_profiler_type type;
    union {
      apex_function_address address;
      const char * name;
    } identifier;
} apex_profiler_id;

#endif //DOXYGEN_SHOULD_SKIP_THIS

/** A reference to the policy object,
 * so that policies can be "unregistered", or paused later
 */
typedef struct _policy_handle
{
    int id;           /*!< The ID of the policy, used internally to APEX */
    apex_event_type event_type;    /*!< The type of policy */
    unsigned long period;       /*!< If periodic, the length of the period */
} apex_policy_handle;

/** The APEX context when an event occurs.
 *
 */
typedef struct _context
{
    apex_event_type event_type;        /*!< The type of the event currently
                                           processing */
    apex_policy_handle* policy_handle; /*!< The policy handle for the current
                                           policy function */
    void * data;  /*!< Data associated with the event, such as the custom_data
                       for a custom_event */
} apex_context;

/** The type of a profiler object
 *
 */
typedef enum _profile_type {
  APEX_TIMER,        /*!< This profile is a instrumented timer */
  APEX_COUNTER       /*!< This profile is a sampled counter */
} apex_profile_type;

/**
 * The profile object for a timer in APEX.
 */
typedef struct _profile
{
    double calls;           /*!< Number of times a timer was called, or the number
                                 of samples collected for a counter */
    double stops;           /*!< Number of times a timer was yielded */
    double accumulated;     /*!< Accumulated values for all calls/samples */
    double sum_squares;     /*!< Running sum of squares calculation for all
                                 calls/samples */
    double minimum;         /*!< Minimum value seen by the timer or counter */
    double maximum;         /*!< Maximum value seen by the timer or counter */
    apex_profile_type type; /*!< Whether this is a timer or a counter */
    double papi_metrics[8]; /*!< Array of accumulated PAPI hardware metrics */
    size_t allocations;     /*!< total calls to [m/c/re]alloc and related */
    size_t frees;           /*!< total calls to free and related (realloc) */
    size_t bytes_allocated; /*!< total bytes allocated in this task */
    size_t bytes_freed;     /*!< total bytes freed in this task */
    int times_reset;        /*!< How many times was this timer reset */
} apex_profile;

/** Rather than use void pointers everywhere, be explicit about
 * what the functions are expecting.
 */
typedef int (*apex_policy_function)(apex_context const context);

/**
 *  A handle to a tuning session.
 */
typedef uint32_t apex_tuning_session_handle;

/** A null pointer representing an APEX function address.
 * Used when a null APEX function address is to be passed in to
 * any apex functions to represent "all functions".
 */
#define APEX_NULL_FUNCTION_ADDRESS 0L // for comparisons
//#define APEX_NULL_FUNCTION_ADDRESS (apex_function_address)(nullptr) // for comparisons

/**
 * Special profile counter for derived idle time
 **/
#define APEX_IDLE_TIME "APEX Idle"
/**
 * Special profile counter for derived non-idle time
 **/
#define APEX_NON_IDLE_TIME "APEX Non-Idle"
/**
 * Special profile counter for derived idle rate
 **/
#define APEX_IDLE_RATE "APEX Idle Rate"
/**
 * Default OTF2 trace path
 **/
#define APEX_DEFAULT_OTF2_ARCHIVE_PATH "OTF2_archive"
/**
 * Default OTF2 trace name
 **/
#define APEX_DEFAULT_OTF2_ARCHIVE_NAME "APEX"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

inline unsigned int sc_nprocessors_onln()
{
#if !defined(_MSC_VER)
    return sysconf( _SC_NPROCESSORS_ONLN );
#else
    return 1;
#endif
}

/**
 * for each of these macros, there are 5 values.
 *  - The environment variable
 *  - The apex_option member variable name
 *  - The type
 *  - The initial/default value
 */
#define FOREACH_APEX_OPTION(macro) \
    macro (APEX_DISABLE, disable, bool, false) \
    macro (APEX_SUSPEND, suspend, bool, false) \
    macro (APEX_PAPI_SUSPEND, papi_suspend, bool, false) \
    macro (APEX_PROCESS_ASYNC_STATE, process_async_state, bool, true) \
    macro (APEX_UNTIED_TIMERS, untied_timers, bool, false) \
    macro (APEX_TAU, use_tau, bool, false) \
    macro (APEX_OTF2, use_otf2, bool, false) \
    macro (APEX_OTF2_COLLECTIVE_SIZE, otf2_collective_size, int, 1) \
    macro (APEX_TRACE_EVENT, use_trace_event, bool, false) \
    macro (APEX_POLICY, use_policy, bool, true) \
    macro (APEX_MEASURE_CONCURRENCY, use_concurrency, int, 0) \
    macro (APEX_MEASURE_CONCURRENCY_PERIOD, concurrency_period, int, 1000000) \
    macro (APEX_SCREEN_OUTPUT, use_screen_output, bool, false) \
    macro (APEX_VERBOSE, use_verbose, bool, false) \
    macro (APEX_PROFILE_OUTPUT, use_profile_output, int, false) \
    macro (APEX_CSV_OUTPUT, use_csv_output, int, false) \
    macro (APEX_TASKGRAPH_OUTPUT, use_taskgraph_output, bool, false) \
    macro (APEX_TASKTREE_OUTPUT, use_tasktree_output, bool, false) \
    macro (APEX_SOURCE_LOCATION, use_source_location, bool, false) \
    macro (APEX_PROC_CPUINFO, use_proc_cpuinfo, bool, false) \
    macro (APEX_PROC_LOADAVG, use_proc_loadavg, bool, true) \
    macro (APEX_PROC_MEMINFO, use_proc_meminfo, bool, false) \
    macro (APEX_PROC_NET_DEV, use_proc_net_dev, bool, false) \
    macro (APEX_PROC_SELF_STATUS, use_proc_self_status, bool, true) \
    macro (APEX_PROC_SELF_IO, use_proc_self_io, bool, false) \
    macro (APEX_PROC_STAT, use_proc_stat, bool, true) \
    macro (APEX_LM_SENSORS, use_lm_sensors, bool, false) \
    macro (APEX_PROC_STAT_DETAILS, use_proc_stat_details, bool, false) \
    macro (APEX_PROC_PERIOD, proc_period, int, 1000000) \
    macro (APEX_THROTTLE_CONCURRENCY, throttle_concurrency, \
        bool, false) \
    macro (APEX_THROTTLING_MAX_THREADS, throttling_max_threads, \
        int, sc_nprocessors_onln()) \
    macro (APEX_THROTTLING_MIN_THREADS, throttling_min_threads, \
        int, 1) \
    macro (APEX_THROTTLE_ENERGY, throttle_energy, bool, false) \
    macro (APEX_THROTTLE_ENERGY_PERIOD, throttle_energy_period, \
        int, 1000000) \
    macro (APEX_THROTTLING_MAX_WATTS, throttling_max_watts, int, 300) \
    macro (APEX_THROTTLING_MIN_WATTS, throttling_min_watts, int, 150) \
    macro (APEX_PTHREAD_WRAPPER_STACK_SIZE, pthread_wrapper_stack_size, \
        int, 0) \
    macro (APEX_ENABLE_OMPT, use_ompt, bool, false) \
    macro (APEX_OMPT_REQUIRED_EVENTS_ONLY, ompt_required_events_only, \
        bool, false) \
    macro (APEX_OMPT_HIGH_OVERHEAD_EVENTS, ompt_high_overhead_events, \
        bool, false) \
    macro (APEX_PIN_APEX_THREADS, pin_apex_threads, bool, true) \
    macro (APEX_TRACK_MEMORY, track_memory, bool, false) \
    macro (APEX_TASK_SCATTERPLOT, task_scatterplot, bool, false) \
    macro (APEX_TIME_TOP_LEVEL_OS_THREADS, top_level_os_threads, bool, false) \
    macro (APEX_POLICY_DRAIN_TIMEOUT, policy_drain_timeout, int, 1000) \
    macro (APEX_ENABLE_CUDA, use_cuda, int, false) \
    macro (APEX_CUDA_COUNTERS, use_cuda_counters, int, false) \
    macro (APEX_CUDA_KERNEL_DETAILS, use_cuda_kernel_details, int, false) \
    macro (APEX_CUDA_RUNTIME_API, use_cuda_runtime_api, bool, true) \
    macro (APEX_CUDA_DRIVER_API, use_cuda_driver_api, bool, false) \
    macro (APEX_CUDA_SYNC_ACTIVITY, use_cuda_sync_activity, bool, true) \
    macro (APEX_CUDA_MEMORY_ACTIVITY, use_cuda_memory_activity, bool, true) \
    macro (APEX_CUDA_KERNEL_ACTIVITY, use_cuda_kernel_activity, bool, true) \
    macro (APEX_ENABLE_HIP, use_hip, int, false) \
    macro (APEX_HIP_COUNTERS, use_hip_counters, int, false) \
    macro (APEX_HIP_KERNEL_DETAILS, use_hip_kernel_details, int, false) \
    macro (APEX_HIP_RUNTIME_API, use_hip_runtime_api, bool, true) \
    macro (APEX_HIP_KFD_API, use_hip_kfd_api, bool, false) \
    macro (APEX_HIP_SYNC_ACTIVITY, use_hip_sync_activity, bool, true) \
    macro (APEX_HIP_MEMORY_ACTIVITY, use_hip_memory_activity, bool, true) \
    macro (APEX_HIP_KERNEL_ACTIVITY, use_hip_kernel_activity, bool, true) \
    macro (APEX_HIP_PROFILER, use_hip_profiler, bool, false) \
    macro (APEX_MONITOR_GPU, monitor_gpu, bool, false) \
    macro (APEX_JUPYTER_SUPPORT, use_jupyter_support, int, false) \
    macro (APEX_KOKKOS_VERBOSE, use_kokkos_verbose, bool, false) \
    macro (APEX_KOKKOS_COUNTERS, use_kokkos_counters, bool, false) \
    macro (APEX_KOKKOS_TUNING, use_kokkos_tuning, bool, true) \
    macro (APEX_KOKKOS_PROFILING_FENCES, use_kokkos_profiling_fences, bool, false) \
    macro (APEX_START_DELAY_SECONDS, start_delay_seconds, int, 0) \
    macro (APEX_MAX_DURATION_SECONDS, max_duration_seconds, int, 0) \
    macro (APEX_USE_SHORT_TASK_NAMES, use_short_task_names, bool, false) \

#define FOREACH_APEX_FLOAT_OPTION(macro) \
    macro (APEX_SCATTERPLOT_FRACTION, scatterplot_fraction, double, 0.01) \

#define FOREACH_APEX_STRING_OPTION(macro) \
    macro (APEX_PAPI_METRICS, papi_metrics, char*, "") \
    macro (APEX_PAPI_COMPONENTS, papi_components, char*, "") \
    macro (APEX_PAPI_COMPONENT_METRICS, papi_component_metrics, char*, "") \
    macro (APEX_PLUGINS, plugins, char*, "") \
    macro (APEX_PLUGINS_PATH, plugins_path, char*, "./") \
    macro (APEX_OUTPUT_FILE_PATH, output_file_path, char*, "./") \
    macro (APEX_OTF2_ARCHIVE_PATH, otf2_archive_path, char*, \
        APEX_DEFAULT_OTF2_ARCHIVE_PATH) \
    macro (APEX_OTF2_ARCHIVE_NAME, otf2_archive_name, char*, \
        APEX_DEFAULT_OTF2_ARCHIVE_NAME) \
    macro (APEX_EVENT_FILTER_FILE, task_event_filter_file, char*, "") \
    macro (APEX_KOKKOS_TUNING_CACHE, kokkos_tuning_cache, char*, "") \
    macro (APEX_ROCPROF_METRICS, rocprof_metrics, char*, "MemUnitBusy,MemUnitStalled,VALUUtilization,VALUBusy,SALUBusy,L2CacheHit,WriteUnitStalled,ALUStalledByLDS,LDSBankConflict")

#if defined(_WIN32) || defined(_WIN64)
#  define APEX_NATIVE_TLS __declspec(thread)
#else
#  define APEX_NATIVE_TLS thread_local
#endif

// This macro is to prevent compiler warnings for stub implementations,
// in particular empty virtual implementations.
#define APEX_UNUSED(expr) do { (void)(expr); } while (0)

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

