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

        // create the file in zarr format, or open if it already exists
        const bool createAsZarr = true;
        try {
            z5::createFile(f, createAsZarr);
            std::cout << "[database_zarr] created new file in zarr format" << std::endl;
        } catch (const std::invalid_argument& e) {
            // File already exists, that's okay - we'll just use the existing file
            std::cout << "[database_zarr] using existing zarr file" << std::endl;
        }

        // create datasets for command and response_status
        std::vector<size_t> shape = {10000}; // Start with 10k entries, can grow dynamically
        std::vector<size_t> chunks = {1000};
        
        // Create or open datasets using supported data types
        try {
            commandDs = z5::createDataset(f, "commands", "uint8", {10000, 32}, {1000, 32}); // 32-byte strings as uint8 arrays
        } catch (const std::invalid_argument&) {
            commandDs = z5::openDataset(f, "commands");
        }

        try {
            responseDs = z5::createDataset(f, "responses", "uint8", {10000, 64}, {1000, 64}); // 64-byte strings as uint8 arrays
        } catch (const std::invalid_argument&) {
            responseDs = z5::openDataset(f, "responses");
        }

        try {
            timestampDs = z5::createDataset(f, "timestamps", "uint64", shape, chunks);
        } catch (const std::invalid_argument&) {
            timestampDs = z5::openDataset(f, "timestamps");
        }

        std::cout << "[database_zarr] opened/created datasets for commands, responses, and timestamps" << std::endl;
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
            z5::types::ShapeType cmd_offset = {entryCount, 0};
            z5::types::ShapeType resp_offset = {entryCount, 0};
            z5::types::ShapeType ts_offset = {entryCount};

            // Create properly sized arrays and copy data
            xt::xarray<uint8_t> cmdArray = xt::zeros<uint8_t>({1, 32});
            xt::xarray<uint8_t> respArray = xt::zeros<uint8_t>({1, 64});
            xt::xarray<uint64_t> tsArray = xt::zeros<uint64_t>({1});

            // Copy string data into arrays (convert char to uint8_t)
            for (size_t i = 0; i < command.size() && i < 32; ++i) {
                cmdArray(0, i) = static_cast<uint8_t>(command[i]);
            }
            for (size_t i = 0; i < response.size() && i < 64; ++i) {
                respArray(0, i) = static_cast<uint8_t>(response[i]);
            }
            tsArray[0] = static_cast<uint64_t>(timestamp);

            z5::multiarray::writeSubarray<uint8_t>(*commandDs, cmdArray, cmd_offset.begin());
            z5::multiarray::writeSubarray<uint8_t>(*responseDs, respArray, resp_offset.begin());
            z5::multiarray::writeSubarray<uint64_t>(*timestampDs, tsArray, ts_offset.begin());
            
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
            z5::types::ShapeType cmd_offset = {index, 0};
            z5::types::ShapeType resp_offset = {index, 0};
            z5::types::ShapeType ts_offset = {index};

            // Create properly sized zero-initialized arrays
            xt::xarray<uint8_t> cmdArray = xt::zeros<uint8_t>({1, 32});
            xt::xarray<uint8_t> respArray = xt::zeros<uint8_t>({1, 64});
            xt::xarray<uint64_t> tsArray = xt::zeros<uint64_t>({1});

            z5::multiarray::readSubarray<uint8_t>(*commandDs, cmdArray, cmd_offset.begin());
            z5::multiarray::readSubarray<uint8_t>(*responseDs, respArray, resp_offset.begin());
            z5::multiarray::readSubarray<uint64_t>(*timestampDs, tsArray, ts_offset.begin());

            // Convert uint8_t back to strings for display
            std::string cmdStr(reinterpret_cast<const char*>(cmdArray.data()), 32);
            std::string respStr(reinterpret_cast<const char*>(respArray.data()), 64);

            // Find null terminator to truncate strings properly
            auto cmdNull = cmdStr.find('\0');
            if (cmdNull != std::string::npos) cmdStr = cmdStr.substr(0, cmdNull);
            auto respNull = respStr.find('\0');
            if (respNull != std::string::npos) respStr = respStr.substr(0, respNull);

            std::cout << "[database_zarr] Retrieved entry " << index
                      << ": cmd=" << cmdStr << " resp=" << respStr
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
