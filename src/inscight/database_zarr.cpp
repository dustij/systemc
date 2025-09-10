/******************************************************************************
 *                                                                            *
 * Minimal no-op implementation for Zarr backend tracing.                     *
 * Prints in fw/bw transaction hooks to verify call paths.                    *
 *                                                                            *
 ******************************************************************************/

#include "z5/filesystem/handle.hxx"
#include "inscight/database_zarr.h"

#include <iostream>

namespace inscight
{

    void database_zarr::init()
    {
        std::cout << "[database_zarr] init()" << std::endl;
        z5::filesystem::handle::File f("data.zr"); // usually an empty std::vector<std::size_t>
        std::cout << "[database_zarr] get handle to a File on the filesystem" << std::endl;
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
        std::cout << "[database_zarr] FW: st=" << st
                  << " obj=" << obj
                  << " proto=" << protocol_str(proto)
                  << " json=\"" << (json ? json : "") << "\"" << std::endl;
    }

    void database_zarr::transaction_trace_bw(id_t obj, sysc_time_t st, protocol_kind proto, const char *json)
    {
        std::cout << "[database_zarr] BW: st=" << st
                  << " obj=" << obj
                  << " proto=" << protocol_str(proto)
                  << " json=\"" << (json ? json : "") << "\"" << std::endl;
    }

    database_zarr::~database_zarr()
    {
        std::cout << "[database_zarr] destructor" << std::endl;
    }

} // namespace inscight
