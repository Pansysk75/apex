/*
 * Copyright (c) 2014-2021 Kevin Huck
 * Copyright (c) 2014-2021 University of Oregon
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

/* This file contains MPI implementations of communication necessary to support
   OTF2 tracing.  For example, event unification and clock Synchronization.
*/

// only compile this file if we have HPX networking support
#if defined(APEX_HAVE_HPX_CONFIG) || defined(APEX_HAVE_HPX)
#include <hpx/config.hpp>
#include <hpx/hpx.hpp>
#endif

#if defined(HPX_HAVE_NETWORKING)

#if 0 // defined(HPX_HAVE_NETWORKING)
#include <hpx/include/lcos.hpp>
#include "otf2_listener.hpp"
#include <limits>

namespace apex {
namespace otf2 {
    uint64_t get_remote_timestamp() {
        return profiler::now_ns();
    }
}
}

HPX_PLAIN_ACTION(apex::otf2::get_remote_timestamp, apex_otf2_get_remote_timestamp);

namespace apex {

int64_t otf2_listener::synchronizeClocks(void) {
    int64_t offset{0};
    const int attempts{10};

    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::uint32_t this_locality = hpx::get_locality_id();
    // synchronize all ranks
    //hpx::lcos::barrier barrier("apex_barrier_sync_1", num_localities, this_locality);

    if (this_locality == 0) {
        /* If rank 0, do nothing. */
        offset = 0;
    } else {
        uint64_t min_latency{ULLONG_MAX};
        uint64_t mytime{0};
        uint64_t ref_ts{0};
        for (int i = 0 ; i < attempts ; i++) {
            // take a timestamp now!
            uint64_t before = get_time();
            apex_otf2_get_remote_timestamp act;
            uint64_t ref_time = act(hpx::naming:: get_id_from_locality_id(0));
            uint64_t after = get_time();
            uint64_t latency = (after - before) / 2;
            if (latency < min_latency) {
                min_latency = latency;
                ref_ts = ref_time;
                mytime = after - latency;
            }
        }
        // our offset is the reference time minus our timestamp between messages.
        offset = ref_ts - mytime;
    }
    // synchronize all ranks again
    //hpx::lcos::barrier barrier2("apex_barrier_sync_2", num_localities, this_locality);

    return offset;
}

int otf2_listener::getCommRank() {
    static int rank{-1};
    if (rank == -1) {
        rank = hpx::get_locality_id();
    }
    return rank;
}

int otf2_listener::getCommSize() {
    static int size{-1};
    if (size == -1) {
        size = hpx::get_num_localities(hpx::launch::sync);
    }
    return size;
}

} // namespace apex

#else // the above is disabled until it can work reliably

#include "otf2_listener.hpp"

namespace apex {

int64_t otf2_listener::synchronizeClocks(void) {
    return 0;
}

int otf2_listener::getCommRank() {
    return apex::instance()->get_node_id();
}

int otf2_listener::getCommSize() {
    return apex::instance()->get_num_ranks();
}

} // namespace apex

#endif // #if 0

#endif // #if !defined(HPX_HAVE_NETWORKING) && !defined(APEX_WITH_MPI)
