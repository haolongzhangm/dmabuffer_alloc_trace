tail = """
</script>  
</body>
</html>"""

import sys
import os

# 获取脚本文件所在目录
script_dir = os.path.dirname(os.path.abspath(__file__))
output_html_path = os.path.join(script_dir, "template.html")

with open(output_html_path, 'r') as fin:
    html = fin.read()

with open(sys.argv[1], 'r') as fin:
    data = fin.read()

with open(sys.argv[2], 'w') as fout:
    fout.write(html)
    fout.write(data)
    fout.write(tail)