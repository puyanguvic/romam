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
abilene_data = read_and_process_data('octopus-abilene.txt')
att_data = read_and_process_data('octopus-att.txt')
cernet_data = read_and_process_data('octopus-cernet.txt')
geant_data = read_and_process_data('octopus-geant.txt')

min_length = min(len(abilene_data), len(att_data), len(cernet_data), len(geant_data))

abilene_data = abilene_data[:min_length]
att_data = att_data[:min_length]
cernet_data = cernet_data[:min_length]
geant_data = geant_data[:min_length]

df = pd.DataFrame({
    'Index': range(min_length), 
    'Abilene': abilene_data,
    'AT&T': att_data,
    'CERNET': cernet_data,
    'GEANT': geant_data
})

# 将DataFrame转化为长格式
df_long = pd.melt(df, id_vars='Index', var_name='Protocol', value_name='Cumulative Average Delay')

# 使用Seaborn绘制线图
plt.figure(figsize=(12, 8))
sns.lineplot(data=df_long, x='Index', y='Cumulative Average Delay', hue='Protocol', style='Protocol', alpha=0.7)

# plt.title('Cumulative Average Packet Delay Over Time for Different Protocols')
plt.xlabel('Index')
plt.ylabel('Average Delay (ms)')
plt.grid(True)
plt.legend(title='Protocol')
plt.show()
