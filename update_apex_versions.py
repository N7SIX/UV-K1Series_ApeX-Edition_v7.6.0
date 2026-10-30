#!/usr/bin/env python
"""Update ApeX version defaults in CMakeLists.txt"""
import os

filepath = r'c:\Users\sebue\Documents\N7SIX\Software\Quansheng\UV-K1Series_ApeX-Edition_v7.6.0\CMakeLists.txt'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace version defaults - ApeX starts from v7.6.0, develops to v7.6.12
# F4HWN editions keep their own versioning
content = content.replace('VERSION_STRING_1 "v0.22"', 'VERSION_STRING_1 "v7.6.0"')
content = content.replace('VERSION_STRING_2 "v4.2"', 'VERSION_STRING_2 "v7.6.12"')

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

print(f'Updated version defaults in {filepath}')
print('VERSION_STRING_1 now: v7.6.0 (ApeX base version)')
print('VERSION_STRING_2 now: v7.6.12 (ApeX developed version)')