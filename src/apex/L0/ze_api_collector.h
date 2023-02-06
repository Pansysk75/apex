//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>

#include <level_zero/zet_api.h>

#include <L0/utils.h>
#include <L0/ze_utils.h>

struct ZeFunction {
    uint64_t total_time;
    uint64_t min_time;
    uint64_t max_time;
    uint64_t call_count;

    bool operator>(const ZeFunction& r) const {
        if (total_time != r.total_time) {
            return total_time > r.total_time;
        }
        return call_count > r.call_count;
    }

    bool operator!=(const ZeFunction& r) const {
        if (total_time == r.total_time) {
            return call_count != r.call_count;
        }
        return true;
    }
};


typedef void (*OnFunctionFinishCallback)(
        void* data, const std::string& name,
        uint64_t started, uint64_t ended);


using ZeFunctionInfoMap = std::map<std::string, ZeFunction>;

static void SetTracingFunctions(zel_tracer_handle_t tracer);

class ZeApiCollector {
    public: // User Interface
        static ZeApiCollector* Create(
                ze_driver_handle_t driver,
                OnFunctionFinishCallback callback = nullptr,
                void* callback_data = nullptr )
        {

            PTI_ASSERT(driver != nullptr);

            ze_context_handle_t context = utils::ze::GetContext(driver);
            PTI_ASSERT(context != nullptr);

            ZeApiCollector* collector = new ZeApiCollector(context, callback, callback_data);
            PTI_ASSERT(collector != nullptr);

            ze_result_t status = ZE_RESULT_SUCCESS;
            zel_tracer_desc_t tracer_desc = {
                ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr, collector};
            zel_tracer_handle_t tracer = nullptr;

            status = zelTracerCreate(&tracer_desc, &tracer);
            if (status != ZE_RESULT_SUCCESS || tracer == nullptr) {
                std::cerr << "[WARNING] Unable to create L0 tracer" << std::endl;
                delete collector;
                return nullptr;
            }

            collector->tracer_ = tracer;
            SetTracingFunctions(tracer);

            status = zelTracerSetEnabled(tracer, true);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);

            return collector;
        }

        void DisableTracing() {
            PTI_ASSERT(tracer_ != nullptr);
            ze_result_t status = ZE_RESULT_SUCCESS;
            status = zelTracerSetEnabled(tracer_, false);
            PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        }

        const ZeFunctionInfoMap& GetFunctionInfoMap() const {
            return function_info_map_;
        }

        static void PrintFunctionsTable(const ZeFunctionInfoMap& function_info_map) {
            std::set< std::pair<std::string, ZeFunction>,
                utils::Comparator > sorted_list(
                        function_info_map.begin(), function_info_map.end());

            uint64_t total_duration = 0;
            size_t max_name_length = kFunctionLength;
            for (auto& value : sorted_list) {
                total_duration += value.second.total_time;
                if (value.first.size() > max_name_length) {
                    max_name_length = value.first.size();
                }
            }

            if (total_duration == 0) {
                return;
            }

            std::cerr << std::setw(max_name_length) << "Function" << "," <<
                std::setw(kCallsLength) << "Calls" << "," <<
                std::setw(kTimeLength) << "Time (ns)" << "," <<
                std::setw(kPercentLength) << "Time (%)" << "," <<
                std::setw(kTimeLength) << "Average (ns)" << "," <<
                std::setw(kTimeLength) << "Min (ns)" << "," <<
                std::setw(kTimeLength) << "Max (ns)" << std::endl;

            for (auto& value : sorted_list) {
                const std::string& function = value.first;
                uint64_t call_count = value.second.call_count;
                uint64_t duration = value.second.total_time;
                uint64_t avg_duration = duration / call_count;
                uint64_t min_duration = value.second.min_time;
                uint64_t max_duration = value.second.max_time;
                float percent_duration = 100.0f * duration / total_duration;
                std::cerr << std::setw(max_name_length) << function << "," <<
                    std::setw(kCallsLength) << call_count << "," <<
                    std::setw(kTimeLength) << duration << "," <<
                    std::setw(kPercentLength) << std::setprecision(2) <<
                    std::fixed << percent_duration << "," <<
                    std::setw(kTimeLength) << avg_duration << "," <<
                    std::setw(kTimeLength) << min_duration << "," <<
                    std::setw(kTimeLength) << max_duration << std::endl;
            }
        }

        ~ZeApiCollector() {
            if (tracer_ != nullptr) {
                ze_result_t status = zelTracerDestroy(tracer_);
                PTI_ASSERT(status == ZE_RESULT_SUCCESS);
            }
        }

        uint64_t GetTimestamp() const {
            std::chrono::duration<uint64_t, std::nano> timestamp =
                std::chrono::steady_clock::now() - base_time_;
            return timestamp.count();
        }

        void AddFunctionTime(const std::string& name, uint64_t time) {
            const std::lock_guard<std::mutex> lock(lock_);
            if (function_info_map_.count(name) == 0) {
                function_info_map_[name] = {time, time, time, 1};
            } else {
                ZeFunction& function = function_info_map_[name];
                function.total_time += time;
                if (time < function.min_time) {
                    function.min_time = time;
                }
                if (time > function.max_time) {
                    function.max_time = time;
                }
                ++function.call_count;
            }
        }

    private: // Implementation Details
        ZeApiCollector(ze_context_handle_t context,
                OnFunctionFinishCallback callback,
                void* callback_data)
            : context_(context),
            callback_(callback),
            callback_data_(callback_data) {
                PTI_ASSERT(context_ != nullptr);
            }

#include <L0/ze_api_callbacks.h>

    private: // Data
        ze_context_handle_t context_ = nullptr;
        zel_tracer_handle_t tracer_ = nullptr;
        std::chrono::time_point<std::chrono::steady_clock> base_time_;

        ZeFunctionInfoMap function_info_map_;
        std::mutex lock_;

        static const uint32_t kFunctionLength = 10;
        static const uint32_t kCallsLength = 12;
        static const uint32_t kTimeLength = 20;
        static const uint32_t kPercentLength = 10;

        OnFunctionFinishCallback callback_ = nullptr;
        void* callback_data_ = nullptr;
};

