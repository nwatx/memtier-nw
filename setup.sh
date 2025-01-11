sudo apt-get install linux-tools-5.15.0-122-generic linux-cloud-tools-5.15.0-122-generic
sudo mount -o remount,mode=755 /sys/kernel/tracing/
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
mkdir build
cd build
cmake ..
mkdir data
touch ./data/trace.data
sudo chmod -R 777 ./data/trace.data

git config user.name "nw (remote)"
git config user.email "neow@utexas.edu"