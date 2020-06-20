#!/bin/bash
 
gnome-terminal -e "./tsd -p 3010 -r RS -h localhost"
gnome-terminal -e "./tsd -p 3020 -r MS -h localhost"
gnome-terminal -e "./tsd -p 3030 -r MS -h localhost"
gnome-terminal -e "./tsd -p 3040 -r MS -h localhost"

echo "Server is ready to connect"

gnome-terminal -e "./tsc -u u1"
gnome-terminal -e "./tsc -u u2"
gnome-terminal -e "./tsc -u u3"
#gnome-terminal -e "./tsc -u u4"

