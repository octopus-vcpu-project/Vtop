#!/bin/bash

# Ensure a VM name has been passed as an argument
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <VM-NAME> <COMPETITOR-VM>"
    exit 1
fi

VM_NAME=$1
COMPETITOR_VM=$2
VTOP_CMD="./vtop/a.out -u 300000 -d 600 -s 5 -f 20"

NUM_CORES=$(nproc)

virsh vcpupin $VM_NAME 0 0
virsh vcpupin $VM_NAME 1 0
virsh vcpupin $VM_NAME 2 80
virsh vcpupin $VM_NAME 3 80
virsh vcpupin $VM_NAME 4 1
virsh vcpupin $VM_NAME 5 1
virsh vcpupin $VM_NAME 6 81
virsh vcpupin $VM_NAME 7 81
virsh vcpupin $VM_NAME 8 20
virsh vcpupin $VM_NAME 9 20
virsh vcpupin $VM_NAME 10 100
virsh vcpupin $VM_NAME 11 100
virsh vcpupin $VM_NAME 12 21
virsh vcpupin $VM_NAME 13 21
virsh vcpupin $VM_NAME 14 101
virsh vcpupin $VM_NAME 15 101

virsh vcpupin $COMPETITOR_VM 0 0
virsh vcpupin $COMPETITOR_VM 1 0
virsh vcpupin $COMPETITOR_VM 2 80
virsh vcpupin $COMPETITOR_VM 3 80
virsh vcpupin $COMPETITOR_VM 4 1
virsh vcpupin $COMPETITOR_VM 5 1
virsh vcpupin $COMPETITOR_VM 6 81
virsh vcpupin $COMPETITOR_VM 7 81
virsh vcpupin $COMPETITOR_VM 8 20
virsh vcpupin $COMPETITOR_VM 9 20
virsh vcpupin $COMPETITOR_VM 10 100
virsh vcpupin $COMPETITOR_VM 11 100
virsh vcpupin $COMPETITOR_VM 12 21
virsh vcpupin $COMPETITOR_VM 13 21
virsh vcpupin $COMPETITOR_VM 14 101
virsh vcpupin $COMPETITOR_VM 15 101


output_title="6prober_output_$(date +%d%H%M).txt"
echo "vCPU pinning completed successfully."
echo "Beginning Accuracy test(COLD)."
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Beginning test:Vtopology accuracy(COLD)\" >> \"\$output_file\";nohup sudo $VTOP_CMD >> \"\$output_file\" 2>&1 &"
sleep 180
ssh -T ubuntu@e-vm1 "sudo killall a.out";
VTOP_CMD="./vtop/a.out -u 300000 -d 600 -s 5 -f 10"
echo "Beginning Accuracy test(HOT)."
ssh -T ubuntu@e-vm3 "nohup sudo sysbench --threads=16 --time=100000 cpu run " &
ssh -T ubuntu@e-vm1 "nohup sudo sysbench --threads=16 --time=100000 cpu run " &
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Beginning test:Vtopology accuracy(HOT)\" >> \"\$output_file\";nohup sudo $VTOP_CMD >> \"\$output_file\" 2>&1 &"
sleep 60
echo "Beginning Total Migration Test."
virsh vcpupin $VM_NAME $0 $40
virsh vcpupin $VM_NAME $1 $40
virsh vcpupin $VM_NAME $2 $41
virsh vcpupin $VM_NAME $3 $41
virsh vcpupin $VM_NAME $4 $61
virsh vcpupin $VM_NAME $5 $61
virsh vcpupin $VM_NAME $6 $62
virsh vcpupin $VM_NAME $7 $62
virsh vcpupin $VM_NAME $8 $141
virsh vcpupin $VM_NAME $9 $141
virsh vcpupin $VM_NAME $10 $142
virsh vcpupin $VM_NAME $11 $142
virsh vcpupin $VM_NAME $12 $121
virsh vcpupin $VM_NAME $13 $121
virsh vcpupin $VM_NAME $14 $120
virsh vcpupin $VM_NAME $15 $120

virsh vcpupin $COMPETITOR_VM $0 $40
virsh vcpupin $COMPETITOR_VM $1 $40
virsh vcpupin $COMPETITOR_VM $2 $41
virsh vcpupin $COMPETITOR_VM $3 $41
virsh vcpupin $COMPETITOR_VM $4 $61
virsh vcpupin $COMPETITOR_VM $5 $61
virsh vcpupin $COMPETITOR_VM $6 $62
virsh vcpupin $COMPETITOR_VM $7 $62
virsh vcpupin $COMPETITOR_VM $8 $141
virsh vcpupin $COMPETITOR_VM $9 $141
virsh vcpupin $COMPETITOR_VM $10 $142
virsh vcpupin $COMPETITOR_VM $11 $142
virsh vcpupin $COMPETITOR_VM $12 $121
virsh vcpupin $COMPETITOR_VM $13 $121
virsh vcpupin $COMPETITOR_VM $14 $120
virsh vcpupin $COMPETITOR_VM $15 $120
ssh -T ubuntu@e-vm1 "=$output_title; echo \"\$(date): VM Migrated\" >> \"\$output_file\";"
sleep 20
echo "Beginning Load Balancing Test."
virsh vcpupin $VM_NAME $0 $40
virsh vcpupin $VM_NAME $1 $41
virsh vcpupin $VM_NAME $2 $42
virsh vcpupin $VM_NAME $3 $43
virsh vcpupin $VM_NAME $4 $61
virsh vcpupin $VM_NAME $5 $62
virsh vcpupin $VM_NAME $6 $63
virsh vcpupin $VM_NAME $7 $64
virsh vcpupin $VM_NAME $8 $141
virsh vcpupin $VM_NAME $9 $142
virsh vcpupin $VM_NAME $10 $143
virsh vcpupin $VM_NAME $11 $144
virsh vcpupin $VM_NAME $12 $121
virsh vcpupin $VM_NAME $13 $122
virsh vcpupin $VM_NAME $14 $123
virsh vcpupin $VM_NAME $15 $124

virsh vcpupin $COMPETITOR_VM $0 $40
virsh vcpupin $COMPETITOR_VM $1 $41
virsh vcpupin $COMPETITOR_VM $2 $42
virsh vcpupin $COMPETITOR_VM $3 $43
virsh vcpupin $COMPETITOR_VM $4 $61
virsh vcpupin $COMPETITOR_VM $5 $62
virsh vcpupin $COMPETITOR_VM $6 $63
virsh vcpupin $COMPETITOR_VM $7 $64
virsh vcpupin $COMPETITOR_VM $8 $141
virsh vcpupin $COMPETITOR_VM $9 $142
virsh vcpupin $COMPETITOR_VM $10 $143
virsh vcpupin $COMPETITOR_VM $11 $144
virsh vcpupin $COMPETITOR_VM $12 $121
virsh vcpupin $COMPETITOR_VM $13 $122
virsh vcpupin $COMPETITOR_VM $14 $123
virsh vcpupin $COMPETITOR_VM $15 $124

ssh -T ubuntu@e-vm1 "=$output_title; echo \"\$(date): VM Load balanced\" >> \"\$output_file\";"
sleep 20
echo "Beginning Overhead Test"
ssh -T ubuntu@e-vm1 "=$output_title; echo \"\$(date): VM Overhead test\" >> \"\$output_file\";"
ssh -T ubuntu@e-vm1 "sudo killall sysbench";
echo "Beginning Overhead Test(control)"
ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=30 cpu run > post_prober_sysbench.txt &
EOF
sleep 30
ssh -T ubuntu@e-vm1 "sudo killall a.out";
echo "Beginning Overhead Test(non control)"
ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=30 cpu run > post_prober_sysbench.txt &
EOF
sleep 30
echo "Finished"