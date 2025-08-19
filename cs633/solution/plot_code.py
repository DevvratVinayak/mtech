import pandas as pd
import numpy as np
df_3_222=pd.read_csv('Outputs/Dataset_64_64_64_3/output_2_2_2.txt')
df_3_222=df_3_222.iloc[1,0:3].astype('float').to_list()
df_3_422=pd.read_csv('Outputs/Dataset_64_64_64_3/output_4_2_2.txt')
df_3_422=df_3_422.iloc[1,0:3].astype('float').to_list()
df_3_442=pd.read_csv('Outputs/Dataset_64_64_64_3/output_4_4_2.txt')
df_3_442=df_3_442.iloc[1,0:3].astype('float').to_list()
df_3_444=pd.read_csv('Outputs/Dataset_64_64_64_3/output_4_4_4.txt')
df_3_444=df_3_444.iloc[1,0:3].astype('float').to_list()
df_7_222=pd.read_csv('Outputs/Dataset_64_64_96_7/output_2_2_2.txt')
df_7_222=df_7_222.iloc[1,0:3].astype('float').to_list()
df_7_422=pd.read_csv('Outputs/Dataset_64_64_96_7/output_4_2_2.txt')
df_7_422=df_7_422.iloc[1,0:3].astype('float').to_list()
df_7_442=pd.read_csv('Outputs/Dataset_64_64_96_7/output_4_4_2.txt')
df_7_442=df_7_442.iloc[1,0:3].astype('float').to_list()
df_7_444=pd.read_csv('Outputs/Dataset_64_64_96_7/output_4_4_4.txt')
df_7_444=df_7_444.iloc[1,0:3].astype('float').to_list()
dataset3=pd.DataFrame([df_3_222,df_3_422,df_3_442,df_3_444])
dataset3.columns=['time1','time2','time3']
dataset3['configs']=['2 2 2','4 2 2','4 4 2','4 4 4']
dataset7=pd.DataFrame([df_7_222,df_7_422,df_7_442,df_7_444])
dataset7.columns=['time1','time2','time3']
dataset7['configs']=['2 2 2','4 2 2','4 4 2','4 4 4']
import seaborn as sns
import matplotlib.pyplot as plt
df=pd.concat([dataset3,dataset7])
df['datasets']=['dataset 3', 'dataset 3', 'dataset 3', 'dataset 3','dataset 7', 'dataset 7', 'dataset 7', 'dataset 7']
fig, ax =plt.subplots(1,3,figsize=(20,6))
sns.set(style="whitegrid")
sns.barplot(data=df,x='datasets', y='time1', hue='configs',ax=ax[0]).set_title('Time 1')
ax[0].set_ylabel('Time in Seconds')
sns.barplot(data=df,x='datasets', y='time2', hue='configs',ax=ax[1]).set_title('Time 2')
ax[1].set_ylabel('Time in Seconds')
sns.barplot(data=df,x='datasets', y='time3', hue='configs',ax=ax[2]).set_title('Time 3')
ax[2].set_ylabel('Time in Seconds')
fig.savefig('Figure_plot_sample.png',dpi=600)
plt.show()