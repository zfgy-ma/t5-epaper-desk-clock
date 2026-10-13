# 编译前检查：若 config.json 不存在，则从 example-config.json 复制一份
# config.json 含敏感信息（WiFi 密码、地区、检测链接），已在 .gitignore 中忽略
Import("env")

import os
import shutil

project_dir = env.subst("$PROJECT_DIR")
config_path = os.path.join(project_dir, "config.json")
example_path = os.path.join(project_dir, "example-config.json")

if not os.path.exists(config_path):
    shutil.copyfile(example_path, config_path)
    print("[ensure_config] 未找到 config.json，已从 example-config.json 复制")
    print("[ensure_config] 请编辑 config.json 填写 WiFi、地区、检测链接等信息")
