import matplotlib.pyplot as plt
import pandas as pd

# Load the data
throughput_data = pd.read_csv('throughput-vs-nodes-high-contention.csv')
delay_data = pd.read_csv('delay-vs-nodes-high-contention.csv')
loss_data = pd.read_csv('packetloss-vs-nodes-high-contention.csv')

# Plot throughput
plt.plot(throughput_data['Nodes'], throughput_data['AvgThroughput(Kbps)'], 'bo-', linewidth=2)
plt.title('Average Throughput vs Number of Nodes (High Contention)')
plt.xlabel('Number of Nodes')
plt.ylabel('Average Throughput (Kbps)')
plt.grid(True)
plt.tight_layout()
plt.savefig("fig1")

plt.clf() # Clear figure

# Plot delay
plt.plot(delay_data['Nodes'], delay_data['AvgDelay(ms)'], 'ro-', linewidth=2)
plt.title('Average Delay vs Number of Nodes (High Contention)')
plt.xlabel('Number of Nodes')
plt.ylabel('Average Delay (ms)')
plt.grid(True)
plt.tight_layout()
plt.savefig("fig2")

# Plot packet loss
plt.plot(loss_data['Nodes'], loss_data['AvgPacketLossRatio'], 'go-', linewidth=2)
plt.title('Average Packet Loss Ratio vs Number of Nodes (High Contention)')
plt.xlabel('Number of Nodes')
plt.ylabel('Average Packet Loss Ratio')
plt.ylim(0, 1)
plt.grid(True)
plt.tight_layout()
plt.savefig("fig3")

plt.tight_layout()
#plt.savefig('adhoc-wifi-high-contention-performance.png')
#plt.show()
