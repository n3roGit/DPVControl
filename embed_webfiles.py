#!/usr/bin/env python3
# embed_webfiles.py v2.0 - Binary-safe version

import os
import sys
import hashlib
from pathlib import Path

def get_content_type(filename):
    ext = Path(filename).suffix.lower()
    types = {
        '.html': 'text/html', '.css': 'text/css', '.js': 'application/javascript',
        '.json': 'application/json', '.png': 'image/png', '.jpg': 'image/jpeg',
        '.gif': 'image/gif', '.ico': 'image/x-icon', '.svg': 'image/svg+xml',
        '.txt': 'text/plain', '.xml': 'text/xml'
    }
    return types.get(ext, 'application/octet-stream')

def sanitize_variable_name(filename):
    name = Path(filename).name
    sanitized = ''.join(c if c.isalnum() else '_' for c in name)
    if sanitized and sanitized[0].isdigit():
        sanitized = 'file_' + sanitized
    return sanitized or 'unnamed_file'

def generate_header(file_path, filename):
    var_name = sanitize_variable_name(filename)
    content_type = get_content_type(filename)
    
    with open(file_path, 'rb') as f:
        data = f.read()
    
    checksum = hashlib.md5(data).hexdigest()[:8]
    
    header = f'''// Auto-generated: {filename} (v2.0 Binary-Safe)
#ifndef EMBEDDED_{var_name.upper()}_H
#define EMBEDDED_{var_name.upper()}_H

#include <Arduino.h>

const uint8_t embedded_{var_name}_data[] PROGMEM = {{
'''
    
    for i in range(0, len(data), 16):
        row = data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02X}' for b in row)
        header += f'    {hex_vals}'
        if i + 16 < len(data):
            header += ','
        header += '\n'
    
    header += f'''}}; 

const size_t embedded_{var_name}_size = {len(data)};
const char embedded_{var_name}_content_type[] PROGMEM = "{content_type}";
const char embedded_{var_name}_filename[] PROGMEM = "{filename}";
const char embedded_{var_name}_checksum[] PROGMEM = "{checksum}";

#endif
'''
    return header, var_name

def main():
    print("="*70)
    print("DPVControl Embed v2.0 - Binary-Safe")
    print("="*70)
    
    root_dir = os.path.dirname(os.path.abspath(__file__))
    upload_dir = os.path.join(root_dir, 'upload')
    generated_dir = os.path.join(root_dir, 'src', 'generated')
    
    if not os.path.exists(upload_dir):
        print(f"ERROR: {upload_dir} not found")
        return 1
    
    os.makedirs(generated_dir, exist_ok=True)
    
    # Clean old files
    for f in os.listdir(generated_dir):
        if f.startswith('embedded_') and (f.endswith('.h') or f.endswith('.cpp')):
            os.remove(os.path.join(generated_dir, f))
    
    vars_list = []
    total_size = 0
    
    print("\nProcessing files:")
    for root, dirs, files in os.walk(upload_dir):
        for fname in sorted(files):
            if fname.startswith('.') or fname.endswith('.md'):
                continue
            
            fpath = os.path.join(root, fname)
            print(f"  {fname}...", end='')
            
            try:
                header, var_name = generate_header(fpath, fname)
                
                header_path = os.path.join(generated_dir, f'embedded_{var_name}.h')
                with open(header_path, 'w', encoding='utf-8') as f:
                    f.write(header)
                
                fsize = os.path.getsize(fpath)
                total_size += fsize
                vars_list.append(var_name)
                
                print(f" OK ({fsize} bytes)")
            except Exception as e:
                print(f" ERROR: {e}")
                return 1
    
    if not vars_list:
        print("No files to embed!")
        return 1
    
    # Generate registry
    print("\nGenerating registry...")
    
    registry_h = '''// Auto-generated registry (v2.0)
#ifndef EMBEDDED_FILES_REGISTRY_H
#define EMBEDDED_FILES_REGISTRY_H

#include <Arduino.h>

'''
    for vn in vars_list:
        registry_h += f'#include "embedded_{vn}.h"\n'
    
    registry_h += '''
struct EmbeddedFile {
    const char* filename;
    const char* content_type;
    const uint8_t* data;
    size_t size;
    const char* checksum;
};

const EmbeddedFile embedded_files[] PROGMEM = {
'''
    
    for vn in vars_list:
        registry_h += f'''    {{
        embedded_{vn}_filename,
        embedded_{vn}_content_type,
        embedded_{vn}_data,
        embedded_{vn}_size,
        embedded_{vn}_checksum
    }},
'''
    
    registry_h += '''};

const size_t embedded_files_count = sizeof(embedded_files) / sizeof(embedded_files[0]);

const EmbeddedFile* findEmbeddedFile(const char* path);

#endif
'''
    
    registry_cpp = '''// Auto-generated registry implementation (v2.0)
#include "embedded_files_registry.h"
#include <string.h>

const EmbeddedFile* findEmbeddedFile(const char* path) {
    if (path && path[0] == '/') path++;
    if (!path || path[0] == '\\0') return nullptr;
    
    char buf[64];
    for (size_t i = 0; i < embedded_files_count; i++) {
        strcpy_P(buf, embedded_files[i].filename);
        if (strcmp(buf, path) == 0) {
            return &embedded_files[i];
        }
    }
    return nullptr;
}
'''
    
    with open(os.path.join(generated_dir, 'embedded_files_registry.h'), 'w', encoding='utf-8') as f:
        f.write(registry_h)
    
    with open(os.path.join(generated_dir, 'embedded_files_registry.cpp'), 'w', encoding='utf-8') as f:
        f.write(registry_cpp)
    
    print("="*70)
    print(f"SUCCESS: {len(vars_list)} files embedded ({total_size} bytes)")
    print("="*70)
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
