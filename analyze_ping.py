#!/usr/bin/env python3
"""
Check4 网络测量数据分析脚本
分析 ping 数据，生成所需的统计和图表
"""

import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime
import sys

def parse_ping_data(filename):
    """解析 ping 数据文件"""
    data = []
    
    with open(filename, 'r') as f:
        for line in f:
            # 解析带时间戳的 ping 回复行
            match = re.match(r'\[(\d+\.\d+)\].*icmp_seq=(\d+).*time=(\d+(?:\.\d+)?)', line)
            if match:
                timestamp = float(match.group(1))
                seq = int(match.group(2))
                rtt = float(match.group(3))
                data.append({
                    'timestamp': timestamp,
                    'seq': seq,
                    'rtt': rtt,
                    'received': True
                })
    
    return data

def calculate_delivery_rate(data):
    """计算送达率"""
    if not data:
        return 0
    
    max_seq = max(d['seq'] for d in data)
    min_seq = min(d['seq'] for d in data)
    total_sent = max_seq - min_seq + 1
    total_received = len(data)
    
    return total_received / total_sent

def find_missing_packets(data):
    """找出丢失的包"""
    received_seqs = set(d['seq'] for d in data)
    max_seq = max(received_seqs)
    min_seq = min(received_seqs)
    
    missing = []
    for seq in range(min_seq, max_seq + 1):
        if seq not in received_seqs:
            missing.append(seq)
    
    return missing

def analyze_basic_stats(data):
    """基础统计分析"""
    rtts = [d['rtt'] for d in data]
    
    print("=== 基础统计 ===")
    print(f"总发送包数: {max(d['seq'] for d in data) if data else 0}")
    print(f"总接收包数: {len(data)}")
    print(f"送达率: {calculate_delivery_rate(data):.2%}")
    print(f"最小RTT: {min(rtts):.1f} ms" if rtts else "无数据")
    print(f"最大RTT: {max(rtts):.1f} ms" if rtts else "无数据")
    print(f"平均RTT: {np.mean(rtts):.1f} ms" if rtts else "无数据")
    print(f"RTT标准差: {np.std(rtts):.1f} ms" if rtts else "无数据")
    
    missing = find_missing_packets(data)
    print(f"丢失包数: {len(missing)}")
    if missing:
        print(f"丢失的序列号: {missing[:10]}{'...' if len(missing) > 10 else ''}")

def plot_rtt_over_time(data, output_file='rtt_over_time.png'):
    """绘制RTT随时间变化图"""
    if not data:
        return
    
    timestamps = [d['timestamp'] for d in data]
    rtts = [d['rtt'] for d in data]
    
    # 转换为相对时间（秒）
    start_time = min(timestamps)
    relative_times = [(t - start_time) for t in timestamps]
    
    plt.figure(figsize=(12, 6))
    plt.plot(relative_times, rtts, 'b-', alpha=0.7, linewidth=0.5)
    plt.xlabel('时间 (秒)')
    plt.ylabel('RTT (毫秒)')
    plt.title('RTT 随时间变化')
    plt.grid(True, alpha=0.3)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"RTT时间图保存到: {output_file}")

def plot_rtt_cdf(data, output_file='rtt_cdf.png'):
    """绘制RTT累积分布函数"""
    if not data:
        return
    
    rtts = [d['rtt'] for d in data]
    sorted_rtts = np.sort(rtts)
    y = np.arange(1, len(sorted_rtts) + 1) / len(sorted_rtts)
    
    plt.figure(figsize=(10, 6))
    plt.plot(sorted_rtts, y, 'b-', linewidth=2)
    plt.xlabel('RTT (毫秒)')
    plt.ylabel('累积概率')
    plt.title('RTT 累积分布函数 (CDF)')
    plt.grid(True, alpha=0.3)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"RTT CDF图保存到: {output_file}")

def main():
    if len(sys.argv) != 2:
        print("使用方法: python3 analyze_ping.py data.txt")
        sys.exit(1)
    
    filename = sys.argv[1]
    print(f"分析文件: {filename}")
    
    try:
        data = parse_ping_data(filename)
        analyze_basic_stats(data)
        plot_rtt_over_time(data)
        plot_rtt_cdf(data)
        print("\n分析完成！")
    except Exception as e:
        print(f"错误: {e}")

if __name__ == "__main__":
    main()
