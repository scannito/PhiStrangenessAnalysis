#!/bin/bash

# number of parallel jobs
NTHREADS=20

run_job() {
    local SAMPLE=$1
    local N=$2

    root -b -l <<EOF
int a = $N;
a = 2*a + 1;
.L Tuple2Hist.cxx
Tuple2Hist($SAMPLE, 364, 365, a);
.q
EOF
}

for SAMPLE in $(seq 0 40)
do
    hadd -f /data/mciacco/out/out__file__tot_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_0_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_1_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_2_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_3_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_4_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_5_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_6_s_${SAMPLE}.root \
        /data/mciacco/out/out_file_7_s_${SAMPLE}.root \

    # parallel loop
    for N in $(seq 0 19)
    do
        run_job $SAMPLE $N &

        # limit number of concurrent jobs
        while [ "$(jobs -r | wc -l)" -ge "$NTHREADS" ]
        do
            sleep 1
        done
    done

    # wait for all jobs of this SAMPLE
    wait

    echo "Finished SAMPLE $SAMPLE"
done
