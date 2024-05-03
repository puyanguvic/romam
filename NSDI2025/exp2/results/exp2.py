import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def read_and_process_data(file_path):
    # 读取数据，并使用sep='\s+'代替delim_whitespace
    data = pd.read_csv(file_path, sep='\s+', header=None, names=['time', 'delay'])
    # 根据时间排序
    data.sort_values('time', inplace=True)
    # 重置索引
    data.reset_index(drop=True, inplace=True)
    # 计算累积平均延时
    data['cumulative_average_delay'] = data['delay'].expanding().mean()
    return data['cumulative_average_delay']

# 加载各协议的数据
ddr_data = read_and_process_data('ddr-geant.txt')
ospf_data = read_and_process_data('ospf-geant.txt')
octopus_data = read_and_process_data('octopus-geant.txt')

# 创建绘图 DataFrame
df = pd.DataFrame({
    'Index': range(len(ddr_data)),  # 假设所有数据集长度相同
    'DDR Delay': ddr_data,
    'OSPF Delay': ospf_data,
    'Octopus Delay': octopus_data
})

# 将DataFrame转化为长格式
df_long = pd.melt(df, id_vars='Index', var_name='Protocol', value_name='Cumulative Average Delay')

# 使用Seaborn绘制线图
plt.figure(figsize=(12, 8))
sns.lineplot(data=df_long, x='Index', y='Cumulative Average Delay', hue='Protocol', style='Protocol', alpha=0.7)

plt.title('Cumulative Average Packet Delay Over Time for Different Protocols')
plt.xlabel('Index')
plt.ylabel('Average Delay (ms)')
plt.grid(True)
plt.legend(title='Protocol')
plt.show()
