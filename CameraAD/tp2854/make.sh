#!/bin/bash

begin_time=$(date "+%s")  

make

cp TP2854B.ko ../mkrootfs/rootfs_glibc/ko/TP2854B.ko 

end_time=$(date "+%s")  
time_distance=$(($end_time - $begin_time));  
hour_distance=$(expr ${time_distance} / 3600)
hour_remainder=$(expr ${time_distance} % 3600)    
min_distance=$(expr ${hour_remainder} / 60)    
min_remainder=$(expr ${hour_remainder} % 60)   
echo -e "\033[0;32mspend time is ${hour_distance}:${min_distance}:${min_remainder} \033[0m"
