#!/usr/bin/env bash
set -euo pipefail
host="$1"
port="$2"
cmd="$3"

echo "Running ${cmd}"

code=""
code+="print('=== ${cmd} begin ===')\n"
code+="import os, sys, binascii\n"

if [ "${cmd}" = "ls-modules" ]; then
    code+="help('modules')\n"
elif [ "${cmd}" = "ls" ]; then
    dir="${4:-FLASH0:}"
    code+="print('=== contents of ${dir} ===')\n"
    code+="for el in sorted(os.listdir('${dir}')):\n"
    code+="    print(el)\n"
elif [ "${cmd}" = "upload" ]; then
    srcfile="${4}"
    targetfile="${5}"
    code+="bin = binascii.a2b_base64(b'''\n"
    code+="$(base64 "${srcfile}")"
    code+="''')\n"
    code+="with open('${targetfile}', 'wb') as f:\n"
    code+="    f.write(bin)\n"
elif [ "${cmd}" = "cat" ]; then
    file="${4}"
    code+="with open('${file}', 'r') as f:\n"
    code+="    while line := f.readline():\n"
    code+="        print(f.readline(), end='')\n"
else
    echo "Unknown command ${cmd}"
    exit 1
fi

code+="print('=== ${cmd} end ===')\n"
code+="EOF\n"

echo -e "${code}" | nc $host $port
