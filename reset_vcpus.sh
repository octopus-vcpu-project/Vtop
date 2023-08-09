VM_NAME="e-vm1"
COMPETITOR_VM="e-vm2"

virsh vcpupin $VM_NAME 0 40
virsh vcpupin $VM_NAME 1 40
virsh vcpupin $VM_NAME 2 41
virsh vcpupin $VM_NAME 3 41
virsh vcpupin $VM_NAME 4 61
virsh vcpupin $VM_NAME 5 61
virsh vcpupin $VM_NAME 6 62
virsh vcpupin $VM_NAME 7 62
virsh vcpupin $VM_NAME 8 141
virsh vcpupin $VM_NAME 9 141
virsh vcpupin $VM_NAME 10 142
virsh vcpupin $VM_NAME 11 142
virsh vcpupin $VM_NAME 12 121
virsh vcpupin $VM_NAME 13 121
virsh vcpupin $VM_NAME 14 120
virsh vcpupin $VM_NAME 15 120

virsh vcpupin $COMPETITOR_VM 0 40
virsh vcpupin $COMPETITOR_VM 1 40
virsh vcpupin $COMPETITOR_VM 2 41
virsh vcpupin $COMPETITOR_VM 3 41
virsh vcpupin $COMPETITOR_VM 4 61
virsh vcpupin $COMPETITOR_VM 5 61
virsh vcpupin $COMPETITOR_VM 6 62
virsh vcpupin $COMPETITOR_VM 7 62
virsh vcpupin $COMPETITOR_VM 8 141
virsh vcpupin $COMPETITOR_VM 9 141
virsh vcpupin $COMPETITOR_VM 10 142
virsh vcpupin $COMPETITOR_VM 11 142
virsh vcpupin $COMPETITOR_VM 12 121
virsh vcpupin $COMPETITOR_VM 13 121
virsh vcpupin $COMPETITOR_VM 14 120
virsh vcpupin $COMPETITOR_VM 15 120