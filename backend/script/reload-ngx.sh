#!/bin/bash
set -e
origin=/etc/nginx/sites-available/default
new=./nginx_conf/default
bak=./nginx_conf/default.bak
status=0
# 检查文件非空
if [ ! -s $new ]; then
    echo "配置文件为空"
    exit "1"
fi
# 做备份
sudo cat $origin > $bak
# 替换
sudo cat $new > $origin
# 检查
if sudo nginx -t && sudo nginx -s reload; then
    echo "配置文件已生效"
else
    echo "配置文件错误"
    # 还原
    sudo cat $bak > $origin
    status=1
fi

exit "$status"