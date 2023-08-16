#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

from uarm.wrapper import SwiftAPI

# 连接uarm swift pro，指定设备路径
swift = SwiftAPI(port="/dev/ttyACM0")

# 将机械臂重置到起始坐标
swift.reset(timeout = 3, x = 110, y = 0, z = 35)
