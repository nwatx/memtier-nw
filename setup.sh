sudo apt-get update -y
sudo apt-get install linux-tools-5.15.0-122-generic linux-cloud-tools-5.15.0-122-generic linux-tools-common-5.15.0-122-generic cmake -y
sudo apt-get install -y linux-tools-common linux-tools-generic
# Add debug symbols packages
sudo apt-get install -y linux-image-$(uname -r)-dbgsym || true
sudo apt-get install -y linux-headers-$(uname -r)

# Clean up any existing perf-related sysctl entries
sudo sed -i '/kernel.perf_event_paranoid/d' /etc/sysctl.conf
sudo sed -i '/kernel.kptr_restrict/d' /etc/sysctl.conf
sudo sed -i '/kernel.nmi_watchdog/d' /etc/sysctl.conf

# Configure kernel settings for perf
sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'
sudo sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict'
sudo sh -c 'echo 0 > /proc/sys/kernel/nmi_watchdog'

# Make the changes permanent (only once)
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.conf
echo "kernel.kptr_restrict = 0" | sudo tee -a /etc/sysctl.conf
echo "kernel.nmi_watchdog = 0" | sudo tee -a /etc/sysctl.conf

# Apply changes
sudo sysctl -p

# Ensure debug symbols are readable
sudo chmod +r /proc/kallsyms
sudo mount -o remount,mode=755 /sys/kernel/tracing/

[ ! -d "build" ] && mkdir build
cd build
cmake ..
cd ..
[ ! -d "data" ] && mkdir data
[ ! -f "./data/trace.data" ] && touch ./data/trace.data
sudo chmod -R 777 ./data/trace.data

git config user.name "nw (remote)"
git config user.email "neow@utexas.edu"