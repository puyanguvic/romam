import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Function to read and parse data from files
def read_data(file_path):
    with open(file_path, "r") as f:
        return [int(line.strip()) for line in f if line.strip()]  # Skip empty lines

# Load data into dictionaries
data = {
    "ospf": read_data("ospf.txt"),
    "ddr": read_data("ddr.txt"),
    "dgr": read_data("dgr.txt"),
    "octopus": read_data("octopus.txt")
}

# Convert to a pandas DataFrame
df = pd.DataFrame(data)
df["topology"] = range(1, len(df) + 1)  # Add a column for topology indices

# Melt the DataFrame to long format for Seaborn
df_melted = df.melt(id_vars=["topology"], var_name="protocol", value_name="cpu_time")

# Create a bar plot
plt.figure(figsize=(12, 6))

current_palette = sns.color_palette()
sns.palplot(current_palette)
sns.barplot(x="topology", y="cpu_time", hue="protocol", data=df_melted)

# Set plot labels and title
plt.xlabel("Topology Index")
plt.ylabel("CPU Time (ms)")
plt.title("CPU Time for Different Routing Protocols Across Topologies")
plt.legend(title="Protocol")

plt.tight_layout()
plt.show()
