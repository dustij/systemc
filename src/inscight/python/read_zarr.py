import zarr
import os
import sys
import numpy as np

# This is for exploring the zarr dataset
# It now supports the full transaction schema: id, st, dir, port, proto, json
def start_exploring(store):
    print("Lets dig into this a bit...")

    keys = [k for k in store.keys()]

    for i in range(len(keys)):
        print(keys[i])

    running = True
    while running:
        user_input = input("Choose (or 'view' to see transactions): ")
        match user_input:
            case "id" | "st" | "port":
                # Scalar uint64 fields
                print(f"You chose {user_input}")
                array = store[user_input]
                maxRows = array.shape[0]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                if rows > maxRows - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                print(array[0:rows])
            case "dir" | "proto":
                # Scalar int32 fields
                print(f"You chose {user_input}")
                array = store[user_input]
                maxRows = array.shape[0]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                if rows > maxRows - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                data = array[0:rows]
                if user_input == "dir":
                    # Show direction as FW/BW
                    for i, val in enumerate(data):
                        print(f"{i}: {val} ({'FW' if val == 0 else 'BW'})")
                else:
                    print(data)
            case "json":
                # String field stored as uint8 array
                print(f"You chose {user_input}")
                array = store[user_input]
                maxRows = array.shape[0]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                if rows > maxRows - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                # Convert bytes to strings
                for i in range(rows):
                    raw = array[i, :]
                    # Find null terminator and decode
                    null_idx = np.where(raw == 0)[0]
                    if len(null_idx) > 0:
                        raw = raw[:null_idx[0]]
                    json_str = bytes(raw).decode('utf-8', errors='ignore')
                    print(f"{i}: {json_str}")
            case "view":
                # Display complete transactions
                print("Viewing complete transactions")
                rows = int(input(f"How many transactions to view? "))
                if rows > store['id'].shape[0]:
                    print("Too many rows requested")
                    continue
                for i in range(rows):
                    print(f"\n--- Transaction {i} ---")
                    print(f"  id:    {store['id'][i]}")
                    print(f"  st:    {store['st'][i]}")
                    print(f"  dir:   {store['dir'][i]} ({'FW' if store['dir'][i] == 0 else 'BW'})")
                    print(f"  port:  {store['port'][i]}")
                    print(f"  proto: {store['proto'][i]}")
                    # Decode JSON string
                    raw = store['json'][i, :]
                    null_idx = np.where(raw == 0)[0]
                    if len(null_idx) > 0:
                        raw = raw[:null_idx[0]]
                    json_str = bytes(raw).decode('utf-8', errors='ignore')
                    print(f"  json:  {json_str}")
            case _:
                print("Bye")
                sys.exit(0)

# ========================================================================

# Get path from command line argument
if len(sys.argv) != 2:
    print("[python.read_zarr] Usage: python read_zarr.py <path_to_zarr>")
    exit(1)

zarr_path = sys.argv[1]

# Check if zarr file exists
if not os.path.exists(zarr_path):
    print(f"[python.read_zarr] Error: {zarr_path} not found")
    exit(1)

# Force zarr v2 format
zarr_format = 2

try:
    # Try opening as zarr v2 group first
    store = zarr.open_group(zarr_path, mode='r')
    print(f"[python.read_zarr] Successfully opened zarr store as group")

    keys = list(store.keys())
    print(f"[python.read_zarr] Zarr store keys: {keys}")

    # Print basic info about each array
    for key in keys:
        try:
            array = store[key]
            print(f"[python.read_zarr] {key}: shape={array.shape}, dtype={array.dtype}")
            # Print first few values if array is small
            if array.size <= 10:
                print(f"[python.read_zarr] {key} data: {array[...]}")
            
        except Exception as e:
            print(f"[python.read_zarr] Error reading array '{key}': {e}")
            
    # Interact and explore
    start_exploring(store)

except Exception as e:
    print(f"[python.read_zarr] Error opening as group: {e}")
    print("[python.read_zarr] Trying as single array...")
    try:
        # Try opening as single array
        array = zarr.open_array(zarr_path, mode='r')
        print(f"[python.read_zarr] Opened as single array: shape={array.shape}, dtype={array.dtype}")
        if array.size <= 10:
            print(f"[python.read_zarr] Data: {array[...]}")
    except Exception as e2:
        print(f"[python.read_zarr] Also failed as single array: {e2}")
