/*
Copyright (c) 2010-present Denis Kozhar (denbip@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <iostream>
#include "../../libs/basefunc_std.h"
#include "../../libs/timer.h"
#include "../../libs/json/json/json.h"
#include "../../libs/crc32.h"

#include "disk_server.hpp"
#include "disk_helper.hpp"
#include "clienttcphandler.hpp"
#include "hstf.h"
#include "extentions/c_manager.h"
#include "extentions/e_manager.h"

int main(int argc, char *argv[])
{
    //storage::JDatabaseManager().test();
    //exit(0);

    if(argc >= 2) //Kill if command is stop
    {
        if (strcmp(argv[1], "stop") == 0) basefunc_std::killbrothers(1);
    }
    basefunc_std::killbrothers(0);

    basefunc_std::set_path_log("/var/log/rssdisk/");

    std::atomic_bool done_atomic { ATOMIC_VAR_INIT(true) };

    std::string conf_path { "/etc/general/" };
    std::string self_name;
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i - 1], "--conf_path")) conf_path = argv[i];
        else if (!strcmp(argv[i - 1], "--self_name")) self_name = argv[i];
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
        {
            basefunc_std::cout("stop - to stop another exec running", "usage");
            basefunc_std::cout("--conf_path - set the path for search config file called rssdisk.json. Default is " + conf_path, "usage");
            basefunc_std::cout("--self_name - set the name of searching json settings in file sett. Default is value of ip in main section of file $CONF_PATH/self.conf", "usage");
            exit(0);
        }
    }

    storage::CDatabaseManager _CDB;
    storage::EDatabaseManager _EDB;

    rssdisk::server rd { conf_path };
    if (!rd.init_server_settings()) exit(0);
    const rssdisk::server::server_conf& server_sett = rd.get_server_settings2();

    if (!server_sett.tmp_conf.hdd_path.empty())
    {
        std::string _cdb_data_s;
        disk::helper::read_file(server_sett.tmp_conf.hdd_path, "_cdb_data", _cdb_data_s, 0);
        _CDB.deserializeData(_cdb_data_s);
    }

    hstf_eco HSTF;

    bool ok_hstf = HSTF.init(conf_path, &rd);
    if (!ok_hstf)
    {
        basefunc_std::cout("Can't init hstf", "error", basefunc_std::COLOR::RED_COL);
        exit(0);
    }

    ClientTCPHandler* tcp_cl = new ClientTCPHandler { &rd, &_CDB, &_EDB };
    tcp_cl->initiate();

    rd.on_write_ = [&](const std::string& filename, std::int64_t tms, std::int64_t crc32)
    {
        HSTF.add(filename, tms, crc32);
    };

    std::thread checker([&]()
    {
        int c3600 { 3800 };
        int c5 { 0 };

        while (done_atomic.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            ++c3600;

            if (c3600 > 3600)
            {
                c3600 = 0;
                rd.run_ttl_remover();
                HSTF.clear();
            }

            ++c5;

            if (c5 >= 5)
            {
                c5 = 0;
                _CDB.clearAll();
                _EDB.reset();
            }
        }
    });

    int sig = 0;
    sigset_t wait_mask;
    sigemptyset(&wait_mask);
    sigaddset(&wait_mask, SIGINT);
    sigaddset(&wait_mask, SIGQUIT);
    sigaddset(&wait_mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &wait_mask, 0);

    basefunc_std::cout("start", "success");
    sigwait(&wait_mask, &sig);

    basefunc_std::set_timer_to_kill(180);

    done_atomic.store(false, std::memory_order_release);
    basefunc_std::cout("stop", "exit");

    HSTF.stop();

    if (checker.joinable()) checker.join();

    tcp_cl->terminate();

    if (!server_sett.tmp_conf.hdd_path.empty())
    {
        std::string _cdb_data = _CDB.serializeData();
        disk::helper::write(server_sett.tmp_conf.hdd_path + "_cdb_data", _cdb_data);
    }

    basefunc_std::cout("success", "exit");

    basefunc_std::stop_log_thread();

    delete tcp_cl;

    return 0;
}
