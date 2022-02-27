#!/bin/bash

echo "Starting slurm..."
runuser -u munge -- munged
slurmd
slurmctld

sleep 1
pgrep munged    > /dev/null || echo "Error: munged not running! Check with: runuser -u munge -- munged -vF"
pgrep slurmd    > /dev/null || echo "Error: slurmd not running! Check with: slurmd -vD"
pgrep slurmctld > /dev/null || echo "Error: slurmctld not running! Check with: slurmctld -vD"
