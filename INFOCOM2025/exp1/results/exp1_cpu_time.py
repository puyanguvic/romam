import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib as mpl

# Function to read and parse data from files
def read_data(file_path):
    with open(file_path, "r") as f:
        return [float(line.strip()) for line in f if line.strip()]  # Read as floats

# Load data into dictionaries
data = {
    "OSPF": read_data("ospf_cpu_time.txt"),  # use all data
    "DDR": read_data("ddr_cpu_time.txt"),  
    "DGR": read_data("dgr_cpu_time.txt"),  
    "KShortest": read_data("kshortest_cpu_time.txt"),
    "Octopus": read_data("octopus_cpu_time.txt")
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

# Set label font size
mpl.rcParams.update({'font.size': 19})

# Create the bar plot
sns.barplot(x="topology", y="storage_size", hue="protocol", data=df_melted)

# Set plot labels and title
plt.xlabel("Topology", fontsize=19)
plt.ylabel("CPU Time (ms)", fontsize=19)
# plt.title("CPU cosumption for Protocols Initialization")
plt.legend(title="Protocols")

plt.tight_layout()
plt.show()

