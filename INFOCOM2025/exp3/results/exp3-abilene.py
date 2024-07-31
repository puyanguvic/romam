import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def process_data(file_path, take_last_n=None):
    # Read and sort data
    data = pd.read_csv(file_path, delim_whitespace=True, header=None, names=['send_time', 'delay'])
    data.sort_values('send_time', inplace=True)
    
    # Optionally take only the last N entries (for Octopus data)
    if take_last_n is not None:
        data = data.tail(take_last_n)
    
    return data['delay']

# File paths
files = {
    'OSPF': 'ospf-abilene.txt',
    'DDR': 'ddr-abilene.txt',
    'DGR': 'dgr-abilene.txt',
    'KShortest': 'kshortest-abilene.txt',
    'Octopus': 'octopus-abilene.txt'
}

# Prepare data for the seaborn box plot
data_list = []
for protocol, file in files.items():
    last_n = 1000 if 'Octopus' in protocol else None
    delay_data = process_data(file, take_last_n=last_n)
    # Each entry in the list is a DataFrame with two columns: Protocol and Delay
    data_frame = pd.DataFrame({'Protocol': protocol, 'Delay': delay_data})
    data_list.append(data_frame)

# Combine all data into a single DataFrame
all_data = pd.concat(data_list)

# Create box plot using seaborn with the "deep" palette
plt.figure(figsize=(10, 6))
sns.boxplot(x='Protocol', y='Delay', data=all_data, palette="deep")

# Set plot properties and y-axis limits
sns.set(style="whitegrid")
plt.xticks(fontsize=19)  # Rotate protocol names for better visibility
plt.xlabel(' ', fontsize=1)
plt.yticks(fontsize=19)
plt.ylabel('Delay (ms)', fontsize=19)
# plt.title('Box Plot of Packet Delays Across Different Protocols')
plt.ylim(0, 250)  # Set y-axis limit up to 250 ms

plt.show()
