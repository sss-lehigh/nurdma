#!/bin/bash
SSHKEY="" # add the location of your ssh key
LOCALROOT=".."
DESTROOT="~/nurdma"

if [[ $# -lt 3 ]]; then
    echo "Usage: $0  <mode> <client> <server>"
    exit
fi

# get run output folers ready
mkdir output
mkdir output/client
mkdir output/server


# input is 
MODE=$1
NODE1="$2" 
NODE2="$3" 
echo "client: ${NODE1}, target: ${NODE2}, mode: ${MODE}"

SRC=0
INCLUDE=0
CONFIG=0
SCRIPTS=0

if [[ $1 == "UPLOAD" ]]; then
    SRC=1
    INCLUDE=1
    CONFIG=1
    SCRIPTS=1
elif [[ $1 == "SRC" ]]; then
    SRC=1
elif [[ $1 == "INCLUDE" ]]; then
    INCLUDE=1
elif [[ $1 == "CONFIG" ]]; then
    CONFIG=1
elif [[ $1 == "SCRIPTS" ]]; then
    SCRIPTS=1
fi

# following used to copy project to remote machines 
if [[ $1 == "UPLOAD" ]]; then
    # make nurdma folder on remote nodes
    echo "making root folder on remote machines..."
    ssh -i ${SSHKEY} ${NODE1} mkdir ${DESTROOT} &
    ssh -i ${SSHKEY} ${NODE2} mkdir ${DESTROOT}
    echo "done"

    echo "copying CMakeLists.txt..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/CMakeLists.txt ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/CMakeLists.txt ${NODE2}:${DESTROOT}
    echo "done"

    echo "copying README.md..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/README.md ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/README.md ${NODE2}:${DESTROOT}
    echo "done"
fi
if [[ $SRC == 1 ]]; then
    # copy only necessary files (i.e., ignore results)
    echo "copying source files..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/src ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/src ${NODE2}:${DESTROOT}
    echo "done"
fi
if [[ $INCLUDE == 1 ]]; then
    echo "copying include files..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/include ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/include ${NODE2}:${DESTROOT}
    echo "done"
fi
if [[ $CONFIG == 1 ]]; then
    echo "copying configuration files..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/config ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/config ${NODE2}:${DESTROOT}
    echo "done"
fi
if [[ $SCRIPTS == 1 ]]; then
    echo "copying scripts..."
    scp -r -i ${SSHKEY} ${LOCALROOT}/scripts ${NODE1}:${DESTROOT} &
    scp -r -i ${SSHKEY} ${LOCALROOT}/scripts ${NODE2}:${DESTROOT}
    echo "done"
fi

# following used to remotely build project
if [[ $1 == "INSTALL" ]]; then
    # build the project on remote nodes
    echo "building project remotely..."
    ssh -i ${SSHKEY} ${NODE1} "mkdir ${DESTROOT}/bin; cd ${DESTROOT}/bin; cmake ..; make"  > output/client/install.txt &
    ssh -i ${SSHKEY} ${NODE2} "mkdir ${DESTROOT}/bin; cd ${DESTROOT}/bin; cmake ..; make"  > output/server/install.txt
    echo "done"
fi
if [[ $1 == "REMAKE" ]]; then
    # rebuild the project on remote nodes
    echo "remaking project remotely..."
    ssh -i ${SSHKEY} ${NODE1} 'cd ${DESTROOT}/bin; make' &
    ssh -i ${SSHKEY} ${NODE2} 'cd ${DESTROOT}/bin; make'
    echo "done"
fi

# following used to run tests
if [[ $1 == "CLOSEDLOOP" ]] || [[ $1 == "ALL" ]]; then
    echo "running closedloop experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/closedloop/sixtrfour_byte --bin=../bin/rc_bench --as_client=True --servername=$3 --outdir=../results/closedloop" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/closedloop/sixtrfour_byte --bin=../bin/rc_bench"
    echo "done"
fi
if [[ $1 == "BASELINE" ]] || [[ $1 == "ALL" ]]; then
    echo "running baseline experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/baseline --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/baseline" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/baseline --bin=../bin/rc_bench"
    echo "done"
fi
if [[ $1 == "MEMSIZE" ]] || [[ $1 == "ALL" ]]; then
    echo "running memsize experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/memsize/clients_eight --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/memsize" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/memsize/clients_eight --bin=../bin/rc_bench"
    echo "done"
fi
if [[ $1 == "WORKERS" ]] || [[ $1 == "ALL" ]]; then
    echo "running workers experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/workers --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/workers" > output/client/workers.txt &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/workers --bin=../bin/rc_bench" > output/server/workers.txt
    echo "done"
fi
if [[ $1 == "RATIO" ]] || [[ $1 == "ALL" ]]; then
    echo "running write_ratio experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/write_ratio --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/write_ratio" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/write_ratio --bin=../bin/rc_bench"
    echo "done"
fi
if [[ $1 == "THINK" ]] || [[ $1 == "ALL" ]]; then
    echo "running think experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/think --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/think" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/think"
    echo "done"
fi
if [[ $1 == "LOAD" ]] || [[ $1 == "ALL" ]]; then
    echo "running load experiments..."
    ssh -i ${SSHKEY} ${NODE1} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/load --bin=../bin/rc_bench --as_client=True --servername=$3 --out_root=../results/load/numa_zero" &
    ssh -i ${SSHKEY} ${NODE2} "cd ${DESTROOT}/scripts; python3 run_all_rc.py --root=../config/load --bin=../bin/rc_bench"
    echo "done"
fi