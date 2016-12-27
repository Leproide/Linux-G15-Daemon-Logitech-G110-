#!/bin/bash
#Script by Leprechaun
#leproide@paranoici.org

#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License.
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#See the GNU General Public License for more details.
#http://www.gnu.org/licenses/gpl-3.0.html


sudo g15daemon
screen -d -m -S "G110Macro" g15macro
sleep 3
CURRENT=`pwd`
PROCESSI=$(ps -e -o pid,comm | grep g15)
tput bel
notify-send -i "$CURRENT"/icon.png "Logitech G110" "$PROCESSI"
sudo gcolor 1
sudo gcolor 255
sudo gcolor 1
sudo gcolor 255
sudo gcolor 1
sudo gcolor 255

