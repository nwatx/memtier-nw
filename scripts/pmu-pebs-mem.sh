#!/bin/bash
# Memory and cache performance events

# Ensure we're in the correct directory
cd "$(dirname "$0")/.."

# Verify perf permissions
if [ "$(cat /proc/sys/kernel/perf_event_paranoid)" != "-1" ]; then
    echo "Error: Perf event paranoid setting is not -1"
    echo "Please run: sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'"
    exit 1
fi

if [ "$(cat /proc/sys/kernel/kptr_restrict)" != "0" ]; then
    echo "Error: kptr_restrict is not 0"
    echo "Please run: sudo sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict'"
    exit 1
fi

# Clean up any existing perf.data and ensure directory permissions
rm -f ./data/perf.data
sudo chown -R $(whoami) ./data

# Run perf record with supported events only
sudo perf record \
    -e cache-misses \
    -e cache-references \
    -e l2_rqsts.all_demand_data_rd \
    -e l2_rqsts.demand_data_rd_hit \
    -e l2_rqsts.demand_data_rd_miss \
    -e cpu-cycles \
    -F 100 \
    -o ./data/perf.data \
    ./build/src/basic_array -DPEBS_FRONTEND

# Add sleep to ensure file is written
sleep 1

# Fix permissions before running perf report
sudo chown $(whoami) ./data/perf.data
sudo chmod 644 ./data/perf.data

# Check if perf.data was created successfully
if [ -s ./data/perf.data ]; then
    # Generate both text and CSV reports
    sudo -E perf report --stdio --no-children -n -f --input ./data/perf.data > ./data/perf_report.txt
    
    # Extract data and create CSV
    echo "Event,Samples,Percentage" > ./data/perf_stats.csv
    sudo -E perf stat -e cache-misses \
        -e cache-references \
        -e l2_rqsts.all_demand_data_rd \
        -e l2_rqsts.demand_data_rd_hit \
        -e l2_rqsts.demand_data_rd_miss \
        -e cpu-cycles \
        -x, \
        -o ./data/perf_stats.csv \
        --append \
        ./build/src/basic_array -DPEBS_FRONTEND 2>&1
    
    echo "Reports generated:"
    echo "  - ./data/perf_report.txt"
    echo "  - ./data/perf_stats.csv"
else
    echo "Error: perf.data is empty or was not created"
    exit 1
fi