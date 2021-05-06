#pragma once

#include <json.hpp>

using nlohmann::json;

#ifndef LIKWID_PERFMON

void likwid_prepare(json &results, bool accumulate) {}

void likwid_before_it() {}

void likwid_after_it(json &rowResult) {}

void likwid_finalize(json &results) {}

#else

#include <likwid.h>

#include <sstream>
#include <string>
#include <vector>

#define MAX_NUM_EVENTS 10

struct RegionData {
    std::vector<double> events;
    double time = 0;
    int count = 0;

    void get(const std::string &region) {
        int nevents = MAX_NUM_EVENTS;
        double events_a[MAX_NUM_EVENTS] = {0.0};
        likwid_markerGetRegion(region.c_str(), &nevents, events_a, &time, &count);
        events.assign(events_a, events_a + nevents);
        // printf("Region '%s' was called %d times, measured %d events, total measurement time is %f\n", region.c_str(), count, nevents, time);
    }

    void diff_update(const RegionData &other) {
        //OGDF_ASSERT(events.size() == other.events.size());
        time = other.time - time;
        count = other.count - count;
        for (int i = 0; i < events.size(); i++) {
            events[i] = other.events[i] - events[i];
        }
    }

    json toJson() const {
        json ret;
        ret["nevents"] = events.size();
        ret["time"] = time;
        ret["count"] = count;
        ret["data"] = events;
        return ret;
    }

    json toJson(std::vector<std::string> &event_names) const {
        //OGDF_ASSERT(event_names.size() == events.size());
        json ret;
        ret["nevents"] = events.size();
        ret["time"] = time;
        ret["count"] = count;
        json &data = ret["data"] = json::object();
        for (int i = 0; i < events.size(); i++) {
            data[event_names[i]] = events[i];
        }
        return ret;
    }
};

std::vector<std::string> regions{
        "label",
        "label_process_neigh",
        "find_tp",
        "find_tp_cnode",
        "update_tp",
        "update_tp_central",
        "update_tp_pnode",
        "update_tp_cnode"
};
std::unordered_map<std::string, RegionData> likwidRegions;
bool likwidAccumulate;


void likwid_prepare(json &results, bool accumulate) {
    results["likwid_perfmon"] = true;
    results["likwid_accumulate"] = likwidAccumulate = accumulate;
    likwid_markerInit();
    likwid_markerThreadInit();
    timer_init();
    for (std::string &region:regions) {
        likwidRegions[region].get(region);
        likwid_markerRegisterRegion(region.c_str());
    }
    std::vector<std::string> likwidEvents;
    int nevents = perfmon_getNumberOfEvents(perfmon_getIdOfActiveGroup());
    likwidEvents.reserve(nevents);
    for (int i = 0; i < nevents; i++) {
        likwidEvents.emplace_back(perfmon_getEventName(perfmon_getIdOfActiveGroup(), i));
        // std::cout << "perfmon_getEventName " << i << ":" << perfmon_getEventName(perfmon_getIdOfActiveGroup(), i) << std::endl;
        // std::cout << "perfmon_getCounterName " << i << ":" << perfmon_getCounterName(perfmon_getIdOfActiveGroup(), i) << std::endl;
    }
    // for (int i = 0; i < perfmon_getNumberOfMetrics(perfmon_getIdOfActiveGroup()); i++) {
    //     std::cout << "perfmon_getMetricName " << i << ":" << perfmon_getMetricName(perfmon_getIdOfActiveGroup(), i) << std::endl;
    // }
    results["likwid_meta"] = {
            {"cpu_freq",    timer_getCpuClock()},
            {"group_id",    perfmon_getIdOfActiveGroup()},
            {"ngroups",     perfmon_getNumberOfGroups()},
            {"nthreads",    perfmon_getNumberOfThreads()},
            {"nregions",    perfmon_getNumberOfRegions()},
            {"group_name",  perfmon_getGroupName(perfmon_getIdOfActiveGroup())},
            // {"nmetrics",   perfmon_getNumberOfMetrics(perfmon_getIdOfActiveGroup())},
            // {"GroupInfoShort" , perfmon_getGroupInfoShort(perfmon_getIdOfActiveGroup()) },
            // {"GroupInfoLong" , perfmon_getGroupInfoLong(perfmon_getIdOfActiveGroup()) },
            {"nevents",     nevents},
            {"event_names", likwidEvents},
    };
}

void likwid_before_it() {
    if (!likwidAccumulate) {
        for (std::string &region : regions) {
            likwid_markerResetRegion(region.c_str());
        }
    }
}

void likwid_after_it(json &rowResult) {
    json &regionsJson = rowResult["likwid"] = json::object();
    for (std::string &region : regions) {
        RegionData &data = likwidRegions[region];
        if (likwidAccumulate) {
            RegionData old_data = likwidRegions[region];
            data.get(region);
            old_data.diff_update(data);
            regionsJson[region] = old_data.toJson();
        } else {
            data.get(region);
            regionsJson[region] = data.toJson();
        }
    }
}

void likwid_finalize(json &results) {
    if (likwidAccumulate) {
        json &regionsJson = results["likwid"] = json::object();
        for (std::string &region : regions) {
            regionsJson[region] = likwidRegions[region].toJson();
        }
    }
    timer_finalize();
    likwid_markerClose();
}

#endif