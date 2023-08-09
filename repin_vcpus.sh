VM_NAME="e-vm1"
COMPETITOR_VM="e-vm2"

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