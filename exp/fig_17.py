# micro throughput -- checkpoint frequency test
from func_timeout import FunctionTimedOut
from pathlib import Path
import json

from utils.cmd_manager import CMDManager
from utils.log_parser import LogParser
from utils.color_printer import print_GOOD, print_WARNING, print_OK
from utils.func_timer import print_func_time

input_path = './params'
style_path = "./styles"
output_path = './results'
fig_num = '17'
small_fig_num = {'YCSB A': 'a', 'YCSB B': 'b', 'YCSB C': 'c', 'YCSB D': 'd'}

# common params
with (Path(input_path) / f'common.json').open(mode='r') as f:
    params = json.load(f)
home_dir      = params['home_dir']
ycsb_dir      = f'{home_dir}/aceso/ycsb-test'
client_ips    = params['client_ips']
master_ip     = params['master_ip']
server_ips    = params['server_ips']
cmake_options = params['cmake_options']

# fig params
with (Path(input_path) / f'fig_{fig_num}.json').open(mode='r') as f:
    fig_params = json.load(f)
methods            = fig_params['methods']
MN_num             = fig_params['MN_num']
ckpt_intervals     = fig_params['ckpt_intervals']

@print_func_time
def main(cmd: CMDManager, tp: LogParser):
    op_names = ['insert', 'update', 'search', 'delete']
    plot_data = {
        'X_data': ckpt_intervals,
        'Y_data': {
            ckpt_interval: {}
            for ckpt_interval in ckpt_intervals
        }
    }

    method = 'aceso'
    project_dir = f"{home_dir}/{method}"
    server_work_dir = f"{project_dir}/build/major-test"
    client_work_dir = f"{project_dir}/build/major-test"
    server_env_cmd = f"cd {server_work_dir}"
    client_env_cmd = f"cd {client_work_dir}"

    # only for aceso
    memcache_dir = f"{project_dir}/src"
    memcache_env_cmd = f"cd {memcache_dir}"

    # update and build project
    cmake_option = cmake_options[method]    # TODO
    SERVER_BUILD_PROJECT = f"cd {project_dir} && mkdir -p build && cd build && rm -f CMakeCache.txt && cmake {cmake_option} .. && make clean && make -j && cd major-test && mkdir -p results"
    CLIENT_BUILD_PROJECT = f"cd {project_dir} && mkdir -p build && cd build && rm -f CMakeCache.txt && cmake {cmake_option} .. && make clean && make -j && cd major-test && mkdir -p results"
    SET_CN_HUGEPAGE = "echo 4000 | sudo tee /proc/sys/vm/nr_hugepages"
    SET_MN_HUGEPAGE = "echo 28000 | sudo tee /proc/sys/vm/nr_hugepages"
    UNSET_HUGEPAGE = "echo 0 | sudo tee /proc/sys/vm/nr_hugepages"
    KILL_SERVER = f"{server_env_cmd} && mkdir -p results && killall -9 server"
    KILL_CLIENT = f"{client_env_cmd} && mkdir -p results && killall -9 client_perf"
    cmd.all_execute(KILL_CLIENT)
    cmd.all_execute(UNSET_HUGEPAGE)
    cmd.all_execute(CLIENT_BUILD_PROJECT, -1, True)
    cmd.server_all_execute(KILL_SERVER)
    cmd.server_all_execute(UNSET_HUGEPAGE)
    cmd.server_all_execute(SERVER_BUILD_PROJECT, 0, -1, True)

    CN_num = len(client_ips)
    client_num_per_CN = 8
    for ckpt_interval in ckpt_intervals:
        for op in op_names:
            SERVER_START = f"{memcache_env_cmd} && ./run_memcached.sh && {server_env_cmd} && ./server {ckpt_interval}"
            MICRO_TEST = f"{client_env_cmd} && ./client_perf {op} {CN_num} {client_num_per_CN} 8"
            
            while True:
                try:
                    cmd.server_all_execute(SET_MN_HUGEPAGE, 0, MN_num)
                    cmd.server_all_long_execute(SERVER_START, 0, MN_num)
                    cmd.all_execute(SET_CN_HUGEPAGE, CN_num)
                    logs = cmd.all_long_execute(MICRO_TEST, CN_num)
                    total_tpt = tp.get_total_tpt(logs)
                    
                    cmd.all_execute(KILL_CLIENT, CN_num)
                    cmd.all_execute(UNSET_HUGEPAGE, CN_num)
                    cmd.server_all_execute(KILL_SERVER, 0, MN_num)
                    cmd.server_all_execute(UNSET_HUGEPAGE, 0, MN_num)
                    break
                except (FunctionTimedOut, Exception) as e:
                    print_WARNING(f"Error! Retry... {e}")
                    cmd.all_execute(KILL_CLIENT, CN_num)
                    cmd.all_execute(UNSET_HUGEPAGE, CN_num)
                    cmd.server_all_execute(KILL_SERVER, 0, MN_num)
                    cmd.server_all_execute(UNSET_HUGEPAGE, 0, MN_num)

            print_GOOD(f"[FINISHED POINT] method={method} ckpt_interval={ckpt_interval}")

            plot_data['Y_data'][ckpt_interval][op] = total_tpt / 10e5
                
    # save data
    Path(output_path).mkdir(exist_ok=True)
    with (Path(output_path) / f'fig_{fig_num}a.json').open(mode='w') as f:
        json.dump(plot_data, f, indent=2)

if __name__ == '__main__':
    print_WARNING("remember to sudo su && remember first CN's server_id = 0")
    cmd = CMDManager(client_ips, master_ip, server_ips)
    tp = LogParser()
    t = main(cmd, tp)
