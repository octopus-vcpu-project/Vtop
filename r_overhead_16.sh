VM_NAME="e-vm1"
COMPETITOR_VM="e-vm2"
VTOP_CMD="./vtop/a.out -u 300000 -d 600 -s 5 -f 5"

virsh vcpupin $VM_NAME 0 40
virsh vcpupin $VM_NAME 1 41
virsh vcpupin $VM_NAME 2 42
virsh vcpupin $VM_NAME 3 43
virsh vcpupin $VM_NAME 4 61
virsh vcpupin $VM_NAME 5 62
virsh vcpupin $VM_NAME 6 63
virsh vcpupin $VM_NAME 7 64
virsh vcpupin $VM_NAME 8 141
virsh vcpupin $VM_NAME 9 142
virsh vcpupin $VM_NAME 10 143
virsh vcpupin $VM_NAME 11 144
virsh vcpupin $VM_NAME 12 121
virsh vcpupin $VM_NAME 13 122
virsh vcpupin $VM_NAME 14 123
virsh vcpupin $VM_NAME 15 124

virsh vcpupin $COMPETITOR_VM 0 40
virsh vcpupin $COMPETITOR_VM 1 41
virsh vcpupin $COMPETITOR_VM 2 42
virsh vcpupin $COMPETITOR_VM 3 43
virsh vcpupin $COMPETITOR_VM 4 61
virsh vcpupin $COMPETITOR_VM 5 62
virsh vcpupin $COMPETITOR_VM 6 63
virsh vcpupin $COMPETITOR_VM 7 64
virsh vcpupin $COMPETITOR_VM 8 141
virsh vcpupin $COMPETITOR_VM 9 142
virsh vcpupin $COMPETITOR_VM 10 143
virsh vcpupin $COMPETITOR_VM 11 144
virsh vcpupin $COMPETITOR_VM 12 121
virsh vcpupin $COMPETITOR_VM 13 122
virsh vcpupin $COMPETITOR_VM 14 123
virsh vcpupin $COMPETITOR_VM 15 124
ssh -T ubuntu@e-vm1 "sudo killall sysbench";
ssh -T ubuntu@e-vm1 "sudo killall a.out";
ssh -T ubuntu@e-vm2 "sudo killall sysbench";
ssh -T ubuntu@e-vm2 "nohup sudo sysbench --threads=16 --time=100000 cpu run " &
output_title="overhead_16$(date +%d%H%M).txt"
echo "Begin overhead test (16) Control."
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Beginning test:Vtopology accuracy(COLD)\" >> \"\$output_file\";nohup sudo $VTOP_CMD >> \"\$output_file\" 2>&1 &"
sleep 180
echo "Beginning Overhead test(16) sysbench"
ssh -T ubuntu@e-vm1 "output_file=$output_title; echo \"\$(date): Sysbench Activated\" >> \"\$output_file\";"
ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=180 cpu run > 12top_pre_prober_sysbench.txt &
EOF
ssh -T ubuntu@e-vm1 "sudo killall a.out";
echo "Beginning Overhead 16 sysbench, no prober"
ssh -T ubuntu@e-vm1 << EOF
    sudo nohup sysbench --threads=16 --time=180 cpu run > 12top_post_prober_sysbench.txt &
EOF
echo "Finished"