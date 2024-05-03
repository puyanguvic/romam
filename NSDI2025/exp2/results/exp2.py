import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def read_and_process_data(file_path):
    # 读取数据
    data = pd.read_csv(file_path, delim_whitespace=True, header=None, names=['time', 'delay'])
    # 根据时间排序
    data.sort_values('time', inplace=True)
    # 重置索引
    data.reset_index(drop=True, inplace=True)
    return data['delay']

# 加载数据
ddr_data = read_and_process_data('ddr-geant.txt')
ospf_data = read_and_process_data('ospf-geant.txt')
octopus_data = read_and_process_data('octopus-geant.txt')

# 准备绘图数据
df = pd.DataFrame({
    'Index': range(len(ddr_data)),
    'DDR Delay': ddr_data,
    'OSPF Delay': ospf_data,
    'Octopus Delay': octopus_data
})

# 将DataFrame转化为长格式
df_long = pd.melt(df, id_vars='Index', var_name='Protocol', value_name='Delay')

# 设置颜色对比度鲜明的调色板
palette = {'DDR Delay': 'red', 'OSPF Delay': 'green', 'Octopus Delay': 'blue'}

# 使用Seaborn绘制散点图
plt.figure(figsize=(12, 8))
sns.scatterplot(data=df_long, x='Index', y='Delay', hue='Protocol', style='Protocol', palette=palette, alpha=0.6, s=50)

plt.title('Packet Delay Over Time Using Seaborn')
plt.xlabel('Index')
plt.ylabel('Delay (ms)')
plt.grid(True)
plt.legend(title='Protocol')
plt.show()
