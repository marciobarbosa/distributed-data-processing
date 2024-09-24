# Distributed Query Execution

This project implements a distributed query execution application with shared
state. The workload is partitioned and distributed among multiple workers to
ensure scalability. Each worker processes its assigned partition, calculating
the frequency of domains. The results are then aggregated at the end to produce
the top 25 most frequent domains across the entire dataset.
