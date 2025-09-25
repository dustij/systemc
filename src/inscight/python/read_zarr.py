import zarr
import os
import sys

# This is for exploring the current zarr dataset
# It assumes keys = timestamps, responses, and commands
# This may not work later if we make changes
def start_exploring(store):
    print("Lets dig into this a bit...")

    keys = [k for k in store.keys()]

    for i in range(len(keys)):
        print(keys[i])

    running = True;
    while running:
        user_input = input("Choose: ")
        match (user_input):
            case "timestamps":
                key = "timestamps"
                print(f"You chose {key}")
                array = store[key]
                maxRows = array.shape[0]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                if rows > maxRows - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                print(array[0:rows])
            case "commands":
                key = "commands"
                print(f"You chose {key}")
                array = store[key]
                maxRows = array.shape[0]
                maxCols = array.shape[1]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                cols = int(input(f"Cols (max {maxCols-1}): "))
                if rows > maxRows - 1 or cols > maxCols - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                print(array[0:rows, 0:cols])
            case "responses":
                key = "responses"
                print(f"You chose {key}")
                array = store[key]
                maxRows = array.shape[0]
                maxCols = array.shape[1]
                rows = int(input(f"Rows (max {maxRows-1}): "))
                cols = int(input(f"Cols (max {maxCols-1}): "))
                if rows > maxRows - 1 or cols > maxCols - 1:
                    print("Nope, huh uh, that's not allowed buddy")
                    continue
                print(array[0:rows, 0:cols])
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
