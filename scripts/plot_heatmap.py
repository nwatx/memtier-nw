#!/bin/python
'''
    This script plots the heatmap of page accesses.
    The y-axis is the address space and the x-axis is the epoch number, and the color
    represents the number of accesses to that page in that epoch.

    Usage: python3 plot_pageheat.py <page_accesses_file.csv>
'''

import csv
import matplotlib.pyplot as plt
import math
import numpy as np
import sys

PAGE_SIZE = 4096 * 512
PAGE_SHIFT = int(math.log(PAGE_SIZE, 2))

if len(sys.argv) < 2:
    print("Usage: python3 plot_pageheat.py <page_accesses_file.csv>")
    exit()

# Read the page accesses from the file into a dictionary
# Use numpy array to store the page accesses
page_accesses = {}
with open(sys.argv[1], 'r') as csvfile:
    reader = csv.reader(csvfile, delimiter=' ')
    for row in reader:
        # Convert row[1:] from string to int
        rowtemp = [int(i) for i in row[1:]]
        addr = (int(row[0], 16)) << PAGE_SHIFT
        if addr in page_accesses:
            page_accesses[addr] = np.add(page_accesses[addr], np.array(rowtemp))
        else:
            page_accesses[addr] = np.array(rowtemp)

# Each row contains cummulative access counts till that epoch
# Calculate actual counts per epoch by subtracting previous element in the row
# For example, if row contains [1, 3, 5, 7], then actual counts are [1, 2, 2, 2]
for k, v in page_accesses.items():
    first_elem = v[0]
    page_accesses[k] = np.diff(v)
    page_accesses[k] = np.insert(page_accesses[k], 0, first_elem)

# Convert to log scale
for k, v in page_accesses.items():
    page_accesses[k] = np.log(v+1)

unmodified_size = len(page_accesses)
num_epochs = len(page_accesses[list(page_accesses.keys())[0]])

# Trim page_accesses with more than num_epochs
for k, v in page_accesses.items():
    if len(v) > num_epochs:
        page_accesses[k] = v[:num_epochs]

# Remove any address beyond 0x7fffffffffff
page_accesses = {k: v for k, v in page_accesses.items() if k <= 0x7fffffffffff}
pages_removed_at_end = unmodified_size - len(page_accesses)

# Remove any address before 0x7f0000000000
page_accesses = {k: v for k, v in page_accesses.items() if k >= 0x7f0000000000}
pages_removed_at_start = unmodified_size - len(page_accesses) - pages_removed_at_end

print("Removed at start: %d, Removed at end: %d" % (pages_removed_at_start, pages_removed_at_end))

start_addr = list(page_accesses.keys())[0]
end_addr = list(page_accesses.keys())[-1]
num_pages = int((end_addr - start_addr) / PAGE_SIZE) + 1

print("Start address: ", hex(start_addr)
      , ", End address: ", hex(end_addr)
      , ", Number of pages: ", num_pages
      , ", Number of epochs: ", num_epochs)

# Create a 2d np array of size num_pages x num_epochs
# For addresses not presetn in page_accesses, fill with 0
page_accesses_array = np.zeros((num_pages, num_epochs))
for i in range(num_pages):
    addr = start_addr + (i*PAGE_SIZE)
    if addr in page_accesses:
        page_accesses_array[i] = page_accesses[addr]
    else:
        page_accesses_array[i] = np.zeros(num_epochs)

# Plot the heatmap
plt.imshow(page_accesses_array, aspect='auto', cmap='gray_r', interpolation='nearest')

plt.xlabel('Epoch')
plt.xticks(np.linspace(0, num_epochs-1, 10), np.linspace(0, num_epochs-1, 10, dtype=int))
plt.ylabel('Address space')
ylabels = np.linspace(start_addr, end_addr, 10, dtype=int)
ylabels = [hex(i) for i in ylabels]
plt.yticks(np.linspace(0, num_pages-1, 10), ylabels)

plt.tight_layout()

# Extract filename from the path
filename = sys.argv[1].split('/')[-1]
filename = filename.split('.')[0]
filename = filename + '_heatmap.png'
plt.savefig(filename)
plt.show()