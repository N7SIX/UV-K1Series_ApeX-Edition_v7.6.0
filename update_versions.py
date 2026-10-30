# Python script to update version strings in CMakeLists.txt
import sys

filepath = r'c:\Users\sebue\Documents\N7SIX\Software\Quansheng\UV-K1Series_ApeX-Edition_v7.6.0\CMakeLists.txt'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace the version strings
new_content = content.replace('AUTHOR_STRING_2 "F4HWN"', 'AUTHOR_STRING_2 "ApeX"')
new_content = new_content.replace('VERSION_STRING_1 "v0.22"', 'VERSION_STRING_1 "v7.6.12"')
new_content = new_content.replace('VERSION_STRING_2 "v4.2"', 'VERSION_STRING_2 "v7.6.12"')

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(new_content)

print('Version strings updated successfully in CMakeLists.txt')