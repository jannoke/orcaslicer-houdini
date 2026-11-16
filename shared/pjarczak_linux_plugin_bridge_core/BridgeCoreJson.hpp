#pragma once

#include <nlohmann/json.hpp>
#include "../../src/slic3r/Utils/bambu_networking.hpp"

namespace Slic3r::PJarczakLinuxBridge::JsonBridge {

inline nlohmann::json to_json(const BBL::PrintParams& p)
{
    return {
        {"dev_id", p.dev_id},
        {"task_name", p.task_name},
        {"project_name", p.project_name},
        {"preset_name", p.preset_name},
        {"filename", p.filename},
        {"config_filename", p.config_filename},
        {"plate_index", p.plate_index},
        {"ftp_folder", p.ftp_folder},
        {"ftp_file", p.ftp_file},
        {"ftp_file_md5", p.ftp_file_md5},
        {"nozzle_mapping", p.nozzle_mapping},
        {"ams_mapping", p.ams_mapping},
        {"ams_mapping2", p.ams_mapping2},
        {"ams_mapping_info", p.ams_mapping_info},
        {"nozzles_info", p.nozzles_info},
        {"connection_type", p.connection_type},
        {"comments", p.comments},
        {"origin_profile_id", p.origin_profile_id},
        {"stl_design_id", p.stl_design_id},
        {"origin_model_id", p.origin_model_id},
        {"print_type", p.print_type},
        {"dst_file", p.dst_file},
        {"dev_name", p.dev_name},
        {"dev_ip", p.dev_ip},
        {"use_ssl_for_ftp", p.use_ssl_for_ftp},
        {"use_ssl_for_mqtt", p.use_ssl_for_mqtt},
        {"username", p.username},
        {"password", p.password},
        {"task_bed_leveling", p.task_bed_leveling},
        {"task_flow_cali", p.task_flow_cali},
        {"task_vibration_cali", p.task_vibration_cali},
        {"task_layer_inspect", p.task_layer_inspect},
        {"task_record_timelapse", p.task_record_timelapse},
        {"task_timelapse_use_internal", p.task_timelapse_use_internal},
        {"task_use_ams", p.task_use_ams},
        {"task_bed_type", p.task_bed_type},
        {"extra_options", p.extra_options},
        {"auto_bed_leveling", p.auto_bed_leveling},
        {"auto_flow_cali", p.auto_flow_cali},
        {"auto_offset_cali", p.auto_offset_cali},
        {"extruder_cali_manual_mode", p.extruder_cali_manual_mode},
        {"task_ext_change_assist", p.task_ext_change_assist},
        {"try_emmc_print", p.try_emmc_print},
        {"svc_context", p.svc_context},
        {"slicer_uid", p.slicer_uid},
        {"queue_plate_id", p.queue_plate_id}
    };
}

inline BBL::PrintParams print_params_from_json(const nlohmann::json& j)
{
    BBL::PrintParams p{};
    p.dev_id = j.value("dev_id", std::string());
    p.task_name = j.value("task_name", std::string());
    p.project_name = j.value("project_name", std::string());
    p.preset_name = j.value("preset_name", std::string());
    p.filename = j.value("filename", std::string());
    p.config_filename = j.value("config_filename", std::string());
    p.plate_index = j.value("plate_index", 0);
    p.ftp_folder = j.value("ftp_folder", std::string());
    p.ftp_file = j.value("ftp_file", std::string());
    p.ftp_file_md5 = j.value("ftp_file_md5", std::string());
    p.nozzle_mapping = j.value("nozzle_mapping", std::string());
    p.ams_mapping = j.value("ams_mapping", std::string());
    p.ams_mapping2 = j.value("ams_mapping2", std::string());
    p.ams_mapping_info = j.value("ams_mapping_info", std::string());
    p.nozzles_info = j.value("nozzles_info", std::string());
    p.connection_type = j.value("connection_type", std::string());
    p.comments = j.value("comments", std::string());
    p.origin_profile_id = j.value("origin_profile_id", 0);
    p.stl_design_id = j.value("stl_design_id", 0);
    p.origin_model_id = j.value("origin_model_id", std::string());
    p.print_type = j.value("print_type", std::string());
    p.dst_file = j.value("dst_file", std::string());
    p.dev_name = j.value("dev_name", std::string());
    p.dev_ip = j.value("dev_ip", std::string());
    p.use_ssl_for_ftp = j.value("use_ssl_for_ftp", false);
    p.use_ssl_for_mqtt = j.value("use_ssl_for_mqtt", false);
    p.username = j.value("username", std::string());
    p.password = j.value("password", std::string());
    p.task_bed_leveling = j.value("task_bed_leveling", false);
    p.task_flow_cali = j.value("task_flow_cali", false);
    p.task_vibration_cali = j.value("task_vibration_cali", false);
    p.task_layer_inspect = j.value("task_layer_inspect", false);
    p.task_record_timelapse = j.value("task_record_timelapse", false);
    p.task_timelapse_use_internal = j.value("task_timelapse_use_internal", false);
    p.task_use_ams = j.value("task_use_ams", false);
    p.task_bed_type = j.value("task_bed_type", std::string());
    p.extra_options = j.value("extra_options", std::string());
    p.auto_bed_leveling = j.value("auto_bed_leveling", 0);
    p.auto_flow_cali = j.value("auto_flow_cali", 0);
    p.auto_offset_cali = j.value("auto_offset_cali", 0);
    p.extruder_cali_manual_mode = j.value("extruder_cali_manual_mode", -1);
    p.task_ext_change_assist = j.value("task_ext_change_assist", false);
    p.try_emmc_print = j.value("try_emmc_print", false);
    p.svc_context = j.value("svc_context", std::string());
    p.slicer_uid = j.value("slicer_uid", std::string());
    p.queue_plate_id = j.value("queue_plate_id", std::string());
    return p;
}

inline nlohmann::json to_json(const BBL::PublishParams& p)
{
    return {
        {"project_name", p.project_name},
        {"project_3mf_file", p.project_3mf_file},
        {"preset_name", p.preset_name},
        {"project_model_id", p.project_model_id},
        {"design_id", p.design_id},
        {"config_filename", p.config_filename}
    };
}

inline BBL::PublishParams publish_params_from_json(const nlohmann::json& j)
{
    BBL::PublishParams p{};
    p.project_name = j.value("project_name", std::string());
    p.project_3mf_file = j.value("project_3mf_file", std::string());
    p.preset_name = j.value("preset_name", std::string());
    p.project_model_id = j.value("project_model_id", std::string());
    p.design_id = j.value("design_id", std::string());
    p.config_filename = j.value("config_filename", std::string());
    return p;
}


inline nlohmann::json to_json(const BBL::FilamentQueryParams& p)
{
    return {{"category", p.category}, {"status", p.status}, {"spool_id", p.spool_id}, {"rfid", p.rfid}, {"offset", p.offset}, {"limit", p.limit}};
}

inline BBL::FilamentQueryParams filament_query_params_from_json(const nlohmann::json& j)
{
    BBL::FilamentQueryParams p{};
    p.category = j.value("category", std::string());
    p.status = j.value("status", std::string());
    p.spool_id = j.value("spool_id", std::string());
    p.rfid = j.value("rfid", std::string());
    p.offset = j.value("offset", 0);
    p.limit = j.value("limit", 20);
    return p;
}

inline nlohmann::json to_json(const BBL::FilamentDeleteParams& p)
{
    return {{"ids", p.ids}, {"rfids", p.rfids}};
}

inline BBL::FilamentDeleteParams filament_delete_params_from_json(const nlohmann::json& j)
{
    BBL::FilamentDeleteParams p{};
    p.ids = j.value("ids", std::vector<std::string>());
    p.rfids = j.value("rfids", std::vector<std::string>());
    return p;
}

inline nlohmann::json to_json(const BBL::AmsSyncParams& p)
{
    nlohmann::json items = nlohmann::json::array();
    for (const auto& i : p.items) {
        items.push_back({
            {"RFID", i.RFID}, {"filamentVendor", i.filamentVendor}, {"filamentType", i.filamentType},
            {"filamentName", i.filamentName}, {"filamentId", i.filamentId}, {"isSupport", i.isSupport},
            {"color", i.color}, {"colorType", i.colorType}, {"colors", i.colors}, {"netWeight", i.netWeight},
            {"totalNetWeight", i.totalNetWeight}, {"trayIdName", i.trayIdName}, {"note", i.note},
            {"amsSn", i.amsSn}, {"slotId", i.slotId}, {"amsId", i.amsId}, {"amsType", i.amsType},
            {"createNew", i.createNew}});
    }
    return {{"devId", p.devId}, {"items", items}};
}

inline BBL::AmsSyncParams ams_sync_params_from_json(const nlohmann::json& j)
{
    BBL::AmsSyncParams p{};
    p.devId = j.value("devId", std::string());
    for (const auto& i : j.value("items", nlohmann::json::array())) {
        BBL::AmsSyncItem item{};
        item.RFID = i.value("RFID", std::string());
        item.filamentVendor = i.value("filamentVendor", std::string());
        item.filamentType = i.value("filamentType", std::string());
        item.filamentName = i.value("filamentName", std::string());
        item.filamentId = i.value("filamentId", std::string());
        item.isSupport = i.value("isSupport", false);
        item.color = i.value("color", std::string());
        item.colorType = i.value("colorType", 0);
        item.colors = i.value("colors", std::vector<std::string>());
        item.netWeight = i.value("netWeight", 0);
        item.totalNetWeight = i.value("totalNetWeight", 0);
        item.trayIdName = i.value("trayIdName", std::string());
        item.note = i.value("note", std::string());
        item.amsSn = i.value("amsSn", std::string());
        item.slotId = i.value("slotId", std::string());
        item.amsId = i.value("amsId", 0);
        item.amsType = i.value("amsType", 0);
        item.createNew = i.value("createNew", false);
        p.items.push_back(std::move(item));
    }
    return p;
}

}
