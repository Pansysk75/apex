/*
 * Copyright (c) 2014-2021 Kevin Huck
 * Copyright (c) 2014-2021 University of Oregon
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "task_identifier.hpp"
#include "thread_instance.hpp"
#include "apex_api.hpp"
#include "utils.hpp"
#include <mutex>
#include <string>
#include <utility>
#include <regex>

#if APEX_HAVE_BFD
#include "address_resolution.hpp"
#ifdef __MIC__
#include <boost/regex.hpp>
#define REGEX_NAMESPACE boost
#else
#include <regex>
#define REGEX_NAMESPACE std
#endif
#endif

namespace apex {

// only let one thread at a time resolve the name of this task
std::mutex bfd_mutex;

    task_identifier::apex_name_map::apex_name_map() :
        tid(thread_instance::get_id()) {}
    task_identifier::apex_name_map::~apex_name_map(void) {
        if (tid == 0) {
            finalize();
        }
        /* We have a small leak of the task_identifier objects in this map.
           unfortunately, we can't clean up the map because some profile
           objects will refer to the pointers in the map, and they won't
           be resolved correctly at exit. The leak only becomes a leak when
           the program exits and the pointers aren't needed any more. */
    }

    task_identifier::apex_addr_map::apex_addr_map() :
        tid(thread_instance::get_id()) {}
    task_identifier::apex_addr_map::~apex_addr_map(void) {
        if (tid == 0) {
            finalize();
        }
        /* We have a small leak of the task_identifier objects in this map.
           unfortunately, we can't clean up the map because some profile
           objects will refer to the pointers in the map, and they won't
           be resolved correctly at exit. The leak only becomes a leak when
           the program exits and the pointers aren't needed any more. */
    }

    std::string task_identifier::get_tree_name() {
        std::string shorter(get_name(true));
        if (!apex_options::use_short_task_names()) {
            return shorter;
        }
        /* trim the kokkos namespaces */
        shorter = std::regex_replace(shorter, std::regex("Kokkos::Experimental::Impl::"), "kok::");
        shorter = std::regex_replace(shorter, std::regex("Kokkos::Experimental::"), "kok::");
        shorter = std::regex_replace(shorter, std::regex("Kokkos::Impl::"), "kok::");
        shorter = std::regex_replace(shorter, std::regex("Kokkos::RangePolicy"), "kok::range");
        shorter = std::regex_replace(shorter, std::regex("Kokkos::MDRangePolicy"), "kok::md");
        shorter = std::regex_replace(shorter, std::regex("Kokkos::TeamPolicy"), "kok::team");
        shorter = std::regex_replace(shorter, std::regex("ParallelFor"), "p_for");
        shorter = std::regex_replace(shorter, std::regex("ParallelReduce"), "p_red");
        shorter = std::regex_replace(shorter, std::regex("ParallelScan"), "p_scan");
        shorter = std::regex_replace(shorter, std::regex("_parallel_launch_local_memory"), "_local");
        shorter = std::regex_replace(shorter, std::regex("_parallel_launch_constant_memory"), "_const");
        return shorter;
        /* trim the arguments? */
        size_t trim_at = shorter.rfind("(");
        if (trim_at != std::string::npos) {
            shorter = shorter.substr(0, trim_at);
        }
        /* Strip the template info? */
        trim_at = shorter.find("<");
        size_t addr = shorter.find("addr", 0);
        if (trim_at != std::string::npos && addr == std::string::npos) {
            shorter = shorter.substr(0, trim_at);
        }
        return shorter;

        //std::cout << get_name(true) << " ==> " << shorter << std::endl;
        // IF this is an openmp loop from a Kokkos kernel, strip the garbage
        /*
        size_t omp = shorter.find("OpenMP ", 0);
        size_t colon = shorter.find(": ");
        if (omp != std::string::npos && colon != std::string::npos) {
            size_t kokkos = shorter.find("Kokkos::parallel");
            if (kokkos != std::string::npos) {
                shorter = shorter.substr(0, colon);
            }
        }
        return shorter;
        */
    }

    std::string task_identifier::get_short_name() {
        std::string shorter(get_name(true));
        size_t trim_at = shorter.find("(");
        if (trim_at != std::string::npos) {
            shorter = shorter.substr(0, trim_at);
        }
        trim_at = shorter.find("<");
        size_t addr = shorter.find("addr", 0);
        if (trim_at != std::string::npos && addr == std::string::npos) {
            shorter = shorter.substr(0, trim_at);
        }
        size_t maxlength = 50;
        // to keep formatting pretty, trim any long timer names
        if (shorter.size() > maxlength) {
            shorter.resize(maxlength-3);
            shorter.resize(maxlength, '.');
        }
        return shorter;
    }

    std::string task_identifier::get_name(bool resolve) {
        if (!has_name && resolve) {
            if (_resolved_name == "" && address != APEX_NULL_FUNCTION_ADDRESS) {
                // only let one thread update the name of this task_identifier
                std::unique_lock<std::mutex> queue_lock(bfd_mutex);
                // check again, another thread may have resolved it.
                if (_resolved_name == "") {
                    //_resolved_name = lookup_address((uintptr_t)address, false);
                    _resolved_name = thread_instance::instance().map_addr_to_name(address);
                    _resolved_name.assign((demangle(_resolved_name)));
                    DEBUG_PRINT("Resolved %p to %s\n", (void*)address, _resolved_name.c_str());
                }
            }
            std::string retval(_resolved_name);
            return retval;
        } else {
            std::string retval(name);
            if (resolve) {
#ifdef APEX_HAVE_BFD
                const std::string addrstr("UNRESOLVED ADDR");
                if (retval.find(addrstr) != std::string::npos) {
                    REGEX_NAMESPACE::regex rx (".*UNRESOLVED ADDR (.*)");
                    if (REGEX_NAMESPACE::regex_match (retval,rx)) {
                        const REGEX_NAMESPACE::regex separator(" ADDR ");
                        REGEX_NAMESPACE::sregex_token_iterator
                            token(retval.begin(), retval.end(), separator, -1);
                        *token++; // ignore
                        std::string addr_str = *token++;
                        void* addr_addr;
                        sscanf(addr_str.c_str(), "%p", &addr_addr);
                        std::string * tmp = lookup_address((uintptr_t)addr_addr, true);
                        /* OK, this looks weird. But...binutils will sometimes resolve
                         * different OpenMP outlined regions to the same function,file,line.
                         * If we don't retain the address, we won't get unique timer names.
                         */
                        std::string s("OpenMP ");
                        if (retval.rfind(s, 0) == 0) {
                            // found an OpenMP timer, so keep the address
                            // ...unless it's already there
                            if (tmp->find(addr_str) != std::string::npos) {
                                REGEX_NAMESPACE::regex old_address("UNRESOLVED ADDR " + addr_str);
                                retval = REGEX_NAMESPACE::regex_replace(retval, old_address,
                                        (*tmp));
                            } else {
                                REGEX_NAMESPACE::regex old_address("UNRESOLVED ADDR");
                                retval = REGEX_NAMESPACE::regex_replace(retval, old_address,
                                        (*tmp));
                            }
                        } else {
                            REGEX_NAMESPACE::regex old_address("UNRESOLVED ADDR " + addr_str);
                            retval = REGEX_NAMESPACE::regex_replace(retval, old_address,
                                    (*tmp));
                        }
                    }
                }
#endif
                const std::string cudastr("GPU: ");
                const std::string kernel("cudaLaunchKernel: ");
                const std::string kernel2("cuLaunchKernel: ");
                if (retval.find(cudastr) != std::string::npos) {
                    std::stringstream ss;
                    std::string tmp = retval.substr(cudastr.size(),
                            retval.size() - cudastr.size());
                    std::string demangled = demangle(tmp);
                    ss << cudastr << demangled;
                    retval.assign(ss.str());
                    return retval;
                } else if (retval.find(kernel) != std::string::npos) {
                    std::stringstream ss;
                    std::string tmp = retval.substr(kernel.size(),
                            retval.size() - kernel.size());
                    std::string demangled = demangle(tmp);
                    ss << kernel << demangled;
                    retval.assign(ss.str());
                    return retval;
                } else if (retval.find(kernel2) != std::string::npos) {
                    std::stringstream ss;
                    std::string tmp = retval.substr(kernel2.size(),
                            retval.size() - kernel2.size());
                    std::string demangled = demangle(tmp);
                    ss << kernel2 << demangled;
                    retval.assign(ss.str());
                    return retval;
                }
            }
            return retval;
        }
    }

  /* When writing a TAU profile, get the appropriate TAU group */
  std::string task_identifier::get_group() {
    std::string name{get_name(true)};
    std::stringstream ss;
    if (name.rfind("GPU: ", 0) == 0) { ss << "TAU_GPU"; }
    else if (name.rfind("OpenMP ", 0) == 0) { ss << "TAU_OPENMP"; }
    else if (name.rfind("OpenACC ", 0) == 0) { ss << "TAU_OPENACC"; }
    else if (name.rfind("Kokkos", 0) == 0) { ss << "TAU_KOKKOS"; }
    else if (name.rfind("MPI_", 0) == 0) { ss << "MPI"; }
    else if (name.rfind("cuda", 0) == 0) { ss << "TAU_CUDA"; }
    else if (name.rfind("hip", 0) == 0) { ss << "TAU_HIP"; }
    else if (name.rfind("pthread", 0) == 0) { ss << "TAU_PTHREAD"; }
    else if (name.rfind("hpx", 0) == 0) { ss << "TAU_HPX"; }
    else { ss << "TAU_USER"; }
    std::string group{ss.str()};
    return group;
  }

  task_identifier::apex_name_map& task_identifier::get_task_id_name_map(void) {
      /* By allocating this map on the heap, it won't get destroyed at shutdown,
       * which causes a crash with Intel compilers.  Can't figure out why. */
      static APEX_NATIVE_TLS apex_name_map * task_id_name_map = new apex_name_map();
      return *task_id_name_map;
  }
  task_identifier::apex_addr_map& task_identifier::get_task_id_addr_map(void) {
      /* By allocating this map on the heap, it won't get destroyed at shutdown,
       * which causes a crash with Intel compilers.  Can't figure out why. */
      static APEX_NATIVE_TLS apex_addr_map * task_id_addr_map = new apex_addr_map();
      return *task_id_addr_map;
  }

  task_identifier * task_identifier::get_task_id (apex_function_address a) {
      auto& task_id_addr_map = get_task_id_addr_map();
      apex_addr_map::const_iterator got = task_id_addr_map.find (a);
      if ( got != task_id_addr_map.end() ) {
          return got->second;
      } else {
          task_identifier * tmp = new task_identifier(a);
          task_id_addr_map[a] = tmp;
          return tmp;
      }
  }

  task_identifier * task_identifier::get_task_id (const std::string& n) {
      auto& task_id_name_map = get_task_id_name_map();
      apex_name_map::const_iterator got = task_id_name_map.find (n);
      if ( got != task_id_name_map.end() ) {
          return got->second;
      } else {
          task_identifier * tmp = new task_identifier(n);
          task_id_name_map.insert(std::pair<std::string,task_identifier*>(n, tmp));
          return tmp;
      }
  }
}

