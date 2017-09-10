#!/bin/bash

size=5000
files_list=""

t=$(($(date +%s%N)/1000000))
for ((i=1; i<=$size; i++)); do
    touch "test$i"
    files_list+="test$i "
done
t=$(($(($(date +%s%N)/1000000))-$t))
echo "Files creation time: $t ms"

t=$(($(date +%s%N)/1000000))
busctl --user call org.trash.trashd /org/trash/trashd org.trash.trashd Trash "as" $size $files_list &> /dev/null
t=$(($(($(date +%s%N)/1000000))-$t))
echo "Trashing time: $t ms"

t=$(($(date +%s%N)/1000000))
busctl --user call org.trash.trashd /org/trash/trashd org.trash.trashd List &> /dev/null
t=$(($(($(date +%s%N)/1000000))-$t))
echo "Listing time: $t ms"

t=$(($(date +%s%N)/1000000))
busctl --user call org.trash.trashd /org/trash/trashd org.trash.trashd EraseAll &> /dev/null
t=$(($(($(date +%s%N)/1000000))-$t))
echo "Erasing time: $t ms"
