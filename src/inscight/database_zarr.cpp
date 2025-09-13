/******************************************************************************
 *                                                                            *
 * Minimal no-op implementation for Zarr backend tracing.                     *
 * Prints in fw/bw transaction hooks to verify call paths.                    *
 *                                                                            *
 ******************************************************************************/

#include <nlohmann/json.hpp>
#include <xtensor/containers/xarray.hpp>

// factory functions to create files, groups and datasets
#include "z5/factory.hxx"
// handles for z5 filesystem objects
#include "z5/filesystem/handle.hxx"
// io for xtensor multi-arrays
#include "z5/multiarray/xtensor_access.hxx"
// attribute functionality
#include "z5/attributes.hxx"

#include "inscight/database_zarr.h"

#include <iostream>

namespace inscight
{

    void database_zarr::init()
    {
        std::cout << "[database_zarr] init()" << std::endl;

        // get handle to a File on the filesystem
        z5::filesystem::handle::File f("transaction_data.zr");
        std::cout << "[database_zarr] get handle to a File on the filesystem" << std::endl;

        // create the file in zarr format
        const bool createAsZarr = true;
        z5::createFile(f, createAsZarr);
        std::cout << "[database_zarr] create the file in zarr format" << std::endl;

        // create datasets for command and response_status
        std::vector<size_t> shape = {10000}; // Start with 10k entries, can grow dynamically
        std::vector<size_t> chunks = {1000};
        
        // Create string datasets for commands and response statuses
        commandDs = z5::createDataset(f, "commands", "S32", shape, chunks); // 32-char strings
        responseDs = z5::createDataset(f, "responses", "S64", shape, chunks); // 64-char strings
        timestampDs = z5::createDataset(f, "timestamps", "uint64", shape, chunks);
        
        std::cout << "[database_zarr] created datasets for commands, responses, and timestamps" << std::endl;
        
        entryCount = 0;
    }

    void database_zarr::gen_meta(const meta_info &info)
    {
        std::cout << "[database_zarr] meta: pid=" << info.pid
                  << " path=" << info.path
                  << " user=" << info.user
                  << " version=" << info.version
                  << " time=" << info.timestamp << std::endl;
    }

    void database_zarr::transaction_trace_fw(id_t obj, sysc_time_t st, protocol_kind proto, const char *json)
    {
        if (json && strlen(json) > 0) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                storeTransactionData(st, j, "FW");
            } catch (const std::exception& e) {
                std::cerr << "[database_zarr] Error parsing JSON in FW: " << e.what() << std::endl;
            }
        }
    }

    void database_zarr::transaction_trace_bw(id_t obj, sysc_time_t st, protocol_kind proto, const char *json)
    {
        if (json && strlen(json) > 0) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                storeTransactionData(st, j, "BW");
            } catch (const std::exception& e) {
                std::cerr << "[database_zarr] Error parsing JSON in BW: " << e.what() << std::endl;
            }
        }
    }

    void database_zarr::storeTransactionData(sysc_time_t timestamp, const nlohmann::json& j, const std::string& direction)
    {
        if (entryCount >= 10000) {
            std::cerr << "[database_zarr] Warning: Dataset full, cannot store more entries" << std::endl;
            return;
        }

        try {
            std::string command = j.contains("command") ? j["command"].get<std::string>() : "UNKNOWN";
            std::string response = j.contains("response_status") ? j["response_status"].get<std::string>() : "UNKNOWN";
            
            // Pad command and response to fit dataset string sizes
            command.resize(31, '\0'); // S32 means 32 chars including null terminator
            response.resize(63, '\0'); // S64 means 64 chars including null terminator

            // Write data to zarr datasets at current entry position
            z5::types::ShapeType offset = {entryCount};
            
            xt::xarray<char> cmdArray = xt::adapt(command.c_str(), {32});
            xt::xarray<char> respArray = xt::adapt(response.c_str(), {64});
            xt::xarray<uint64_t> tsArray = {static_cast<uint64_t>(timestamp)};
            
            z5::multiarray::writeSubarray<char>(commandDs, cmdArray, offset.begin());
            z5::multiarray::writeSubarray<char>(responseDs, respArray, offset.begin());
            z5::multiarray::writeSubarray<uint64_t>(timestampDs, tsArray, offset.begin());
            
            std::cout << "[database_zarr] " << direction << " stored entry " << entryCount 
                      << ": cmd=" << command.c_str() << " resp=" << response.c_str() 
                      << " ts=" << timestamp << std::endl;
            
            entryCount++;
        } catch (const std::exception& e) {
            std::cerr << "[database_zarr] Error storing transaction data: " << e.what() << std::endl;
        }
    }

    void database_zarr::retrieveTransactionData(size_t index)
    {
        if (index >= entryCount) {
            std::cerr << "[database_zarr] Index " << index << " out of range (0-" << entryCount-1 << ")" << std::endl;
            return;
        }

        try {
            z5::types::ShapeType offset = {index};
            
            xt::xarray<char> cmdArray({32});
            xt::xarray<char> respArray({64});
            xt::xarray<uint64_t> tsArray({1});
            
            z5::multiarray::readSubarray<char>(commandDs, cmdArray, offset.begin());
            z5::multiarray::readSubarray<char>(responseDs, respArray, offset.begin());
            z5::multiarray::readSubarray<uint64_t>(timestampDs, tsArray, offset.begin());
            
            std::cout << "[database_zarr] Retrieved entry " << index 
                      << ": cmd=" << cmdArray.data() << " resp=" << respArray.data() 
                      << " ts=" << tsArray[0] << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[database_zarr] Error retrieving transaction data: " << e.what() << std::endl;
        }
    }

    database_zarr::~database_zarr()
    {
        std::cout << "[database_zarr] destructor" << std::endl;
    }

} // namespace inscight
