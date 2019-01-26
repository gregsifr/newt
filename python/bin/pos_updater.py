#!/usr/bin/env python
import datetime
import subprocess
import sys
import time
import os

logfile = os.environ["EXEC_LOG_DIR"] + "/posupdater.log"
command = os.environ["EXEC_ROOT_DIR"] + "/bin/gt-server-umsg -c 400 1 4 5 6 7"

exit_time = datetime.datetime.now().replace(hour=16,minute=1,second=0)
# we start querying the position server at 9:21 for positions and locates
start_time = datetime.datetime.now().replace(hour=9,minute=21,second=0)

def main():
  while 1:
    if datetime.datetime.now() >= start_time:
      break
    time.sleep(60)
  while 1:
    if datetime.datetime.now() > exit_time:
      break
    retcode = os.system(command)
    if retcode != 0:
      logfd = open(logfile,"a")
      logfd.write(str(datetime.datetime.now()) + ": CRIT Posupdater return code: " + str(retcode) + "\n")
      logfd.close()
    time.sleep(300)

if __name__ == "__main__":
  sys.exit(main())
