import zarr
import os
import sys

# Get path from command line argument
if len(sys.argv) != 2:
    print("[python.read_zarr] Usage: python read_zarr.py <path_to_zarr>")
    exit(1)

zarr_path = sys.argv[1]

# Check if zarr file exists
if not os.path.exists(zarr_path):
    print(f"[python.read_zarr] Error: {zarr_path} not found")
    exit(1)

try:
    # Try to open zarr store with different approaches
    store = zarr.open(zarr_path, mode='r')
    print(f"[python.read_zarr] Successfully opened zarr store")

    # Try to get keys safely
    try:
        keys = list(store.keys())
        print(f"[python.read_zarr] Zarr store keys: {keys}")

        # Print basic info about each array
        for key in keys:
            try:
                array = store[key]
                print(f"[python.read_zarr] {key}: shape={array.shape}, dtype={array.dtype}")
            except Exception as e:
                print(f"[python.read_zarr] Error reading array '{key}': {e}")

    except Exception as e:
        print(f"[python.read_zarr] Error listing keys: {e}")
        # Try alternative approach - check if it's a single array
        try:
            print(f"[python.read_zarr] Store shape: {store.shape}, dtype: {store.dtype}")
        except:
            print("[python.read_zarr] Could not determine store structure")

except Exception as e:
    print(f"[python.read_zarr] Error opening zarr store: {e}")
    print("[python.read_zarr] Trying with zarr v2 compatibility...")
    try:
        import zarr
        store = zarr.open_group(zarr_path, mode='r')
        print(f"[python.read_zarr] Opened as group, keys: {list(store.keys())}")
    except Exception as e2:
        print(f"[python.read_zarr] Also failed as group: {e2}")