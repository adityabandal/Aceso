#!/bin/bash

# List of VMs (replace with actual IPs or hostnames)
VM_LIST=(
    "ms0931.utah.cloudlab.us"
    "ms0916.utah.cloudlab.us"
    "ms0909.utah.cloudlab.us"
    "ms0942.utah.cloudlab.us"
    "ms0911.utah.cloudlab.us"
    "ms0920.utah.cloudlab.us"
    "ms0921.utah.cloudlab.us"
    "ms0912.utah.cloudlab.us"
)

# SSH user (replace with your username)
SSH_USER="adbandal"

# Commands to run on each VM
# COMMANDS=$(cat <<EOF
# echo 'Running commands on $(hostname)';
# sudo ln -s /dev/nvme0n1p4 /dev/sda4;
# cd ~;
# bash setup-sda4.sh;
# cd ~/home;
# git clone https://github.com/adityabandal/Aceso.git aceso;
# cd ~/home/aceso/setup;
# sudo bash setup-env.sh;
# sudo reboot;
# EOF
# )

# COMMANDS=$(cat <<EOF
# cd ~/home;
# git clone https://github.com/adityabandal/Aceso.git aceso;
# EOF
# )

COMMANDS=$(cat <<EOF
cd ~/home;
git clone https://github.com/adityabandal/Aceso.git aceso;
cd ~/home/aceso && mkdir build && cd build;
cmake .. && make -j;
EOF
)





# Loop through each VM in parallel
for VM in "${VM_LIST[@]}"; do
    echo "Connecting to $VM..."
    # scp ./setup/setup-sda4.sh "${SSH_USER}@${VM}:~/" &
    ssh "${SSH_USER}@${VM}" "${COMMANDS}" &
done

# Wait for all background processes to complete
wait

echo "All commands executed on all VMs."
