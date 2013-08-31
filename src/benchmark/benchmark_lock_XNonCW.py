#!/usr/bin/python

import sys
import os
import datetime
import subprocess
import re

import sys

bin_dir_path = os.path.dirname(os.path.realpath(__file__))

sys.path.append(os.path.join(bin_dir_path, '..', 'src', 'lock'))

from extract_numa_structure import numa_structure

_ignore1, num_of_cpus_per_node,ignore2 = numa_structure()

parameters = sys.argv

parameters.pop(0)

if len(parameters) < 10:
    print """Not enough parameters:

Look at bin/run_benchmarks_on_intel_i7.py and
bin/run_benchmarks_on_sandy.py for examples and explanations of
parameters.

"""
    sys.exit()

output_dir_base = parameters.pop(0)

if output_dir_base=='standard':
    output_dir_base = (subprocess.check_output(['git', 'rev-parse', 'HEAD'])[-11:-1] 
                       + '_' + datetime.datetime.now().strftime("%Y.%m.%d.%H.%M.%S"))

benchmark_names = parameters.pop(0).split(',')

lock_ids = parameters.pop(0).split(',')

pinning_settings = parameters.pop(0).split(',')

thread_counts = parameters.pop(0).split(",")

#* Percentage read
#* Number of seconds to benchmark
#* Iterations spent in write critical section
#* Iterations spent in read critical section
#* Iterations spent in non critical section


percentages_reads = parameters.pop(0).split(',')

run_times_seconds = parameters.pop(0).split(',')

iterations_wcs = parameters.pop(0).split(',')

iterations_rcs = parameters.pop(0).split(',')

iterations_ncs = parameters.pop(0).split(',')


for benchmark_id in [benchmark_name + "_" + lock_id 
                     for benchmark_name in benchmark_names 
                     for lock_id in lock_ids]:
    for settings in [[tc,pr,rts,iw,ir] 
                     for tc in thread_counts
                     for pr in percentages_reads
                     for rts in run_times_seconds
                     for iw in iterations_wcs
                     for ir in iterations_rcs]:
        for pinning in pinning_settings:
            output_file_dir_str = ('bench_results/' + 
                                   benchmark_id + output_dir_base + '/')
            if not os.path.exists(output_file_dir_str):
                os.makedirs(output_file_dir_str)
            output_file_str = (output_file_dir_str +
                               'xncw_' + pinning + '_' + '_'.join(settings) + '.dat')
            with open(output_file_str, "w") as outfile:
                print "\n\n\033[32m -- STARTING BENCHMARKS FOR " + output_file_str + "! -- \033[m\n\n"
                for non_cs_work in iterations_ncs:
                    [tc,pr,rts,iw,ir] = settings
                    command = [bin_dir_path + '/' + benchmark_id,tc,pr,rts,iw,ir,non_cs_work]
                    print command
                    if pinning=='no':
                        process = subprocess.Popen(command, stdout=outfile)
                        process.wait()
                    else:
                        max_node_id = (int(tc)-1) / num_of_cpus_per_node
                        nomactrl = ['numactl', '--cpunodebind=' + ",".join([str(x) for x in range(0,max_node_id+1)])]
                        process = subprocess.Popen(nomactrl + command, stdout=outfile)
                        process.wait()
                print "\n\n\033[32m -- BENCHMARKS FOR " + output_file_str + " COMPLETED! -- \033[m\n\n"


