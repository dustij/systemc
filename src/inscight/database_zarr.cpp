/******************************************************************************
 *                                                                            *
 * Minimal no-op implementation for Zarr backend tracing.                     *
 * Prints in fw/bw transaction hooks to verify call paths.                    *
 *                                                                            *
 ******************************************************************************/

#include <nlohmann/json.hpp>
#include <xtensor/containers/xarray.hpp>

// factory functions to create files, groups and datasets
#include <z5/factory.hxx>
// handles for z5 filesystem objects
#include <z5/filesystem/handle.hxx>
// io for xtensor multi-arrays
#include <z5/multiarray/xtensor_access.hxx>
// attribute functionality
#include <z5/attributes.hxx>

#include "inscight/database_zarr.h"

#include <iostream>
#include <fstream>

namespace inscight
{
    // Helper function to fix zarr metadata fill_value from float to integer
    void fixZarrMetadata(const std::string& metadataPath) {
        std::ifstream inFile(metadataPath);
        if (!inFile.is_open()) {
            std::cerr << "[database_zarr] Warning: Could not open " << metadataPath << " for reading" << std::endl;
            return;
        }

        nlohmann::json metadata;
        try {
            inFile >> metadata;
            inFile.close();

            // Fix fill_value from 0.0 to 0 for integer types
            if (metadata.contains("fill_value") && metadata["fill_value"] == 0.0) {
                metadata["fill_value"] = 0;

                std::ofstream outFile(metadataPath);
                if (outFile.is_open()) {
                    outFile << metadata.dump(4) << std::endl;
                    outFile.close();
                    std::cout << "[database_zarr] Fixed fill_value in " << metadataPath << std::endl;
                } else {
                    std::cerr << "[database_zarr] Warning: Could not write to " << metadataPath << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[database_zarr] Error fixing metadata " << metadataPath << ": " << e.what() << std::endl;
        }
    }

    void database_zarr::init()
    {
        std::cout << "[database_zarr] init()" << std::endl;

        // get handle to a File on the filesystem
        z5::filesystem::handle::File f("data.zr");
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

        // Create datasets matching SQLite schema
        std::vector<size_t> shape = {10000}; // Start with 10k entries
        std::vector<size_t> chunks = {1000};
        std::vector<size_t> jsonShape = {10000, 512}; // 512-byte strings for JSON
        std::vector<size_t> jsonChunks = {1000, 512};

        // Create or open datasets using supported data types
        // id field (auto-incrementing integer)
        try {
            idDs = z5::createDataset(f, "id", "uint64", shape, chunks);
            fixZarrMetadata("data.zr/id/.zarray");
        } catch (const std::invalid_argument&) {
            idDs = z5::openDataset(f, "id");
        }

        // st field (timestamp)
        try {
            stDs = z5::createDataset(f, "st", "uint64", shape, chunks);
            fixZarrMetadata("data.zr/st/.zarray");
        } catch (const std::invalid_argument&) {
            stDs = z5::openDataset(f, "st");
        }

        // dir field (direction: 0=FW, 1=BW)
        try {
            dirDs = z5::createDataset(f, "dir", "int32", shape, chunks);
            fixZarrMetadata("data.zr/dir/.zarray");
        } catch (const std::invalid_argument&) {
            dirDs = z5::openDataset(f, "dir");
        }

        // port field (object id)
        try {
            portDs = z5::createDataset(f, "port", "uint64", shape, chunks);
            fixZarrMetadata("data.zr/port/.zarray");
        } catch (const std::invalid_argument&) {
            portDs = z5::openDataset(f, "port");
        }

        // proto field (protocol kind)
        try {
            protoDs = z5::createDataset(f, "proto", "int32", shape, chunks);
            fixZarrMetadata("data.zr/proto/.zarray");
        } catch (const std::invalid_argument&) {
            protoDs = z5::openDataset(f, "proto");
        }

        // json field (full JSON text)
        try {
            jsonDs = z5::createDataset(f, "json", "uint8", jsonShape, jsonChunks);
            fixZarrMetadata("data.zr/json/.zarray");
        } catch (const std::invalid_argument&) {
            jsonDs = z5::openDataset(f, "json");
        }

        std::cout << "[database_zarr] opened/created datasets: id, st, dir, port, proto, json" << std::endl;
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
        storeTransactionData(obj, st, proto, json, 0); // 0 = FW direction
    }

    void database_zarr::transaction_trace_bw(id_t obj, sysc_time_t st, protocol_kind proto, const char *json)
    {
        storeTransactionData(obj, st, proto, json, 1); // 1 = BW direction
    }

    void database_zarr::storeTransactionData(id_t obj, sysc_time_t timestamp, protocol_kind proto, const char* json, int direction)
    {
        if (entryCount >= 10000) {
            std::cerr << "[database_zarr] Warning: Dataset full, cannot store more entries" << std::endl;
            return;
        }

        try {
            // Prepare offset for all scalar fields
            z5::types::ShapeType offset = {entryCount};
            z5::types::ShapeType jsonOffset = {entryCount, 0};

            // Create arrays for each field
            xt::xarray<uint64_t> idArray = xt::zeros<uint64_t>({1});
            xt::xarray<uint64_t> stArray = xt::zeros<uint64_t>({1});
            xt::xarray<int32_t> dirArray = xt::zeros<int32_t>({1});
            xt::xarray<uint64_t> portArray = xt::zeros<uint64_t>({1});
            xt::xarray<int32_t> protoArray = xt::zeros<int32_t>({1});
            xt::xarray<uint8_t> jsonArray = xt::zeros<uint8_t>({1, 512});

            // Fill in the data
            idArray[0] = entryCount;  // Auto-incrementing ID
            stArray[0] = static_cast<uint64_t>(timestamp);
            dirArray[0] = direction;
            portArray[0] = static_cast<uint64_t>(obj);
            protoArray[0] = static_cast<int32_t>(proto);

            // Copy JSON string (or empty string if null)
            std::string jsonStr = (json && strlen(json) > 0) ? json : "";
            jsonStr.resize(511, '\0'); // Ensure null termination within 512 bytes
            for (size_t i = 0; i < jsonStr.size() && i < 512; ++i) {
                jsonArray(0, i) = static_cast<uint8_t>(jsonStr[i]);
            }

            // Write all fields
            z5::multiarray::writeSubarray<uint64_t>(*idDs, idArray, offset.begin());
            z5::multiarray::writeSubarray<uint64_t>(*stDs, stArray, offset.begin());
            z5::multiarray::writeSubarray<int32_t>(*dirDs, dirArray, offset.begin());
            z5::multiarray::writeSubarray<uint64_t>(*portDs, portArray, offset.begin());
            z5::multiarray::writeSubarray<int32_t>(*protoDs, protoArray, offset.begin());
            z5::multiarray::writeSubarray<uint8_t>(*jsonDs, jsonArray, jsonOffset.begin());

            std::cout << "[database_zarr] " << (direction == 0 ? "FW" : "BW") << " stored entry " << entryCount
                      << ": id=" << entryCount << " st=" << timestamp << " dir=" << direction
                      << " port=" << obj << " proto=" << proto << std::endl;

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
            z5::types::ShapeType jsonOffset = {index, 0};

            // Create arrays to read into
            xt::xarray<uint64_t> idArray = xt::zeros<uint64_t>({1});
            xt::xarray<uint64_t> stArray = xt::zeros<uint64_t>({1});
            xt::xarray<int32_t> dirArray = xt::zeros<int32_t>({1});
            xt::xarray<uint64_t> portArray = xt::zeros<uint64_t>({1});
            xt::xarray<int32_t> protoArray = xt::zeros<int32_t>({1});
            xt::xarray<uint8_t> jsonArray = xt::zeros<uint8_t>({1, 512});

            // Read all fields
            z5::multiarray::readSubarray<uint64_t>(*idDs, idArray, offset.begin());
            z5::multiarray::readSubarray<uint64_t>(*stDs, stArray, offset.begin());
            z5::multiarray::readSubarray<int32_t>(*dirDs, dirArray, offset.begin());
            z5::multiarray::readSubarray<uint64_t>(*portDs, portArray, offset.begin());
            z5::multiarray::readSubarray<int32_t>(*protoDs, protoArray, offset.begin());
            z5::multiarray::readSubarray<uint8_t>(*jsonDs, jsonArray, jsonOffset.begin());

            // Convert JSON bytes back to string
            std::string jsonStr(reinterpret_cast<const char*>(jsonArray.data()), 512);
            auto jsonNull = jsonStr.find('\0');
            if (jsonNull != std::string::npos) jsonStr = jsonStr.substr(0, jsonNull);

            std::cout << "[database_zarr] Retrieved entry " << index << ":" << std::endl;
            std::cout << "  id=" << idArray[0] << std::endl;
            std::cout << "  st=" << stArray[0] << std::endl;
            std::cout << "  dir=" << dirArray[0] << " (" << (dirArray[0] == 0 ? "FW" : "BW") << ")" << std::endl;
            std::cout << "  port=" << portArray[0] << std::endl;
            std::cout << "  proto=" << protoArray[0] << std::endl;
            std::cout << "  json=" << jsonStr << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[database_zarr] Error retrieving transaction data: " << e.what() << std::endl;
        }
    }

    database_zarr::~database_zarr()
    {
        std::cout << "[database_zarr] destructor" << std::endl;
    }

} // namespace inscight
