import os
import re

def fix_dump(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        return
    
    original = content
    # Replace .dump() -> .dump(-1, ' ', false, nlohmann::json::error_handler_t::replace)
    content = re.sub(r'\.dump\(\)', '.dump(-1, \' \', false, nlohmann::json::error_handler_t::replace)', content)
    
    # Replace .dump(N) -> .dump(N, ' ', false, nlohmann::json::error_handler_t::replace)
    content = re.sub(r'\.dump\(\s*(-?\d+)\s*\)', r'.dump(\1, \' \', false, nlohmann::json::error_handler_t::replace)', content)
    
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed {filepath}")

for root, dirs, files in os.walk('/home/ws/nped/src'):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.h'):
            fix_dump(os.path.join(root, file))

print("Fixed JSON dumps")