VM_NAME="e-vm3"
VTOP_CMD="./vtop/a.out -u 300000 -d 600 -s 5 -f 5"



VCPUS=32  # for example; adjust as needed

for ((vcpu_num=0; vcpu_num < $VCPUS; vcpu_num++)); do
    if ((vcpu_num % 2 == 0)); then
        cpu_num=$((vcpu_num / 2) + 8)
    else
        cpu_num=$((vcpu_num / 2 + 88))
    fi
    virsh vcpupin $VM_NAME $vcpu_num $cpu_num
done

VCPUS=64

for ((vcpu_num=32; vcpu_num < $VCPUS; vcpu_num++)); do
    if ((vcpu_num % 2 == 0)); then
        cpu_num=$((vcpu_num / 2) + 28)
    else
        cpu_num=$((vcpu_num / 2 + 108))
    fi
    virsh vcpupin $VM_NAME $vcpu_num $cpu_num
done

ssh -T ubuntu@e-vm3 "sudo killall sysbench";
ssh -T ubuntu@e-vm3 "sudo killall a.out";

output_title="overhead_64$(date +%d%H%M).txt"
echo "Begin overhead test (64) Control."
ssh -T ubuntu@e-vm3 "output_file=$output_title; echo \"\$(date): Beginning test:Vtopology accuracy(COLD)\" >> \"\$output_file\";nohup sudo $VTOP_CMD >> \"\$output_file\" 2>&1 &"
sleep 60
echo "Beginning Overhead test(64) sysbench"
ssh -T ubuntu@e-vm3 "output_file=$output_title; echo \"\$(date): Sysbench Activated\" >> \"\$output_file\";"
ssh -T ubuntu@e-vm3 << EOF
    sudo nohup sysbench --threads=64 --time=30 cpu run 
EOF
ssh -T ubuntu@e-vm3 << EOF
    sudo nohup sysbench --threads=64 --time=60 cpu run > 64_with_prober_sysbench.txt &
EOF
ssh -T ubuntu@e-vm3 "sudo killall a.out";
echo "Beginning Overhead 64 sysbench, no prober"
ssh -T ubuntu@e-vm3 << EOF
    sudo nohup sysbench --threads=64 --time=60 cpu run > 64_without_prober_sysbench.txt &
EOF
echo "Finished"