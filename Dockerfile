# 使用官方的 Ubuntu 18.04 镜像作为基础镜像
FROM ubuntu:18.04

# 更新软件包列表并安装一些基本工具
RUN apt-get update && apt-get install -y \
    vim \
    && rm -rf /var/lib/apt/lists/*

# 创建一个挂载点
VOLUME /data

# 在容器启动时运行的命令
CMD ["bash"]

