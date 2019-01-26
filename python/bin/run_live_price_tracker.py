#!/usr/bin/env python
# Runs the live price-tracker program for the entire universe

import os
import subprocess
import util

if __name__=="__main__":
    execfile(os.environ["BIN_DIR"] + "/constants_and_params.py")

    # binary of the price-tracker cpp program
    tracker_bin = os.environ["BIN_DIR"] + "/pricetracker"
    configuration_file = os.environ["CONFIG_DIR"] + "/exec/pricetracker.conf"
    output_dir = os.environ["EXEC_LOG_DIR"] + "/priceData"
    output_files_prefix = output_dir + "/data"

    p = subprocess.Popen('mkdir -p ' + output_dir, env=os.environ, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    retcode = p.wait()

    command = "{} -C {} ".format(tracker_bin, configuration_file)
    # add the debug-stream arguments
    command += "--debug-stream.path {} ".format(output_dir)
    # add DM's arguments
    command += "--dm.outfile {} --dm.symbol-file {} ".format(output_dir + "/cl_output.log", os.environ["RUN_DIR"] + "/universe")
    # add price-tracker's arguments
    command += "--out-file {} ".format(output_files_prefix)

    p = subprocess.Popen(command_line, env=os.environ, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output = p.stdout.read()
    retcode = p.wait()
    if retcode!=0:
        util.email("WARNING. Failed to run the following command line on {}:\n{}\nThe output:\n{}\nThe return code:{}"
                .format(os.environ['HOSTNAME'], command, output, retcode), 'price-tracker returned a non 0 return code')
        exit(-1)
