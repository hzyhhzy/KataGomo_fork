#!/usr/bin/python3
import sys
import os
import argparse
import math
import torch

description = """
replace all variable of target model whose name is in the source model
"""

parser = argparse.ArgumentParser(description=description)
#parser.add_argument('-source', default="D:/kata1/n2n/data/train/b40graft2/checkpoint.ckpt")D:\renju28b\data\train\b28c512n
parser.add_argument('-source', default="D:/renju28b/data/train/b28c512n/checkpoint.ckpt")
args = vars(parser.parse_args())

source_path = args["source"]
source_data = torch.load(source_path,map_location="cpu")

print(source_data["config"])
#print(source_data["model"]["module.blocks.35.blockstack.1.normactconv2.conv.weight"][0,0:5])
#exit(0)

for name in source_data["model"].keys():
    if "normactconv" in name and ".conv.weight" in name:
        tensor=source_data["model"][name]
        #计算tensor的平均二范数，并显示

        #如果是3x3卷积核，分别计算中央和周围一圈的平均二范数

        # 计算整个张量的平均二范数
        total_norm = torch.norm(tensor, p=2) / tensor.numel()
        print(f"Layer: {name}")
        print(f"  - Total Average L2 Norm: {total_norm.item()}")

        # 检查是否是3x3卷积核
        if tensor.size(-1) == 3 and tensor.size(-2) == 3:
            # 计算中心核的二范数
            center = tensor[:, :, 1, 1]  # 提取中心元素
            center_norm = torch.norm(center, p=2) / center.numel()

            # 计算周围一圈的二范数
            outer = torch.cat((
                tensor[:, :, 0, :].reshape(-1),  # 上边展平
                tensor[:, :, 2, :].reshape(-1),  # 下边展平
                tensor[:, :, 1, 0].reshape(-1),  # 左边展平
                tensor[:, :, 1, 2].reshape(-1)   # 右边展平
            ))
            outer_norm = torch.norm(outer, p=2) / outer.numel()

            print(f"  - Center Average L2 Norm: {center_norm.item()}")
            print(f"  - Outer Average L2 Norm: {outer_norm.item()}")

        channel_norms = torch.norm(tensor.view(tensor.size(0), -1), p=2, dim=1)
        #print(f"  - Channel Norms: {channel_norms}")

        # 检查是否有退化的通道
        low_norm_channels = (channel_norms < 0.1).nonzero(as_tuple=True)[0]
        if len(low_norm_channels) > 0:
            print(f"  - Degraded Channels (Norm < low_norm_threshold): {low_norm_channels.tolist()}")
