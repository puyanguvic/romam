import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def process_data(file_path, take_last_n=None):
    # Read and sort data
    data = pd.read_csv(file_path, delim_whitespace=True, header=None, names=['send_time', 'delay'])
    data.sort_values('send_time', inplace=True)
    
    # Optionally take only the last N entries (for Octopus data)
    if take_last_n is not None:
        data = data.tail(take_last_n)
    
    return data['delay']

def calculate_cdf(data, max_delay=150):
    # Calculate the CDF
    value_counts = data.value_counts().sort_index().cumsum()
    cdf = value_counts / value_counts.iloc[-1]
    
    # Extend the CDF to ensure it continues to 150 ms
    last_index = cdf.index[-1]
    if last_index < max_delay:
        extended_index = np.arange(last_index + 1, max_delay + 1)
        extended_cdf = np.ones(len(extended_index))
        cdf = pd.concat([cdf, pd.Series(extended_cdf, index=extended_index)])
    
    return cdf

# File paths
files = {
    'OSPF': 'ospf-att.txt',
    'DDR': 'ddr-att.txt',
    'DGR': 'dgr-att.txt',
    'KShortest': 'kshortest-att.txt',
    'Octopus': 'octopus-att.txt'
}

plt.figure(figsize=(10, 6))

for protocol, file in files.items():
    last_n = 1000 if 'Octopus' in protocol else None
    delay_data = process_data(file, take_last_n=last_n)
    cdf = calculate_cdf(delay_data)
    plt.plot(cdf.index, cdf.values, label=protocol)

plt.xlabel('Delay (ms)')
plt.ylabel('Cumulative Distribution Function (CDF)')
plt.title('CDF of Packet Delays Across Different Protocols')
plt.legend()
plt.grid(True)
plt.xlim([0, 150])  # Set x-axis limits
plt.show()
