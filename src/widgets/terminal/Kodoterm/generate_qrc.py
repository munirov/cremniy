import os
import glob

def generate_qrc(qrc_file, theme_dirs):
    with open(qrc_file, 'w') as f:
        f.write('<!DOCTYPE RCC><RCC version="1.0">\n')
        f.write('<qresource prefix="/">\n')
        for theme_dir in theme_dirs:
            files = sorted(glob.glob(os.path.join(theme_dir, '**/*'), recursive=True))
            for file_path in files:
                if os.path.isfile(file_path):
                    rel_path = os.path.relpath(file_path).replace(os.sep, '/')
                    f.write('    <file>' + rel_path + '</file>\n')
        f.write('</qresource>\n')
        f.write('</RCC>\n')

if __name__ == '__main__':
    generate_qrc('KodoTermThemes.qrc', ['KodoTermThemes'])
