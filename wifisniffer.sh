#!/bin/sh
PID=""
WLDEV_DEFAULT="phy0_mon0"
APP_NAME="wifisniffer"
get_pid () {
   PID=`cat $APP_NAME.pid`
}

stop () {
   get_pid
   if [ -z $PID ]; then
      echo "$APP_NAME is not running."
      exit 1
   else
    echo -n "Stopping $APP_NAME..."
    kill $PID
    rm $APP_NAME.pid
    sleep 1

    echo "... Done."
   fi
}

start () {
    get_pid   
   _now=$(date +"%m_%d_%Y_%HH:%MM:%SS")

    if [ -z $PID ]; then
       echo  "Starting $APP_NAME $*..." 
       
       #./$APP_NAME 2> $APP_NAME.log 2>&1
       ./$APP_NAME 2> $APP_NAME.log &
       echo $! > $APP_NAME.pid
       get_pid
       
       echo "Done. PID=$PID"
    else
        echo "$APP_NAME is already running, PID=$PID"
    fi    
 
     
}

restart () {
   echo  "Restarting Strategy..."
   get_pid
   if [ -z $PID ]; then
      start
   else
      stop
      start
   fi
}


status () {
   get_pid
   if [ -z  $PID ]; then
      echo "$APP_NAME is not running."
      exit 1
   else
      echo "$APP_NAME is running, PID=$PID"
   fi
}

case "$1" in
   start)
      start
   ;;
   stop)
      stop
   ;;
   restart)
      restart
   ;;
   status)
      status
   ;;
   *)
      echo "Usage: $0 {start|stop|restart|status}"

esac
