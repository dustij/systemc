import os
import sys
import json

if len(sys.argv) != 2:
    print("Usage: python inspect_zarr.py <path_to_zarr>")
    exit(1)

zarr_path = sys.argv[1]

if not os.path.exists(zarr_path):
    print(f"Error: {zarr_path} not found")
    exit(1)

print(f"[inspect] Examining zarr structure at: {zarr_path}")

# List directory contents
print(f"[inspect] Directory contents:")
for item in os.listdir(zarr_path):
    item_path = os.path.join(zarr_path, item)
    if os.path.isdir(item_path):
        print(f"  DIR:  {item}/")
        # List subdirectory contents
        for subitem in os.listdir(item_path):
            print(f"    {subitem}")
    else:
        print(f"  FILE: {item}")

# Look for .zarray and .zgroup files
zgroup_path = os.path.join(zarr_path, '.zgroup')
zarray_path = os.path.join(zarr_path, '.zarray')

if os.path.exists(zgroup_path):
    print(f"\n[inspect] Found .zgroup file:")
    with open(zgroup_path, 'r') as f:
        print(f"  {f.read()}")

if os.path.exists(zarray_path):
    print(f"\n[inspect] Found .zarray file:")
    with open(zarray_path, 'r') as f:
        content = f.read()
        print(f"  {content}")
        try:
            metadata = json.loads(content)
            print(f"  Parsed metadata: {metadata}")
        except json.JSONDecodeError as e:
            print(f"  JSON parse error: {e}")

# Check subdirectories for arrays
for item in os.listdir(zarr_path):
    item_path = os.path.join(zarr_path, item)
    if os.path.isdir(item_path):
        subarray_path = os.path.join(item_path, '.zarray')
        if os.path.exists(subarray_path):
            print(f"\n[inspect] Found array in {item}/:")
            with open(subarray_path, 'r') as f:
                content = f.read()
                print(f"  {content}")
                try:
                    metadata = json.loads(content)
                    print(f"  Shape: {metadata.get('shape')}")
                    print(f"  Dtype: {metadata.get('dtype')}")
                    print(f"  Fill value: {metadata.get('fill_value')}")
                except json.JSONDecodeError as e:
                    print(f"  JSON parse error: {e}")