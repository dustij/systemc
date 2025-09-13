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
        z5::filesystem::handle::File f("data.zr");
        std::cout << "[database_zarr] get handle to a File on the filesystem" << std::endl;

        // create the file in zarr format
        const bool createAsZarr = true;
        z5::createFile(f, createAsZarr);
        std::cout << "[database_zarr] create the file in zarr format" << std::endl;

        // create a new zarr dataset
        const std::string dsName = "data";
        std::vector<size_t> shape = {1000, 1000, 1000};
        std::vector<size_t> chunks = {100, 100, 100};
        auto ds = z5::createDataset(f, dsName, "float32", shape, chunks);
        std::cout << "[database_zarr] create a new zarr dataset" << std::endl;

        // write array to roi
        z5::types::ShapeType offset1 = {50, 100, 150};
        xt::xarray<float>::shape_type shape1 = {150, 200, 100};
        xt::xarray<float> array1(shape1, 42.0);
        z5::multiarray::writeSubarray<float>(ds, array1, offset1.begin());
        std::cout << "[database_zarr] write array to roi" << std::endl;

        // read array from roi (values that were not written before are filled with a fill-value)
        z5::types::ShapeType offset2 = {100, 100, 100};
        xt::xarray<float>::shape_type shape2 = {300, 200, 75};
        xt::xarray<float> array2(shape2);
        z5::multiarray::readSubarray<float>(ds, array2, offset2.begin());
        std::cout << "[database_zarr] read array from roi (values that were not written before are filled with a fill-value)" << std::endl;

        // get handle for the dataset
        const auto dsHandle = z5::filesystem::handle::Dataset(f, dsName);
        std::cout << "[database_zarr] get handle for the dataset" << std::endl;

        // read and write json attributes
        nlohmann::json attributesIn;
        attributesIn["bar"] = "foo";
        attributesIn["pi"] = 3.141593;
        z5::writeAttributes(dsHandle, attributesIn);
        std::cout << "[database_zarr] write json attributes" << std::endl;

        nlohmann::json attributesOut;
        z5::readAttributes(dsHandle, attributesOut);
        std::cout << "[database_zarr] read json attributes" << std::endl;
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
        // std::cout << "[database_zarr] FW: st=" << st
        //           << " obj=" << obj
        //           << " proto=" << protocol_str(proto)
        //           << " json=\"" << (json ? json : "") << "\"" << std::endl;
    }

    void database_zarr::transaction_trace_bw(id_t obj, sysc_time_t st, protocol_kind proto, const char *json)
    {
        // std::cout << "[database_zarr] BW: st=" << st
        //           << " obj=" << obj
        //           << " proto=" << protocol_str(proto)
        //           << " json=\"" << (json ? json : "") << "\"" << std::endl;
    }

    database_zarr::~database_zarr()
    {
        std::cout << "[database_zarr] destructor" << std::endl;
    }

} // namespace inscight
