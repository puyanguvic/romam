import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def read_and_process_data(file_path):
    # Read data, replacing delim_whitespace with sep='\s+'
    data = pd.read_csv(file_path, sep='\s+', header=None, names=['time', 'delay'])
    # Sort by time
    data.sort_values('time', inplace=True)
    # Reset index
    data.reset_index(drop=True, inplace=True)
    # Calculate cumulative average delay
    data['cumulative_average_delay'] = data['delay'].expanding().mean()
    return data['cumulative_average_delay']

# Load data for each protocol
abilene_data = read_and_process_data('octopus-abilene.txt')
att_data = read_and_process_data('octopus-att.txt')
cernet_data = read_and_process_data('octopus-cernet.txt')
geant_data = read_and_process_data('octopus-geant.txt')

# Determine the minimum length of data
min_length = min(len(abilene_data), len(att_data), len(cernet_data), len(geant_data))

# Trim data to minimum length
abilene_data = abilene_data[:min_length]
att_data = att_data[:min_length]
cernet_data = cernet_data[:min_length]
geant_data = geant_data[:min_length]

# Create DataFrame
df = pd.DataFrame({
    'Index': range(min_length), 
    'Abilene': abilene_data,
    'AT&T': att_data,
    'CERNET': cernet_data,
    'GEANT': geant_data
})

# Convert DataFrame to long format
df_long = pd.melt(df, id_vars='Index', var_name='Protocol', value_name='Cumulative Average Delay')

# Plot using Seaborn
plt.figure(figsize=(12, 8))
sns.lineplot(data=df_long, x='Index', y='Cumulative Average Delay', hue='Protocol', style='Protocol', alpha=0.7, linewidth=3)

# Set label font size
plt.rcParams.update({'font.size': 19})

# Set tick font size for both x and y-axis
plt.xticks(fontsize=19)
plt.yticks(fontsize=19)

# plt.title('Cumulative Average Packet Delay Over Time for Different Protocols')
plt.xlabel('Packet Index', fontsize=19)
plt.ylabel('Average Delay (ms)', fontsize=19)
plt.grid(True)
plt.legend(title='Topologies', fontsize=19)
plt.show()
