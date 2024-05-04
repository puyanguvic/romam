import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Function to read and parse data from files
def read_data(file_path):
    with open(file_path, "r") as f:
        return [float(line.strip()) for line in f if line.strip()]  # Read as floats

# Load data into dictionaries
data = {
    "OSPF": read_data("ospf_storage.txt"),  # 使用所有数据
    "DDR": read_data("ddr_storage.txt"),  
    "DGR": read_data("dgr_storage.txt"),
    "KShortest": read_data("kshortest_storage.txt"),  
    "Octopus": read_data("octopus_storage.txt")
}

# Convert to a pandas DataFrame
df = pd.DataFrame(data)
df["topology"] = ["Abilene", "AT&T", "CERNET", "GEANT"]  # Set names for the 4 topologies

# Melt the DataFrame to long format for Seaborn
df_melted = df.melt(id_vars=["topology"], var_name="protocol", value_name="storage_size")

# Create a bar plot
plt.figure(figsize=(12, 6))

# Use a sophisticated palette
sns.set_palette("deep")
sns.set(style="whitegrid")

# Create the bar plot
sns.barplot(x="topology", y="storage_size", hue="protocol", data=df_melted)

# Set plot labels and title
plt.xlabel("Topology")
plt.ylabel("Route Information Base Size (kB)")
plt.title("Memory Cosumption of Routing Protocol")
plt.legend(title="Protocol")

plt.tight_layout()
plt.show()
