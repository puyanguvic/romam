import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Function to read and parse data from files
def read_data(file_path):
    with open(file_path, "r") as f:
        return [float(line.strip()) for line in f if line.strip()]  # 读取为小数

# Load data into dictionaries
data = {
    "OSPF": read_data("ospf_cpu_time.txt"),  # 使用所有数据
    "DDR": read_data("ddr_cpu_time.txt"),  
    "DGR": read_data("dgr_cpu_time.txt"),  
    "KShortest": read_data("kshortest_cpu_time.txt"),
    "Octopus": read_data("octopus_cpu_time.txt")
}

# Convert to a pandas DataFrame
df = pd.DataFrame(data)
df["topology"] = ["Abilene", "AT&T", "CERNET", "GEANT"] # 添加索引列

# Melt the DataFrame to long format for Seaborn
df_melted = df.melt(id_vars=["topology"], var_name="protocol", value_name="cpu_time")

# Create a bar plot
plt.figure(figsize=(12, 6))

# 使用高级调色板
sns.set_palette("deep")

# 创建条形图
sns.barplot(x="topology", y="cpu_time", hue="protocol", data=df_melted)

# Set plot labels and title
#plt.xlabel("Topology Index")
plt.ylabel("CPU Time (ms)")
# plt.title("CPU cosumption for Protocols Initialization")
plt.legend(title="Protocols")

plt.tight_layout()
plt.show()
