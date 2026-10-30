"""只打包界面静态资源，插件运行时不使用网络或外部构建工具。"""
import sys
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
source, target = map(Path, sys.argv[1:])
with ZipFile(target, 'w', ZIP_DEFLATED) as archive:
    for path in sorted(source.rglob('*')):
        if path.is_file() and path.suffix in {'.html', '.css', '.js', '.json', '.txt'} and path.name not in {'preview.html','preview.js','parameter-schema.json'}:
            archive.write(path, path.relative_to(source).as_posix())
